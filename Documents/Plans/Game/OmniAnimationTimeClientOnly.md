<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-13T23:24:39.387Z","dependsOn":[]} -->
# Make the Omni animation time a client-only column

## Context

`OmnisInterpolate::pAnimationTimes` is listed in `SharedMembers()`
(`Omnis.h:155`), so it is saved, replayed, and sent in full state, and it is
carried in the transfer payload as `OmnisPostRender::SpawnInfo::animationTime`
(`Omnis.h:365`, its `Fields()` entry at `:383`; the source Transfer literal at
`OmnisNavigation.cpp:38`; the destination store at `Omnis.cpp:274`).
`SharedCrcMembers()` already excludes it as "client-only animation time"
(`Omnis.h:180-183`).

`Engine/Source/Frame/AGENTS.md` `## Simulation and Visual State` says a column
no simulation decision reads is visual and does not belong in simulation
state. Its one exception is a client-consumed value that must cross cells with
its entity, which travels in the transfer payload and so is simulation state.

Verified from code, the exception does not apply:

- No simulation decision reads the column. Its only readers are Interpolate
  `Update`, which carries it (`Omnis.cpp:351`, `:382`), the client-only advance
  (`Omnis.cpp:407-413`), the Transfer literal, and the client render
  (`OmnisRender.cpp:225`).
- Only the client advances it: the server's Interpolate `Update` stores the
  previous value unchanged, so on the server every row keeps its spawn value,
  the `SpawnInfo` default of zero.
- Only the server consumes `transferRequests`
  (`Engine/Source/Network/Server/ServerTransferManager.cpp`), so the value that
  crosses a cell transfer is the server's never-advanced value, not the
  client's animation clock. The transfer therefore does not preserve what the
  client shows; a full state likewise overwrites the client's advanced value
  with the server's.
- Fighters and Battleships keep their `pAnimationTimes` in `ClientMembers()`
  under `BT_CLIENT` (`Fighters.h:118`, `:133`; `Battleships.h:116`, `:125`),
  and Fighters' `Spawn` zeroes it on every spawn, transfers included
  (`Fighters.cpp:498`).

So the code holds a visual column in simulation state, against the documented
rule and against the Fighters and Battleships pattern. Originating gap: a
user-requested investigation of client-only state in the game Frame code; the
user approved this change as a follow-up Plan.

## Design

Recommended direction: move the column to client-only state, matching
Fighters and Battleships and the documented rule, rather than making the server
advance a visual clock or documenting a new exception.

- Declare `pAnimationTimes` inside the `BT_CLIENT` block of `OmnisInterpolate`
  and move its tuple entry from `SharedMembers()` to `ClientMembers()`. Update
  the `SharedCrcMembers()` comment, which no longer needs to name animation
  time.
- Move the load and store in `OmnisInterpolate::Update` under `BT_CLIENT`,
  keeping the client advance.
- Remove `SpawnInfo::animationTime`, its `Fields()` entry, and the Transfer
  literal entry. In `OmnisPostRender::Spawn`, set the column to zero under
  `BT_CLIENT`, as Fighters does, and drop "and animation time" from the
  `bTransfer` comment.
- Remove the column from the server `edit_frame` column list
  (`ServerFrameEdit.cpp:22`; the server build's `static_assert` enforces it)
  and `animationTime` from the `SpawnOmni` row of
  `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`.
- Update `Omnis/AGENTS.md` `## State Boundaries`, whose first bullet names
  client animation time among the CRC-excluded state.

Apply `/add-collection-member`'s layout checklist in reverse for the removed
shared column.

Behavior change: an Omni's animation clock becomes purely client-local. It
restarts at zero on every spawn and transfer arrival, which the server's
never-advanced transfer value already causes today. After a full state it
takes the same handling as Fighters' and Battleships' client-only animation
time, instead of the server's value.

Risk tier: Tier 3. Triggers:

- Wire and save layout: removing a `SharedMembers()` column changes the server
  snapshot, save, and replay layout, so bump `OmnisInterpolate::kiVersion`,
  which `Frame::kiVersion` composes.
- Status-change payload: removing a `SpawnInfo` `Fields()` entry changes the
  transfer and spawn payload, so bump `FrameInput::kiVersion` and
  `engine::kiProtocolVersion` under
  `Projects/BrokenEngineSandbox/Source/Network/AGENTS.md`
  `## Status-Change Wire Format`.
- No backward compatibility: older saves, replays, and peers are rejected by
  those versions.

Invariant: the shared CRC is unchanged, since `SharedCrcMembers()` never held
the column.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/Omnis.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/Omnis.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/OmnisNavigation.cpp`
- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerFrameEdit.cpp`
- `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/AGENTS.md`
- `Projects/BrokenEngineSandbox/Source/Frame/FrameInput.h` (version)
- `Engine/Source/Network/NetworkProtocol.h` (version)

## In scope

- `OmnisInterpolate::pAnimationTimes`: declaration, guard, and its
  `SharedMembers()`/`ClientMembers()` entries; the `SharedCrcMembers()`
  comment.
- `OmnisInterpolate::Update`: the column's load and store.
- `OmnisPostRender::SpawnInfo::animationTime`, its `Fields()` entry, and the
  `bTransfer` comment; `OmnisPostRender::Spawn`'s column initialization; the
  Omnis Transfer `SpawnInfo` literal in `OmnisNavigation.cpp`, the only
  initializer that names `animationTime`.
- The Omnis `edit_frame` column list in `ServerFrameEdit.cpp` and the
  `SpawnOmni` row of `commands-server.md`.
- `Omnis/AGENTS.md` `## State Boundaries`, first bullet.
- `OmnisInterpolate::kiVersion`, `FrameInput::kiVersion`, and
  `engine::kiProtocolVersion`.

## Out of scope

- `OmnisPostRender::pDestroyedExplosionTimes`,
  `OmnisInterpolate::pfRotationAccelerationXs` and `pfRotationAccelerationYs`,
  and any other client-only column or `BT_CLIENT` guard change.
- Fighters and Battleships `pAnimationTimes`, already client-only.
- A separate client Frame object, per-collection client headers, client-only
  source files, or extracting inline effect blocks into functions.
- Any edit to `Engine/Source/Frame/AGENTS.md`; its rule already describes the
  intended state.

## Acceptance criteria

- `OmnisInterpolate::SharedMembers()` and `OmnisPostRender::SpawnInfo::Fields()`
  no longer name the animation time, and the column is declared and used only
  under `BT_CLIENT`.
- The client and server builds succeed, and the collection-layout auditor run
  `/add-collection-member` documents is clean.
- A live client run shows Omnis animating. The supplied evidence records no
  harness setup; the executing session picks one through `/agent-harness`.

## Notes

- The server never ran the animation, so the server simulation and its CRC do
  not change; the removed bytes only ever held a constant from the server.
