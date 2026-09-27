<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-26T22:51:39.461Z","dependsOn":[]} -->
# Remove client-side checks on server-to-client data and rescope the Plans built on them

## Context

User decisions this session, recorded here as decided. Server-to-client
traffic is protected by ENet's per-datagram checksum, enabled on both hosts
(Q7a). The client trusts server data fully and drops both value checks and
structural or memory-safety checks on it (Q6b). Replays and the agent command
channel are exempt developer tools (Q9a). Saves are server input and get full
validation (Q10). The prerequisite Plan enables the checksum and records the
policy. This Plan removes the client checks that the checksum makes
unnecessary.

Line numbers are at `1192c531`; re-derive them at claim. The change that
created this Plan removed `ValidateTransferPlayerFireTime` and its callers.
Confirm that removal is present at claim.

The survey found these client-only checks on server data. No save or
client-to-server read reaches them:

- `Engine/Source/Network/Client/ClientReceive.cpp`:
  - `DecompressAndReadFrame`: the size/null precondition throw (`:17-21`), the
    LZ4 exact-size throw (`:26-29`), and the post-read `!frameStream` throw
    (`:34-37`).
  - `ServerCoordFullState`: the slot-range throw (`:179-182`) and the
    envelope/Frame tick-mismatch throw (`:215-218`).
  - `ServerCoordStaticData`: the `iSize <= 0` throw (`:254-257`), the
    slot-range throw (`:259-262`), the `!staticStream` throw (`:298-302`), and
    the island-template membership loop (`:304-313`).
  - `ServerCoordUpdateOrResend`: the slot-range throw (`:361-364`).
  - `ServerUnsubscribeAck`: the slot-range throw (`:573-576`).
  - `ServerTimespeedUpdate`: the fixed-size throw (`:616-619`) and the
    non-positive-ratio throw (`:623-629`).
- `Engine/Source/Network/NetworkSerialization.cpp`:
  - `DeserializeStatusChangeBatch` (`:276-393`): the truncated-header
    (`:296-300`), type-byte range (`:305-309`), count-cap (`:311-315`), and
    truncated-item (`:322-326`) throws, and the `game::IsAdoptableStatusChange`
    rejection (`:384-388`) that rejects a `kTransferBlaster` change naming an
    unregistered Blaster type index.
  - `DecompressStatusChangeBatch` (`:430-469`): the short-source (`:432-435`),
    size-prefix-bound (`:446-450`), and LZ4-failure (`:460-464`) throws.
  - Their only caller is the client (`ClientReceive.cpp:379`, through
    `game::NetworkSessionContract::DecompressStatusChanges`).
- `Engine/Source/Frame/NavCellData.cpp` `NavData::Read` (`:478-559`): the three
  `ValidateDeserializedCount` calls, the finite-vertex throws, and the topology
  throws. The only caller is `FrameStaticData::Read` with `bIncludeNavData`
  true, which only the client network path passes
  (`ClientReceive.cpp:297`; the save path passes false at
  `Engine/Source/File/GridSave.cpp:116`).
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp`:
  - `FrameInterpolate::ServerRead`: the `AdmitSpawnTimer` call (`:728`).
  - `Frame::ServerRead`: the two `ValidateCollectionPairs` calls (`:857`,
    `:859`).
  - The save path's `AdmitSpawnTimer` (`:715`) and `ValidateCollectionPairs`
    calls in `Frame::operator>>` stay.
- `Projects/BrokenEngineSandbox/Source/Network/PlayerEvents.cpp`
  `ParsePlayerEvents`: the two exact-size throws (`:20-23`, `:30-33`) and the
  unknown-wire-type throw (`:37-40`).
- `Projects/BrokenEngineSandbox/Source/Network/GameMessages.h`
  `FleetSyncMessage::ReadPayload` (`:131-178`): the three `IsValid` throws
  (`:136-139`, `:147-150`, `:158-161`), the flagship relation throw
  (`:164-171`), which requires an empty fleet to have no flagship ID and a
  nonempty fleet's flagship global player ID to name one of its members, and
  the `AtEnd` throw (`:174-177`). Its only caller is the client
  (`Projects/BrokenEngineSandbox/Source/Network/PlayerEvents.cpp:64`).

The client agent command `client_packet_fault_fixture`
(`Projects/BrokenEngineSandbox/Source/Agent/Commands/ClientPacketFaultFixture.cpp`)
offers three cases. Two of them exist only to hit checks this Plan removes:

- `status_change` (`:82-116`) sends a coord update whose uncompressed-size
  prefix is out of range. It hits the `DecompressStatusChangeBatch`
  size-prefix-bound throw.
- `game_packet` (`:117-120`) sends a short `kServerAssignPlayer` packet. It hits
  the first `ParsePlayerEvents` exact-size throw. It is the only case that
  delivers a game packet, so it is the only reason
  `InjectArmedClientPacketFault` calls
  `ClientSession::ProcessReceivedGamePackets` (`:157`, `:159-162`). That call
  is also the reason for the `friend` declaration and its comment in
  `Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.h:84-86`.

The remaining case, `engine_envelope`, hits `NetworkMessages::Read`, which
stays, so the command stays. Its documentation is the
`client_packet_fault_fixture` entry in
`Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-client.md`
`### Client commands` (`:10-18`).

