<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-21T20:17:27.762Z","dependsOn":[]} -->
# Engine-owned Frame registry replacing the game Targets collection

## Context

The game owns a `Targets` collection whose only purpose is to let one collection
find rows of another: Spaceships publish a target handle, Missiles and player
homing consume it through `Frame::GetMissileTarget`, and the collection stores a
duplicated copy of positions, alignments, and subscriber counts that must be
serialized, CRC'd, transferred, and version-gated like real simulation state.

Separately, engine server code reaches into the game `PlayersPostRender`
collection for identity questions — destination liveness, global-id to
frame-local uuid lookup, and the arrival client-GUID bind — because no engine
seam answers them.

This Plan replaces `Targets` with `engine::FrameRegistry`: a universal,
engine-owned, transient registry that binds existing game SOA columns directly
and answers both kinds of question — batched spatial acquisition during the tick,
and main-thread identity/liveness lookup after the tick — without copying entity
records, without exposing row indices, and without adding persistent state.

The design is the one the user selected through `/external-design-interface`:
Design 2 (transient POD layer views, batched spatial query context, post-tick
ownership layer, exactly one named write) enhanced with Design 3's scalar
convenience one-liners layered over the ownership layer. This Plan supersedes the former
`GenericSpatialQueries` feature document, deleted by the same session that
created this Plan (see Git history). What carries forward unchanged is its
spatial semantics: radius always applied, the subscriber-then-angle selection
policy, previous-position fallback, retention and release rules, the query-window
timing, and preserved uuid consumption order. The deliberate deltas are that the
configurable rank keys become one fixed ranking, the ownership layer and its
single write are new, the scratch contract is revised into the caller-made
two-part allocation described under **Lifecycle**, and the fixture and acceptance
criteria are expanded.

## Design

### Shape

Two transient bindings over columns the game already owns, plus free functions:

- **Query context** — built once per query window from source layers (id,
  current position, optional previous position, optional alignment, eligible
  rows) and subscription layers (consumer target columns). Used from tick phases;
  batched.
- **Ownership layer** — one POD layer (id as bytes, optional global id, optional
  non-const client-GUID column) built by value on the main thread after the tick.
  Used for identity, liveness counting, and the single named write.

The registry allocates nothing and stores nothing between windows. Scratch is one
caller-made exact-size workbuffer allocation per query window holding only row
references, results, and derived `uint8_t` subscriber counts — never source
entity records.

### Public interface

`Engine/Source/Frame/FrameRegistry.h` and `.cpp`, namespace `engine`, both
builds, no `BT_CLIENT`/`BT_SERVER` guards:

