<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-05T13:04:44.757Z","dependsOn":[]} -->
# Convert Fighters to the Ramped Turn

## Context

Collection paths below are relative to `Projects/BrokenEngineSandbox/Source/Frame/Collections/`.
`Frame.cpp` and `Frame/AGENTS.md` are the files directly under
`Projects/BrokenEngineSandbox/Source/Frame/`, not `Engine/Source/Frame/`. Line numbers are from
`5c2e6a8d`.

The user chose this session to convert Fighters to the Battleship ramped turn, accepting Fighter
retuning and the simulation version change (the authoring agent's record of that choice), and
reported this symptom: "I often see fighters twist rapidly suddenly when turning sharply". Fighter
steering changes here and is not bit-identical. `Ships/ShipBehaviours.h` declares
`RampHeadingTurnRate`, `SteerHeading`, and `CaptureHeading`, tuned by the four turn fields of
`HeadingMovementConstants`.

Originating gap: a refactor log row in `Documents/Investigations/Engine/RefactorCandidates.md`,
since removed but readable in `5c2e6a8d`, says Fighters keep the signed-max cone branch plus
`SteerHeadingTowards` pair that gave Battleships two alternating turn rates. Commit `488c27e6`
removed that pair from Battleships and added that row.

Observed symptom and evidence: the user's sentence above is the only recorded observation. The
supplied evidence records no harness setup, tick log, or screenshot for it, and does not say whether
the twist is heading, roll, or another visual, or which Fighters show it.

What the code shows, by reading and arithmetic at the 32 Hz tick; no run has measured these:

- Fleet Fighter outside the 45-degree cone: `Fighters/Fighters.cpp:634-639` writes the signed
  `kfFighterMaxTurnRate`, 8 rad/s, in one tick, with no ramp.
- Fleet Fighter inside the cone: `Fighters.cpp:642` calls `SteerHeadingTowards`
  (`Ships/ShipBehaviours.cpp:580-590`), a bang-bang lerp toward +/-64 rad/s with smoothing 0.064 and
  decay 6. Its steady rate is about 0.61 rad/s, and a rate of 8 decays about 17% per tick.
- So a fleet Fighter has two turn rates about 13 times apart. Battleships had the same pair before
  `488c27e6`, at about 0.08 and 1.0 rad/s.
- Roll reads the stored rate directly: `Fighters/FightersRender.cpp:19` and `:156` compute
  `-kfRoll * pfDeltaRotations` with `kfRoll` 0.2, and the stored rate holds still between ticks. At
  8 rad/s the roll is 1.6 rad, about 92 degrees. Leaving the cone steps the roll about 85 degrees in
  one tick, and a reversal of the sign steps it about 183 degrees.
- Random-spawn Fighters run only `SteerHeadingTowards` (`Fighters.cpp:659`), so they never reach that
  step while alive.

The evidence does not confirm the cause. The roll step is the leading hypothesis, but the fleet
signed-max write is shown only to be capable of it, nothing ties it to what the user saw, and the
candidates below are also open.

## Design

The conversion is the user's choice. The rest is the author's recommendation, each item with its
rationale.

### 1. Confirm the root cause

On a build of this Plan's parent, run `/external-diagnose-bug` for the recorded symptom, using the
setup in `## Acceptance criteria`, before any edit. Candidates to separate:

- Inside the bounding region: the single-tick roll step from the signed-max write; the yaw rate of
  8 rad/s itself; the rate carried at +/-8 rad/s through the one-second arrival grace, where
  `Fighters.cpp:591` and `:690` hold it unchanged; a terrain bounce's +/-8 rad/s on a dying row
  (`Ships/ShipBehaviours.cpp:663`).
- Outside it: the climb tilt, which `Fighters.cpp:292` sets from the height step of one tick and
  which can coincide with a turn, since attack and veer-off tactics write the target plane; the
  skeletal animation; the matrix order at `FightersRender.cpp:161`; and the yaw matrix in
  `QuaternionFromDirection` (`Common/Math/MathUtils.cpp:19-34`).

The fix region is the bound in `## In scope`. If the confirmed cause lies outside it, stop and
surface the finding for re-planning instead of absorbing it. If the symptom cannot be reproduced,
surface that with the captured ticks instead of claiming a cause.

### 2. Convert

Mirror the Battleship code from `488c27e6`, as it stands with the shared ramped turn in
`Ships/ShipBehaviours`:

- `Fighters.h`: the four turn fields of `HeadingMovementConstants` set in `kFighterMovement`.
  Remove `kfFighterMaxTurnRate`, `kfFighterSteeringSmoothing`, `kfFighterSteeringDecay`, and
  `kfFighterChaseTurnRate`.
- Fleet branch (`Fighters.cpp:634-643`): `bool bAligning = !IsWithinHeadingCone(...)`, then
  `fDeltaRotation = SteerHeading(vecDirection, vecAiDirection, kFighterMovement)`. `bAligning` stays
  the acceleration gate and becomes local to this branch.
- Random-spawn branch (`Fighters.cpp:659`): mirror `Battleships.cpp:558-565` without its flee
  negation. Zero `vecAiDirection` and `fDeltaRotation`; with a destination, store the normalized
  horizontal direction to it and call `SteerHeading`. Random-spawn rows then store a wanted heading,
  which is a new CRC-visible value in the existing `pVecAiDirections` column, needed by the capture.
- Remove `fNavigationDeltaRotation` and the restore at `Fighters.cpp:669` and `:677-680`. It never
  changes anything today: `bAligning` needs a live row, while the bounce at `:671` needs a dying row
  because Fighters allow plane 1.
- `Fighters.cpp:690`: `RampHeadingTurnRate(rPreviousInterpolate.pfDeltaRotations[i], ...)`.
- `Fighters/FightersNavigation.cpp:55-57`: mirror `BattleshipsNavigation.cpp:55-58`.
- `FightersInterpolate::Update`: after the rotation at `Fighters.cpp:299`, the same
  `fDeltaRotation == 0.0f` test and `CaptureHeading` call `BattleshipsInterpolate::Update` makes.
- Remove `SteerHeadingTowards` from `Ships/ShipBehaviours.h/.cpp`; Fighters are its last caller.

The shared `kfTerrainBounceRotation` stays 8.0; the ramp limits what a bounce can do.

### 3. Retune

The per-type parameters are the four turn fields of `HeadingMovementConstants`. Final Fighter values
are chosen by live check against the behavior criteria in `## Acceptance criteria`. The recommended
starting values in `kFighterMovement` are:

| Field | Start | Rationale |
| --- | --- | --- |
| `fMaxTurnRate` | `3.0f` | Roll at this rate is 34 degrees, not 92; radius at 45 m/s is 15 m, inside the 20 m veer-off start; above the escort slot's 0.42 rad/s. |
| `fTurnAcceleration` | `12.0f` | Full rate in 0.25 s; at most 4.3 degrees of roll change per tick. |
| `fCaptureAngle` | `XM_PI / 60.0f` | 3 degrees. Battleships' 1 degree overshot by 1.7 degrees at this acceleration in the model below. |
| `fCaptureRate` | `4.0f` | The Battleship value; it did not affect the model's settling. |

The source of those figures is a small numeric model of the tick order against a fixed wanted
heading, run while authoring this Plan, not a run of the game: from every start angle these values
settled within 41 ticks with no overshoot. Moving targets are what the live check is for.

`kfRoll` stays 0.2. Roll now moves with a ramped rate, so its value needs no change unless the
confirmed cause shows otherwise, which would be outside the bound.

### 4. Version

Bump the `Frame::kiVersion` base (`Frame.cpp:105`, 164 at `5c2e6a8d`) by one from the value found at
landing. The change shifts CRC'd state (`pVecVelocities` and `pVecAiDirections` are in
`FightersPostRender::SharedCrcMembers`) with no contributing collection bump, which is the case
`Frame.cpp:102` and `Frame/AGENTS.md` assign to the base; `488c27e6` did the same for Battleships. No
collection `kiVersion` moves, because no column or layout changes. `engine::kiProtocolVersion`,
`FrameInput::kiVersion`, and `ReplayMeta::kiVersion` stay, because no written byte layout changes;
`kiGameVersion` is informational. The bump rejects old saves (`Engine/Source/File/GridSave.cpp:79`),
old replays, and a mismatched client at the Hello (`Engine/Source/Network/Server/Server.cpp:630`).

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/Fighters.h`, `.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/FightersNavigation.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipBehaviours.h`, `.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp`
- `AGENTS.md` under `Collections/Ships`, `Collections/Fighters`, and `Collections/Battleships`

## In scope

- The root-cause investigation in `## Design` step 1.
- The fix region, which bounds any fix: the steering in `FightersPostRender::Update` (the two
  branches, the restore, and the final turn-rate write), the rate write in
  `FightersPostRender::AvoidTerrain`, the capture in `FightersInterpolate::Update`, and the four
  turn field values in `kFighterMovement`.
