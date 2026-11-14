<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-13T23:24:38.399Z","dependsOn":[]} -->
# Derive the death explosion cadence from pDestroyedTimes

## Context

`FightersPostRender::pDestroyedExplosionTimes` and
`OmnisPostRender::pDestroyedExplosionTimes` are client-only repeat timers for
the staggered death explosions of a dying ship. Each is declared under
`BT_CLIENT` and listed only in its struct's `ClientMembers()`
(`Fighters.h:269`, `:280`; `Omnis.h:317`, `:328`), so server `Members()` equal
`SharedMembers()` without it: it is in no CRC, wire, or save byte.

Every site that touches it exists only to write or carry it:

- `BeginExplosion` sets it to the interval when the row starts dying
  (`FightersCombat.cpp:46-48`, `OmnisCombat.cpp:103-105`).
- The `SpawnInfo` overload of `Spawn` zeroes it (`Fighters.cpp:525-527`,
  `Omnis.cpp:304-306`).
- PostRender `Update` loads it minus the tick delta and stores it
  (`Fighters.cpp:586-588`, `:705-707`; `Omnis.cpp:596-598`, `:774-776`, which
  also clamps at zero).
- The readers test it for `<= 0` and reset it to the interval: the Fighters
  PostRender `Spawn` phase hook (`Fighters.cpp:429-431`) and
  `OmnisPostRender::SpawnDeathExplosions` (`OmnisCombat.cpp:288`, `:293`).

The value is derivable from the shared simulation column `pDestroyedTimes` on
the Interpolate half, which `BeginExplosion` sets to the destroy duration
(`kFighterDestroyTime`, `kDestroyTime`) and Interpolate `Update` counts down to
zero (`Fighters.cpp:305-309`; `Omnis.cpp:374-377`, only while the previous
PostRender row is dying). The stored column is therefore redundant client
state.

Verified facts the derivation rests on:

- Phase order within a tick is Interpolate, PostRender, Collision, Transfer,
  Destroy/Spawn (`Documents/Architecture/FrameUpdatePipeline.md`). The tick
  runs Interpolate `Update` with `kfDeltaTime` (`FrameBase.cpp:332`), which
  stores it as `FrameInterpolateBase::fDeltaTime` (`FrameBase.cpp:191`).
- Every `BeginExplosion` call (PostCollision, AreaDamage, and the
  `kDestroyShip` status change in Update, `ShipStatusChanges.cpp:86`, `:90`)
  runs before the Spawn phase, so on the death tick the hook reads the full
  destroy duration; on every later tick it reads that tick's counted-down
  value.