The replay reader has the same adoptability check:
`Projects/BrokenEngineSandbox/Source/Frame/FrameInput.cpp` `operator>>`
rejects a payload `IsAdoptableStatusChange` refuses (`:54-58`). Replays are an
exempt developer tool, and this Plan leaves replay readers unchanged, so that
check stays, and `IsAdoptableStatusChange`
(`Projects/BrokenEngineSandbox/Source/SpawnTransfer.cpp:105-113`) stays as its
remaining caller's predicate.

Some checks run on the client but are shared with the server's save read, which
decision Q10 requires to stay validated:

- Collection count/capacity and id-map checks in `Collection::Read`.
- The `PostRead` hooks reached through `NormalizeAfterRead`
  (`Engine/Source/Frame/Collections/Collection.h:73-83`, `:443`).
- `FrameStaticData::Read`'s placement-count bound.
- The engine `NetworkMessages` reader, which also decodes client-to-server
  packets.

Two queued Plans justify their checks partly by client-side or replay reads
that the policy makes unnecessary. Each carries this Plan in `dependsOn`:

- `Documents/Plans/Engine/CollectionIdMapIdentity.md`: the save part stays, the
  replay part is exempt, and the full-state part is unnecessary. Rescope it to
  the save read.
- `Documents/Plans/Engine/CollectionTypeIndexValidation.md`: the check sits in
  `ExplosionsInterpolate::PostRead`, which the save read runs. Rescope its
  motivation and wording to the save read.

## Design

