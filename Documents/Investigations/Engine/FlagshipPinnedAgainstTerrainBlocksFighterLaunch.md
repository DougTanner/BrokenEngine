# Flagship Pinned Against Terrain Blocks Fighter Launch

A fleet's flagship Battleship can stay stuck nose-first against terrain. While it is stuck, Fighters added to that fleet never spawn. The defect predates the fleet-replay change: the Fighter launch branch and all Frame navigation code are older than that change, and its acceptance run found the defect. This is an Investigation, not a Plan, because no one has decided yet which term holds the ship.

## Symptom

- In a live game and in its recordings, the flagship stayed at about (-242,-245) in cell (0,1), between islands #10 and #11. For thousands of ticks it stayed inside a box of 1.2 m x 0.5 m.
- It faced NNE while its AI direction pointed S and then W. The sign of its velocity kept flipping, and its delta rotation sat at the 1.0 clamp.
- Its wanted cell was (-1,1), but it never changed cell again.

## Consequence: Fighters never launch

- `SpawnFleetMember` returns at the terrain-avoidance gate (`Projects/BrokenEngineSandbox/Source/Network/Server/ServerClientManager.cpp:170-176`). The gate waits on purpose while terrain is ahead of the launch pose, which is `kfFighterLaunchOffset` (3 m, `:89`) off the bow. The members stay dead, and their respawn timers keep getting more negative.
- Elimination proved that this gate is the return taken. The other early returns are the launch interval (`:142-146`), the cell lookup (`:149-152`), the flagship row lookup (`:155-158`), and `InsideArea` (`:166-169`).
- The gate works as designed. The root cause is the pinning.

## Suspected mechanism (not proven)

- Steering: `SteerHeadingTowards` writes `fDeltaRotation` toward `vecAiDirection` (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/Battleships.cpp:608`).
- Bounce: on contact, `ApplyTerrainBounce` (`Battleships.cpp:633`) overwrites `fDeltaRotation` with ±`kfTerrainBounceRotation` (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipBehaviours.cpp:614`, constant 8.0 at `:37`). The sign comes from the heading crossed with the contact normal, not from the wanted direction. The bounce also reflects the velocity about the normal (`:617-618`).
- Clamp: `Battleships.cpp:639` clamps the result to `kfBattleshipMaxTurnRate` (1.0).
- Navigation avoidance blends only 0.5% of its wanted turn into the current turn each tick (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/BattleshipsNavigation.cpp:50`, `Projects/BrokenEngineSandbox/Source/Frame/TerrainUtils.cpp:137`, factor 0.995 at `:90`).
- Hypothesis: steering and the bounce push the turn in opposite directions on alternate ticks, and the reflected velocity keeps the hull at the shore.

## Open decision

1. First prove which term holds the ship. At that spot, log the per-tick steering `fDeltaRotation`, the bounce sign and contact normal, and any path recompute.
2. Then choose a fix:
   - make the bounce turn agree with the wanted direction;
   - skip or invert steering while in contact;
   - reverse away from the terrain;
   - give the Fighter launch a fallback pose. This only hides the symptom.

## Proposed acceptance

- A Fighter added to a fleet whose flagship moves freely in a cell other than the origin spawns within 2 ticks.
- A flagship never stays pinned against terrain while its wanted direction points away from it.

## Notes

- Any fix is Tier 3, because it changes PostRender ship movement and therefore the determinism and CRC results.
- The Battleship and Fighter collections stay parallel (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/AGENTS.md:3`), so a movement fix applies to both.
- Session worktrees do not check out the island heightfields. Run `git sparse-checkout disable` or a Local generation build before you measure terrain heights.
