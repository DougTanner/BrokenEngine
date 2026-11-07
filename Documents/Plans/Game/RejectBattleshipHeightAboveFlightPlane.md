<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-07T18:25:57.195Z","dependsOn":[]} -->
# Reject a Battleship height above its flight plane at frame read

## Context

Root `AGENTS.md` (trust policy) says the server fully validates every grid save under the bad-value
rule in `.agents/references/cpp-conventions.md`. A `/next-plan` run on `ShipCombatAndFleetCleanups`
proved that a frame read admits a Battleship height no production write produces; the gap is
pre-existing (baseline `8fa794fa` behaves the same) and outside that change.

Line numbers below were taken from the tree when this Plan was written; locate each site by the
named symbol if lines have moved. Paths without a directory are under
`Projects/BrokenEngineSandbox/Source/Frame/`.

Root cause, read from source:

- `BattleshipsInterpolate::PostRead` (`Collections/Battleships/Battleships.h:75-94`) admits any
  height from the lowest to the highest flight plane's height. It cannot test the effective height,
  which depends on the row's controller, a PostRender column.
- Battleships fly only plane 0 (`kiBattleshipHighestFlightPlane`, `Battleships.h:41`). Every
  production spawn places a Battleship exactly at `EffectiveShipFlightHeight` of plane 0
  (`Collections/Battleships/Battleships.cpp:407`), a transfer carries a height produced the same
  way, PostRender Update stores velocity Z as 0, the terrain response and pusher correction keep Z,
  and Battleships take no terrain rise (`Collections/Ships/AGENTS.md` `## Ship Invariants`). So no
  production Battleship height lies above its plane-0 effective height.
- `ClimbTowardsFlightPlane` (`Collections/Ships/ShipBehaviours.cpp:135-153`) retargets a descent
  blocked on the lower plane's graph to the next plane up and stores that plane on arrival. A
  Battleship admitted above its plane-0 effective height whose descent is blocked therefore climbs
  toward plane 1 and can store plane 1, which `IsFlightPlaneAllowed` disallows for Battleships,
  and its missile launch rule (`Collections/Battleships/BattleshipsCombat.cpp:238-248`) then runs on
  a state the simulation never produces, with no assert.
- The game frame read (`operator>>`, `Frame.cpp:737-749`) already runs whole-frame checks that need
  both halves of a row, `ValidatePusherOwners` and `ValidateFlyingMissiles` (`Frame.cpp:20-100`).

Only a corrupt or tampered save, or an `edit_frame` write under the agent-channel exemption, reaches
such a height.

## Design

1. The author recommends one static whole-frame check in `Frame.cpp` beside
   `ValidateFlyingMissiles`, called from `operator>>` after it: for every Battleship row, throw
   `std::ios_base::failure("Frame Battleship height")` when the row's position Z is above
   `EffectiveShipFlightHeight(ShipType::kBattleship, pController[i], kiBattleshipHighestFlightPlane)`.
   Rationale: the controller column the bound needs is PostRender data, and the PostRender `PostRead`
   has already rejected an unknown controller before this runs, so the lookup is in range. Testing
   only "above" removes the recorded route; a height below the effective height is outside this
   Plan's evidence.
2. Update the docs the change makes incorrect: the Battleship clause of the read-admission bullet in
   `Collections/Ships/AGENTS.md` `## Ship Invariants` ("Battleships retain the lowest-to-highest
   plane range"), and the read-boundary bullet in `Frame/AGENTS.md` `## Invariants` that lists the
   whole-frame read checks.
3. No `kiVersion` bump: the stored bytes and their meaning are unchanged, and no value the
   simulation writes is newly rejected.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/AGENTS.md`
- `Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md`

## In scope

- The new static check in `Frame.cpp` and its call in `operator>>`.
- The two doc sentences named in Design step 2.

## Out of scope

- `BattleshipsInterpolate::PostRead`, `Frame::ServerRead`, and client network full-state reads.
- `ClimbTowardsFlightPlane`'s blocked-descent retarget and the missile launch rule.
- Heights of Fighters and Omnis, and Battleship heights below the effective height.
- `inject_payload` admission and replay `FrameInput` admission.
- Any `kiVersion`.

## Risk and invariants

- Tier trigger: Tier 2 — adding a check at an existing read boundary (the game frame read, which
  grid saves, replays, and `edit_frame` commits share) with the format and the trust unchanged.
  Accepted values behave exactly as before, so CRC, determinism, wire, and save layout are
  unchanged.
- Rejection follows the bad-value rule: throw through the existing read failure path, never clamp.
- Server and client both compile `Frame.cpp`; the check is pure and allocation-free. No threading,
  shader, or wire exposure.

## Acceptance criteria

1. BrokenEngineSandbox client and server Debug|x64 build with no new warnings.
2. Live, through `/agent-harness` on a paused server: `edit_frame` writing a Battleship row's
   `pVecPositions` z to the highest flight plane's height returns `ok:false` with the frame
   unchanged, and writing back its current position succeeds. The supplied evidence records no
   reproducing setup; the Agent Harness `edit_frame` recipe (pause, `create_cell` or
   `inject_payload` `SpawnBattleship`, then `edit_frame`) is the expected route.
3. A grid save written by a running server holding fleet and random-spawn Battleships loads with
   `resetToFresh:false`.

## Notes

- Ordinary debt follow-up. No Plan covered this root cause when this Plan was written.
- `Documents/Plans/Game/RejectInvalidShipHeadingsOnRead.md` edits the same `Ships/AGENTS.md`
  read-admission bullet and `BattleshipsInterpolate::PostRead`; the two change different clauses
  and may land in either order.
- `Documents/Plans/Game/ValidateGridSaveShipIdentities.md` records a separate grid-save gap found in
  the same run.
