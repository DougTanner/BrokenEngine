# Generic Layered Spatial Queries

## Summary

Replace the game-owned `Targets` collection with an engine-owned, transient spatial-query framework. Game collections bind their existing SOA arrays directly; the engine scans those arrays without copying entity records or exposing row indices.

This is Tier 3 because it changes deterministic behavior, shared collection members, CRC composition, save/replay data, full-frame network data, and engine/game phase integration.

## In scope

- Add `Engine/Source/Frame/SpatialQuery.{h,cpp}` containing the common POD interface and deterministic direct scan.
- In Spaceships collection layout and lifecycle regions, replace the publisher target handle with `engine::spatial_query_t`.
- In Missiles collection layout, Update, Spawn, destruction, and transfer regions, replace the consumer target handle and migrate retention, release, and acquisition.
- Update player Missile spawning, Frame target registration/version regions, Agent collection extraction and debug fixtures, Visual Studio membership, and affected Frame/Agent documentation.
- Delete the Targets collection, its Frame registration, `game::target_t`, and `Frame::GetMissileTarget`.

The listed regions are the target and ceiling. Adjacent cleanup or abstraction work is unauthorized unless mechanically required by these changes.

## Out of scope

- Persistent proxy collections or copied positions, alignments, radii, or subscriber counts.
- Spatial grids, hashes, acceleration structures, reverse publisher identity, event dispatch, or raw row handles.
- Fluent APIs, view objects, inheritance, callbacks, virtual dispatch, or game-policy templates.
- Player target-selection migration, backward compatibility, and unit tests.

## Public Interface

Use plain data structures and free functions:

```cpp
namespace engine
{

struct SpatialQueryEntryTag;
using spatial_query_t = id_t<SpatialQueryEntryTag>;

struct SpatialQuerySourceLayer
{
	const spatial_query_t* puiIds = nullptr;
	const XMVECTOR* pVecCurrentPositions = nullptr;
	const XMVECTOR* pVecPreviousPositions = nullptr;
	const alignment_t* pAlignments = nullptr;
	std::span<const int64_t> rows {};
	int64_t iSourceCount = 0;
};

struct SpatialQuerySubscriptionLayer
{
	const spatial_query_t* puiTargets = nullptr;
	std::span<const int64_t> rows {};
	int64_t iSourceCount = 0;
};

enum class SpatialQueryRankKey : uint8_t
{
	kSubscribers,
	kAngle,
};

struct SpatialQueryFilter
{
	float fRadius = 0.0f;
};

struct SpatialQueryRank
{
	SpatialQueryRankKey aKeys[2] {};
	uint8_t uiKeyCount = 0;
};

struct SpatialQueryResult
{
	spatial_query_t id {};
	XMVECTOR vecCurrentPosition {};
	XMVECTOR vecPreviousPosition {};
};

struct SpatialQueryBatch
{
	spatial_query_t* puiTargets = nullptr;
	const XMVECTOR* pVecOrigins = nullptr;
	const XMVECTOR* pVecDirections = nullptr;
	const alignment_t* pAlignments = nullptr;
	std::span<const int64_t> rows {};
	std::span<SpatialQueryResult> results {};
	int64_t iSourceCount = 0;
};

struct SpatialQueryContext
{
	std::span<const SpatialQuerySourceLayer> sourceLayers {};
	std::span<uint8_t> subscriberCounts {};
	const Alignments* pAlignments = nullptr;
};

int64_t SpatialQueryScratchBytes(
	std::span<const SpatialQuerySourceLayer> sourceLayers);

SpatialQueryContext BuildSpatialQueryContext(
	const Alignments& rAlignments,
	std::span<const SpatialQuerySourceLayer> sourceLayers,
	std::span<const SpatialQuerySubscriptionLayer> subscriptionLayers,
	std::span<std::byte> scratch);

void AcquireSpatialQueries(
	SpatialQueryContext& rContext,
	const SpatialQueryBatch& rBatch,
	const SpatialQueryFilter& rFilter,
	const SpatialQueryRank& rRank);

bool ResolveSpatialQueryHandle(
	const SpatialQueryContext& rContext,
	spatial_query_t id,
	SpatialQueryResult& rResult);

void ReleaseSpatialQuery(
	SpatialQueryContext& rContext,
	spatial_query_t& rId);

}
```

