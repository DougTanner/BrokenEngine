# Ship Forces in Three Dimensions

Ships now move in Z between three flight planes (15, 40, and 65 m, `Projects/BrokenEngineSandbox/Source/Frame/FlightPlaneTable.h:9-11`), but the forces that push a ship still act only horizontally, and explosions push nothing.
The user asked for these forces to act in true 3D.
Two choices are still open: whether a pushed ship drifts back to its plane height, and whether explosions push ships at all. The user has already decided that an Omni pushed off its plane height does not launch missiles. This is an Investigation, not a Plan, until the user makes the two open choices.

Paths below `Frame/` mean `Projects/BrokenEngineSandbox/Source/Frame/Collections/`. Line numbers cite baseline `0519683ab`.

## Starting state this builds on

The cross-plane weapons change, which runs before this work, keeps explosions, pushers, and knockback as they are. It adds a stored flight-plane column, `puiFlightPlanes`, to each ship's Interpolate collection: the plane index while the ship is settled on a plane, and `kuiNoFlightPlane` while it climbs.
A small vertical nudge therefore does not change a ship's plane.
The user's decisions for that change also cover this work: collisions and explosion falloff are 3D, sim rotation stays heading-only, and there is no backward compatibility.

## What happens today

- **Velocity Z is cleared on every tick.** Each ship type's PostRender Update stores velocity Z as 0 in its last velocity write: Fighters at `Frame/Fighters/Fighters.cpp:632`, Omnis at `Frame/Omnis/Omnis.cpp:602`, and Battleships at `Frame/Battleships/Battleships.cpp:627`.
  `Frame/Ships/AGENTS.md` `## Ship Invariants` documents this ("pushers, knockback, and terrain contact move ships only horizontally. Change a ship's height through its target plane, not its velocity").
  The Fighter and Battleship comments give the reason: a stored Z speed would reach the next tick's `ApplyHeadingMovement`, which rescales its blend to the full 3D speed.
  `engine::ApplyMovement<true>` (`Engine/Source/Frame/FrameUtils.h:43-49`) normalizes the 3D velocity and blends it toward the horizontal heading, so it turns vertical speed into horizontal speed.