```cpp
namespace engine
{

struct RegistryEntryTag;
using registry_id_t = id_t<RegistryEntryTag>;

struct RegistrySourceLayer
{
	const registry_id_t* puiIds = nullptr;              // Required; unique across all bound layers
	const XMVECTOR* pVecCurrentPositions = nullptr;     // Required for spatial queries
	const XMVECTOR* pVecPreviousPositions = nullptr;    // Optional; results fall back to current
	const alignment_t* pAlignments = nullptr;           // Optional; enables Alignments::CanCollide
	std::span<const int64_t> rows {};                   // Eligible rows: strictly ascending, unique, in range
	int64_t iSourceCount = 0;
};

struct RegistrySubscriptionLayer
{
	const registry_id_t* puiTargets = nullptr;
	std::span<const int64_t> rows {};
	int64_t iSourceCount = 0;
};

struct RegistryFilter
{
	float fRadius = 0.0f;
};

struct RegistryResult
{
	registry_id_t id {};
	XMVECTOR vecCurrentPosition {};
	XMVECTOR vecPreviousPosition {};
};

// rows and results are caller-provided storage, sized by the caller from its own consumer count.
struct RegistryBatch
{
	registry_id_t* puiTargets = nullptr;
	const XMVECTOR* pVecOrigins = nullptr;
	const XMVECTOR* pVecDirections = nullptr;
	const alignment_t* pAlignments = nullptr;
	std::span<const int64_t> rows {};
	std::span<RegistryResult> results {};
	int64_t iSourceCount = 0;
};

struct RegistryQueryContext
{
	std::span<const RegistrySourceLayer> sourceLayers {};
	std::span<uint8_t> subscriberCounts {};
	const Alignments* pAlignments = nullptr;
};

// Registry-internal, source-derived storage only: the eligible source rows and the derived
// subscriber counts. It does not and cannot size RegistryBatch::rows or RegistryBatch::results,
// which scale with the caller's consumer count, not with the source layers.
[[nodiscard]] int64_t RegistryScratchBytes(std::span<const RegistrySourceLayer> sourceLayers);

[[nodiscard]] RegistryQueryContext BuildRegistryQueryContext(const Alignments& rAlignments,
	std::span<const RegistrySourceLayer> sourceLayers,
	std::span<const RegistrySubscriptionLayer> subscriptionLayers,
	std::span<std::byte> scratch);

// Fixed policy: fewest subscribers, then smallest angle, ties by layer order then row order.
void AcquireRegistryTargets(RegistryQueryContext& rContext, const RegistryBatch& rBatch,
	const RegistryFilter& rFilter);

[[nodiscard]] bool ResolveRegistryHandle(const RegistryQueryContext& rContext, registry_id_t id, RegistryResult& rResult);

void ReleaseRegistryTarget(RegistryQueryContext& rContext, registry_id_t& rId);

// Ownership layer: main-thread, post-tick identity and liveness.
// Binds a foreign-typed id column as bytes. A cross-type view (const uuid_t* or
// const registry_id_t* over an id_t<T> array) is undefined behavior, so the
// implementation reads each element back through its true type instead.
template <typename T>
[[nodiscard]] const std::byte* RegistryIdBytes(const id_t<T>* puiIds)
{
	static_assert(sizeof(id_t<T>) == sizeof(uuid_t));
	static_assert(std::is_trivially_copyable_v<id_t<T>>);
	return reinterpret_cast<const std::byte*>(puiIds);
}

// The caller holds this layer by value for as long as it uses it. There is no multi-layer view type:
// a span over a caller-built temporary layer would dangle the moment the builder returned.
struct RegistryOwnershipLayer
{
	const std::byte* pIdBytes = nullptr;                // Required; element stride sizeof(uuid_t), bind via RegistryIdBytes
	const global_id_t* pGlobalIds = nullptr;            // Optional; a layer without it matches no global id
	ClientGuid* pClientGuids = nullptr;                 // Optional; sole write channel, CRC-excluded columns only
	int64_t iCount = 0;
};

struct RegistryIdentity
{
	uuid_t uuid {};
	global_id_t globalId {};
	ClientGuid clientGuid {};
	bool bClientOwned = false;
};

// Counts every bound row; the caller's choice of layer is the filter.
[[nodiscard]] int64_t CountRegistryRows(const RegistryOwnershipLayer& rLayer);

// The registry's only write.
bool AssignRegistryClientGuid(const RegistryOwnershipLayer& rLayer, global_id_t globalId, const ClientGuid& rGuid);

// Scalar lookups over the ownership layer; inline, no new state.
[[nodiscard]] uuid_t RegistryUuidByGlobalId(const RegistryOwnershipLayer& rLayer, global_id_t globalId); // {} when absent
[[nodiscard]] bool FindRegistryIdentity(const RegistryOwnershipLayer& rLayer, global_id_t globalId, RegistryIdentity& rIdentity);

}
```

