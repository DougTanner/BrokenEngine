<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-03T16:52:02.244Z","dependsOn":[]} -->
# Battleship wreck collections and two seconds of hull explosions

## Context

Replace the client-only `sWrecks` vector with a real paired SOA Collection. A combat-dead battleship hands off its exact death-tick pose, retains its fall/sink/terrain behavior, and emits the existing complete explosion effect for two seconds from random convex-hull anchors on the moving wreck.

Evidence baseline: `d1d471c956fd559d7296faab7318f693249d8cad`. `BattleshipsRender.cpp::UpdateWrecks` captures
interpolated dying ships and advances a vector using render delta. `kBattleshipDestroyTime` is only 0.25
seconds. `ExplosionsPostRender::Spawn` appends shared rows and consumes the shared random engine.
`HydrateReceivedFullState` reconstructs ship effects, but does not retain independent wreck history when
`ClientSessionRuntime` replaces snapshots.

This is one implementation stage. Saving this document is Tier 1; executing it is Tier 3 because it changes collection ownership, cross-subsystem effects, reconciliation handoff and shared RNG draws. Implementation requires the Tier-3 preparation, review and approval workflow before source edits.

## Design

### Persistent client owner

Add a BT_CLIENT-only `BattleshipVisuals` owner under game `Source/Frame/Collections/BattleshipWrecks/`, held
by `Game::mBattleshipVisuals`, keyed by GridCoord. Each entry owns one
`BattleshipWrecksInterpolate/PostRender` pair and instances of the existing Explosions, PointLights, Puffs,
SmokeTrails and WindRadials pairs. These objects persist outside authoritative snapshots, exactly where the
current vector's lifetime lives. They contain no game Frame or shared simulation state.

The owner has matching `interpolate.Collections()` and `postRender.Collections()` tuples in this order:
Wrecks, Explosions, PointLights, Puffs, WindRadials, SmokeTrails. These tuples serve allocation/copy and
`CompactCollections`, including existing `OwnedHandles()` release. They are local owner tuples, not additions
to FrameBase or game Frame collection tuples. Do not introduce GameServer collection subsets, ClientExplosions
types, frame serialization or version folds for these instances.

Use normal `Collection<T>` allocation, `Members()`, paired growth/addition, PostRender `pLifecycleFlags`,
MarkDestroy and compaction. Explicitly instantiate the new Wrecks pair. Apply /add-collection and
/add-collection-member to storage/lifecycle; document the deliberate non-frame ownership and narrow explicit
dispatch exception in the owning Frame/Collections AGENTS. The standard game tuple registration rule applies
to frame-owned collections; this owner is a client visual lifetime outside that system.

Wreck Interpolate holds cell-local position (W=1), direction (W=0), frozen animation time, evaluated tilt,
fall age and speed. PostRender holds source battleship ID/controller, captured damage direction,
start/rest/top heights, target terrain quaternion, sink/rest mode, emission age, local RNG and lifecycle
flags. Initialize every column on Add. No external handle names a wreck, so no wreck ID map is needed. Its
containing owner supplies its coordinate; never store an absolute position.

The owner also holds committed previous/current Interpolate storage and a render scratch copy, using standard Collection allocation/copy helpers. Only committed local ticks mutate PostRender, RNG and ownership. Render interpolation writes scratch continuously from the committed state, and never spawns, expires, compacts or consumes RNG. New rows initialize previous and current to the exact captured pose at age zero.

### Exact death-tick handoff

At the end of game `FramePostRender::Destroy`, after Battleships Destroy and before its single
`CompactCollections`, copy each combat-dying ship's stored position, direction, controller, animation time,
damage direction, source ID and tick into a narrow pending capture list for its cell. Require nonnegative
destroyed time and exclude kDestroy/kTransfer removals. This reads the final tick pose after collision, area
damage and terrain resolution, not the render interpolate. Keep the shared 0.25-second removal, death sound,
stopped velocity and fleet-health exception.

Pending captures are value records for this one handoff, not wreck storage or a generic event system. Allocate
per-cell owner entries on the main thread before `ReconcileDispatcher::Run` dispatches workers. A coord worker
alone writes its coord's pending list; map topology and paired visual Collections are never mutated by
workers. Clear/replace pending captures in the unpresented rollback range before replay, then recapture the
resulting ticks. No locks or cross-cell mutation are required.