- **Only the climb changes height.** Interpolate Update first adds the full velocity to the position (`Fighters.cpp:249`; the Omni's integration at `Omnis.cpp:324-327` skips dying rows) and then calls `ClimbTowardsFlightPlane` (`Frame/Ships/ShipBehaviours.cpp:90-111`).
  The climb moves height toward the target plane at the type's climb rate: Fighter 25 m/s, Omni 12 m/s, Battleship 5 m/s (`Fighters.h:34`, `Omnis.h:45`, `Battleships.h:34`).
  When the climb arrives it uses the table's exact height, and `FlightPlaneAtHeight`'s exact equality depends on that.
- **Pushers.** `ApplyPusherPush` (`Frame/Ships/ShipBehaviours.cpp:624-633`) takes the 3D push from `engine::PushersInterpolate::ApplyPush`, which uses 3D falloff (`Engine/Source/Frame/Collections/Pushers/AGENTS.md`).
  It applies that push through `engine::ApplyClampedPush` (`:631`), and the clearing of velocity Z then throws the Z component away (the Omni comment at `Omnis.cpp:601` says so).
  Two ships at exactly the same height produce a push with no Z component, so the vertical part only shows up between ships at different heights, which mostly means ships that are climbing.
- **Death knockback.** A dying Fighter or Battleship replaces its velocity with `-pVecDamageDirections[i] * kfDeathKnockbackSpeed` (20 m/s) (`Fighters.cpp:623`, `Battleships.cpp:618`).
  The damage direction is 3D: it is the reversed velocity of the blaster that hit the ship, or the direction to the nearest area-damage source (`Frame/Fighters/FightersCombat.cpp:120,164`).
  Its Z component is cleared at `:632` and `:627`. Omnis have no knockback, and a dying Omni does not integrate velocity at all (`Omnis.cpp:324`).
- **Explosions apply no impulse.** `engine::AreaDamage::Get` (`Engine/Source/Frame/AreaDamage.cpp:24-56`) returns only the summed damage with 3D linear falloff, plus the nearest source position.
  Each ship type's `AreaDamage` phase body (`FightersCombat.cpp:135-168`, `Frame/Omnis/OmnisCombat.cpp:187`, `Frame/Battleships/BattleshipsCombat.cpp:135`) subtracts health and, on a kill, sets the damage direction.
  A living ship's velocity is never changed.
  Area damage is added during PostCollision and queried during AreaDamage (`Engine/Source/Frame/AGENTS.md` `## Architecture`). Missiles are the only source (`Frame/Missiles/Missiles.cpp:449-455`).
- **Missiles.** A powered missile clears velocity Z every tick (`Frame/Missiles/MissilesUpdate.cpp:195`), so it keeps the Z it was launched at. Falling flight applies gravity (`:199`, `Missiles.cpp:406`). An Omni launches missiles from its own position (`OmnisPostRender::SpawnMissiles`, `OmnisCombat.cpp:293`, spawn position at `:353`), and the cross-plane weapons change lets it launch only while `FlightPlaneAtHeight` of its Z equals its stored plane, so every missile starts exactly on a plane.
- **Terrain contact and navigation read height.** The cross-plane weapons change keeps both reads on position Z, not the stored column (`ShipBehaviours.cpp:73-88`).
  `SkipsTerrainContact` finds the plane through `FlightPlaneAtHeight`, which returns none for any height that is not exactly a plane's height, so a ship pushed off its plane height counts as between planes for terrain contact.
  `NavigationFlightPlane` takes the highest plane at or below Z, capped at the target plane, and never returns none, so a ship pushed below its plane height navigates as if on the plane below.

## Open decisions for the user

### 1. Does a ship pushed off its plane height drift back, and how fast?

- (a) **The climb pulls it back.** Keep velocity Z instead of clearing it. Have heading and accelerated movement act only on the horizontal part, and let the vertical part decay through each type's drag. `ClimbTowardsFlightPlane` already pulls height back toward the target plane at the type's climb rate every tick, so no new state is needed. The return speed then differs a lot between types (25, 12, and 5 m/s).
- (b) **A separate settle rate.** Use the same velocity Z handling as (a), but pull a settled ship back with its own constant, for example a fraction of the offset per second, instead of the full climb rate. This adds one constant per type, or one shared constant.
- (c) **No drift: the offset stays.** The ship stays wherever it was pushed until its target plane changes. This conflicts with the exact plane height that `FlightPlaneAtHeight` and missile launches rely on.

The author recommends (a): it reuses the existing climb and adds no column. The cost is that the return speed depends on the ship type.

### 2. Do explosions apply an impulse to ships?

- (a) **No (today).** Only death knockback uses the explosion's direction. In that case the work here reduces to keeping velocity Z for pushers and knockback.
- (b) **Yes, a radial impulse with 3D falloff.** `AreaDamage::Get`, or a sibling query, also returns a summed impulse pointing from each source to the ship and scaled by the same falloff. Each ship type's `AreaDamage` body applies it through `ApplyClampedPush` with a per-type cap. `AreaDamageSource` (`Engine/Source/Frame/AreaDamage.h:8`) may need an impulse-strength field.
  The phase order is unchanged: the impulse is read in AreaDamage and changes the velocity that the next Interpolate Update integrates.
- (c) **Yes, but only for killed ships.** This option is already covered by keeping knockback's Z component under decision 1.

### 3. Decided: missiles launch only exactly on the plane

The user decided that an Omni launches missiles only while its Z is exactly its stored plane's height. This is already today's behavior (the launch gate under **Missiles** above), so this work changes nothing for missiles; an Omni that keeps getting pushed off its plane height stops firing missiles until it is back on the plane.

## What a Plan from this would touch

- Velocity Z clearing and its comments: `Fighters.cpp:630-632`, `Omnis.cpp:601-602`, `Battleships.cpp:625-627`. Heading and accelerated movement (`ShipBehaviours.cpp:546-558`, `Engine/Source/Frame/FrameUtils.h:35-52`) so that they leave vertical speed alone.
- Under decision 2 (b): `Engine/Source/Frame/AreaDamage.h/.cpp` and the three `AreaDamage` phase bodies.
- The terrain-contact and navigation height reads (`ShipBehaviours.cpp:73-88`), which still read position Z.
- The invariant in `Frame/Ships/AGENTS.md` `## Ship Invariants` that reads "stores velocity Z as 0 ... move ships only horizontally".

## Risk

Every option here is Tier 3 (`.agents/references/risk-tiers.md`).
Each changes CRC'd PostRender velocities and positions, so the `Frame::kiVersion` base goes up (`Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:64-67`), and that rejects older saves, replays, and clients.
Decision 2 (b) also spans the engine area-damage queue and the game ship collections. Determinism: an impulse adds only DirectXMath basic operations in the existing phase order, and no random draw is added. Verify live with `/agent-harness`, using a replay determinism loop that includes pushes between ships climbing past each other and missile explosions next to ships.

## Related

- `Documents/Features/Graphics/ShipWreckage.md` reads the knockback velocity at `Destroy`. A vertical knockback would change what a wreck inherits.
- `Documents/Investigations/Engine/SameCollectionCollision.md` covers pusher strength and same-type separation, which is the horizontal side of the same mechanism.
