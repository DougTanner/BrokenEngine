<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-05T13:04:41.224Z","dependsOn":[]} -->
# Move the Battleship Ramped Turn Into ShipBehaviours

## Context

Paths below are relative to `Projects/BrokenEngineSandbox/Source/Frame/Collections/`, and line
numbers are from `5c2e6a8d`.

Originating gap: a refactor log row in `Documents/Investigations/Engine/RefactorCandidates.md`,
since removed but readable in `5c2e6a8d`, says `RampBattleshipTurnRate` and
`SteerBattleshipHeading` are Battleship-local while Fighters keep the signed-max cone branch plus
`SteerHeadingTowards` pair that gave Battleships two alternating turn rates, and that one shared
ramped soft-clamp turn in `ShipBehaviours` would keep the two collections parallel.

The user chose this session to convert Fighters to the Battleship ramped turn, accepting Fighter
retuning and the simulation version change (the authoring agent's record of that choice). This Plan
is the first of two stages; `Documents/Plans/Game/ConvertFightersToRampedTurn.md` is the second and
carries the Fighter behavior change.

They are two Plans because each stage ends at its own buildable cut with its own acceptance, and
the verification differs. The conversion bumps `Frame::kiVersion`, which makes the changed build
refuse every earlier replay (`Engine/Source/File/DifferenceStream.h:226`), so replaying a parent
recording can prove the Battleship results unchanged only before that bump. This stage has no
Fighter edit; the Fighter steering change and its retuning are entirely in the second Plan.

The turn is Battleship-local today, added by commit `488c27e6`:

- `Battleships/Battleships.h:34-38`: `kfBattleshipMaxTurnRate`, `kfBattleshipTurnAcceleration`, and
  the `RampBattleshipTurnRate` declaration.
- `Battleships/Battleships.cpp:35-36`: `kfBattleshipHeadingCaptureAngle` and
  `kfBattleshipHeadingCaptureRate`.
- `Battleships/Battleships.cpp:465-468`: `RampBattleshipTurnRate`.
- `Battleships/Battleships.cpp:470-482`: `SteerBattleshipHeading`, the soft clamp.
- `Battleships/Battleships.cpp:220-233`: the heading capture in `BattleshipsInterpolate::Update`.
- Callers: `Battleships.cpp:552`, `:594`, and `:612`, and `Battleships/BattleshipsNavigation.cpp:58`.

## Design

The author recommends the following, each mirroring an existing pattern.

- One constants struct beside `HeadingMovementConstants` in `Ships/ShipBehaviours.h`, the existing
  shape for per-type ship tuning, with the rationale that a struct passed by reference keeps the
  three functions below from each growing four float parameters:

  ```cpp
  // Tuning for the ramped heading turn, instantiated in Battleships.h.
  struct HeadingTurnConstants
  {
  	float fMaxTurnRate = 0.0f;
  	float fTurnAcceleration = 0.0f;
  	// Within this angle of its wanted heading a ship stops turning and Interpolate Update rotates it the rest of the way.
  	float fCaptureAngle = 0.0f;
  	float fCaptureRate = 0.0f;
  };
  ```

- `kBattleshipTurn` in `Battleships.h` beside `kBattleshipMovement`, holding the four existing values
  unchanged: `0.25f`, `0.25f`, `XM_PI / 180.0f`, and `4.0f`. The four old constants go.
- Three functions in `ShipBehaviours.h/.cpp`, placed with `IsWithinHeadingCone` and
  `SteerHeadingTowards`, their bodies moved with only the constants replaced by `rConstants` fields:
  - `RampHeadingTurnRate(fPreviousDeltaRotation, fWantedDeltaRotation, deltaTime, rConstants)`.
  - `SteerHeading(vecDirection, vecWantedHeading, rConstants)`.
  - `CaptureHeading(vecDirection, vecWantedHeading, deltaTime, rConstants)`, the body of the
    `Battleships.cpp:224-232` block, returning `vecDirection` unchanged when the wanted heading is
    zero or outside the capture angle. `BattleshipsInterpolate::Update` keeps the
    `fDeltaRotation == 0.0f` test and its comment, and passes `pVecAiDirections`.
- `SteerHeadingTowards` stays in this stage because Fighters still call it.

Why Battleship results stay bit-identical: each function keeps the same IEEE operations in the same
order (`Documents/FloatingPointDeterminism.txt` sections 1 and 2: `/fp:strict` in both projects and
FMA3 disabled), so a call in another translation unit computes the same bits. The only difference
is that a constant becomes a run-time load. The product `2.0f * fTurnAcceleration * (...)` keeps its
left-to-right order, and `2.0f * 0.25f` is exact either way. Call order inside
`BattleshipsPostRender::Update`, `BattleshipsPostRender::AvoidTerrain`, and
`BattleshipsInterpolate::Update` does not change.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipBehaviours.h`, `.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/Battleships.h`, `.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/BattleshipsNavigation.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/AGENTS.md`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/AGENTS.md`

## In scope

- New `HeadingTurnConstants`, `RampHeadingTurnRate`, `SteerHeading`, and `CaptureHeading`.
- New `kBattleshipTurn`; removal of the four Battleship turn constants, `RampBattleshipTurnRate`
  with its declaration and comment, and `SteerBattleshipHeading`.
- The capture block in `BattleshipsInterpolate::Update`, and the callers at `Battleships.cpp:552`,
  `:594`, and `:612` and `BattleshipsNavigation.cpp:58`.
- `/update-claude-docs` sync of the names in `Battleships/AGENTS.md` `## Turning Invariants`, and a
  one-line mention of the shared turn in the `Ships/AGENTS.md` opening.

## Out of scope

- Every Fighter file and `SteerHeadingTowards`.
- Any Battleship turn value, the wanted-heading rules, the cone test, and terrain bounce or
  avoidance.
- `Frame::kiVersion`, any collection version, and any wire, save, or column change.
- Moving `Battleships/AGENTS.md` `## Turning Invariants` to `Ships/AGENTS.md`, which the second
  stage does once two types share them.

## Risk and invariants

- Tier 2. Trigger: one subsystem's scoped change with a changed public signature, so it is not
  Tier 1. It is not Tier 3, because it changes nothing the CRC surface computes.
- Determinism: bit-identical by the argument in `## Design`, with no random draw and no column
  change. A CRC difference is a defect in this change, never a reason to bump a version.
- Threading and allocation: the moved functions are pure per-row math.

## Acceptance criteria

- No `kfBattleshipMaxTurnRate`, `kfBattleshipTurnAcceleration`, `kfBattleshipHeadingCaptureAngle`,
  `kfBattleshipHeadingCaptureRate`, `RampBattleshipTurnRate`, or `SteerBattleshipHeading` remains
  under `Projects/`.
- `Frame::kiVersion` and every collection version are unchanged.
- Client and server build through `/compile`.
- A replay recorded by a build of this Plan's parent commit plays on the changed build, with the
  same `--app-data-directory`, through a completed loop with no checksum mismatch, by the
  `/agent-harness` `.agents/skills/agent-harness/references/replay.md`
  `### Replay determinism acceptance` steps. `read_frame` shows nonzero `pfDeltaRotations` on a
  Battleship row during the recording, so the recording exercises the moved code.
- Harness setup: the supplied evidence records no setup for a Battleship turn and no recording
  played across builds. Before `replay_record`, send `inject_payload` with a `SpawnBattleship`
  entry of `eController` `"RandomSpawn"` and an enemy `SpawnFighter` entry on an active coordinate
  (`Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`), and record the command
  sequence. If the changed build refuses the baseline recording, surface that instead of weakening
  this criterion. No unit tests.

## Coordination

No directional prerequisites. Reciprocal coordination with
`Documents/Plans/Game/RouteShipFleeReturnAndVeerOffGoals.md`: that Plan rewrites the Battleship flee
lines directly above the `Battleships.cpp:594` call, so whichever lands second refreshes that block.
Each Plan keeps its own acceptance, and this Plan's replay compares against its own parent commit,
so it holds in either order.

## Notes

- `Ships/ShipBehaviours.cpp` measures 10,105 `bt-token-v1`, above the 10,000 threshold, and this stage
  adds about 500. The author does not recommend a `dependsOn` on
  `Documents/Plans/Game/MoveAcquireTargetToShipQueries.md`, because neither change needs the other:
  they edit disjoint regions, that Plan's acceptance still holds at about 9,900 after this stage,
  and the threshold is a `/reduce-file` trigger, not a gate. With both stages, that move, and the
  helper `RouteShipFleeReturnAndVeerOffGoals.md` adds, the file ends near 9,800.
