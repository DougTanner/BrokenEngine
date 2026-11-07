<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-06T19:16:33.107Z","dependsOn":[]} -->
# Reject invalid ship headings at frame read

## Context

Root `AGENTS.md` (trust policy) says the server fully validates every grid save under the bad-value
rule in `.agents/references/cpp-conventions.md`. Ship headings are read from saves, replays, and
`edit_frame` commits with no check. An adversarial review of the change that added
`HasShipLineOfSight` left this as a pre-existing, out-of-scope residual: every ship type already had
the gap, and that change only made its effect wider.

Line numbers below were taken from the tree when this Plan was written; locate each site by the
named symbol if lines have moved. Paths without a directory are under
`Projects/BrokenEngineSandbox/Source/Frame/Collections/`.

Root cause, read from source:

- `pVecDirections` is a shared Interpolate column of all three ship types (`Fighters/Fighters.h:106`,
  `Omnis/Omnis.h:127`, `Battleships/Battleships.h:105`), so every frame read seats it from the stream.
- `FightersInterpolate::PostRead` (`Fighters.h:77-96`), `OmnisInterpolate::PostRead`
  (`Omnis.h:103-122`), and `BattleshipsInterpolate::PostRead` (`Battleships.h:76-95`) check the
  destroyed time, the height, and the stored plane, and nothing about the heading.
- `common::ValidateVector` (`Common/Math/MathUtils.h:28`) is an `ASSERT` for spawn and transfer
  boundaries. `Projects/BrokenEngineSandbox/Source/Save` and `Engine/Source/File` name neither it
  nor `pVecDirections`.

Effect of a bad heading:

- A stored zero stays zero: each type's Interpolate Update rotates the stored heading and the
  Fighter and Battleship Updates then normalize it (`Battleships.cpp:219`, `Fighters.cpp:299`,
  `Omnis.cpp:367`), which leaves zero unchanged. The ship never turns.
- A Battleship row that is a navigation obstacle also blocks sight. `HasShipLineOfSight`
  (`Ships/ShipBehaviours.cpp:931-980`) poses the model box from the heading's x and y as cosine and
  sine, so a zero heading collapses all four corners onto the ship's position. `PointInPolygon` then
  reports neither end of the segment inside, every edge normal is zero, and
  `common::ClipSegmentToPrism` (`Common/Math/ConvexHull.h:232-272`) takes its `fRate == 0.0f` branch
  on every half-space and returns true. That row blocks every sight test on the planes it occupies
  in its cell. A NaN heading reaches the same result: both clip comparisons are false, `std::max` and
  `std::min` keep their first argument, and the interval stays open.

Values the simulation produces:

- A ship heading has two writers per type, spawn and Interpolate Update (`Fighters.cpp:315,487`,
  `Battleships.cpp:249,409`, `Omnis.cpp:272,376`). Spawn takes a fixed `(1,0,0,0)`
  (`Network/Server/ServerClientManager.cpp:227`), a flattened normalized wave heading
  (`Frame/Frame.cpp:421`), a launch bay direction yawed by the Battleship's heading
  (`ServerClientManager.cpp:277-280`), or the carried heading on transfer.
- Every one of those has Z 0 and W 0 and rotation about Z keeps both exactly, so the
  `Ships/AGENTS.md` rule that headings stay horizontal holds bit-exactly.
- Fighter and Battleship headings are normalized on every Update. Omni headings are not:
  `common::RotateTowardsPercent` (`Common/Math/MathUtils.cpp:53-58`) only rotates, so an Omni's length
  carries accumulated rounding for as long as the row lives. The drift was not measured.
- Only a corrupt or tampered save reaches a bad heading in ordinary running. `inject_payload`
  `vecDirection` and `edit_frame` `pVecDirections` can also write one, under the agent-channel
  exemption.

`edit_frame` commits through the save reader's checks, `PostRead` hooks included
(`Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md` `edit_frame` section), and
its column lists name `pVecDirections` for all three ship types
(`Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerFrameEdit.cpp:21-23`).

## Design

1. The author recommends one predicate in `Ships/ShipBehaviours.h`, beside
   `IsNavigationDelayInRange`, which all three ship headers already include:
   `IsShipHeadingValid(FXMVECTOR vecDirection)`, true when Z and W are exactly 0 and the horizontal
   length squared lies in `[0.25, 4]`. Written as one range test, it also fails NaN and both
   infinities in any lane. Rationale for a wide band over a unit test: an Omni's length drifts by
   rounding (Context), so a tight tolerance could reject a frame the simulation wrote, while any
   band that excludes zero removes the failure this Plan records. Rationale for the exact Z and W
   tests: the simulation keeps both bit-exact, so they cost no tolerance decision.