After reconciliation joins, retain pending records until presentation reaches their source tick. Consume them in source-tick order, retaining the earliest capture for each source ID,
from the existing Battleships BeginRender path before model buffer sizing. Deduplicate by cell
incarnation plus source ID; do not retain row indices across compaction. Keep consumed source IDs until the
source is absent from the current snapshot and older than the reconciliation ring, so pruning a submerged
wreck cannot recreate it from a still-dying row. Repeated/recalculated ticks never spawn a second wreck or GPU
burst.

The pending list bridges several ticks between renders, including a low-FPS burst that removes the original
row before the next render. Do not rely exclusively on scanning the finite snapshot ring. If an authoritative
replacement invalidates an unpresented capture, discard it; a still-dying replacement may supply one canonical
capture at its received stored pose/tick when no original death record exists. Never fabricate a death for an
already-absent ship. Presented local visuals are not rolled back or advanced again by reconciliation.

At first presentation of the death tick, Add the wreck with exact source pose and zero fall/emission age, emit its age-zero burst, and apply no tick or render delta to that new row during that call. Skip the corresponding dying live model only when its wreck is present. At the next local tick it advances normally. Captures whose source tick is still ahead of presentation remain pending, so speculative tail state cannot cause an early handoff.

### Local clock, map policy and lifetime

Advance this owner from the main-thread presentation path, once per new presented source tick, using
`kfDeltaTime`; retain a per-cell source-tick watermark. Multiple ticks between renders advance existing local
rows in order. Rendering the same tick again or reconciliation replay never advances it twice. The fixed-step
local update only walks the six owned visual pairs; it does not call RunFrameTick, simulate inactive frames or
construct a temporary game Frame.

Preserve the existing opaque-map rule: world-hidden frames freeze motion, emission, capture adoption and
visibility prune. Track the source watermark while hidden, without accumulating motion or emission debt. On
return discard pending deaths whose source has already disappeared; a still-dying ship may adopt its retained
exact death record at age zero. Pause freezes motion/emission and interpolation. Do not catch up hidden or
paused time. Map gates come from stable main-thread presentation inputs and never affect shared simulation.

While world-visible, remove owners outside the active/renderable set or the non-map world visible area. Mark
fully submerged hulls for destruction; their already-emitted children finish their own lifetimes while the
cell remains visible. Cell deactivation/re-subscription and `Game::ResetGameState` /
`ClientSession::OnServerLoad` discard the owner, pending records, watermark and consumed IDs. Recreated cells
start fresh, so reused local IDs cannot match old wrecks. Ordinary full-state hydration leaves existing local
owners and child handles intact.

At capture sample the existing hull-box center and four corners through the cell static data. Preserve
highest-sample rest height, sea-floor sink decision, top offset and target terrain normal. Use one motion
function for fixed stepping and render interpolation: one-second gravity ramp, gravity 9.8, water braking 30,
sink speed 0.5, terrain clamp and tilt fraction. Verify tick-end equals next-window-zero and exact t=0. Fixed
stepping changes numerical trajectories from the old render-delta integrator; preserve physical rules and
continuity, not bit-identical old trajectories.

### Persistent full explosion effects

Reuse the existing Explosions pair in the local owner; it is a separate instance of the existing type, not a
second registered type. Extract narrowly typed operations accepting explicit destination pairs, current time
and RNG from existing explosion/child functions where both shared and local destinations use them. Keep the
public shared Spawn and phase entry points as wrappers with their existing draw order, bounds checks and
recalculation suppression. No shared row or shared UUID/RNG is touched by local emission.

The local route uses the wreck RNG and the existing battleship ExplosionType/controller registries and tuning.
It spawns primary/secondary lights and puffs, wind deposits, smoke trails and GPU particles. All CPU children
live in this same persistent owner. Give its PointLights and SmokeTrails insertions a client-only UUID counter
through `AddGeneratedIndexableElement` / MakeUuid; handle lookup and `OwnedHandles()` resolve exclusively
against this owner's tuples. Never mix these handles with game Frame handles.

