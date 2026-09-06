<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-30T23:54:31.474Z","dependsOn":[]} -->
# Preserve Spaceship hysteresis flags across cell transfers

## Context

The current source still drops Spaceship behavior flags at the cross-cell
boundary.  `SpaceshipsPostRender::Transfer` emits position, direction,
velocity, alignment, health, the blaster timer, and delta rotation, but no
flags (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/Spaceships.cpp:369-420`).
`TransferData::SharedMembers` has no Spaceship flag field
(`Projects/BrokenEngineSandbox/Source/Frame/StatusChange.h:90-109`), the
spaceship serializer/deserializer and item-width mirror omit it
(`Engine/Source/Network/NetworkSerialization.cpp:21-30,77-86,124-146`), and
`SpawnTransfer` passes neither flags nor a transfer marker
(`Projects/BrokenEngineSandbox/Source/SpawnTransfer.cpp:13-27`).
`SpaceshipsPostRender::Spawn` consequently zeros every destination row's flags
(`Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/Spaceships.cpp:550-592`).

The omitted `kFleePlayer` and `kReturnToIslandCenter` flags are shared
PostRender state (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/Spaceships.h:103-151`).
`ComputeSteering` changes them with distance hysteresis
(`SpaceshipsNavigation.cpp:45-117`), and `Update` feeds them into the first
destination steering and acceleration decision (`Spaceships.cpp:623-667`).
The source, destination, and resulting CRC state therefore lose a persistent
gameplay decision even when the transfer lands inside a hysteresis band.

The governing transfer contract requires carried gameplay state to be restored
verbatim, with only arrival grace and named client-only/shield state using fresh
defaults (`Projects/BrokenEngineSandbox/Source/Frame/Collections/AGENTS.md:12-13`).
These two flags are shared gameplay state, not one of those exceptions.
`Spaceships/AGENTS.md:7-8` documents hysteresis behavior and turn-rate
continuity; the collection-wide verbatim-transfer contract alone resolves the
policy to carry these two non-transient flags.  `kTransfer` and `kExploding`
retain their transient semantics and are masked out.  The existing
`SpaceshipsPostRender::SpawnInfo` has no transfer marker, which is the local
materialization gap called out by the governing contract.

## Design

Author's recommendation: extend the existing `kTransferSpaceship` arm with one
Spaceship-specific `uint8` field in `TransferData::SharedMembers`, adjacent to
the existing Spaceship fields.  `SpaceshipsPostRender::Transfer` should write
exactly `kFleePlayer` and `kReturnToIslandCenter`, masking `kTransfer` and
`kExploding` before the value enters the payload.  Keep the field in the
existing spaceship serializer/deserializer arm and update its per-arm
item-width expression.  Verify the shared `kiMaxStatusChangeBytesPerItem`
remains sufficient; raise that maximum only if the then-current largest
payload exceeds it.

Pass the carried value into `SpaceshipsPostRender::SpawnInfo` together with a
local `bTransfer` marker.  `SpawnTransfer` already knows the status type, so
the marker remains local and is not serialized.  `Spawn` should select the
carried two-bit value only when `bTransfer` is true and retain zero/default
flags for genuine spawns.  Preserve the fresh one-second arrival grace,
health, timers, delta rotation, client hydration, and all existing transient
flag behavior.