- **Decided (dispatch brief, following the user's Q6b):** leave the `PostRead`
  hooks running on client network reads. They are required on the server's
  save read, they cannot tell which source they are reading, and guarding them
  by build or path adds code for no behavioral gain. Client execution is
  redundant but harmless.
- **Decided (dispatch brief, following the user's Q6b):** remove the
  `status_change` and `game_packet` cases of `client_packet_fault_fixture`
  together with the checks they exercise. Keep `engine_envelope` and the
  command.
  - Narrow the case validation and its error strings to `engine_envelope`.
  - Delete the two case branches.
  - Delete the game-packet drain in `InjectArmedClientPacketFault`.
  - Delete the `ClientSession.h` `friend` declaration and its comment.
  - Rewrite the `commands-client.md` entry: the schema becomes
    `{"case":"engine_envelope"}`, and the `game_packet` log alternative, the
    two table rows, and the `status_change` slot paragraph go.
- Author's recommendation: delete each client-only check listed in
  `## Context`, leaving the decode logic around it unchanged. Where a check was
  the sole use of a value, delete that value too:
  - Drop the `iMaxCount`/`iCapacity` parameter from
    `DeserializeStatusChangeBatch`, `DecompressStatusChangeBatch`, the engine
    `NetworkSessionContract` concept requirement
    (`Engine/Source/Network/NetworkSessionContract.h:19`), the game
    `DecompressStatusChanges`, and the one caller.
  - Delete `kiMaxUncompressedFrameBytes`
    (`Engine/Source/Network/NetworkProtocol.h:88`). Its only use is the
    removed precondition.
  - Keep `BoundedCursor` in `DeserializeStatusChangeBatch` only as the loop's
    remaining-bytes driver.
- Author's recommendation: keep these client checks:
  - The protocol-state checks: load generation, epoch, subscription state,
    ghost handling, the `kConnectionAccepted` gate, and the monotonic
    echoed-timestamp guard. `Engine/Source/Network/AGENTS.md`
    `## Corrupt Input Policy` defines these as not corruption.
  - The `ServerSubscribeAccept` out-of-range branch. It is a recovery
    unsubscribe that only agent fixtures reach
    (`Engine/Source/Network/Client/AGENTS.md`), and the fixture observes it.
  - The RTT seed bound, which checks local time, not server data.
  - The `ios_base::failure` catches in `Client::Receive` and
    `ClientSession::ProcessReceivedGamePackets`, because shared readers still
    throw.
- Author's recommendation: reword the documentation and comments that describe
  the removed checks, in the same change:
  - `Engine/Source/Network/AGENTS.md` `## Ownership`, the batch
    all-or-nothing bullet: the receive side no longer throws.
  - `Engine/Source/Network/Client/AGENTS.md`
    `## Subscription Receive Invariants`: the static-data unknown-island
    sentence and the full-state tick-mismatch sentence.
  - `Engine/Source/Frame/AGENTS.md`, the navigation deserialization bullet:
    navigation data arrives only from the server and is trusted.
  - `Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md`, the spawn-timer
    bullet: drop "and full-state".
  - The trust-boundary comments at the removed sites and at the client catches
    (`Engine/Source/Network/Client/Client.cpp:359-362`;
    `Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.cpp:73-76`;
    `NetworkSerialization.cpp:125-129`, `:283-287`, `:442-445`;
    `NetworkSerialization.h:37`, `:44`; `NavCellData.cpp:482-483`).
  - The comments that cite the FleetSync relation check as a guarantee
    (`Projects/BrokenEngineSandbox/Source/FleetSelection.cpp:172`;
    `Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetManager.cpp:338`):
    the server's fleet invariant, not the reader, provides it.
- Author's recommendation for the two queued Plans: prose-only edits, each
  preserving its metadata marker byte for byte.
  - `CollectionIdMapIdentity.md`: rescope `## Context`, `## Design`,
    `## In scope`, `## Risk tier and invariants`, and `## Acceptance criteria`
    to the save read (`Frame::operator>>`). Remove the full-state/network
    wording. Note that replays share the reader and are exempt.
  - `CollectionTypeIndexValidation.md`: name the save read as the boundary in
    the same sections. Note that the check also runs on client reads through
    the shared `PostRead`.

## Critical files

- `Engine/Source/Network/Client/ClientReceive.cpp`
- `Engine/Source/Network/NetworkSerialization.cpp`, `NetworkSerialization.h`, `Engine/Source/Network/NetworkSessionContract.h`
- `Engine/Source/Frame/NavCellData.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp`
- `Projects/BrokenEngineSandbox/Source/Network/PlayerEvents.cpp`, `GameMessages.h`, `NetworkSessionContract.h`
- `Projects/BrokenEngineSandbox/Source/FleetSelection.cpp`, `Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetManager.cpp` (comments only)
- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ClientPacketFaultFixture.cpp`, `Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.h`
- `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-client.md`
- The AGENTS.md files and comments named under `## Design`.
- The two queued Plan files named in `## Context`.

## In scope

- Deleting the client-only checks listed in `## Context`, plus the parameter
  and constant clean-up the Design names.
- The documentation and comment rewording listed under `## Design`.
- In `ClientPacketFaultFixture.cpp` `CommandClientPacketFaultFixture`: the case
  validation, its error strings, and the `status_change` and `game_packet`
  branches. In `InjectArmedClientPacketFault`: the game-packet drain.
- The `ClientSession.h` `friend void InjectArmedClientPacketFault()`
  declaration and its comment.
- The `client_packet_fault_fixture` entry in `commands-client.md`
  `### Client commands`.
- Prose-only edits to the two queued Plans as the Design describes.

## Out of scope

- `PostRead` hooks, `Collection::Read` count/capacity and id-map checks,
  `FrameStaticData::Read`, `NetworkMessages`/`MessageReader`, and every other
  reader shared with a save or client-to-server path.
- The save path's `AdmitSpawnTimer` and `ValidateCollectionPairs` calls.
- Protocol-state checks, the subscribe-accept recovery branch, the RTT seed
  bound, and the client catch blocks themselves.
- Server-side validation, replay readers (including the `FrameInput.cpp`
  adoptability check and `IsAdoptableStatusChange` itself), and agent
  commands other than `client_packet_fault_fixture`.
- The fixture's `engine_envelope` case, its arm/deliver/reset flow, and the
  `Game.cpp` delivery call.
- Rejecting or deleting either queued Plan.
- Any wire, save, Frame, or protocol version change.
- `Documents/Plans/Engine/ReplayTransferCaptureListGrowth.md` and
  `Documents/Plans/Game/AgentCoordinateIntegralValidation.md`.

## Risk tier and invariants

Change Workflow Tier 3. Triggers: a trust-boundary change (the client stops
checking server data), and the change spans independently owned subsystems
(engine Network Client, engine Network codec, engine Frame navigation, game
Frame and Network).

Invariants:

- Valid server data decodes, adopts, and simulates exactly as before, with an
  unchanged CRC and no desync.
- Every check the server's save read or client-to-server decode relies on
  stays.
- Protocol-state handling (generation, epoch, subscription state, ghosts) is
  unchanged.
- No wire or persisted layout changes.

## Acceptance criteria

- The diff removes exactly the checks listed in `## Context` and keeps every
  check listed as kept.
- `client_packet_fault_fixture` accepts only `{"case":"engine_envelope"}` and
  rejects `status_change` and `game_packet`. Its `commands-client.md` entry
  documents only that case. Run live, `engine_envelope` still logs
  `Client::Receive dropped corrupt packet` and asserts.
- A live client connects, subscribes across a cell boundary, follows a
  transfer, and plays for at least a minute with no desync, disconnect, or
  assert. A server debug load (Quickload) followed by resubscription still
  works.
- The two queued Plans carry the edits described. The plan validation step
  reports each one valid with its marker unchanged.
- Client and Server `Debug|x64` build clean through `/compile`.

## Notes

The survey found that the boundary between "structural" and "value" checks is
not sharp on the client. Decision Q6b removes both kinds, so this Plan's only
test is whether a save or client-to-server path also reaches the check.