Use the same typed calculations for local Update/Destroy and controlled-child expiration. Advance explosion
trail heads before SmokeTrails update, release expired trail handles through this owner, then compact after
all destroy decisions. Fire-and-forget children expire independently after emission stops. Existing explosion
duration multipliers and cleanup behavior remain unchanged. A received shared snapshot cannot restart or
delete these children because no local row or handle is stored there; no hydration reconstruction payload or
handle rebinding is needed.

Rendering appends local child instances into existing per-type buffers after the ordinary frame instances and
before their EndRender publication. Factor only the actual collection render/capacity calculation needed by
these two consumers into typed overloads. Reserve combined capacities once; reset counts once; append each
visible cell once; publish final counts once. Include smoke/wind deposit paths, not only sprites. Rebase
cell-local CPU positions at GPU submission and particles at their existing spawn boundary. GPU particles use
their existing lifetime/reset mechanism and are submitted only on a committed local emission.

Battleship live/wreck rendering shares the existing narrow model-layout calculation, model alignment, yaw/tilt
order, frozen animation and skinning convention. Keep Battleships BeginRender/Render/EndRender as the game
tuple entry points; append the owner's wreck rows from these hooks rather than adding Wrecks to game tuples.
Each controller batch reserves valid combined skinning slots. Preserve the separate Rendered and Wrecks
counter meanings and zero-count publication when no cell is renderable.

### Hull anchors and two-second emission

Use `gShipHitboxShapes[kBattleship][controller].hitbox`, whose cached hull includes authored alignment and the -Y-to-+X correction, including box-level models. Do not use BuildShipHitbox's optional transformed outline or apply alignment twice. Area-weight a vertex-zero triangle fan and sample a selected triangle with reflected-square barycentric coordinates. Require valid nondegenerate admitted content; do not substitute a circle or bounding box.

Use aligned model-local Z=0, then exactly the yaw, evaluated tilt and cell-local translation used by the wreck
renderer. Emit a complete burst immediately at age zero, then once per advancing local fixed tick while age is
strictly below two seconds: 64 emissions at 32 Hz, ages 0 through 63/32. At each emission sample a fresh
anchor on the current moving/tilted hull. Use normalized two-second age for existing declining size/smoke/time
tuning and retain damage-direction jitter; add no primary position jitter after sampling.

Primary anchors may lie outside the owning hexagon when the hull crosses an edge. The local visual route accepts those cell-relative anchors, retains W=1 and the owning coordinate, and performs no transfer or neighbouring-cell mutation. The shared route keeps its existing inside-cell admission. Secondary flashes, trails and particles may expand beyond the primary silhouette.

Remove old shared SpawnBattleshipExplosion calls/helper, staggered death emission, and the unused
`pDestroyedExplosionTimes` column/producers/logging/harness field. Keep other ship and shared explosion
callers unchanged. Increment BattleshipsPostRender::kiVersion for the removed shared column, rejecting old
saves/replays. Removed shared draws intentionally change subsequent shared RNG in both builds; new client
emission changes no shared state. Local owners are transient, absent from wire, CRC, saves and replay
payloads. Replay playback reconstructs them locally through the same capture/update path.

## Critical files

- Game `Source/Game.h/.cpp`; new `Source/Frame/Collections/BattleshipWrecks/` owner/pair implementation; existing Battleships header, lifecycle/combat/render files and narrow model-layout helper.
- Game `Source/Frame/Frame.cpp::FramePostRender::Destroy`; client reconciliation/session entry points and OnServerLoad; `Source/Agent/Commands/ServerFrameEdit.cpp` for the removed timer field.
- Engine `Source/Network/Client/ReconcileReplay.cpp` dispatch preparation/rollback integration and `ClientSessionRuntime.cpp` cell teardown/full-state adoption hooks; `GameBase.cpp` presentation timing only where the game hook needs explicit source tick/fraction.
- Existing engine Explosions, PointLights, Puffs, SmokeTrails and WindRadials add/update/destroy/render functions, for explicit owner destinations and shared calculation extraction only. Existing `CollectionLifecycle.h` / `FrameUtils.h` helpers are reused without framework redesign.
- Game client/server `.vcxproj` and `.vcxproj.filters`; ModelViewer membership only if a changed consumed header requires it; owning Frame, Collections, Battleships and Explosions AGENTS and `Documents/Architecture/FrameUpdatePipeline.md` for this ownership/dispatch exception.

## In scope