Interface rules:

- Source IDs must be valid and unique across all eligible rows in all bound layers. Debug builds validate this contract.
- Nonempty row spans must be strictly ascending, in range, and contain no duplicates.
- Each live consumer row appears exactly once across subscription layers.
- Providing source and consumer alignment arrays enables `Alignments::CanCollide`; omitting either disables alignment filtering.
- Acquisition always applies `fRadius`. The former visibility check is omitted because the current 45-unit radius already implies its 65×45 axis bounds.
- Exact ranking ties resolve by source-layer order and then source-row order.
- If previous positions are absent, results copy the current position into both fields.
- Retention checks only handle existence in the currently eligible source rows. Range, alignment, and ranking apply only during acquisition.
- Release decrements scratch accounting when the source still exists and always clears the consumer handle.

## Data Flow and Lifecycle

### Scratch ownership

- The query framework allocates nothing.
- Each query window makes one exact-size workbuffer allocation and partitions it into eligible source rows, acquisition rows, result records, and subscriber counts.
- No further workbuffer allocation occurs while the context is live.
- Scratch stores only row references, results, and derived `uint8_t` subscriber counts—never source entity records.
- Contexts may survive consumer collection growth after subscription binding has been consumed, but never source-layer movement, reallocation, or cell transfer.

### Spaceship publishing

- Replace `SpaceshipsInterpolate::puiTargets` with shared persistent `puiSpatialQueryIds` in the same member and `PersistentMembers` positions.
- Generate the ID at the exact former Targets-add location after pusher creation, preserving UUID consumption order.
- Existing copy/swap operations carry the ID with its Spaceship row.
- Clear it at existing owner teardown and source-cell transfer points; destination spawn generates a new cell-local ID.
- Increment `SpaceshipsInterpolate::kiVersion` from 2 to 3.

### Missile Update window

- Replace `MissilesPostRender::puiTargets` with `puiSpatialTargets` in the same shared tuple position.
- Before Spaceships Update, bind current and previous Spaceship positions.
- Build eligible Spaceship rows from valid current spatial IDs and previous-frame arrival-grace state, matching the former `kDestination` timing.
- Derive subscriber counts from previous-frame Missile handles because current handles are initialized during Update.
- Preserve the existing per-Missile RNG and physics order.
- Resolve retained handles against current eligibility; retained Missiles home from the previous position.
- Release subscriptions during falling, exploding, destruction, and lifetime transitions.
- Write every local handle, including invalid handles, into the current Missile column.
- Append only still-active, targetless Missiles to the acquisition rows.
- Acquire those rows in ascending Missile order after the loop. Acquired handles are stored but do not home until the next tick.
- Increment `MissilesPostRender::kiVersion` from 10 to 11.

### Player Spawn window

- Rebuild source eligibility and subscriber counts after Update, Transfer, and Destroy using current Spaceship lifecycle state and fully initialized current Missile handles.
- Reuse that context through the player spawn loop.
- Submit each player’s acquisition as a one-entry batch immediately before its Missile spawn, preserving existing RNG and UUID interleaving.
- Missile collection growth is permitted because the context retains only Spaceship source pointers and scratch counts.

## Removal, Compatibility, and Documentation