- At the default `kiTickRate` the tick delta exceeds both
  `kFighterDestroyExplosionInterval` and `kDestroyExplosionInterval`, so the
  current timer reaches `<= 0` on every tick after the death tick: one
  explosion per dying row per tick, none on the death tick from the hook
  (Fighters' `BeginExplosion` emits its own at `FightersCombat.cpp:59-60`).
- `pDestroyedTimes` is in `SharedMembers()` of both Interpolate structs
  (`Fighters.h:128`, `Omnis.h:155`), so it arrives with a full state from the
  server.
- Death explosions are `ExplosionsPostRender::Spawn` frame rows, and the timer
  column ticks and rolls back with its cell, so a `FrameFlags::kRecalculated`
  re-simulated tick replays them from that tick's state today, and
  `ExplosionsSpawn.cpp` already gates its effect outside the Frame on that
  flag.

Originating gap: a user-requested investigation of client-only state in the
game Frame code; the user approved this change as a follow-up Plan.

## Design

Recommended rule, the same in both readers (`D` is the collection's destroy
duration constant, `I` its explosion interval constant, `t` the current
frame's `rCurrentInterpolate.pDestroyedTimes[i]`, and `dt`
`rFrame.interpolate.fDeltaTime`):

- `elapsed = D - t`; `previous = max(elapsed - dt, 0)`.
- Emit when `floor(elapsed / I) > floor(previous / I)`.
- Keep every other gate and computation of each reader unchanged: Fighters
  tests `kDying` only, Omnis tests `kDying` and `t > 0`, and both keep their
  `fPercent = t / D`.

How it matches the current behavior:

- Death tick: `t == D`, so `elapsed` and `previous` are both 0 and nothing is
  emitted, as today, where `BeginExplosion` set the timer to `I > 0`.
- Later ticks at the default tick rate: `elapsed - previous == dt > I`, so the
  floors always differ and every tick emits, as today.
- Final tick: `t` is clamped to 0, `elapsed == D`, and the floors still differ
  by the same argument; the Fighters reader emits on that tick as it does
  today, and the Omnis `t > 0` gate excludes it as it does today.
- Full state: the rule reads only the shared `pDestroyedTimes` and the frame
  delta, so it needs no client-side restore.
- `kRecalculated`: the rule reads only state that re-simulation recomputes, so
  a re-simulated tick emits what the derived rule gives for that tick; no
  `kRecalculated` gate is added or removed.

Accepted behavior difference: at a tick rate whose delta is below an interval
(Fighters at 64 or 128 ticks per second), the current timer resets to `I` and
drops the overshoot, while the derived rule emits on interval crossings, so the
explosion cadence may shift slightly. It is a client-only visual with no CRC,
wire, or save byte effect.

Then delete the column: its declaration and `ClientMembers()` entry in both
PostRender structs, and every `#if defined(BT_CLIENT)` block or statement that
only writes or carries it (the sites `## Context` lists). Keep the interval
constants, which the rule reads.

Apply the `/add-collection-member` layout checklist in reverse for a removed
column. Its version step applies: bump `FightersPostRender::kiVersion` and
`OmnisPostRender::kiVersion`, as earlier client-only Fighters column changes
did. Keep the Fighters and Omnis changes parallel.

Risk tier: Tier 3. Trigger: serialization or data layout — the removed column
changes the client build-local `Members()` layout, and the version bumps
change `Frame::kiVersion`, so saves and replays written before the change are
rejected (no backward compatibility). Invariants: no CRC, wire, or server save
byte changes, and no simulation byte reads the new rule's output.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/Fighters.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/Fighters.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/FightersCombat.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/Omnis.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/Omnis.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/OmnisCombat.cpp`

## In scope

- `FightersPostRender::pDestroyedExplosionTimes` and
  `OmnisPostRender::pDestroyedExplosionTimes`: declarations and
  `ClientMembers()` entries.
- `FightersPostRender::BeginExplosion` and `OmnisPostRender::BeginExplosion`:
  the timer assignment and its guard.
- `FightersPostRender::Spawn` and `OmnisPostRender::Spawn` (`SpawnInfo`
  overloads): the timer initialization and its guard.
- `FightersPostRender::Update` and `OmnisPostRender::Update`: the timer load and
  store and their guards.
- The Fighters PostRender `Spawn` phase hook and
  `OmnisPostRender::SpawnDeathExplosions`: the emit test and timer reset,
  replaced by the derived rule.
- `FightersPostRender::kiVersion` and `OmnisPostRender::kiVersion`.

## Out of scope

- `OmnisInterpolate::pfRotationAccelerationXs` and
  `pfRotationAccelerationYs`, and any other reduction of `BT_CLIENT` guards or
  client-only columns.
- A separate client Frame object, per-collection client headers, client-only
  source files, or extracting inline effect blocks into functions.
- Battleships death effects, `SpawnFighterExplosion`, and the explosion
  parameters each reader passes.
- `Documents/Features/` text that names death-explosion intervals.

## Acceptance criteria

- No source file names `pDestroyedExplosionTimes`.
- The client and server builds succeed, and the collection-layout auditor run
  `/add-collection-member` documents is clean.
- A live client run shows dying Fighters and Omnis still emitting their
  staggered death explosions until removal. The supplied evidence records no
  harness setup that reproduced a death; the executing session picks one
  through `/agent-harness`.

## Notes

- The new rule is client-only and stays inside the readers' existing
  `BT_CLIENT` guards.