The persistent per-cell owner and real Wrecks pair, exact pre-compaction capture records and
coord-worker/main-thread handoff, once-per-presented-tick local dispatch, render interpolation, persistent
existing explosion/child pairs, full hull bursts, reset/subscription handling, render appending, obsolete
shared timer/emission removal and version gate described above. Apply /update-affected-code, /update-vcxproj
and required documentation synchronization only to correctness-dependent sites. New implementation files
belong only to client consuming targets; shared modified files remain in both builds. No server wreck query is
added.

## Out of scope

Other ship deaths, gameplay death duration, damage/collision/navigation, new assets or tuning UI, generic event/effect frameworks, new wire messages, server wrecks, shared RNG for local effects, frame tuple/subset redesign, local visual serialization, rollback-preservation/reconstruction infrastructure, backward compatibility, unrelated cleanup, unit tests and sparse-checkout asset edits.

## Risk triggers and invariants

Tier 3: paired lifecycle/layout, worker/main handoff, client-only effects and identity, shared random-sequence
removal/version change and exact tick/render boundary. Pair counts/capacities remain equal. Workers write only
per-coord pending capture values; main owns all local pair mutation. Local owner handles never resolve into
snapshots. Shared simulation never reads client visual state. Render interpolation has no spawning side
effects. Ordinary snapshot replacement preserves owners; cell/reset boundaries destroy them.

## Acceptance and verification

Before source edits use /agent-harness to capture one isolated random-controller battleship's combat death, fall/rest and map/pause baseline. Trigger health depletion, not kDestroy; fleet battleships intentionally do not die. Baseline red is the inspected 0.25-second source lifetime and absent hull sampling; no runtime evidence is claimed here.

| Criterion | Required evidence |
|---|---|
| Lifecycle and isolation | Collection-layout auditor has no violations; explicit trace accounts for the non-frame owner tuples, paired copies/compaction, child handles and client-only project membership. No vector wreck storage, obsolete timer, new shared tuple machinery or local serialization remains. |
| Exact handoff | Capture source tick/ID/coordinate/pose/controller/animation before compaction and first wreck values at zero age; they match exactly. Screenshots immediately around handoff show one model. Include zero delta, incomplete interpolation window and a low-FPS multi-tick burst that removes the source before rendering. |
| Duration and following hull | At 32 Hz record 64 primary ages in [0,2), none at/after 2; screenshots near 0, 0.5, 1.5 and 2.1 seconds show full composite effects after source removal. Record triangle containment and yaw/tilt transform on sinking and terrain-resting hulls. |
| Placement and rendering | Hull crossing a cell edge still emits all primaries. Camera-cell changes preserve cell-local pose. Combined buffer capacities, skinning reservations, final counts and smoke/wind deposits include local children exactly once; empty renderable sets publish zero. |
| Existing behavior | Compare baseline fall/sea braking/rest/tilt and frozen animation. Pause and opaque map freeze pose/emission without catch-up. A death entirely completed while map-hidden leaves no wreck. Cell leave/reenter, reset and load leave no stale capture or effects. |
| Reconciliation and ownership | Repeated hydration after age 0.25 preserves wrecks, child age, local RNG and smoke IDs. Replay before capture and catch up without duplicate bursts/particles. Invalid unpresented pending captures disappear; ordinary full-state replacement never rebuilds local children. |
| Shared agreement and persistence | Client/server CRCs agree; owning version changes and old save/replay rejection are demonstrated. Current save/load and new replay playback pass with the skill-owned determinism check; payloads contain no local rows. |

Run /compile for BrokenEngineSandbox client and BrokenEngineSandboxServer, Debug|x64; include ModelViewer Debug|x64 only if consumed headers/project membership require it. Run the collection-layout checker exactly as /add-collection documents. Use /agent-harness for all runtime actions, traces and screenshots. Retain evidence paths and scenario preconditions; temporary instrumentation may be removed after capture. No unit tests.

Keep runtime scenarios under ten minutes: isolated handoff/duration, pause/map, edge and short reconciliation/replay/reset. Compilation is separate. If evidence needs a longer run, use the owning workflow's longer-run authorization gate; never replace an unverified runtime criterion with a code assertion.

## Blockers

None for saving this plan. Client ownership and phase integration are resolved above; no architecture investigation remains. Source implementation remains unapproved pending its Tier-3 review/approval workflow.