Increment the current `engine::kuiProtocolVersion`, `FrameInput::kiVersion`,
and `SpaceshipsPostRender::kiVersion` once for this incompatible current
format, deriving each value from the then-current baseline.  `Frame::kiVersion`
already composes the Spaceship collection version, so do not add an independent
base bump.  Do not add a compatibility reader or a new status-change type.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/Spaceships.cpp:369-420,550-592,623-667` — transfer producer, destination initialization, and first behavior decision.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/Spaceships.h:103-175` — flag enum, CRC-visible collection, and `SpawnInfo`.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/SpaceshipsNavigation.cpp:45-125` — hysteresis and flag-driven movement.
- `Projects/BrokenEngineSandbox/Source/Frame/StatusChange.h:90-164` — `TransferData` shared payload and raw replay shape.
- `Projects/BrokenEngineSandbox/Source/SpawnTransfer.cpp:13-27` — transfer materialization.
- `Engine/Source/Network/NetworkSerialization.cpp:21-30,77-86,124-146,186-197,348-359` — spaceship encode/decode and per-arm item-width expression.
- `Engine/Source/Network/NetworkSerialization.h:18-23` — shared per-item and batch-size bounds; verify the shared maximum and raise it only if the then-current largest payload exceeds it.
- `Engine/Source/Network/NetworkProtocol.h:63` — protocol compatibility gate.
- `Projects/BrokenEngineSandbox/Source/Frame/FrameInput.h:9-18` — replay input compatibility gate.
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:15-18,702-715` — composed Frame version and shared CRC path.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/AGENTS.md:12-16` and `Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/AGENTS.md:7-13` — transfer and Spaceship behavior authority.

## In scope

- Carrying exactly `kFleePlayer` and `kReturnToIslandCenter` through the
  existing `kTransferSpaceship` producer, `TransferData`, codec,
  `SpawnTransfer`, and `SpaceshipsPostRender::SpawnInfo` path.
- Keeping `kTransfer` and `kExploding` out of the carried value while retaining
  their current lifecycle semantics.
- Selecting carried flags through the explicit local transfer marker and
  retaining genuine-spawn zero/default flags.
- Updating the current spaceship transfer per-arm item-width expression;
  verifying the shared `kiMaxStatusChangeBytesPerItem` remains sufficient and
  raising it only if the then-current largest payload exceeds it; updating the
  protocol, raw replay, and Spaceship collection version gates as described
  above; keep the composed `Frame::kiVersion` base unchanged.
- A focused deterministic transfer scenario covering both hysteresis bands,
  the first destination steering/acceleration decision, genuine spawn
  defaults, wire round-trip and old-generation rejection, client/server CRC,
  record/replay, and the existing Debug client/server build checks.

## Out of scope

- Arrival-grace duration or targeting/behavior policy, health validation,
  health/timer/delta-rotation semantics, or genuine fresh-spawn tuning.
- Navigation algorithms, hysteresis thresholds, first-dispatch scheduling,
  collision/damage, or any other Spaceship behavior changes.
- Other transfer arms, a new status-change type, a serialized transfer marker,
  compatibility readers/shims, normal save layout, `.pack` data, or unrelated
  client-only visual state.
- Unit tests, source implementation in this planning stage, and unrelated
  Plans or documentation changes.

## Risk tier and invariants

Expected future Change Workflow Tier 3.  Trigger: the change alters shared
deterministic gameplay state at a cross-cell boundary, an incompatible network
wire and raw replay payload, CRC-visible destination state, and client/server
parity.

Preserve these invariants:

- A transferred Spaceship restores the two non-transient hysteresis flags
  bitwise before its first destination behavior decision.
- Genuine spawns retain zero/default flags, selected by the explicit local
  marker rather than by testing a carried value.
- `kTransfer`, `kExploding`, arrival grace, health, timers, delta rotation, and
  client-only hydration retain their independent behavior.
- Transfer codec write/read order and per-arm item-width expression remain
  paired with the shared maximum, which is raised only if the then-current
  largest payload exceeds it; protocol gate, replay input version, collection
  version, and composed Frame version remain paired; client and server produce
  the same CRC and replay result.

## Acceptance criteria

- A deterministic scenario places a Spaceship in each flee and return
  hysteresis band, captures its flags before transfer, and observes bitwise-
  equal carried flags immediately after destination materialization and through
  the first steering/acceleration decision.
- A genuine Spaceship spawn in the same run retains zero/default behavior flags,
  while a transfer uses the explicit marker and never carries `kTransfer` or
  `kExploding`.
- The expanded current-format spaceship transfer round-trips through the
  network codec; the incremented protocol and `FrameInput` gates reject the old
  format through existing compatibility handling without a shim.
- The transfer produces matching client/server CRCs, and recording then
  replaying the scenario completes without checksum, read, or desynchronization
  errors.
- Existing arrival grace, health/timer/delta-rotation, collision, and genuine
  spawn behavior remain unchanged, and client/server `Debug|x64` builds pass
  through `/compile`.

## Coordination

`Documents/Plans/Game/PlayerTransferNavigationState.md` independently
changes `TransferData`, the Player per-arm codec width (its planned arm is 152
bytes), and the protocol/replay version gates.  That 152-byte arm independently
requires and informs the shared maximum change.  No dependency is required.
Whichever Plan lands second must re-derive the current field order and each
per-arm width from the then-current baseline, verify the shared
`kiMaxStatusChangeBytesPerItem` remains sufficient for the resulting largest
arm, and raise it only if that largest arm exceeds the then-current maximum,
while preserving the fields and version changes from the first Plan.  This
Plan's Spaceship arm becomes 65 bytes after the added flag and must be included
in that verification; it must not hard-code a next version or reorder the
Player arm's fields.

## Notes

This Plan supersedes
`Documents/Investigations/Engine/SpaceshipTransferBehaviorFlags.md`.  The
superseded investigation recorded the frozen source candidate
`CPT/shard-0050/001` at frozen audit commit
`80896f33661aaab99cf180a96db54600099be652`.  Its source finding remains valid;
the current live evidence and references are captured above, and its
carry-versus-recompute choice is resolved by the current governing transfer
contract.  No source, build, harness, claim, commit, or landing change is part
of Plan authoring.