2. Call it in each ship Interpolate `PostRead` row loop beside the existing checks, throwing
   `std::ios_base::failure("<Type>Interpolate pVecDirections")`, and extend each `PostRead` comment
   with one clause.
3. In `ParseSpawnEntry` (`Agent/Commands/ServerSimulationHarnessRigs.cpp:250-285`), refuse a ship
   `vecDirection` that fails the predicate, with the same `FRAME_COLUMN(...)::kName` message form the
   `navigationDelay` refusal uses, inside that refusal's existing ship-only `requires` branch. The
   function's comment already names this reason: a later frame read would reject the value.
4. Update the docs the change makes incorrect: the `PostRead` sentence of the `Ships/AGENTS.md`
   `## Ship Invariants` read-admission bullet, and the `inject_payload` refusal list in
   `commands-server.md`.
5. No `kiVersion` bump: the stored bytes and their meaning are unchanged, and no value the simulation
   writes is newly rejected.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipBehaviours.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/Fighters.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/Omnis.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/Battleships.h`
- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationHarnessRigs.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/AGENTS.md`
- `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`

## In scope

- The new `IsShipHeadingValid` predicate.
- The row loops and comments of `FightersInterpolate::PostRead`, `OmnisInterpolate::PostRead`, and
  `BattleshipsInterpolate::PostRead`.
- The ship-only branch and comment of `ParseSpawnEntry`.
- The `Ships/AGENTS.md` read-admission bullet and the `inject_payload` refusal list.

## Out of scope

- Normalizing the Omni heading in Update or inside `RotateTowardsPercent`: it changes CRC-fed values.
- Any repair of a bad heading at read, in `HasShipLineOfSight`, or in `ClipSegmentToPrism`.
- Other vector columns: `pVecAiDirections`, velocities, and Missile and Blaster directions.
- `common::ValidateVector`, the engine read path, and `Spawn(SpawnInfo)`.
- Replay `FrameInput` admission (`AdmitFrameInputStatusChange`) and transfer status-change payloads.
- A DataPacker check on a launch bay's horizontal part: export requires only a nonzero direction,
  and the authored bays are horizontal once aligned.
- Any `kiVersion`.

## Risk and invariants

- Tier trigger: Tier 2 — adding checks at existing read boundaries (`PostRead` for save, replay, and
  `edit_frame`; the `inject_payload` admission) with the format and the trust unchanged. Accepted
  values behave exactly as before, so CRC, determinism, wire, and save layout are unchanged.
- Rejection follows the bad-value rule: throw through the existing `PostRead` failure path, never
  clamp, substitute, or normalize.
- `PostRead` also runs on client network full-state reads
  (`Engine/Source/Frame/Collections/AGENTS.md`), so a heading the server holds must pass it. Design
  steps 2 and 3 close both harness routes that could seat one that does not.
- No allocation, threading, shader, or wire exposure.

## Acceptance criteria

1. BrokenEngineSandbox client and server Debug|x64 build with no new warnings.
2. Live, through `/agent-harness` on a paused server: `edit_frame` writing `pVecDirections` `[0,0,0]`
   on a Fighter, an Omni, and a Battleship row each returns `ok:false` with the frame unchanged, and
   writing `[0,1,0]` to each succeeds. The supplied evidence records no reproducing setup; the Agent
   Harness `edit_frame` recipe (pause, `create_cell` or `inject_payload` spawn, then `edit_frame`) is
   the expected route. NaN and a nonzero W are not writable through JSON and are settled by reading
   the predicate.
3. `inject_payload` refuses a `vecDirection` of `[0,0,0]` on `SpawnFighter`, `SpawnOmni`, and
   `SpawnBattleship`, and accepts the `[1,0]` default.
4. A grid save written by a running server holding all three ship types loads.

## Notes

- Ordinary debt follow-up. No Plan covered this root cause when this Plan was written.
- `Documents/Plans/Game/FighterBattleshipFireTimeRangeChecks.md` adds sibling read checks in the
  PostRender `PostRead` hooks and `BuildInjectedEntry`; the two change different regions and may land
  in either order.