- Delete the Targets sources and scoped documentation; remove their project entries, explicit collection instantiations, Frame pointers/construction, tuple entries, agent query arm, alias, accessor, and version terms.
- Add SpatialQuery sources to both client/server projects and filters and aggregate the public header in Engine’s Frame section.
- At the current baseline, `Frame::kiVersion` evaluates to 200. Removing Targets contributes −3, the two collection increments contribute +2, and changing the Frame base literal from 124 to 126 contributes +2, producing 201.
- Do not retain compatibility with version 200 saves or replays.
- Update the Frame pipeline and Engine/Game Frame, Collections, and Agent documentation with the two query windows, scratch lifetime, ID uniqueness, and deterministic ordering rules.
- Run `/update-affected-code`, `/update-vcxproj`, `/code-style-review`, and `/update-claude-docs` as required by the changed artifacts.

## Verification

- Run `pwsh -NoProfile -File .agents/scripts/Test-CollectionLayout.ps1`; expect no layout/version violations.
- Validate project/filter membership through `/update-vcxproj`.
- Compile BrokenEngineSandbox client and server through `/compile`.
- Confirm by scoped source search that Targets types, `pTargets`, `game::target_t`, and `GetMissileTarget` are gone and that the evaluated Frame version changes from 200 to 201.
- Add a debug-input-only `spatial_query_fixture`, following the existing collection-layout fixture pattern, using the real public API over fixed arrays. It must prove:
  - subscriber-then-angle distribution;
  - radius and alignment acceptance/rejection;
  - stable resolution after source-row permutation and context rebuild;
  - release and reacquisition after removing an ID from eligible rows;
  - lower layer/row wins an exact tie;
  - scratch contains no copied source records.
- Extend Spaceship agent rows with `spatialQueryId` and Missile rows with `spatialTargetId`; remove the Targets query.
- Through `/agent-harness`, verify:
  - every nonzero live Missile target equals a current Spaceship spatial ID;
  - a transferred Spaceship receives a valid destination-local ID;
  - a transferred Missile arrives with an invalid target;
  - the replay scenario produces matching client/server and replay CRCs.
- Independently inspect phase code to confirm previous-state eligibility during Missile Update, current-state eligibility during Spawn, post-transition acquisition-row construction, and acquired-target homing delay.
- Add no unit tests.

## Rejected Alternatives

- Persistent `SpatialDirectory`: duplicates source data and adds unnecessary serialized state.
- Fluent `SpatialQueries` façade: conflicts with the engine’s POD/static-function style.
- Scalar-only query API: rejected because the selected design is explicitly batched and SOA-oriented.
- Uniform grid or hash index: no measured need in v1; direct ordered scans are simpler and deterministic.
- Categories, generic attributes, source radii, and distance ranking: no current consumer; source-layer selection and eligible rows cover v1.
- Game templates or inherited layer types: unnecessary; game extension uses POD composition.
- Two-stage candidate filtering: adds scratch and another traversal.
- Publisher payload or reverse identity: would expand discovery into generic source dispatch.
- Universal event bus and raw row indices: broader than the requirement and unsafe across row movement.
- Bare `Registry` name: conflicts with existing `TypeRegistry` and `AgentUiRegistry`.

## Execution Card

- Objective: introduce generic, copy-free cross-collection spatial queries while preserving current Missile targeting behavior.
- Tier: Tier 3—deterministic CRC behavior, shared SOA types, save/replay and full-frame wire layout, phase ordering, and parallel-cell scratch lifetime.
- Roles: disjoint Engine/layout/Frame/game implementers; affected-code propagation; client/server builder; C++, scope, and adversarial reviewers; style/project/docs cleanup; harness verification; final read-only verification and landing flow.
- Acceptance signals:
  - layout auditor plus independent client/server compilation;
  - debug fixture plus independent code review of ordering and lifetime;
  - live handle/transfer observations plus independent replay CRC;
  - static phase inspection plus runtime gameplay behavior.
- Any change to the public interface, identity rules, scan order, phase windows, or rejected boundaries requires a new Tier-3 plan audit.
- Landing remains subject to `/verify-changes`, `/finalize-changes`, and one explicit confirmation before primary changes.
