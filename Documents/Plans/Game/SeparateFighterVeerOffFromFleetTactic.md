<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-05T12:41:13.726Z","dependsOn":[]} -->
# Separate Fighter Veer-off From the Shared Fleet Tactic

## Context

`ChooseFleetTactic` serves fleet Fighters and fleet Omnis. Paths below are relative to
`Projects/BrokenEngineSandbox/Source/Frame/Collections/`, and line numbers are from `5c2e6a8d`.
Originating gap: veer-off is Fighter-only, yet the shared function carries it, so Omnis pay for it:

- `Ships/FleetCoordination.cpp:181` and `:199` test `eType == ShipType::kFighter` to gate
  `ContinueVeerOff` and `AttackOrStartVeerOff`; lines 173 and 207 clear `kVeerOff` for both types.
- Its `rFlags` and `rVecVeerOffPoint` parameters exist only for veer-off.
- `Omnis/Omnis.cpp:638-640` passes a zeroed `vecVeerOffPoint` it then discards.

Verified root cause: commit `9e6807b1` replaced the fleet Omni veer-off with circling and kept the
shared function by adding those two type tests. The commit left the type tests in place only
because that change's scope stopped there; its refactor log row, since removed but readable in
`9e6807b1`, said so. The plan option that row cited is not in tracked history, and it scoped that
landed change only.

The `9e6807b1` row overstated one gain: `ChooseFleetTactic` keeps `eType` after this change,
because escort differs by type (`:208`) and the highest plane, default plane, and clearance tier
all read it. Only the two veer-off type tests and the two parameters go.

## Design

The author recommends one Fighter-only wrapper in `Ships/FleetCoordination.cpp` around a veer-off
free `ChooseFleetTactic`, not a move into `Fighters/Fighters.cpp` as the `9e6807b1` row proposed. Rationale:
`ContinueVeerOff`, `AttackOrStartVeerOff`, and the Fighter-only `ChooseAttackTactic` already live in
`FleetCoordination.cpp`, and `Fighters.cpp` measures 9,938 `bt-token-v1`, so moving those helpers
there would push it over the 10,000 threshold.

`ChooseFleetTactic` loses `rFlags` and `rVecVeerOffPoint`, the clears at lines 173 and 207, the
continuation block at lines 180-186, and the start at lines 199-202, leaving
`return AttackTactic(vecTargetPosition);`.

A new `ChooseFighterFleetTactic` takes the old parameters minus `eType`:

```cpp
ShipTactic XM_CALLCONV ChooseFighterFleetTactic(const Frame& rPreviousFrame, const engine::CellStaticData& rStaticData, const FleetView& rView, engine::GlobalId globalId, FXMVECTOR vecPosition, NavRectangleBoard& rNavRectangleBoard, uint8_t uiFlightPlane, engine::AlignmentIdentifier alignment, ShipFlags_t& rFlags, XMVECTOR& rVecVeerOffPoint, engine::registry_id_t& rRegistryTarget)
{
	if (rView.flagshipGlobalId.iValue != 0 && rView.eStance == FleetStance::kWeaponsFree)
	{
		ShipTactic tactic {};
		if (ContinueVeerOff(vecPosition, rVecVeerOffPoint, rFlags, tactic))
		{
			// Attack ignores enemies above the type's highest plane, so only a veer-off point loaded from a save can lie above it.
			tactic.iTargetFlightPlane = std::min(tactic.iTargetFlightPlane, HighestFlightPlane(ShipType::kFighter));
			return tactic;
		}
	}

	ShipTactic tactic = ChooseFleetTactic(ShipType::kFighter, rPreviousFrame, rStaticData, rView, globalId, vecPosition, rNavRectangleBoard, uiFlightPlane, alignment, rRegistryTarget);
	if (tactic.bAttacking)
	{
		return AttackOrStartVeerOff(vecPosition, tactic.vecDestination, rFlags, rVecVeerOffPoint);
	}
	rFlags.Set(ShipFlags::kVeerOff, false);
	return tactic;
}
```

Why each Fighter path stays bit-identical:

- Veer-off continuing: the same stance gate runs before `FindAttackTarget`, so `rRegistryTarget`
  stays untouched as it does today, and the plane clamp moves along.
- Attack in sight: `AttackTactic`'s destination is the enemy position, which is what line 201 passes
  to `AttackOrStartVeerOff`.
- Hidden target: `RouteTactic` leaves `bAttacking` false. `kVeerOff` is already clear there, because
  a set flag either continued above or was cleared by `ContinueVeerOff`, so the clear is a no-op.