- Removal of the four Fighter constants and of `SteerHeadingTowards`.
- The `Frame::kiVersion` base.
- `/update-claude-docs` sync: move `Battleships/AGENTS.md` `## Turning Invariants` to `Ships/AGENTS.md`
  as the shared rule, and update the Fighter and Battleship sentences in `Ships/AGENTS.md`
  `## Fleet Navigation Invariants`, the turn-only decay in `Fighters/AGENTS.md`
  `## Behavior Invariants`, and the difference list opening `Battleships/AGENTS.md`.

## Out of scope

- `Fighters/FightersRender.cpp`, `kfRoll`, the climb tilt, the skeletal animation, and
  `QuaternionFromDirection`.
- Any Battleship turn value or Battleship steering line, and Omni steering.
- `IsWithinHeadingCone`, the cone's acceleration gate, `ComputeTerrainAvoidance`, and
  `ApplyTerrainBounce`.
- Fleet tactics, veer-off distances, escort, target picking, and navigation.
- New columns, collection versions, and wire or save formats.

## Risk and invariants

- Tier 3. Trigger: determinism/CRC, because this changes CRC-visible Fighter steering and
  `Frame::kiVersion`, which also breaks save and replay compatibility.
- Battleship results: `RampHeadingTurnRate`, `SteerHeading`, `CaptureHeading`, and the turn fields
  of `kBattleshipMovement` are untouched, so a Battleship's own turn computation is bit-identical for
  the same inputs. Whole-frame CRCs change whenever Fighters exist, and a Battleship's trajectory can
  still diverge through Fighter-dependent inputs such as pushers and target choice. The bit-identity
  proof is the replay that verified the Battleship turn's move into `Ships/ShipBehaviours`; the bump
  makes it unrepeatable here, so the diff settles this.