The ownership layer's id column is type-erased because the registry must read id
columns whose element type it does not know (`PlayersPostRender::puiIds` is
`game::player_t`, not `registry_id_t`). Forming a `const registry_id_t*` or a
`const uuid_t*` over an `id_t<T>` array and indexing it is undefined behavior in
C++23 — distinct `id_t` specializations are not similar types, so the cross-type
read is not permitted, and pointer arithmetic through the first member does not
traverse the array. This verdict came back REFUTED from external verification of
the "pointer-interconvertible so a cast is fine" claim; do not simplify the erased
bind back into a cast. `RegistryIdBytes` performs the one legal conversion (object
pointer to `std::byte*`) and `static_assert`s the size and trivial-copyability
preconditions that `Engine/Source/Frame/Collections/CollectionId.h` satisfies;
the implementation reads element `i` with `std::bit_cast<uuid_t>` (equivalently
`std::memcpy`) from `pIdBytes + i * sizeof(uuid_t)`.

This erased bind exists only for cross-typed columns. The spatial source and
subscription layers bind `SpaceshipsInterpolate::puiRegistryIds` and
`MissilesPostRender::puiRegistryTargets`, which are natively `registry_id_t`, so
they stay typed pointers with no erasure and no conversion.

Naming details may be adapted only where repository style forces it (Hungarian
prefixes, `r`/`p` parameter prefixes, member ordering). The set of types,
functions, parameters, and semantics is fixed.

### Settled decisions

These are final; the implementation makes no further choices about them.

- **Transient only.** No persistent registry collection, no duplicated positions,
  alignments, radii, subscriber counts, or identity records. Nothing the registry
  produces is serialized, CRC'd, or transferred.
- **One write.** `AssignRegistryClientGuid` is the registry's only mutation of
  game data. It is legal only on the main thread, after the tick, through an
  ownership layer whose `pClientGuids` is bound to a CRC-excluded column
  (`PlayersPostRender::SharedCrcMembers()` already excludes `pClientGuids`).
  Debug builds validate main-thread affinity at the call.
- **Game supplies the views.** The engine never names a game collection. New
  `game::Frame` helpers build the layers: the Missile-Update and Player-Spawn
  spatial windows described below, plus
  `Frame::OwnershipLayer(const Frame&)` returning one
  `engine::RegistryOwnershipLayer` **by value** over `PlayersPostRender` with
  `pIdBytes = engine::RegistryIdBytes(pPlayers->puiIds)`,
  `pGlobalIds = pPlayers->pGlobalPlayerIds`, and
  `pClientGuids = pPlayers->pClientGuids`. Callers keep that returned layer in
  their own storage and pass it to the ownership functions; the registry defines
  no multi-layer ownership view type, because a span held by such a view could
  only point at the builder's dead local.
- **Players gain no new column.** The ownership layer binds existing columns only.
- **Ownership counting is layer-scoped.** `CountRegistryRows` counts every bound
  row and applies no predicate. The only ownership layer this Plan creates binds
  `PlayersPostRender` alone, so its count is the live player count; any narrower
  question is answered by binding a narrower layer, not by a filter argument.
  `RegistryIdentity::bClientOwned` and `clientGuid` stay on results, because the
  user-directed identity data callers need is per-row, not a filter.
- **Identity results.** On a hit, `FindRegistryIdentity` returns true and fills
  `uuid` from the layer's id bytes, `globalId` with the matched id, `clientGuid`
  with a copy of the bound row's `pClientGuids` entry, and
  `bClientOwned = !clientGuid.IsEmpty()` — the established emptiness predicate at
  `Engine/Source/Network/NetworkProtocol.h:99`. When `pClientGuids` is not bound,
  a hit leaves `clientGuid` empty and `bClientOwned` false. On a miss it returns
  false and leaves every `RegistryIdentity` field at its default. A layer with no
  `pGlobalIds` never matches, so every global-id lookup against it is a miss.
- **`SpaceshipsInterpolate::puiTargets` becomes `puiRegistryIds`**, typed
  `engine::registry_id_t`, in the same `SharedMembers()` and `PersistentMembers()`
  slot, CRC-included as today; `kiVersion` 2 → 3.
