<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:49:47.532Z","dependsOn":[]} -->
# Validate persisted Player fire timers before weapon spawning

## Context

The accepted finding `CAI/shard-0048/002` identifies a persisted scalar trust
boundary gap.  `pfNextBlasterFireTimes` is a serialized Player column and is
copied through `PlayersPostRender::Update` without a finite post-read check
(`Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/Players.h:269-285`;
`Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:731-753`).  Raw float
deserialization preserves IEEE non-finite bytes (`Common/Serialization.h:80-92`),
and staged grid-save adoption checks stream structure rather than this field
(`Engine/Source/File/GridSave.cpp:104-167`).  When target acquisition sets
`kFireBlaster`, `SpawnBlasters` subtracts the tick delta and loops while the
timer is nonpositive (`PlayersCombat.cpp:303-376`).  A persisted `-∞` reaches
non-finite vector math and then the Blaster spawn validator; a large finite
negative value can drive an unbounded catch-up burst.  The same timer is carried
by Player transfer data.

The source finding is recorded at
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0048.md:71`
and the consolidated selector at
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:1143`.
Assigned source and authority hashes match frozen baseline
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`; the missing scalar admission is
pre-existing, unresolved, and outside the audit work.

Impact: a structurally valid save, replay, full-state read, or transfer can
terminate the next normal tick or run an unbounded weapon-spawn loop.

## Design

Author's recommendation: validate the Player fire countdown at each existing
frame-read and transfer-spawn admission boundary.  Reject non-finite values and
values that would require more than the documented one-tick catch-up window
through the existing corrupt-state/transfer recovery path; preserve finite
normal countdowns and the current interval/random behavior.  Ensure the loop
has the same bounded catch-up guarantee for accepted finite values without
adding a silent large-burst policy.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/Players.h:269-285` — serialized timer column and Player collection shape.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersCombat.cpp:303-376` — timer decrement and spawn loop.
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:731-753` — frame read/admission boundary.
- `Projects/BrokenEngineSandbox/Source/SpawnTransfer.cpp:75-88` and `Engine/Source/Network/NetworkSerialization.cpp:50-71,111-132` — transfer timer path.
- `Engine/Source/File/GridSave.cpp:104-167` and `Engine/Source/File/Replay.cpp` — staged save/replay consumers.

## In scope

- Finite and bounded catch-up validation for `pfNextBlasterFireTimes` during
  save/replay/full-state read and Player transfer materialization.
- Existing corrupt-state or malformed-transfer failure propagation before
  `SpawnBlasters` can produce non-finite vectors or an unbounded loop.
- Valid timer initialization, fixed-tick firing interval, and client/server
  shared state.

## Out of scope

- Fire-rate tuning, target acquisition, Blaster collision, registry policy,
  transfer wire fields, or unrelated Player timers.
- Clamping malformed state into an arbitrary burst or changing replay/save
  format for valid values.
- Generic float validation unrelated to this serialized Player countdown.

## Risk tier and invariants

Expected Change Workflow Tier 3.  Trigger: opaque save/replay/transfer scalar
state enters CRC-affecting fixed-tick weapon spawning and vector validation.

Preserve these invariants:

- An accepted fire countdown is finite and bounded so one fixed tick's spawn
  loop terminates and produces finite positions/velocities.
- Invalid persisted or transferred values fail before Player/Blaster state
  mutation; valid normal countdowns retain current firing and CRC behavior.
- Client/server shared serialization and transfer fields remain paired.

## Acceptance criteria

- Saves, replays, full-state reads, and transfers containing `NaN`, `+∞`, or
  `-∞` fail before a target-triggered `SpawnBlasters` call can create a row.
- A large finite negative timer cannot create an unbounded burst; ordinary
  finite countdowns and normal one-tick catch-up retain current shot timing.
- Client and server `Debug|x64` builds clean through `/compile`; a focused
  target-present scenario observes controlled rejection rather than a tick abort.

## Notes

The report has no duplicate-family or external-claim hint.  The separate
Spaceship-health Plan owns the analogous but independently rooted health column.