- No random draw is added, no read moves to another frame, and client and server run the same code.
- Cross-phase: `FightersInterpolate::Update` newly reads `rPreviousPostRender.pVecAiDirections`, as
  `BattleshipsInterpolate::Update` does. A transfer arrival stores a zero heading, which skips the
  capture until the next steering tick.
- `Fighters/AGENTS.md` `## Behavior Invariants` keeps the rule that a turn rate crosses cells with
  the ship. The rule is outside the `## Turning Invariants` section that moves to `Ships/AGENTS.md`,
  so that move leaves it in place.

## Acceptance criteria

- Root cause: the session records the confirmed cause, with tick-level evidence from the parent
  build, before the conversion. A cause outside the fix region is surfaced for re-planning.
- Recorded symptom: at the setup below on the changed build, the symptom the user reported, "twist
  rapidly suddenly when turning sharply", no longer reproduces in screenshots stepped through the
  sharp turn, and the user confirms it if the harness images cannot settle it.
- Step limit: across that turn, the tick-to-tick change of `pfDeltaRotations` on the Fighter row
  stays within `fTurnAcceleration / 32` and its magnitude within `fMaxTurnRate`.
- Behavior, on the final values: a fleet Fighter reaches and holds its escort slot; a weapons-free
  fleet Fighter completes attack, veer-off, and renewed attack; a random-spawn Fighter attacks,
  and returns to an island with no enemy; and a Fighter's beam still fires, shown by a
  `pNextBeamFireTimes` reset.
- No `SteerHeadingTowards`, `kfFighterMaxTurnRate`, `kfFighterSteeringSmoothing`,
  `kfFighterSteeringDecay`, or `kfFighterChaseTurnRate` remains under `Projects/`, and Fighters and
  Battleships reach the same three shared functions.
- The `Frame::kiVersion` base rose by exactly one, and no collection version changed.
- `pwsh -NoProfile -File .agents/scripts/Measure-Tokens.ps1 -Path <Fighters.cpp path>` reports at
  most 10,000 tokens.
- Client and server build through `/compile`. A same-build replay completes a loop by the
  `.agents/skills/agent-harness/references/replay.md` `### Replay determinism acceptance` steps, and
  client and server show no desync. No unit tests.
- Harness setup: the supplied evidence records no harness setup for this symptom. The closest
  existing way to observe a sharp Fighter turn is, with a client connected: spawn a fleet Fighter
  through `inject_payload` `SpawnFighter`, send `pause {"paused":true}` once its arrival grace has
  run out, `edit_frame` its `pVecDirections` to the reverse of `read_frame` `pVecAiDirections`, step
  `timescale` to 1/32, advance one tick at a time by the `pause` recipe in
  `.agents/skills/agent-harness/references/command-reference.md`, and on each tick take `read_frame`
  `pfDeltaRotations` and `pVecDirections` and a client `screenshot`. The natural equivalent is a
  weapons-free veer-off start, reached with `inject_payload` enemy spawns near a flagship. Record the
  command sequence, and use the same one before and after the change.

## Coordination

Reciprocal coordination with
`Documents/Plans/Game/RouteShipFleeReturnAndVeerOffGoals.md`: that Plan edits the
`ComputeFleeOrReturnDestination` call directly above `Fighters.cpp:659` and also bumps the
`Frame::kiVersion` base, so whichever lands second refreshes that block and takes its own bump from
the value it finds. It holds turn rates out of its scope, so this Plan owns them. Each Plan keeps its
own acceptance.

## Notes

- `Documents/Plans/Game/SeparateFighterVeerOffFromFleetTactic.md` edits only the call at
  `Fighters.cpp:621`, disjoint from this Plan's regions, and its replay compares against its own
  parent commit, so neither order constrains the other.
- `Fighters.cpp` measures 9,938 `bt-token-v1`. A scratch measurement of this change gave 9,885
  (-53) with no new comments and 9,968 (+30) with the Battleship comments mirrored, both under the
  10,000 limit in `## Acceptance criteria`.
- `Ships/ShipBehaviours.cpp` loses about 199 tokens with `SteerHeadingTowards`.