- **`MissilesPostRender::puiTargets` becomes `puiRegistryTargets`**, typed
  `engine::registry_id_t`, in the same `SharedMembers()` slot, CRC-included as
  today; `kiVersion` 10 → 11.
- **`Targets` is deleted** — the collection pair, its sources, project entries,
  explicit instantiations, `Frame` pointers and construction, tuple entries,
  agent query arm, `game::target_t`, `Frame::GetMissileTarget`, and version terms.
- **Frame version.** The current expression at
  `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:18` is
  `124 + engine::kiNavDataVersion(14) + Blasters(1+2) + Missiles(2+10) +
  Players(13+19) + Spaceships(2+6) + Targets(2+1) + engine Explosions/Pushers
  (1+1+1+1)`, evaluating to 200. After this change: base literal 124 → 126 (+2),
  removing the two Targets terms (−3), `SpaceshipsInterpolate` 2 → 3 (+1),
  `MissilesPostRender` 10 → 11 (+1) — evaluating to 201.
- **No compatibility.** Version-200 saves and replays are not readable
  afterwards, and no compatibility path is written.
- **No wire change.** No new message type and no protocol-version bump.
- **Files.** `Engine/Source/Frame/FrameRegistry.h` and
  `Engine/Source/Frame/FrameRegistry.cpp`, in both client and server projects and
  filters, with the header aggregated in `Engine.h`'s Frame include section.

### Query semantics (carried forward verbatim)

- Source ids must be valid and unique across all eligible rows in all bound
  layers; debug builds validate this.
- Nonempty row spans are strictly ascending, in range, and duplicate-free.
- Each live consumer row appears exactly once across subscription layers.
- Source and consumer alignment arrays together enable `Alignments::CanCollide`;
  omitting either disables alignment filtering.
- Acquisition always applies `fRadius`. There is no separate visibility check.
- Acquisition ranking is one fixed policy with no configuration: fewest
  subscribers first, then smallest angle. Exact ties resolve by source-layer
  order, then source-row order.
- Acquisition does not filter by ownership; the caller's choice of source layers
  is the only candidate filter, matching today's behavior.
- Absent previous positions make results copy the current position into both
  fields.
- Retention checks only handle existence among currently eligible source rows;
  range, alignment, and ranking apply only during acquisition.
- Release decrements scratch accounting when the source still exists and always
  clears the consumer handle.
- A context never survives source-layer movement, reallocation, or cell transfer.
  It may survive consumer-collection growth once subscription binding has been
  consumed.

### Lifecycle

Scratch: one exact-size workbuffer allocation per window, made by the caller and
covering two separately sized parts. The registry itself allocates nothing.

- The registry-internal part is sized by `RegistryScratchBytes(sourceLayers)` and
  handed to `BuildRegistryQueryContext`. It exactly sizes the source-derived
  storage only: the eligible source rows and the derived `uint8_t` subscriber
  counts.
- The acquisition rows and `RegistryResult` records passed through
  `RegistryBatch::rows` and `RegistryBatch::results` are caller-provided storage
  sized from the caller's own consumer count (Missiles), which no source-layer
  function can know. Each window below adds that consumer-derived size to the same
  single allocation.

No further allocation happens while the context is live.

Spaceship publishing: generate the registry id at the exact former Targets-add
location after pusher creation, preserving UUID consumption order; existing
copy/swap operations carry it with the row; clear it at existing owner teardown
and source-cell transfer points, with the destination spawn generating a new
cell-local id.

Missile Update window: bind current and previous Spaceship positions before
Spaceships Update; build eligible Spaceship rows from valid current registry ids
and previous-frame arrival-grace state, matching the former `kDestination`
timing; derive subscriber counts from previous-frame Missile handles because
current handles are initialized during Update; preserve per-Missile RNG and
physics order; resolve retained handles against current eligibility, with
retained Missiles homing from the previous position; release subscriptions during
falling, exploding, destruction, and lifetime transitions; write every local
handle, including invalid ones, into the current Missile column; append only
still-active, targetless Missiles to the acquisition rows and acquire them in
ascending Missile order after the loop, with acquired handles stored but not
homing until the next tick.

