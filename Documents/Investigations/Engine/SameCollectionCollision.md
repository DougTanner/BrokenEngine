# Same-Collection Collision

Ships are being split into the Fighters, Omnis, and Battleships Collections. Fleets mix types, ships fly on three flight planes, and bumping and pushers use 3D distance falloff. The engine only collides different collision layers, so same-type ships are kept apart only by Pushers.

## Why same-layer collision is unsupported

- The rule is documented in `Engine/Source/Frame/AGENTS.md:85-86`, and `Engine/Source/Frame/Collision.cpp:316-319` asserts that no layer's `uiCollidesWith` contains its own `uiCategory`.
- Pairs are enumerated as `j = i + 1` (`Collision.cpp:322-324`), so the loop never reaches the diagonal. Each layer pair fills zone lists with layer B's objects only, and the narrowphase computes each layer-A object's zone range itself (`Collision.cpp:71-76`, `:352`, `:657`). If A were the same layer as B, the narrowphase walk (`:649-690`) would test each object against itself and test every unordered pair twice.
- Result routing is not the blocker: pending results are keyed by (layer, object) (`:402`, `:433`), and `CommitCandidate` already writes both sides (`:504-505`).
- The broadphase is an 8x8 grid in XY over the cell (`Collision.h:8-9`, `Collision.cpp:10-13`), while the narrowphase tests are 3D (`:258`, `:578`). Flight planes therefore remove no broadphase work but cause no false hits.
- Determinism comes from a global sort on (time, layer A, object A, layer B, object B) (`:386-389`). The order is total only while each unordered pair appears once.
- Alignment gate: `TestAndCollectPair` drops non-enemy pairs (`:548`) because `CanCollide` requires `kuiEnemies` (`Engine/Source/Frame/Alignments.cpp:46`); the game has two alignments (`Projects/BrokenEngineSandbox/Source/Game.cpp:19-22`). The missing case is only enemy ships of one type; friendly ships never collide under options (a)-(b) unless the alignment rules change.

## Pushers compared with collision

- A pusher is a radial field per entity, `(1 - d²/r²)^power * intensity`, measured in 3D and summed (`Engine/Source/Frame/Collections/Pushers/PushersUpdate.cpp:175-195`), on one 50x50 XY zone grid per flight-plane layer (`:8-17`). Pushers ignore alignment and collection and filter only by flags (`:163-173`).
- The response is a velocity impulse capped by `ApplyClampedPush` (`Pushers.h:27-32`) at half the maximum speed (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/Fighters.h:58`, `Fighters.cpp:610`, `Ships/ShipBehaviours.cpp:605-619`).
  Its only position correction is a 0.001 m X offset from a peer within 0.0001 m in XY (`PushersUpdate.cpp:184-188`), and it produces no events or damage. The fighter pusher radius of 2.25 (`Fighters.h:55`) lies inside the 3.0 contact distance of two 1.5 radii (`Fighters.h:27`), so same-type ships can rest overlapping.
- Collision produces events (`Collision.h:64-74`) that the game turns into damage (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/OmnisCombat.cpp:158-162`, `Fighters/FightersCombat.cpp:102-106`), never separation. It runs after Update (`Engine/Source/Frame/FrameBase.cpp:293-299`).

## Open question for the user

What should a same-type collision do?
- Damage events, as a bump that deals damage. The game currently defines no ship-collision damage amount.
- Hard separation, meaning ships never interpenetrate.
- Both.

Should it apply to enemies only, which the alignment gate already enforces, or also to allies?

## Options

### (a) Self-pair mode in the engine

Allow `uiCollidesWith & uiCategory` on one layer and enumerate the diagonal once with `i < j`. Both rows receive the result through the existing routing.
- Sites: assert `Collision.cpp:316-319`; loop `:322-324`; skip `iOther <= i` in `:678-687`; doc `Frame/AGENTS.md:85-86`; game masks `HealthDamage.h:39-53`; own-category handling in each PostCollision. The zone insert (`:352`) already inserts only layer B and needs no change.
- Determinism and CRC: the sort stays total because each pair is unique. The collision scratch is `thread_local` outside the Frame (`Collision.h:127-145`); damage outcomes enter the CRC (`Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:651`). Bump `Frame::kiVersion` (`Frame.cpp:106`) so replays recorded under the old rules are rejected.
- Cost: n²/2 tests per zone for dense fleets. The candidate preallocation of 512 (`Collision.h:13`) overflows with a `DEBUG_BREAK` (`Collision.cpp:604-609`).
- Risks: low. The change is contained in one engine function, and other layers are unchanged.

### (b) A second layer view onto the same collection

Register the collection twice with paired categories X and Y that collide with each other. No engine change is needed.
- Sites: two `AddLayer` calls per type (pattern `FightersCombat.cpp:64-74`), a paired category bit for each of the three per-type ship categories in `uint16_t` (`Collision.h:43-44`; five used, `HealthDamage.h:30-34`, ship types `:31-33`), and a layer budget of 16 (`Collision.h:11`).
- Determinism: deterministic, but (X,i,Y,j) and (X,j,Y,i) both pass, so every contact appears twice; the game must read one view, and blasters and missiles must target only X (`HealthDamage.h:44-46`). Only the alignment check drops the self pair (`Collision.cpp:548`).
- Cost: double the tests, candidates, and sort length.
- Risks: high. Duplicate-event discipline is spread across every consumer.

### (c) Stronger pushers plus a hard non-penetration resolve

Raise the pusher radius above the contact distance or raise the cap (`Fighters.h:55-58`). Optionally add a deterministic position projection that runs in Update in index order.
- Sites: game constants and a resolve pass before PreCollision. PreCollision registers the collision layer from final positions (`FightersCombat.cpp:64-74`), so the resolve must run first. The terrain response is at `Fighters.cpp:650`.
- Determinism: single-threaded per cell, and positions enter the CRC; bump `Frame::kiVersion`.
- Cost: O(n·k) per pusher zone.
- Risks: jitter in dense fleets, a residual overlap after one pass, and pushes into terrain. It produces no events, and it covers allies and every type.

### (d) Contact query on the Pushers grid

Add a sibling to `ApplyPush` (`PushersUpdate.cpp:138-199`) that returns pushers within the contact distance, for bump events across every type and both alignments.
- Sites: `PushersPostRender` carries only `pIds` (`Pushers.h:93`), so it would need owner, category, and alignment columns and a `kiVersion` bump (`Pushers.h:86-87`).
- Determinism: deterministic, because zone lists are filled in index order (`PushersUpdate.cpp:80-136`).
- Cost and risks: a second event system outside the global time-of-impact ordering.

## Recommendation

- If the answer is damage events between enemies: (a). It is the smallest change, keeps one event per pair, and preserves the sort invariant.
- If the answer is hard separation: (c), independently of (a).
- If the answer is both: (a) and (c).
- Reject (b) for its duplicates and (d) for duplicating Collision.

A Plan needs the user's answer to the open question, the scope (enemies only, or allies too, which needs an alignment-rule change), and the ordering against the Fighters/Omnis/Battleships split Plan.
