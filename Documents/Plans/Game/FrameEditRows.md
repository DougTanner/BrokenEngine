<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-06T17:48:32.995Z","dependsOn":[]} -->
# Unified spawn system: one status-change family carrying each collection's `SpawnInfo`

Line numbers cite baseline `d8ce779b3b4fb984b572522896b6382d8dd19d19`. Where a statement here and the code disagree, the code wins; report the contradiction instead of matching one side to the other.

## Context

Today a cell's tick input (`game::FrameInput`, `Projects/BrokenEngineSandbox/Source/Frame/FrameInput.h:8-18`) carries eight `StatusChange` kinds (`Frame/StatusChange.h:10-22`): one player spawn with its own `SpawnPlayerData`, four cross-cell transfer kinds sharing one `TransferData` union of every transferable collection's carried state, and three destroy/update kinds. Spawning from outside the tick exists only for players; spaceships, blasters, missiles, and explosions can be created only inside the tick or as arrivals, and the agent harness reaches them only through `inject_payload` transfer entries behind `kbDebugInput` (`Agent/Commands/ServerSimulationHarnessRigs.cpp:427-517`). Arrivals are mapped from `TransferData` to each collection's `SpawnInfo` by `SpawnTransfer.cpp:13-107`, a second copy of what the producers already know.

User decisions, binding over every other source (`.agents/references/authority-order.md`):

- D1C: cell-to-cell arrivals and spawns are one message family.
- D2A: the agent harness spawns through `inject_payload`; the user allows harness changes to ride the instant post-tick path if that makes the design simpler.
- D3B: players fold into the same unified system; new `.cpp`/`.h` pairs are welcome to keep existing code clean.
- D4: the system is generic; explosions are a spawn type; owned rows (pushers) can only be created from their owner's spawn path.
- D5: the harness injects into the server; the server sends status changes to clients as normal.
- Arrival timing: an arrival is applied to the destination right after the tick that produced it, exactly as today (no one-tick gap; "we can't have ships disappearing for 1/30 of a second").
- Chosen design: the collection's own `SpawnInfo` is the payload; `ServerSpawnQueue::Spawn(coord, SpawnInfo)`; a `Fields()` table per payload drives the wire codec and the harness parser; the explosion spawn signature is unified with the others.
- Staging: this Plan is Stage 1; Stage 2 (harness row removal through the regular Destroy phase) is a separate Plan that depends on it.