Player Spawn window: rebuild source eligibility and subscriber counts after
Update, Transfer, and Destroy from current Spaceship lifecycle state and fully
initialized current Missile handles; reuse that context through the player spawn
loop; submit each player's acquisition as a one-entry batch immediately before
its Missile spawn, preserving existing RNG and UUID interleaving. Missile
collection growth during the loop is permitted.

Ownership layer: built by value on demand on the main thread after the tick from
the current `FramePostRender` and held in the caller's own storage; never
retained across a tick, a transfer, or a collection reallocation.

## Critical files

- `Engine/Source/Frame/FrameRegistry.h`, `Engine/Source/Frame/FrameRegistry.cpp` — new.
- `Engine/Source/Engine.h` — Frame-section aggregation of the new header.
- `Engine/Source/Frame/AGENTS.md` — registry ownership, windows, scratch lifetime, the single write.
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.h`, `Frame.cpp` — Targets pointers/construction removal, `kiVersion` base, `GetMissileTarget` removal, the new view helpers including `Frame::OwnershipLayer`.
- `Projects/BrokenEngineSandbox/Source/Frame/FrameCollections.h` — Targets tuple entries.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Targets/` — deleted.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/Spaceships.h`, `Spaceships.cpp`, `SpaceshipsCombat.cpp` — column rename/retype, version, add/clear/sync/transfer sites.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/Missiles.h`, `Missiles.cpp`, `MissilesUpdate.cpp` — column rename/retype, version, retention/release/acquisition.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersCombat.cpp` — player Missile spawning acquisition.
- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServerQueries.cpp`, `AgentCommands.cpp` — agent row extensions and the removed Targets query arm and frame count.
- `Projects/BrokenEngineSandbox/Source/Agent/AgentScene.cpp` — scene `counts` Targets total.
- `Projects/BrokenEngineSandbox/Source/Server/ServerDisplay.cpp` — Targets profile counter, repaint hash, cell entity sum, overlay line.
- `Projects/BrokenEngineSandbox/Source/Profile/ProfileManager.h` — Targets CPU counter enumerator and name.
- `Projects/BrokenEngineSandbox/Documents/AgentHarness.md` — the `query_collection` targets option and the `query_frame` and `describe_scene` counts schemas.
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/*.vcxproj`, `*.vcxproj.filters` — membership for added and deleted files.
- `.agents/skills/add-collection/SKILL.md`, `.agents/skills/add-collection-member/SKILL.md` — the Targets exemplar rows these skills cite stop existing.
- `Documents/Architecture/FrameUpdatePipeline.md`, game `Frame/AGENTS.md` and `Frame/Collections/AGENTS.md`, `Agent/AGENTS.md` — pipeline, collection, and agent documentation.

## In scope

- Add `Engine/Source/Frame/FrameRegistry.h/.cpp` with exactly the types and free
  functions listed under **Public interface**, the deterministic direct scan
  behind `AcquireRegistryTargets`, `ResolveRegistryHandle`,
  `ReleaseRegistryTarget`, `RegistryScratchBytes`,
  `BuildRegistryQueryContext`, `RegistryIdBytes`, and the ownership functions
  `CountRegistryRows`, `AssignRegistryClientGuid`,
  `RegistryUuidByGlobalId`, and `FindRegistryIdentity`.
- Aggregate the new header in `Engine.h`'s Frame section and add both files to
  the client and server projects and filters.
- In `SpaceshipsInterpolate`: rename and retype `puiTargets` to
  `puiRegistryIds` (`engine::registry_id_t`) in the declaration,
  `SharedMembers()`, `PersistentMembers()`, and `LogDifferences`; bump
  `kiVersion` 2 → 3; update the id generation site in
  `SpaceshipsPostRender::Spawn`-side code (`Spaceships.cpp`, current
  `TargetsPostRender::Add` call), `SyncSpaceship`, the arrival-grace sync, the
  teardown/transfer clears in `Spaceships.cpp` and `SpaceshipsCombat.cpp`.
- In `MissilesPostRender`: rename and retype `puiTargets` to
  `puiRegistryTargets` (`engine::registry_id_t`) in the declaration,
  `SharedMembers()`, and `LogDifferences`; bump `kiVersion` 10 → 11; migrate the
  retention, release, spawn-assignment, and destruction sites in `Missiles.cpp`
  and the Update homing/acquisition loop in `MissilesUpdate.cpp` to the registry.
- Build the two spatial windows as game `Frame` helpers: the Missile-Update
  context and the Player-Spawn context described under **Lifecycle**, including
  the eligible-row and subscriber-count construction and their workbuffer scratch.
- Add `Frame::OwnershipLayer(const Frame&)` returning one
  `engine::RegistryOwnershipLayer` by value over `PlayersPostRender` as
  specified, and delete `Frame::GetMissileTarget`.
- Migrate player Missile spawning in `PlayersCombat.cpp` to one-entry
  `AcquireRegistryTargets` batches at the existing call points.
- Delete the `Targets` collection pair, its directory, project and filter
  entries, explicit instantiations, `Frame` pointers and construction,
  `FrameCollections.h` tuple entries, `game::target_t`, and the Targets terms in
  `Frame::kiVersion`; change the `Frame::kiVersion` base literal 124 → 126.
- Extend the agent Spaceship rows with `registryId` and Missile rows with
  `registryTargetId` (replacing `spatialQueryId`/`spatialTargetId` from the
  superseded feature document) and remove the Targets agent query and
  `ExtractTargets`: in `AgentCommandsServerQueries.cpp`, the
  `rResult["targets"]` count line in `CommandQueryFrame` (`:179`) and the
  `collection == "targets"` arm in the `query_collection` dispatch (`:219`).
- Remove the deleted `targets` collection option and its target-row field list
  from the `query_collection` line of
  `Projects/BrokenEngineSandbox/Documents/AgentHarness.md` (`:371`), leaving the
  remaining collections' text unchanged.
- Remove `targets` from the `query_frame` response schema on the same file's
  `:369` — "Returns counts under `players`, `spaceships`, `missiles`,
  `blasters`, and `targets`." — leaving the four remaining counts listed.
- Remove `targets` from the `describe_scene` response schema on the same file's
  `:399`, in the cell-wide
  ``counts{players,spaceships,missiles,blasters,targets}`` term, leaving the four
  remaining counts and the rest of that line unchanged. These two schema lines
  and the `query_collection` line are the only `targets` text this Plan removes
  from that document.
- Remove the Targets reads in `Projects/BrokenEngineSandbox/Source/Agent/AgentScene.cpp`:
  the `Frame/Collections/Targets/Targets.h` include (`:9`), the `iTargetTotal`
  accumulator declaration (`:166`), its `rFrame.postRender.pTargets->iCount`
  accumulation (`:203`), and the `{"targets", iTargetTotal}` entry in the scene
  `counts` object (`:275`). The scene report counts live collection rows, and the
  registry keeps no rows, so this reports nothing after the deletion; no
  registry-derived substitute is introduced.
- Remove the Targets reads in `Projects/BrokenEngineSandbox/Source/Server/ServerDisplay.cpp`:
  the `Frame/Collections/Targets/Targets.h` include (`:14`), the `iTotalTargets`
  accumulator (`:94`), its `pTargets->iCount` accumulation (`:104`), the
  `SetCount(game::kCpuCounterTargets, ...)` call (`:112`), the
  `Mix(rFrame.interpolate.pTargets->iCount)` repaint-hash term (`:159`), the
  `pTargets->iCount` term in the per-cell `iEntityCount` sum (`:320`), and the
  `"Targets: %lld"` overlay line (`:634`). The Targets rows were one per
  Spaceship, so the neighbouring Spaceship terms already carry that repaint and
  cell-label signal; no replacement term is added.
- Remove the Targets profile counter in
  `Projects/BrokenEngineSandbox/Source/Profile/ProfileManager.h`: the
  `kCpuCounterTargets` enumerator (`:18`) and its `"Targets"` entry in
  `kGameCpuCounterNames` (`:62`), which are removed together so the existing
  `std::size(kGameCpuCounterNames)` static assert still holds.
- Add the debug-input-only `registry_fixture` described under
  **Acceptance criteria**.
- Update the Targets exemplar citations in `.agents/skills/add-collection/SKILL.md`
  and `.agents/skills/add-collection-member/SKILL.md` to a collection that still
  exists, keeping each row's teaching point intact.
- Update `Documents/Architecture/FrameUpdatePipeline.md`, engine
  `Frame/AGENTS.md`, and the game Frame, Collections, and Agent `AGENTS.md`
  files for the two query windows, the ownership layer and its single write,
  scratch lifetime, id uniqueness, and deterministic ordering.
- Satisfy the `/add-collection-member` layout checklist for both retyped columns
  and run `pwsh -NoProfile -File .agents/scripts/Test-CollectionLayout.ps1`.

The listed regions are the target and the ceiling. Adjacent cleanup or
abstraction is unauthorized unless mechanically required by these changes.

## Out of scope

- **Migrating the engine server consumers.** `ServerTransferManager.cpp`
  (destination liveness at `IsDestinationLive`, the find-and-bind at
  `TrackClientTransfers`) and `ServerBroadcaster.cpp` (global-id to uuid lookup)
  keep their current Player-collection reads in this Plan. Moving them onto the
  ownership layer and the scalar wrappers belongs to
  `Documents/Plans/Engine/ServerTransferPlayerContract.md`, which depends on this
  Plan. This Plan builds the registry and migrates the Missile/Targets machinery
  only.
- Persistent registry collections or copied positions, alignments, radii,
  subscriber counts, or identity records.
- Spatial grids, hashes, acceleration structures, reverse publisher identity,
  event dispatch, raw row handles, or two-stage candidate filtering.
- Fluent APIs, view objects with behavior, inheritance, callbacks, virtual
  dispatch, game-policy templates, categories, generic attributes, per-source
  radii, or distance ranking.
- Any registry write other than `AssignRegistryClientGuid`, and any write at all
  from a tick phase or a worker thread.
- Player target-selection policy changes, backward compatibility with
  version-200 saves or replays, wire-format or protocol-version changes, and
  unit tests.

## Risk tier and invariants

Tier 3. The change alters CRC composition through two collection versions and the
`Frame::kiVersion` base, changes save/replay and full-frame layout, removes a
serialized collection, moves deterministic acquisition ordering into engine code,
integrates with tick phase ordering and per-cell parallel scratch lifetime, and
spans independently owned Engine and game subsystems.

Preserve: PostRender bit determinism and the shared CRC stamp; UUID consumption
order at the former Targets-add site; per-Missile RNG and physics order; the
existing player spawn RNG/UUID interleaving; Interpolate/PostRender count and
capacity parity; transfer semantics for the retyped persistent columns; the
allocation-tracking contract (one workbuffer allocation per window, none while a
context is live); main-thread affinity for the ownership layer and its single
write; and the CRC exclusion of `pClientGuids`.

## Acceptance criteria

- `pwsh -NoProfile -File .agents/scripts/Test-CollectionLayout.ps1` reports no
  layout or version violations.
- `/update-vcxproj` validates client and server membership and filters for the
  added and deleted files.
- BrokenEngineSandbox client and server both compile through `/compile`.
- A scoped source search finds no `Targets` collection types, no `pTargets`, no
  `game::target_t`, no `TargetsInterpolate`/`TargetsPostRender`, no
  `ExtractTargets`, and no `GetMissileTarget`, and no `.agents/skills` file cites
  a `Targets` exemplar path.
- The evaluated `Frame::kiVersion` changes from 200 to 201, with the base literal
  at 126.
- The debug-input-only `registry_fixture`, following the existing
  collection-layout fixture pattern and using the real public API over fixed
  arrays, proves: the fixed subscriber-then-angle distribution; radius and
  alignment acceptance and rejection; stable resolution after source-row
  permutation and context rebuild; release and reacquisition after removing an id
  from the eligible rows; a lower layer and row winning an exact tie; the
  registry-internal scratch containing no copied source records and being exactly
  `RegistryScratchBytes(sourceLayers)` while the batch rows and results come from
  separate caller storage; `CountRegistryRows` returning the total bound row
  count; `FindRegistryIdentity` and `RegistryUuidByGlobalId` agreeing on a hit,
  and a miss leaving every `RegistryIdentity` field at its default and returning
  `{}`; `FindRegistryIdentity` populating identity ownership exactly as the
  **Identity results** decision states — a hit on a row whose `pClientGuids`
  entry is nonempty copying that exact guid and setting `bClientOwned` true, a
  hit on a row whose entry is empty leaving the guid empty and `bClientOwned`
  false, a hit through a layer with `pClientGuids` unbound likewise leaving the
  guid empty and `bClientOwned` false while still returning that row's uuid and
  global id, and a miss leaving the guid empty and `bClientOwned` false;
  `AssignRegistryClientGuid` on a hit changing exactly the matching row's
  `pClientGuids` entry with every other row's bytes unchanged and returning true,
  and on a miss changing no row at all and returning false; and a cross-typed
  ownership layer bound through `RegistryIdBytes` over an `id_t<T>` array
  resolving the same identities as a natively typed one.
- A source-order inspection confirms the spaceship spawn path consumes the
  per-frame uuid counter in the same order as today: the pusher id first (the
  `engine::PushersPostRender::Add` call, `Spaceships.cpp:585` region) and the
  registry id second, generated at the exact position the removed
  `TargetsPostRender::Add` call occupied (`Spaceships.cpp:594` region). The
  `registry_fixture` additionally spawns one spaceship through the normal spawn
  path and confirms the frame uuid counter advanced by exactly two, with the
  spaceship's pusher id being the first of the two values and its registry id the
  second.
- Through `/agent-harness`: every nonzero live Missile registry target equals a
  current Spaceship registry id; a transferred Spaceship receives a valid
  destination-local id; a transferred Missile arrives with an invalid target; the
  replay scenario produces matching client/server and replay CRCs, with no new
  transfer, CRC, or desync errors.
- Independent code inspection confirms previous-state eligibility during Missile
  Update, current-state eligibility during Spawn, post-transition
  acquisition-row construction, acquired-target homing delay, and that
  `AssignRegistryClientGuid` is the only registry write and is unreachable from a
  tick phase.
- No unit tests are added.

## Execution card

- Objective: replace the game `Targets` collection with a universal transient
  engine `FrameRegistry` providing copy-free batched spatial acquisition and a
  main-thread ownership layer, preserving current Missile targeting behavior.
- Tier: 3 — CRC composition, save/replay layout, deterministic ordering, phase
  integration, parallel-cell scratch lifetime, engine/game span.
- Roles: disjoint implementers for the engine registry, the collection layout
  changes, the game Frame/window wiring, and documentation/skill citations;
  `/update-affected-code` propagation; a builder for client and server;
  C++, scope, and adversarial reviewers; style, project-membership, and docs
  cleanup; harness verification; a final read-only acceptance review and the
  landing flow.
- Any change to the public interface, identity rules, scan order, query windows,
  the single-write rule, or the out-of-scope boundaries requires a new Tier-3
  plan audit.