- Escort, no enemy found, and no flagship: `bAttacking` is false, and the wrapper clears `kVeerOff`
  as lines 173 and 207 do.

Omnis lose only the clears. Nothing sets `kVeerOff` on an Omni row (the only setter is
`AttackOrStartVeerOff`, which only Fighter paths reach), so the clears are no-ops in every normal
run. The one difference is a harness-injected Omni flag, which would stay set; no Omni code reads it.

`Fighters/Fighters.cpp:621` calls the wrapper. `Omnis/Omnis.cpp:638-640` drops the comment, the local,
and the two arguments.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/FleetCoordination.cpp`, `.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/Fighters.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/Omnis.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/AGENTS.md`

## In scope

- `ChooseFleetTactic`: signature, body, and header comment.
- New `ChooseFighterFleetTactic` declaration, comment, and definition.
- The `ChooseFleetTactic` call in `FightersPostRender::Update` and in `OmnisPostRender::Update`,
  including the Omni `vecVeerOffPoint` local and its comment.
- `/update-claude-docs` sync of `Ships/AGENTS.md` `## Fleet Coordination Invariants`, whose shared
  tactic sentence names veer-off.

## Out of scope

- `ChooseAttackTactic`, `ContinueVeerOff`, `AttackOrStartVeerOff`, `VeerOffTactic`, and `RouteTactic`
  bodies and signatures.
- Veer-off distances, escort, target picking, and `pVecVeerOffPoints`.
- Moving veer-off code into `Fighters.cpp`.
- Any `Frame::kiVersion`, collection version, wire, or save change.

## Risk and invariants

- Tier 2. Trigger: one subsystem's scoped change with a changed public signature, so it is not
  Tier 1. It is not Tier 3, because it changes nothing the CRC surface computes.
- Determinism: bit-identical by the path table in `## Design`. No random draw, no read of another
  frame, and no column changes. A CRC difference is a defect in this change, never a reason to bump
  a version.
- Ordering: the veer-off continuation still precedes `FindAttackTarget`, the order that keeps a kept
  attack target untouched during a veer-off (`Ships/AGENTS.md` `## Fleet Coordination Invariants`).

## Acceptance criteria

- `ChooseFleetTactic` has no `rFlags` or `rVecVeerOffPoint` parameter and no
  `eType == ShipType::kFighter` test, and `Omnis.cpp` has no `vecVeerOffPoint`.
- `Frame::kiVersion` and every collection version are unchanged.
- Client and server build through `/compile`.
- A replay recorded by a build of this Plan's parent commit plays on the changed build, with the
  same `--app-data-directory`, through a completed loop with no checksum mismatch, by the
  `/agent-harness` `.agents/skills/agent-harness/references/replay.md`
  `### Replay determinism acceptance` steps. `describe_scene` shows `kVeerOff` on a fleet Fighter row
  during the recording, so the recording exercises the moved path.
- Harness setup: the supplied evidence records no setup for a veer-off and no recording played across
  builds. Reach weapons free with `inject_payload` enemy spawns near a flagship
  (`Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`) and record the command
  sequence. If the changed build refuses the baseline recording, surface that instead of weakening
  this criterion. No unit tests.

## Coordination

No directional prerequisites. Reciprocal coordination with
`Documents/Plans/Game/RouteShipFleeReturnAndVeerOffGoals.md`: that Plan adds the cell static data and
the board to `ContinueVeerOff` and `AttackOrStartVeerOff`, so whichever lands second refreshes its
call sites inside `FleetCoordination.cpp`. `ChooseFighterFleetTactic` already takes `rStaticData` and
`rNavRectangleBoard` and passes them through. Each Plan keeps its own acceptance; this Plan's replay
compares against its own parent commit, so it holds in either order.

Reciprocal coordination with `Documents/Plans/Game/BlockLineOfSightWithLargeShips.md`: that Plan
passes the `NavRectangleBoard&` that `ChooseFleetTactic` and `ChooseFighterFleetTactic` already take
into its `FindAttackTarget` and `HasLineOfSight` calls, so whichever lands second refreshes those
calls. Reciprocal coordination with
`Documents/Plans/Game/ChooseVeerOffTurnFromNavigation.md`: that Plan adds arguments to
`AttackOrStartVeerOff`, so whichever lands second refreshes the call. The same replay reasoning holds
for both.

## Notes

- `Fighters.cpp` stays under 10,000 tokens because the call only changes its name and loses `eType`.