Plan coordination (main's decision): this Plan replaces the text of `Documents/Plans/Game/FrameEditRows.md` in place, keeping that path, its `createdUtc`, and every `dependsOn` edge that names it (`Documents/Plans/Game/ShipTypeSplit.md`, `Documents/Plans/Game/FrameEditCellPins.md`, `Documents/Plans/Engine/AgentHarnessIntoEngine.md`). Stage 2 is the new Plan `Documents/Plans/Game/FrameEditRowRemoval.md`, whose `dependsOn` names `Documents/Plans/Game/FrameEditRows.md`. The earlier `edit_frame` spawn edit and in-command destroy pass are not built.

Decisions this Plan makes (author's, with rationale):

1. The harness spawns through the next-tick input path, not the instant post-tick path. The tick-input queue must exist anyway for the fleet player spawn, `kDestroyPlayer`, `kUpdatePlayer`, and `kUpdateFleet`, and it already has the `tick` scheduling and "wait until the coordinate is active" semantics `inject_payload` documents; routing harness spawns through it removes the harness's second queue (`replayTransferHarnessRigs`) and its drain inside the transfer harvest. The instant path has no scheduling and would keep two queues.
2. Spawn versus arrival is told by the type tag, not by a payload field: `kSpawn*` tags enter the tick and `kTransfer*` tags are applied post-tick, both with the same payload type per collection and the same apply function. The client must know per wire item whether it applies inside or after the tick, the tag already carries that and costs no wire byte, the replay abort checks (`Engine/Source/File/Replay.cpp:864-887`) and `CountCapturedReplayTransfers` keep working on tags, and Spaceships and Blasters need no new `bTransfer` member. `bTransfer` on Players and Missiles stays what it is today: the carried-state marker the collection reads (`Players.cpp:413-414`, `Missiles.cpp:395-409`).
3. The enumerator layout is rewritten once, contiguous by family, under the protocol and replay-input version bumps (no backward compatibility: old streams are rejected by version). The append-only rule resumes after this change.
4. `ServerSpawnQueue` is the tick-input queue only. `ServerTransferManager` stays as the post-tick arrival path (collect, sort, replay capture, apply, CRC recompute, owned relink), changed only in its payload. Nothing new goes into the grid save: natural producers queue for the first tick of the update that builds them and are drained before the autosave at the end of that update (`Engine/Source/GameBase.cpp:476`), arrivals never sit in the queue, and harness entries scheduled for a later tick were never saved today (`Engine/Source/File/AGENTS.md` `## Grid Saves`: saves hold no tick input). `Frame::kiVersion` is unchanged.
5. `ProcessUpdatePlayerRequests` and `FleetNavigationController` keep pushing their update kinds straight into `mFrameInputs`; only spawn producers use the queue. `ServerSpawnQueue::Drain` stable-sorts every active coordinate's input by type after appending, so the applied order is the sorted order on every coordinate regardless of who wrote it, and the harness drain's conditional sort (`ServerSimulationHarnessRigs.cpp:804-807`) goes.
6. An explosion's cell coordinate is not on the wire: `ApplySpawnStatusChange` stamps `SpawnInfo::coordinate` from the applying cell's static data, so a payload can never name a different cell than the one it is applied to.
7. `inject_payload` `Transfer*` entries, their `kbDebugInput` gate, and `pendingTransferHarnessRigCount` go; `Spawn*` entries with `bTransfer` cover carried-state rows. `replay_transfer_capture` stays and observes natural arrivals, the only post-dispatch events left. `pauseAfterWriterInput` drops its "requires a transfer entry" check: every batch entry enters the next tick's writer input, which is what `ObserveWriterInput` pauses on.
8. `ServerBroadcaster::ResetState` keeps clearing the queue, so the documented behaviour that a resynchronizing `edit_frame`, `load`, and `reset` drop queued `inject_payload` entries stays as it is (`commands-server.md:147-149`).
9. `PlayersPostRender::SpawnInfo::iPendingFleetWantedCoordinateTicks` and `iPendingWeaponModeTicks` become `uint8_t`, their column width, so the Players wire image (113 bytes) stays under `kiMaxStatusChangeBytesPerItem` 120 (`Engine/Source/Network/NetworkSerialization.h:19`).
10. `StatusChange::operator==` and the payload structs' `operator==` are removed: no caller compares status changes (the replay dedup uses `FrameInput::Crc()`), and a defaulted operator over `SpawnInfo` alternatives would be silently deleted.
11. The harness refuses a key whose member type is `engine::GlobalId` ("minted by the server") instead of keeping a per-type exclusion list, so the parser stays table-driven and `SpawnPlayer` keeps minting `globalPlayerId` at commit as today.

## Design

### Status-change family (`Projects/BrokenEngineSandbox/Source/Frame/StatusChange.h`, both builds)

```cpp
#include "Frame/Collections/Blasters/Blasters.h"
#include "Frame/Collections/Explosions/Explosions.h"
#include "Frame/Collections/Missiles/Missiles.h"
#include "Frame/Collections/Players/Players.h"
#include "Frame/Collections/Spaceships/Spaceships.h"

namespace game
{

// Underlying values are wire, replay, and save bytes: append-only from here on; this layout is the one the
// kiProtocolVersion 26 / FrameInput::kiVersion 21 bump introduced.
enum class StatusChangeType : uint8_t
{
	// Tick input, applied in the Spawn phase by ApplySpawnStatusChanges.
	kSpawnPlayer,
	kSpawnSpaceship,
	kSpawnBlaster,
	kSpawnMissile,
	kSpawnExplosion,
	// Cross-cell arrivals, applied to the destination right after its tick; never inside a FrameInput.
	kTransferPlayer,
	kTransferSpaceship,
	kTransferBlaster,
	kTransferMissile,
	kDestroyPlayer,
	kUpdatePlayer,
	kUpdateFleet,

	kCount,
};

inline constexpr bool IsKnownStatusChangeType(StatusChangeType eType);
inline constexpr bool IsSpawnType(StatusChangeType eType);    // kSpawnPlayer..kSpawnExplosion
inline constexpr bool IsTransferType(StatusChangeType eType); // kTransferPlayer..kTransferMissile
// (static_asserts keep both ranges contiguous, as the transfer range is asserted today)
inline constexpr StatusChangeType kOwnedEntityTransferType = StatusChangeType::kTransferPlayer;
inline const char* StatusChangeTypeName(StatusChangeType eType);

struct DestroyPlayerData { int64_t iPlayerUuid = 0; static constexpr auto Fields(); };
struct UpdatePlayerData  { /* members unchanged */ static constexpr auto Fields(); };
struct UpdateFleetData   { /* members unchanged */ static constexpr auto Fields(); };

using StatusChangeData = std::variant<
	PlayersPostRender::SpawnInfo,
	SpaceshipsPostRender::SpawnInfo,
	BlastersPostRender::SpawnInfo,
	MissilesPostRender::SpawnInfo,
	engine::ExplosionsPostRender::SpawnInfo,
	DestroyPlayerData,
	UpdatePlayerData,
	UpdateFleetData>;

// A spawn payload's collection and tags. Pushers have no SpawnInfo and no specialization, so they cannot be
// queued, parsed, or applied from outside their owner's Spawn (D4).
template <typename SPAWN_INFO> struct SpawnTraits;
template <> struct SpawnTraits<PlayersPostRender::SpawnInfo>    { using Collection = PlayersPostRender;    static constexpr StatusChangeType kSpawn = StatusChangeType::kSpawnPlayer,    kTransfer = StatusChangeType::kTransferPlayer; };
template <> struct SpawnTraits<SpaceshipsPostRender::SpawnInfo> { using Collection = SpaceshipsPostRender; static constexpr StatusChangeType kSpawn = StatusChangeType::kSpawnSpaceship, kTransfer = StatusChangeType::kTransferSpaceship; };
template <> struct SpawnTraits<BlastersPostRender::SpawnInfo>   { using Collection = BlastersPostRender;   static constexpr StatusChangeType kSpawn = StatusChangeType::kSpawnBlaster,   kTransfer = StatusChangeType::kTransferBlaster; };
template <> struct SpawnTraits<MissilesPostRender::SpawnInfo>   { using Collection = MissilesPostRender;   static constexpr StatusChangeType kSpawn = StatusChangeType::kSpawnMissile,   kTransfer = StatusChangeType::kTransferMissile; };
template <> struct SpawnTraits<engine::ExplosionsPostRender::SpawnInfo> { using Collection = engine::ExplosionsPostRender; static constexpr StatusChangeType kSpawn = StatusChangeType::kSpawnExplosion; };

// The replay difference stream writes payloads as raw images (Common/Serialization.h:99-103).
static_assert(std::is_trivially_copyable_v<PlayersPostRender::SpawnInfo> && std::is_trivially_copyable_v<SpaceshipsPostRender::SpawnInfo> && std::is_trivially_copyable_v<BlastersPostRender::SpawnInfo> && std::is_trivially_copyable_v<MissilesPostRender::SpawnInfo> && std::is_trivially_copyable_v<engine::ExplosionsPostRender::SpawnInfo>);

inline StatusChangeData DefaultDataForType(StatusChangeType eType); // the alternative each tag seats (tags of one collection share one)

struct StatusChange
{
	StatusChangeType eType {};
	StatusChangeData data {};
};

template <typename SPAWN_INFO> StatusChange MakeSpawn(const SPAWN_INFO& rInfo);    // {SpawnTraits::kSpawn, rInfo}
template <typename SPAWN_INFO> StatusChange MakeTransfer(const SPAWN_INFO& rInfo); // {SpawnTraits::kTransfer, rInfo}

// Payload accessors over spawn and transfer tags; the engine transfer and reconcile code calls them by name.
XMVECTOR SpawnPosition(const StatusChange& rChange);
XMVECTOR& SpawnPositionRef(StatusChange& rChange);                 // PrepareTransferRequest's rebase target
void ValidateSpawnVectors(const StatusChange& rChange);            // common::ValidateVector on position, direction, velocity where the payload has them
engine::GlobalId OwnedSpawnGlobalId(const StatusChange& rChange);  // kSpawnPlayer / kTransferPlayer payload's globalPlayerId
engine::ClientGuid OwnedSpawnClientGuid(const StatusChange& rChange);
bool IsAdoptableStatusChange(const StatusChange& rChange);         // Blasters/Explosions 0 <= iTypeIndex < their sTypes size (moved from SpawnTransfer.cpp; the payload member is int64_t)

// The one application point every path shares: tick input, server post-tick arrival, client post-tick arrival.
// Calls SpawnTraits<...>::Collection::Spawn(rFrame, info); for an explosion, info.coordinate = coordinate first.
bool ApplySpawnStatusChange(Frame& __restrict rFrame, const StatusChange& rChange, engine::GridCoord coordinate);
// Every IsSpawnType entry of the input in list order (the input is type-sorted, so players first).
void ApplySpawnStatusChanges(Frame& __restrict rFrame, const FrameInput& __restrict rFrameInput, const engine::CellStaticData& rStaticData);

} // namespace game
```

`StatusChange.cpp` (new, both builds) holds the non-inline functions above and the type-name table. `ApplySpawnStatusChange` is a `std::visit` with `if constexpr (requires { typename SpawnTraits<T>::Collection; })`; non-spawn alternatives are unreachable (`ASSERT`).

### Include layout (PCH order)

Every translation unit force-includes `Pch.h` (`ForcedIncludeFiles` in both `.vcxproj`), so the first entry into `StatusChange.h` is always this chain: `Pch.h` → `Common.h`, `Shaders/ShaderLayouts.h` → `Frame/Frame.h` → `Frame/FrameBase.h` (engine collections, `Explosions.h` at `:9`, `Pushers.h`, then `FrameUtils.h` at `:22`) → `Frame/FrameInput.h` → `Frame/StatusChange.h` → `Network/NetworkProtocol.h`, then the five collection headers; back in `Frame.h`, `FrameRegistry.h` and the rest; then `NetworkSessionContract.h`, `Engine.h`, `GameMessages.h`. The game collection headers enter the PCH ahead of `Engine.h` for the first time. By read, they need only these names from outside their own includes: `game::Frame`, `FrameInput`, `FrameInterpolate`, `FramePostRender` (forward-declared by `Engine/Source/Frame/Collections/Collection.h:11-14`), `engine::CellStaticData` (forward-declared in each header or its engine includes), `engine::ClientGuid` (`NetworkProtocol.h`, already `StatusChange.h:4`), `FRAME_COLUMN`/`FrameColumnList` (`FrameUtils.h`, reached through `FrameBase.h` before `FrameInput.h`), plus two names defined later in the chain. The cycle is broken by moving or including exactly those:

- `player_t` (`Players.h:273`, today defined at `Frame.h:25`, after the `FrameInput.h` include): the alias moves into `Players.h` as `struct PlayersInterpolate; using player_t = engine::Id<PlayersInterpolate>;` at the top of `namespace game`, and `Frame.h:25` goes. No other header uses `player_t`; every `.cpp` gets it through the PCH.
- `engine::registry_id_t` (`Missiles.h` `SpawnInfo::uiTarget`, `Spaceships.h`; defined in `Engine/Source/Frame/FrameRegistry.h:11`, which `Frame.h:7` includes after `FrameInput.h`): `Missiles.h` and `Spaceships.h` include `Frame/FrameRegistry.h` directly (it includes only `CollectionId.h`, `Alignments.h`, `NetworkProtocol.h`).
- `Explosions.h` is included by `FrameBase.h:9`, before `FrameUtils.h` (`:22`), so its new `Fields()` needs `#include "Frame/FrameUtils.h"` in `Explosions.h` (`FrameUtils.h` includes only `GridCoord.h`).

`Engine.h` and `Pch.h` order are unchanged. `StatusChangeFields.h` and `ServerSpawnQueue.h` stay out of `Engine.h`: `StatusChangeCodec.cpp` includes the first; `ServerSession.cpp`, `ServerBroadcaster.cpp`, `ServerClientManager.cpp`, `ServerSimulationHarnessRigs.cpp`, and `AgentCommandsServer.cpp` include the second. A `SpawnInfo` or collection-header edit now rebuilds every translation unit (a header-weight cost accepted with the chosen design).

### Collection `SpawnInfo` changes

Each `SpawnInfo` gains a `Fields()` table built from `engine::FrameColumnList`/`FRAME_COLUMN` (`Engine/Source/Frame/FrameUtils.h:19-30`). A member not in the table never crosses the wire, cannot be set by the harness, and stays default when the payload is read.

- `PlayersPostRender::SpawnInfo` (`Players.h:331-357`): `vecPosition, vecDirection, vecVelocity, alignment, fArmor, fShield, nextBlasterFireTime, nextSecondarySpawnTime, shieldCooldown, shieldDownSoundCooldown, animationTime, flags, transferLockTimer, arrivalGracePeriod, navigationDelay, globalPlayerId, fleetWantedCoordinate, iPendingFleetWantedCoordinateTicks, iPendingWeaponModeTicks, bTransfer` (113 wire bytes). Not listed: `clientGuid` (server-local, as today's `SpawnPlayerData::clientGuid` and `TransferData` GUID halves), `fShieldRotation`, `fShieldShrink` (client visuals). The two pending-tick members become `uint8_t` (decision 9); `Players.cpp:434-435` drop their casts.
- `SpaceshipsPostRender::SpawnInfo` (`Spaceships.h:182-192`): every member (68 bytes).
- `BlastersPostRender::SpawnInfo` (`Blasters.h:101-111`): every member, including the three wind-trail floats (today's arrival path sets them deliberately, `SpawnTransfer.cpp:38-41`) (57 bytes).
- `MissilesPostRender::SpawnInfo` (`Missiles.h:168-186`): every member except `uiTarget`, a registry link that only in-tick code may set (98 bytes; `flags` is one byte).
- `engine::ExplosionsPostRender::SpawnInfo` (`Explosions.h:276-289`): gains `GridCoord coordinate {}` (today's positional parameter, the cell `vecPosition` is local to) and `Fields()` listing every member except `coordinate` (decision 6) (77 bytes; `flags` is one byte). Its spawn signature becomes the shared shape:

```cpp
// Explosions.h — replaces Spawn(game::Frame&, GridCoord, SimulationTime, const SpawnInfo&); the start time is
// TickToSimulationTime(rFrame.interpolate.iTick), which every caller passes today (Missiles.cpp:256, PlayersCombat.cpp:463, Spaceships.cpp:215).
static bool Spawn(game::Frame& __restrict rFrame, const SpawnInfo& rSpawnInformation);
```

The three in-tick callers add `.coordinate = emitterCoordinate` to their literal and drop the two positional arguments.

### Field codec (`Engine/Source/Network/StatusChangeFields.h`, both builds, new, header-only)

```cpp
namespace engine
{
// Table-driven wire codec for any payload with Fields(). Member types written: XMVECTOR (WriteVector4), float,
// std::chrono::duration<float> (as float), bool (u8), uint8_t, uint16_t, int64_t, GridCoord, AlignmentIdentifier
// (u32), GlobalId (i64), common::Flags<E> (underlying). Any other member type fails to compile.
template <typename DATA> void WriteStatusChangeFields(uint8_t*& rpCursor, const DATA& rData);
template <typename DATA> void ReadStatusChangeFields(const uint8_t*& rpCursor, DATA& rData);
} // namespace engine
```

`StatusChangeCodec.cpp` collapses to one `std::visit` per direction calling these; the eight hand-written cases (`:12-226`) go. Reading seats the alternative with `DefaultDataForType` first, as today (`:177`). `DestroyPlayerData`, `UpdatePlayerData`, and `UpdateFleetData` `Fields()` list their members in today's wire order, so their bytes are unchanged.

### Application point in the tick

`FramePostRender::Spawn` (`Frame.cpp:373-389`) becomes, in order:

1. `ApplySpawnStatusChanges(rFrame, rFrameInput, rStaticData)`.
2. `PlayersPostRender::Spawn(rFrame, rFrameInput, rStaticData)`: `kDestroyPlayer`, client owned visuals, player blasters, missiles, death explosions. `ProcessSpawnStatusChanges` (`Players.cpp:214-274`) loses its `kSpawnPlayer` half and is renamed `ProcessDestroyStatusChanges`.
3. `engine::ForEachPostRenderSpawn(GamePostRenderTypes {})`.
4. The spaceship group spawner.

Step 1 before step 2 so a player spawned from input gets its wind trail, hex shield, and first blaster timing in the same tick as today's `kSpawnPlayer`. `kUpdatePlayer`/`kUpdateFleet` stay in Update (`Frame.cpp:172`). The engine spawn walk and the CRC stamp (`FrameBase.cpp:311-315`) are untouched: every spawned row is inside the stamp.

### Server tick-input queue (`Engine/Source/Network/Server/ServerSpawnQueue.h/.cpp`, `BT_SERVER`, new)

```cpp
namespace engine
{
// The queue every out-of-tick spawn producer writes and the only writer of server tick input besides the update
// kinds. Main thread, between ticks. Names game::StatusChange under the engine/game exception ServerBroadcaster
// holds; stays out of Engine.h and is included directly.
class ServerSpawnQueue
{
public:
	// Out-of-tick mirror of T::Spawn(rFrame, info) for the first tick after the current one. The producer passes a
	// position inside the cell; an outside one is a producer bug and ASSERTs here, at the queuing call.
	template <typename SPAWN_INFO>
	void Spawn(GridCoord coordinate, const SPAWN_INFO& rInfo);

	// An already-built change for an explicit tick (the harness; a change that is not a spawn).
	void Queue(GridCoord coordinate, int64_t iTick, const game::StatusChange& rChange);

	// PrepareTick: entries with iTick <= miTickCounter whose coordinate is active with a committed pCurrent append
	// to gpGame->mFrameInputs[coord]; then every active coordinate's input is stable-sorted by eType. Runs before
	// the broadcast snapshot and before SyncReplayTick records the input.
	void Drain();

	int64_t PendingCount() const; // status.pendingAgentStatusChangeCount
	void ResetState();            // load, reset, and client resynchronization clear it, as the harness queue is cleared today

private:
	struct Entry
	{
		int64_t iTick = 0;
		game::StatusChange change;
	};
	std::unordered_map<GridCoord, std::vector<Entry>> mPending; // today's pendingAgentStatusChanges map, moved
};
} // namespace engine
```

`Spawn` is `ASSERT(common::InsideArea(rInfo.vecPosition, LocalCellArea()))` then `Queue(coordinate, game::gpGame->miTickCounter + 1, game::MakeSpawn(rInfo))`. `Drain`'s activity test is today's (`ServerSimulationHarnessRigs.cpp:770-795`); it runs inside `ServerBroadcaster::PrepareTickStatusChanges` in place of `DrainPendingAgentStatusChanges` (`ServerBroadcaster.cpp:44`), under the existing allocation suppression. `ServerBroadcaster::ResetState` calls `ResetState()` in place of `ResetPendingAgentStatusChanges` (`:193`). `game::ServerSession` owns it as `std::unique_ptr<engine::ServerSpawnQueue> mpSpawnQueue` beside `mpTransferManager`.

### Producers

Fleet player spawn (`ServerClientManager::SpawnWaitingClients`, `ServerClientManager.cpp:109-147`; the `kSpawnPlayer` push at `:133-134` becomes):

```cpp
gpServerSession->mpSpawnQueue->Spawn(engine::kOriginCoordinate, PlayersPostRender::SpawnInfo
{
	.vecPosition = XMVectorSet(kfFleetSpawnOffsetX, kfFleetSpawnOffsetY, engine::gpFlightPlanes->Lowest().fHeightMeters, 1.0f), // 45, -12: SpawnPlayerData's defaults, now two file-scope constants here
	.vecDirection = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f),
	.alignment = gpGame->mPlayerAlignment,
	.flags = spawnFlags, // kBlasterSpawnLeft, plus kIsFlagship from the fleet lookup
	.arrivalGracePeriod = kArrivalGracePeriod,
	.globalPlayerId = globalPlayerId,
	.clientGuid = rClientSpawnInformation.clientGuid, // born owned; never on the wire
	.fleetWantedCoordinate = spawnFleetWantedCoordinate,
	.iPendingFleetWantedCoordinateTicks = static_cast<uint8_t>(iSpawnPendingFleetTicks),
});
// SendAssignPlayer, SendPlayerState, mClientOwnedEntities.Add, OnPlayerSpawned stay here, keyed on the id, as today.
```

`BuildAdvancingFrameInputs` runs at update start (`GameBase.cpp:1048`) with `miTickCounter` not yet advanced, so the entry targets the first tick of that update and drains in its `PrepareTick`; the comment at `ServerSession.cpp:324-326` is rewritten to say so.

Cell-to-cell arrival (each collection's Transfer phase; `Spaceships.cpp:361-392` shown, `Blasters.cpp:161-174`, `Missiles.cpp:313-334`, `PlayersNavigation.cpp:220-249` alike):

```cpp
TransferRequest request {.change = MakeTransfer(SpaceshipsPostRender::SpawnInfo
{
	.vecPosition = rCurrentInterpolate.pVecPositions[i],
	.vecDirection = rCurrentInterpolate.pVecDirections[i],
	.vecVelocity = rCurrentPostRender.pVecVelocities[i],
	.alignment = rCurrentPostRender.pAlignments[i],
	.fHealth = rCurrentPostRender.pfHealths[i],
	.nextBlasterSpawnTime = rCurrentPostRender.pNextBlasterSpawnTimes[i],
	.arrivalGracePeriod = kArrivalGracePeriod,
	.fDeltaRotation = rCurrentInterpolate.pfDeltaRotations[i],
})};
if (rFrame.postRender.PrepareTransferRequest(bounds, request)) [[unlikely]] { /* capacity LOG + DEBUG_BREAK as today */ }
rFrame.postRender.PushTransferRequest(request);
```

The values `SpawnTransfer.cpp:13-107` fills on arrival move into these literals, where they already are: Players add `.transferLockTimer = 1 s`, `.arrivalGracePeriod = kArrivalGracePeriod`, `.navigationDelay`, `.globalPlayerId`, `.clientGuid`, the fleet fields, `.bTransfer = true`; Missiles add `.vecStoredDirection = direction`, `.flags` with `kTargetEnemy`/`kTargetPlayer` by alignment, `.uiTarget` left default, the timers, `.bTransfer = true`; Blasters add `.iTypeIndex` and the three wind-trail `mfDefault`s by alignment (`Ui/WindDepositsWrappers.h`, which `SpawnTransfer.cpp:8` includes today; `Blasters.cpp` does not, so it gains that include).

Harness `inject_payload` (`ServerSimulationHarnessRigs.cpp:404-695`):

```json
{"entries":[
  {"coord":[0,0],"type":"SpawnSpaceship","vecPosition":[10,20],"alignment":"enemy"},
  {"coord":[0,0],"type":"SpawnPlayer","vecPosition":[45,-12],"flags":132},
  {"coord":[0,0],"type":"SpawnMissile","vecPosition":[6,6],"vecVelocity":[0,20],"fAcceleration":40},
  {"coord":[0,0],"type":"SpawnExplosion","iTypeIndex":0,"vecPosition":[7,7],"iTrailCount":4,"tick":1234}
]}
```

- `type` is one of the `StatusChangeTypeName`s that is not a transfer type; the error message lists `SpawnPlayer|SpawnSpaceship|SpawnBlaster|SpawnMissile|SpawnExplosion|DestroyPlayer|UpdatePlayer|UpdateFleet`. Every entry needs an active `coord`.
- Keys of a spawn entry are the `Fields()` member tokens of its payload type: one `ParseStatusChangeFields<DATA>(rEntry, rData)` walks `DATA::Fields()` with the existing per-type branches (`:282-358`) plus `engine::AlignmentIdentifier` (`"player"`/`"enemy"` to `gpGame->mPlayerAlignment`/`mEnemyAlignment`, anything else fails), `common::Flags<E>` (integer within the underlying type), `int64_t`, `uint16_t`, and `engine::GlobalId` (present: fails with "minted by the server"). An `XMVECTOR` named `vecPosition` takes z = lowest flight plane and w = 1; any other takes z = 0, w = 0. `INJECTED_FIELD`/`InjectedField` go.
- Defaults before parsing: `vecPosition` = cell centre at the lowest plane; `vecDirection`, `vecVelocity`, and Missiles' `vecStoredDirection` = `(1,0,0,0)`; `alignment` = player; Players' `fleetWantedCoordinate` = the entry coord. After parsing, a `SpawnMissile` entry without a `flags` key gets `flags` = `kTargetEnemy` when its parsed `alignment` is the player alignment, else `kTargetPlayer`. Every other member keeps its struct default (so `bTransfer` is false and a player gets `kfPlayerArmor`; an entry that sets `bTransfer:true` must also set `fArmor`/`fShield`).
- Validation before queuing, whole batch first: unknown key, inactive coord, non-finite value, position inside the cell (`ValidateInjectedPosition`), `IsAdoptableStatusChange` (Blasters and Explosions `0 <= iTypeIndex < sTypes` size), Explosions `iTrailCount >= 0` and `fTimePercent` in `[0,1]` (the ranges `ExplosionsInterpolate::PostRead`, `Explosions.h:170-186`, rejects in a save), Players `nextBlasterFireTime` and `navigationDelay` ranges and the `kfPlayerMaximumSpeed` velocity cap (`:501-512`, now for `SpawnPlayer`), `UpdatePlayer` navigation delay as today.
- Commit: `SpawnPlayer` mints `globalPlayerId` from `miNextGlobalId` exactly as today (`:651-675`; the `TransferPlayer` branch goes); then `mpSpawnQueue->Queue(coord, iTick, change)` per entry; `iTick` defaults to `miTickCounter + 1` (`:413`). `DestroyPlayer`, `UpdatePlayer`, `UpdateFleet` parse through the same `ParseStatusChangeFields` over their `Fields()` with today's required-key and default rules; required keys (`iPlayerUuid`, `UpdateFleet`'s `fleetWantedCoordinate`) are checked by the caller per type, as `INJECTED_FIELD(..., true)` does today.
- `status`: `pendingAgentStatusChangeCount` reads `mpSpawnQueue->PendingCount()`; `pendingTransferHarnessRigCount` goes (`AgentCommandsServer.cpp:110`). `QueueAgentStatusChange`, `QueueReplayTransferHarnessRig`, `DrainPendingAgentStatusChanges`, `DrainReplayTransferHarnessRigs`, `ResetPendingAgentStatusChanges`, `ResetReplayTransferHarnessRigs`, `CountPendingAgentStatusChanges`, `CountReplayTransferHarnessRigs`, `ScheduledStatusChange`, and the two maps go; `CountCapturedReplayTransfers` stays; the state slot (`ServerSimulationHarnessRigState`, `sHarnessRig`), `Bind` (and its call at `:703`), and `DetachServerSimulationHarnessRigs` (`.h:36`, `.cpp:895-901`) with its `ServerSession.cpp:37` call go, because with both maps gone nothing reads them.

Future server portal (once per update, from `BuildAdvancingFrameInputs` or a portal manager):

```cpp
for (const PortalArrival& rArrival : mpPortalManager->Due(gpGame->miTickCounter))
{
	mpSpawnQueue->Spawn(rArrival.coordinate, SpaceshipsPostRender::SpawnInfo {.vecPosition = rArrival.vecLocalPosition, .vecDirection = rArrival.vecDirection, .alignment = gpGame->mEnemyAlignment});
	mpSpawnQueue->Spawn(rArrival.coordinate, engine::ExplosionsPostRender::SpawnInfo {.iTypeIndex = kiPortalExplosionType, .vecPosition = rArrival.vecLocalPosition});
}
```

The producer targets an active cell (a queued entry waits until its cell is active) and a position inside it (`Spawn` asserts on an outside one).

Future carrier launch bay (inside the tick, the carrier collection's Spawn-phase hook):

```cpp
if (rCurrentPostRender.pNextLaunchTimes[i] < std::chrono::duration<float>::zero())
{
	rCurrentPostRender.pNextLaunchTimes[i] = kLaunchCooldown;
	SpaceshipsPostRender::Spawn(rFrame, {.vecPosition = vecBay, .vecDirection = vecHeading, .alignment = rCurrentPostRender.pAlignments[i]});
}
```

Same literal and the same `Spawn` the queue path reaches, with no record: the launch is decided from CRC'd frame state every participant holds, so server, clients, and replay compute it identically, as player blaster fire does (`PlayersCombat.cpp:314`). Tick input exists for decisions made outside the deterministic tick; routing an in-tick decision through it would delay it a tick and spend wire bytes on data every side already has.

### Arrival path (timing unchanged)

- `TransferRequest` (`Frame.h:78-84`) becomes `{ StatusChange change; int8_t iDeltaX; int8_t iDeltaY; }`.
- `FramePostRenderBase::PrepareTransferRequest` (`FrameBase.cpp:139-154`) computes the delta from `game::SpawnPosition(rRequest.change)` and rebases through `game::SpawnPositionRef(rRequest.change)`; it stays the single conversion point, and the `FrameBase.cpp:148` comment's "SpawnTransfer" becomes "`ApplySpawnStatusChange`". `PushTransferRequest` (`:156-167`) calls `game::ValidateSpawnVectors(rRequest.change)` in place of the three `ValidateVector` lines and keeps its suppressed push.
- `ServerTransferManager::CollectTransfers` (`ServerTransferManager.cpp:48-111`) pushes `rRequest.change` unchanged (`:94-98`), logs with `rRequest.change.eType` and `game::SpawnPosition`, and builds `ClientTransferInfo` from `game::OwnedSpawnGlobalId`/`OwnedSpawnClientGuid` (`:100-108`); `TransferDataClientGuid` (`:17-20`) goes. `HarvestTransfers` (`:269-300`) drops the `DrainReplayTransferHarnessRigs` call (`:279`). `SpawnTransfers` (`:124-143`) calls `game::ApplySpawnStatusChange(rDestinationFrame, rTransfer, rCoord)` (`:139-140`). `ApplyPreparedTransfers`'s owned-id log reads `OwnedSpawnGlobalId` (`:190`); `ApplyReplayTransfers` (`:317-348`) likewise (`:332-343`). `ResetState` (`:350-356`) drops `ResetReplayTransferHarnessRigs`. The CRC recompute (`:162-168`), `TrackClientTransfers` (`:215-267`), and the adjacency and liveness rules are untouched.
- Client `ReconcileReplayTick.cpp:120-155`: the post-tick loop calls `game::ApplySpawnStatusChange(*pNext, rStatusChange, rWork.pCell->coordinate)` and reads the owned id through `game::OwnedSpawnGlobalId`; the erase and CRC recompute stay.
- Client `Projects/BrokenEngineSandbox/Source/Network/Client/ReconcileReplayClientState.cpp:30-58` (`ReconcileUpdateClientState`'s scan of `transferRequests`): tests `rRequest.change.eType != StatusChangeType::kTransferPlayer`, compares `game::OwnedSpawnGlobalId(rRequest.change)` with the client's global player id, and reads the carried armor as `std::get<PlayersPostRender::SpawnInfo>(rRequest.change.data).fArmor` in place of `rRequest.data.fHealth`.
- Replay (`Replay.cpp:312-343`, `:853-897`): unchanged; the abort checks keep using `IsTransferType`. The post-dispatch channel still carries the raw payload images. The edits are the stale comment at `Replay.cpp:878-879`, which names `std::get<TransferData>` (it names the arrival's `ApplySpawnStatusChange` instead), and the seven `game::OnReplayStreamsInvalidated()` calls (`:272`, `:301`, `:589`, `:702`, `:709`, `:722`, `:740`), which go with that hook: `game::OnReplayStreamsInvalidated` (`Save/GameSaveLoad.cpp:35-38`) only calls the removed `ResetReplayTransferHarnessRigs`, so its definition, its `GameSaveLoad.h:14` declaration, and the `GameSaveLoad.h:10` comment's "replay-only game harness rigs" go too.
- Publication (`ServerBroadcaster.cpp:56-188`): unchanged; the batch still concatenates the broadcast snapshot and the transfer batch per coordinate, and the codec still groups by ascending type (`NetworkSerialization.cpp:36-87`).

### Ordering guarantee

Server applied order == replay recorded order == wire order for every coordinate and tick: `Drain` is the last writer before the snapshot and sorts every active coordinate's input; the codec emits ascending type groups preserving in-type order, which equals a sorted input; the client applies wire order inside `RunFrameTick`; arrivals are sorted by `SortTransfersByType` as today and applied post-tick on both sides.

### Wire, replay, save, admission

- `engine::kiProtocolVersion` 25 to 26 (`Engine/Source/Network/NetworkProtocol.h:67`): tag set and every payload layout change.
- `FrameInput::kiVersion` 20 to 21 (`FrameInput.h:10`): replay input and post-dispatch payloads are the `SpawnInfo` images. Every `SpawnInfo` stays trivially copyable (the difference stream writes raw images, `Common/Serialization.h:99-103`).
- `Frame::kiVersion` (`Frame.cpp:27`): unchanged; no CRC semantics or save shape changes. Grid saves hold no queue entries (decision 4).
- `AdmitFrameInputStatusChange` (`FrameInput.cpp:25-38`): for a spawn tag or `kTransferPlayer`, `SpawnPosition` inside the cell (closes the asserting Players overload against a raw image). The other transfer tags are not position-tested: a recorded arrival may rebase exactly onto the destination edge, which a refusing collection drops (`Engine/Source/Frame/AGENTS.md:52`), so admission must not reject it. Then `IsAdoptableStatusChange`; `kSpawnPlayer` `globalPlayerId` nonzero (as today); `kSpawnMissile`/`kTransferMissile` `uiTarget` empty (link rule).
- Per-item bound: the largest table (Players) is 113 bytes under `kiMaxStatusChangeBytesPerItem` 120; the `ASSERT` in `SerializeGroup` (`NetworkSerialization.cpp:32`) guards it. Per-cell cap 1,024 unchanged.
- Trust: no client-to-server record changes; the client trusts its server; replay and save readers admit as above; the harness validates before queuing.

### Documentation

- `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`: `status` fields (`:8-18`: drop `pendingTransferHarnessRigCount`; `harvestedTransferTotal` unchanged); `replay_transfer_capture` (`:39-60`: events are natural arrivals); the `inject_payload` entry (`:195-281`) rewritten to the contract above: the type table with the five spawn types and their `Fields()` keys, the value forms per member type (add `alignment` strings, flags masks, `int64_t`, `uint16_t`, the refused `globalPlayerId`), the defaults, the validation list, the "edge coast" recipe with a `SpawnPlayer` carrying `vecPosition:[433.86,0]`, `vecVelocity:[33.33,0]`, `transferLockTimer:1`, the tick semantics (a spawn entry enters the input of its target tick; no harvest wording), the queue-clearing sentences, `pauseAfterWriterInput` without the transfer-entry requirement, the response without `TransferPlayer`; the per-cell cap note (the `## Out of scope` per-cell cap hazard, documented not fixed).
- `replay.md`: `:13` ("no `inject_payload` transfer entry" becomes "no cell crossing"); `### Replay transfer-capture harness rig` and the A to G matrix, `### Replay capture invalidation`, `### Live adjacent-generation control`, and `### Replay stop-before-dispatch decisions` item 1 rewritten so each arrival event comes from a `SpawnPlayer`/`SpawnSpaceship`/`SpawnBlaster`/`SpawnMissile` entry placed near a cell edge with a velocity that crosses it (the edge-coast recipe), and the kind-2 header byte description names the `TransferPlayer` byte value under the new enum (5) and "that record's serialized `PlayersPostRender::SpawnInfo` image".
- `cross-cell.md:9` (`"bIsFlagship":true` becomes `"flags":132`), `verification.md:17-19` (`fSpawnOffsetX`/`fSpawnOffsetY` become `vecPosition`).
- `Projects/BrokenEngineSandbox/Documents/AgentHarness.md:45`: "transfer harness rigs" becomes "transfer capture".
- `Projects/BrokenEngineSandbox/Source/Save/AGENTS.md:16`: the bullet routing replay-stream invalidation to the harness queue goes.
- `Projects/BrokenEngineSandbox/Source/Agent/AGENTS.md` `## Contracts`: rewrite `:31`'s second sentence: `inject_payload` validates a batch and queues it into the engine `ServerSpawnQueue` the server session owns; the harness module keeps no state; delete the transfer-entries bullet; "field keys are the `Fields()` member tokens of each payload".
- `Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md`: status-change consumption bullet (spawn kinds in Spawn through `ApplySpawnStatusChanges` before Players; the admission list).
- `Projects/BrokenEngineSandbox/Source/Network/AGENTS.md` and `Engine/Source/Network/AGENTS.md`: "`StatusChange` or `TransferData` layout" becomes "`StatusChange` payload (`SpawnInfo` `Fields()`) layout"; the codec is table-driven.
- `Projects/BrokenEngineSandbox/Source/Network/Server/AGENTS.md`: spawn assignment queues the spawn; the spawn queue drain replaces the agent drain wording; the harness-state wording goes from `:15` (the session-bound harness rig slot and its teardown detach), `:43-44` (replay-transfer and injected-status harness rig state), and `:51` (the harness rig queues the two engine managers' resets clear).
- `Engine/Source/Network/Server/AGENTS.md` `## Transfers and Publication`: `ServerSpawnQueue` owns tick-input queuing and sorting; the game names list (`game::ApplySpawnStatusChange`, `SpawnPosition`, `SpawnPositionRef`, `ValidateSpawnVectors`, `OwnedSpawnGlobalId`, `OwnedSpawnClientGuid`, `MakeSpawn`); the harness-rig transfer drain sentence goes.
- `Engine/Source/Frame/Collections/CollectionId.h:9` comment (`TransferData` becomes the player `SpawnInfo`).
- `Documents/Architecture/FrameUpdatePipeline.md`: Spawn-phase participation (`ApplySpawnStatusChanges` first).
- `.agents/skills/add-collection-member/references/worker.md:115-119`: the `TransferData` bullet (`:115`) becomes the collection's `SpawnInfo` member and its `Fields()` entry; source `TransferRequest` construction (`:116`) becomes the Transfer-phase `MakeTransfer(SpawnInfo {...})` literal; the receive mapping (`:119`) is the destination `Spawn` assignment of that `SpawnInfo` member.
- `Documents/Architecture/Network.md`: `:269` and `:309` "`StatusChange`/`TransferData` layout" become "`StatusChange` payload (`SpawnInfo` `Fields()`) layout"; `:298` "player and spaceship TransferData wire payloads exclude arrival grace" becomes "player and spaceship transfer payloads carry the arrival grace their source sets".
- Leftover L1 (`Documents/FloatingPointDeterminism.txt` section 7 stale citations, Tier 1): `ServerSessionRuntime.cpp:299` becomes `Engine/Source/Network/Server/ServerSessionRuntime.cpp:305-325` (a file this change does not edit); the game `ServerSession.cpp:320` citation becomes `ServerSession::AddGameRequiredCoordinates` in `Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.cpp`, and the `Projects/BrokenEngineSandbox/Source/Network/Server/ServerTransferManager.cpp:52` citation becomes `ServerTransferManager::CollectTransfers` in `Engine/Source/Network/Server/ServerTransferManager.cpp`, both by function name because this change edits both files; `Engine/Source/Frame/AGENTS.md:90` becomes `:108`.

## Critical files

- Engine new: `Engine/Source/Network/StatusChangeFields.h`; `Engine/Source/Network/Server/ServerSpawnQueue.h`, `.cpp`.
- Game new: `Projects/BrokenEngineSandbox/Source/Frame/StatusChange.cpp`.
- Deleted: `Projects/BrokenEngineSandbox/Source/SpawnTransfer.h`, `.cpp`.
- Engine changed: `Frame/FrameBase.h`, `.cpp`; `Frame/Collections/Explosions/Explosions.h`, `ExplosionsSpawn.cpp`; `Frame/Collections/CollectionId.h` (comment); `Network/NetworkProtocol.h`; `Network/Server/ServerTransferManager.h`, `.cpp`; `Network/Server/ServerBroadcaster.cpp`; `Network/Client/ReconcileReplayTick.cpp`; `File/Replay.cpp` (the `:878-879` comment and the seven `game::OnReplayStreamsInvalidated()` calls only).
- Game changed: `Frame/StatusChange.h`; `Frame/FrameInput.h`, `.cpp`; `Frame/Frame.h`, `.cpp`; `Frame/Collections/Players/Players.h`, `Players.cpp`, `PlayersNavigation.cpp`, `PlayersCombat.cpp`; `Spaceships/Spaceships.h`, `.cpp`; `Blasters/Blasters.h`, `.cpp`; `Missiles/Missiles.h`, `.cpp`; `Network/StatusChangeCodec.cpp`; `Network/Client/ReconcileReplayClientState.cpp`; `Network/Server/ServerSession.h`, `.cpp`; `Network/Server/ServerClientManager.cpp`; `Agent/Commands/ServerSimulationHarnessRigs.h`, `.cpp`; `Agent/AgentCommandsServer.cpp`; `Save/GameSaveLoad.h`, `.cpp`; both `.vcxproj`/`.filters` for the added and removed files.
- Documentation under `### Documentation`.

## In scope

- `StatusChange.h`: the enum layout, predicates, `SpawnTraits`, variant, `DefaultDataForType`, `MakeSpawn`/`MakeTransfer`, the accessor and apply declarations; removal of `SpawnPlayerData`, `TransferData`, the `operator==`s. `StatusChange.cpp`: their definitions, `IsAdoptableStatusChange` moved from `SpawnTransfer.cpp`.
- Deletion of `SpawnTransfer.h/.cpp`; the four `#include "SpawnTransfer.h"` lines go (`ReconcileReplayTick.cpp:12`, `ServerTransferManager.cpp:12`, `FrameInput.cpp:3`, `ServerSimulationHarnessRigs.cpp:16`).
- The five `SpawnInfo` structs: `Fields()`; Players' two `uint8_t` members; Explosions' `coordinate` member and `Spawn` signature, `ExplosionsSpawn.cpp:26-47` (time from the tick, coordinate from the info), and the three in-tick explosion call sites.
- The include layout under `### Include layout (PCH order)`: `Frame.h:25` removed and the `player_t` alias added to `Players.h`; `#include "Frame/FrameRegistry.h"` in `Missiles.h` and `Spaceships.h`; `#include "Frame/FrameUtils.h"` in `Explosions.h`; `#include "Ui/WindDepositsWrappers.h"` in `Blasters.cpp`.
- `StatusChangeFields.h`; `StatusChangeCodec.cpp` `WriteStatusChangePayload`/`ReadStatusChangePayload`.
- `Frame.cpp` `FramePostRender::Spawn` order; `Players.cpp:214-274` reduced to `kDestroyPlayer`, renamed, and `Players.cpp:341-343`.
- `Frame.h` `TransferRequest`; `FrameBase.h/.cpp` `PrepareTransferRequest`, `PushTransferRequest`; the four Transfer-phase producer sites.
- `ServerSpawnQueue.h/.cpp`; `ServerSession.h/.cpp` (`mpSpawnQueue` construction beside `mpTransferManager`, the `:324-326` comment, the `DetachServerSimulationHarnessRigs` call at `:37`); `ServerBroadcaster.cpp` (`:7-9` includes, `:44`, `:193`); `ServerClientManager.cpp:109-147`.
- `ServerTransferManager.h/.cpp` payload and harness-drain changes listed under `### Arrival path`; `ReconcileReplayTick.cpp:120-155`; `ReconcileReplayClientState.cpp:30-58`; the `Replay.cpp:878-879` comment.
- `GameSaveLoad.h:14` declaration and the `:10` comment's "replay-only game harness rigs", `GameSaveLoad.cpp:35-38`, and the seven `game::OnReplayStreamsInvalidated()` calls in `Replay.cpp` (`:272`, `:301`, `:589`, `:702`, `:709`, `:722`, `:740`).
- `FrameInput.h` version; `FrameInput.cpp` admission and include; `NetworkProtocol.h` version.
- `ServerSimulationHarnessRigs.h/.cpp`: the spawn-entry parser, type names, defaults, validation, commit, queue calls, `pauseAfterWriterInput` check, removal of the two maps and their functions; `AgentCommandsServer.cpp:110`.
- Every switch or predicate over `StatusChangeType` that must name the new tags (`/update-affected-code` finds them).
- vcxproj membership for the added and removed files; the documentation under `### Documentation`, Leftover L1 included.

## Out of scope

- Any new `Collection<T>` or SOA column; any `Spawn(SpawnInfo)` body beyond Explosions' signature; `DestroySweep`, the Destroy phase, `CollectionLifecycle.h`.
- Arrival timing, the destination CRC recompute, `HarvestTransfers`' place in `FinalizeFrameTick`, the replay post-dispatch channel, `Replay.cpp` beyond its `:878-879` comment and its seven `game::OnReplayStreamsInvalidated()` calls, `ReplayHarnessRigs`, `replay_transfer_capture`'s shape, `CountCapturedReplayTransfers`.
- Grid-save contents and `Frame::kiVersion`; `GridSave.cpp`; queue persistence.
- `ProcessUpdatePlayerRequests`, `FleetNavigationController`, `kUpdatePlayer`/`kUpdateFleet`/`kDestroyPlayer` semantics and wire bytes; `edit_frame`, `read_frame`, `query_*`.
- A destination-liveness or active-coordinate test inside `ServerSpawnQueue`; `AddGameRequiredCoordinates`; any gameplay producer (portal, carrier): none is added.
- The per-cell 1,024 status-change cap (`Engine/Source/Network/NetworkProtocol.h:94`): `inject_payload` still puts no bound on entries per cell tick, and an over-cap batch still hits `ServerBufferedFrames.cpp:51-58` (debug break, client resync). Pre-existing; documented in the `inject_payload` entry and left to the follow-up Plan `Documents/Plans/Game/InjectPayloadPendingBound.md`, which depends on this one.
- The pre-existing `Projects/BrokenEngineSandbox/Source/AGENTS.md` sentence that the client removes transfers "before normal Spawn processing" (it is after); untouched.
- Client-only files other than `ReconcileReplayTick.cpp` and `ReconcileReplayClientState.cpp`; `NetworkSerialization.h/.cpp`; the batch envelope.
- `Pch.h` and `Engine.h` include order.
- `Documents/Features/**` (manual; `Documents/Features/Frame/TeleportMovementOverride.md:11,64` names `SpawnPlayerData`/`TransferData`).
- Every other Plan under `Documents/Plans/`: the coordination edits that rewriting this Plan in place implies are made when it is saved, not by this change.

## Risk triggers and invariants

Tier 3 (`.agents/references/risk-tiers.md`): wire tags, payload layouts, and `kiProtocolVersion`; the replay input format (`FrameInput::kiVersion`); CRC'd row creation inside the tick on both builds; server tick-input ordering; spanning engine Network, engine Frame, game Frame, game Network, and game Agent.

- Every spawn issued from outside the tick is carried in the cell's tick input; server, every client, and replay playback apply it through `ApplySpawnStatusChange` inside the same tick before the CRC stamp. Arrivals apply post-tick through the same function with the CRC recompute, on both sides, as today.
- Server applied order == recorded order == wire order for every coordinate and tick (`Drain` sorts every input; nothing writes an input after it).
- Owned rows: pushers have no `SpawnInfo` and no `SpawnTraits`, so they cannot be queued, parsed, or applied from outside `PlayersPostRender::Spawn`/`SpaceshipsPostRender::Spawn`; `uiTarget` is outside the missile table and rejected nonzero by replay admission; uuids and registry ids are minted inside `Spawn` in today's order.
- The asserting Players overload is reached only after a bounds test or a bounds `ASSERT`: `ServerSpawnQueue::Spawn` asserts at the queuing call (producers pass in-cell positions); the harness and replay admission (spawn tags and `kTransferPlayer`) refuse an outside position; the arrival rebase produces a destination-local position.
- Every `SpawnInfo` stays trivially copyable (`static_assert` in `StatusChange.h`); the largest wire image stays under 120 bytes; `kiProtocolVersion` 26 and `FrameInput::kiVersion` 21; `Frame::kiVersion` unchanged.
- Pause, inject, unpause applies every injected spawn in the first unpaused tick.
- Main-thread producers never touch a cell's frame while it ticks: queue, drain, collect, and apply run between ticks; `RunFrameTick` reads only its `FrameInput`.
- No client-to-server record changes; the trust model is unchanged. `clientGuid` never reaches the wire or the CRC.

## Acceptance criteria

Builds: BrokenEngineSandbox server Debug and Profile, client Debug; zero errors.

Pre-change runs: none. Every runtime criterion is new behaviour the unedited tree refuses by code read (`inject_payload` rejects `SpawnSpaceship`, `ServerSimulationHarnessRigs.cpp:392`), except check 5, whose arrival timing must match today's and is settled by the code read in check 9 plus the no-gap observation.

Harness setup (`/agent-harness`, Debug server, `reset`, then `pause {"paused":true}`; `T` = `status.tick`; "one tick" means `timescale {"faster":false}` five times to 1/32, `pause false`, wait 1.2 s, `pause true`, and require `status.tick == T + 1`):

| # | Check | Expected observation |
|---|---|---|
| 1 | While paused, first `edit_frame {"coord":[0,0],"edits":[{"collection":"frame","writes":{"fSpawnTimer":0}}]}` so the spaceship group spawner cannot add rows in the injection tick, then one `inject_payload` at `[0,0]` in non-type order: `SpawnMissile` `vecPosition:[6,6]`; `SpawnExplosion` `iTypeIndex:0`, `vecPosition:[7,7]`; `SpawnSpaceship` `vecPosition:[10,20]`, `alignment:"enemy"`; `SpawnPlayer` `vecPosition:[45,-12]`, `flags:132`; `SpawnBlaster` `vecPosition:[5,5]` | `injected:5`, `globalIds:[G]`, `deferred:true`; `status.pendingAgentStatusChangeCount` 5 and `query_frame` counts unchanged while paused; after one tick: pending 0, `harvestedTransferTotal` unchanged; `read_frame`: spaceships +1 with `pAlignments` equal to `read_frame frame` `enemyAlignment`, `pfHealths` 10, nonzero `puiPushers` and `puiRegistryIds`, `pArrivalGracePeriods` 0; players +1 with `pGlobalPlayerIds` G, `pClientGuids [0,0]`, `pfArmors` 50, `pFlags` containing 128; pushers +2; blasters +1; missiles +1 with `pDeltaRotationDelays` positive; explosions +1 with `puiTypeIndices` 0 and `pStartTimes` equal to `read_frame frame` `iTick` x 31,250,000 |
| 2 | Rejections, each its own command: `type:"TransferSpaceship"`; `SpawnSpaceship` `vecPosition:[10000,0]`; `alignment:"neutral"`; `SpawnBlaster` `iTypeIndex:255`; `SpawnBlaster` `iTypeIndex:-1`; `SpawnExplosion` `iTypeIndex:255`; `SpawnExplosion` `iTypeIndex:-1`; `SpawnExplosion` `iTrailCount:-1`; `SpawnExplosion` `fTimePercent:1.5`; `SpawnMissile` with `uiTarget`; `SpawnPlayer` with `globalPlayerId`; `SpawnSpaceship` at inactive `[5,5]`; `pauseAfterWriterInput:true` with a `tick` entry | `ok:false` each; pending count and `status.nextGlobalId` unchanged |
| 3 | Fresh `reset`, unpaused, one connected client subscribed at `[0,0]`: the check 1 batch; run 128 ticks | no `CONFIRMED DESYNC` in the client log |
| 4 | Same client: `click {"label":"[+]##Fleet"}`, `click {"label":"[+]##Player"}`, poll `set_client_grid_coord {"coord":[0,0]}` until `ok:true` | a `players` row with nonzero `pClientGuids`; a `SendPlayerState State: Spawned` line; no `CONFIRMED DESYNC` |
| 5 | Cross-cell, no gap: same client, `pause {"paused":true}`; `inject_payload` `SpawnPlayer` at `[0,0]` with `vecPosition:[433.86,0]`, `vecVelocity:[33.33,0]`, `transferLockTimer:1`, and `G` = its `globalIds` entry; then 40 one-tick steps (the setup's "one tick", each requiring `status.tick` to advance by exactly 1); after every step read `status`, `query_players {"coord":[0,0]}`, and, while `[1,0]` is in `activeCoords`, `query_players {"coord":[1,0]}` | after every step `G` appears in exactly one of the two cells' rows (an absent `[1,0]` counts as no rows); at the first step `X` where `G` is at `[1,0]`, `[1,0]` has joined `activeCoords` and `harvestedTransferTotal` has risen by exactly 1 over the previous step; no `CONFIRMED DESYNC` |
| 6 | Replay: `reset`, `replay_record {"start":true}`, wait `status.recording`, `pause {"paused":true}`; one `inject_payload` at `[0,0]` holding the check 1 batch followed by the check 5 `SpawnPlayer`, with `G` = the last `globalIds` entry; one-tick steps until `query_players {"coord":[1,0]}` holds `G`, and `X` = that step's `status.tick`; then `timescale {"faster":true}` five times to 1/1, `pause false`, record about 64 more ticks, stop, `replay_play` | `replay_transfer_capture` has an `events` entry with `recordingEventTick` `X` and `playerCount` 1, and after playback every entry's `playbackEventTick` equals its `recordingEventTick`; an `End replay ..., looping` marker; no `LogDifferences CRC` line (the playback reads and admits the four non-player spawn payloads and both player spawns from the recorded input) |
| 7 | After check 1 plus 64 ticks: `save`, `load {"pauseAfterLoad":true}` | `resetToFresh:false`; counts equal before and after; no new `Assert` line |
| 8 | Documentation diff read | `commands-server.md` `inject_payload` lists the five spawn types with their `Fields()` keys, defaults, value forms, validation, and the cap note, names no `Transfer*` type or `pendingTransferHarnessRigCount`; `replay.md` recipes drive arrivals by cell crossings; `cross-cell.md` and `verification.md` use `flags`/`vecPosition`; `AgentHarness.md:45` says "transfer capture"; `Save/AGENTS.md` has no harness-queue bullet; the AGENTS.md files under `### Documentation` name `ApplySpawnStatusChanges`, `ServerSpawnQueue`, and the table-driven codec; `FloatingPointDeterminism.txt` section 7 cites the corrected locations; `Documents/Architecture/Network.md` and `add-collection-member/references/worker.md` name no `TransferData` or `SpawnTransfer.cpp`; no contradicting sentence |
| 9 | Code read | versions 26 and 21, `Frame::kiVersion` unchanged; `ApplySpawnStatusChanges` runs first in `FramePostRender::Spawn`; `Drain` runs inside `PrepareTickStatusChanges` before the snapshot and sorts every active input, and no other sort of an input remains; `ServerTransferManager::SpawnTransfers` and the client post-tick loop both call `ApplySpawnStatusChange`, both followed by the CRC recompute, and `FinalizeFrameTick` still harvests before `SwapFrames`; `kOwnedEntityTransferType == kTransferPlayer`; no `SpawnTraits` for pushers; `uiTarget` absent from the missile table; `clientGuid` absent from the player table; the harness tests the position before queuing and refuses `globalPlayerId`; the fleet spawn calls `mpSpawnQueue->Spawn`; `ServerSpawnQueue::Spawn` returns `void` and `ASSERT`s the position inside the cell; `StatusChange.h` `static_assert`s that the five `SpawnInfo`s are trivially copyable; `ServerSpawnQueue::Spawn`/`Queue`/`Drain`/`ResetState`, `CollectTransfers`, and both arrival `ApplySpawnStatusChange` calls run on the main thread between ticks, never from `RunFrameTick` or a dispatch job; admission position-tests spawn tags and `kTransferPlayer` only |

## Execution card

Execution card:
### What does this plan do?
It makes one message family carry every creation of a ship, player, shot, missile, or explosion that is decided outside the simulation tick: the message is the collection's own spawn settings struct, a per-struct field table drives both the network codec and the agent harness, and a new server queue is the one place every producer (the fleet spawn, the harness, a future portal) puts a spawn so it enters the cell's next tick on the server, every client, and any replay identically. Cross-cell arrivals become the same message with a transfer tag and keep landing in the destination right after the tick that produced them, through the same apply function.
### Why this is good for the codebase
Planned features such as ships arriving through portals need the server to create entities at chosen positions in a way every client and replay reproduces exactly; today only fleet players and cross-cell arrivals can do that, and arrivals carry a second copy of every collection's spawn data. Reusing each collection's `SpawnInfo` as the payload removes that copy, the eight hand-written codec cases, and the harness's two side queues, and gives agents one `inject_payload` path to build any deterministic, recordable scene.
- Goal: the status-change family, field tables, codec, `ServerSpawnQueue`, Spawn-phase application, producer and arrival rewrites, harness contract, version bumps, and documentation under `## In scope`.
- Out of scope: per `## Out of scope`; in particular no arrival-timing change, no grid-save change, no new Collection or column, no gameplay producer, no cap fix.
- Tier trigger: Tier 3; wire tags and `kiProtocolVersion`, replay input format, CRC'd row creation inside the tick, server tick-input ordering, spanning engine Network and Frame and game Frame, Network, and Agent.
- Interfaces and invariants: per `## Design` declarations and `## Risk triggers and invariants`; `kiProtocolVersion` 26, `FrameInput::kiVersion` 21, `Frame::kiVersion` unchanged; `inject_payload` types `SpawnPlayer|SpawnSpaceship|SpawnBlaster|SpawnMissile|SpawnExplosion|DestroyPlayer|UpdatePlayer|UpdateFleet`, response shape unchanged minus `TransferPlayer` ids; `status` loses `pendingTransferHarnessRigCount`.
- Acceptance checks: the `## Acceptance criteria` table; pre-change runs none.
- Roles:
  - Plan review: `reviewer` `/plan-audit`; fresh `reviewer` `/plan-simplicity-review`; `implementer` evidence for `/external-grill-plan` rounds; main `/verify-external-claims` via `locator` if a reviewer asks.
  - Implement: `builder` `/compile` for any unedited-tree build a `before the first edit` check needs (none); `implementer` `/implement-plan` per slice (slice A: `StatusChange.h/.cpp`, the include layout, `SpawnInfo` tables, `StatusChangeFields.h`, codec, Explosions signature, `FrameInput` admission and version; slice B: `TransferRequest`, `FrameBase`, the four Transfer sites, `ServerTransferManager`, `ReconcileReplayTick`, `ReconcileReplayClientState`, the `Replay.cpp` comment; slice C: `ServerSpawnQueue`, `ServerSession`, `ServerBroadcaster`, `ServerClientManager`, `Frame.cpp` Spawn order, `Players.cpp`; slice D: harness, `AgentCommandsServer.cpp`, `GameSaveLoad.h/.cpp` and the seven `Replay.cpp` hook calls, documentation); `implementer` `/update-affected-code`.
  - Pre-review: main runs `.agents/scripts/Invoke-StaticChecks.ps1`; `mechanic` `/code-style-review`; `builder` `/compile` (server Debug and Profile, client Debug).
  - Review: `reviewer` `/over-engineering-review` finder then fresh `implementer` validator; `reviewer` `/repo-code-review`; `mechanic` `/comment-review`; fresh `reviewer` `/coherence-review` for the changed non-C++ artifacts; `reviewer` `/adversarial-review`; `implementer` `/resolve-findings`; `implementer` `/external-diagnose-bug` when a fix fails retest.
  - Cleanup: `mechanic` `/update-vcxproj` (files added and removed); `implementer` `/update-claude-docs`; fresh `reviewer` `/progressive-disclosure-review` (AGENTS.md and a skill reference change); fresh `reviewer` `/external-skill-creator` validate mode (`.agents/skills/add-collection-member/references/worker.md` changes).
  - Runtime: `implementer` drives `/agent-harness` for checks 1-7.
  - Landing: `implementer` `/finalize-changes`.
- Data mode: Shared; no `runtime-data-mode.md` Local trigger.
- Leftovers: L1 (`FloatingPointDeterminism.txt` section 7 citations, Tier 1) is in the documentation slice. The per-cell cap hazard goes to `Documents/Plans/Game/InjectPayloadPendingBound.md` (`## Out of scope`).

## Unresolved decisions

None.
