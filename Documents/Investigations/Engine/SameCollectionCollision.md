# Same-Collection Collision

Ships are being split into the Fighters, Omnis, and Battleships Collections. Fleets mix types, ships fly on three flight planes, and bumping and pushers use 3D distance falloff. The engine only collides different collision layers, so same-type ships are kept apart only by Pushers.

## Why same-layer collision is unsupported

- The rule is documented in `Engine/Source/Frame/AGENTS.md:18`, and `Engine/Source/Frame/Collision.cpp:282-286` asserts that no layer's `uiCollidesWith` contains its own `uiCategory`.
- Pairs are enumerated as `j = i + 1` (`Collision.cpp:289-291`), so the loop never reaches the diagonal. Each layer pair fills separate A and B zone lists (`Collision.cpp:68-71`, `:318-319`). If A were the same layer as B, the narrowphase walk (`:610-647`) would test each object against itself and test every unordered pair twice.
- Result routing is not the blocker: pending results are keyed by (layer, object) (`:369`, `:400`), and `CommitCandidate` already writes both sides (`:471-472`).
- The broadphase is an 8x8 grid in XY over the cell (`Collision.h:8-9`, `Collision.cpp:265-266`), while the narrowphase tests are 3D (`:216`, `:539`). Flight planes therefore remove no broadphase work but cause no false hits.
- Determinism comes from a global sort on (time, layer A, object A, layer B, object B) (`:352-356`). The order is total only while each unordered pair appears once.
- Alignment gate: `TestAndCollectPair` drops non-enemy pairs (`:495`) because `CanCollide` requires `kuiEnemies` (`Engine/Source/Frame/Alignments.cpp:46`); the game has two alignments (`Projects/BrokenEngineSandbox/Source/Game.cpp:41-43`). The missing case is only enemy ships of one type; friendly ships never collide under options (a)-(b) unless the alignment rules change.

## Pushers compared with collision

- A pusher is a radial field per entity, `(1 - d²/r²)^power * intensity`, measured in 3D and summed (`Engine/Source/Frame/Collections/Pushers/PushersUpdate.cpp:152-165`), on a 50x50 XY zone grid (`:6-7`). Pushers ignore alignment and collection and filter only by flags (`:134-144`).
- The response is a velocity impulse capped by `ApplyClampedPush` (`Pushers.h:27-32`) at half the maximum speed (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/Spaceships.h:32`, `SpaceshipsNavigation.cpp:125-136`). It applies no position correction and produces no events or damage. The spaceship pusher radius of 2.25 (`Spaceships.h:29`) lies inside the 3.0 contact distance (`Spaceships.h:22`), so same-type ships can rest overlapping.
- Collision produces events (`Collision.h:47-57`) that the game turns into damage (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersCombat.cpp:236-238`, `Spaceships/SpaceshipsCombat.cpp:147-149`), never separation. It runs after Update (`Engine/Source/Frame/FrameBase.cpp:248-252`).

## Open question for the user

What should a same-type collision do?
- Damage events, as a bump that deals `kDamageSpaceshipCollision` (`Projects/BrokenEngineSandbox/Source/Frame/HealthDamage.h:11`, `:19`).
- Hard separation, meaning ships never interpenetrate.
- Both.

Should it apply to enemies only, which the alignment gate already enforces, or also to allies?

## Options

### (a) Self-pair mode in the engine

Allow `uiCollidesWith & uiCategory` on one layer and enumerate the diagonal once with `i < j`. Both rows receive the result through the existing routing.
- Sites: assert `Collision.cpp:282-286`; loop `:289-291`; single zone insert `:318-319`; skip `iOther <= i` in `:639-647`; doc `Frame/AGENTS.md:18`; game masks `HealthDamage.h:52-64`; own-category handling in each PostCollision.
- Determinism and CRC: the sort stays total because each pair is unique. The collision scratch is `thread_local` outside the Frame (`Collision.h:74-115`); damage outcomes enter the CRC (`Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:781-785`). Bump `Frame::kiVersion` (`Frame.cpp:38`) so replays recorded under the old rules are rejected.
- Cost: n²/2 tests per zone for dense fleets. The candidate preallocation of 512 (`Collision.h:13`) overflows with a `DEBUG_BREAK` (`Collision.cpp:565-569`).
- Risks: low. The change is contained in one engine function, and other layers are unchanged.

### (b) A second layer view onto the same collection

Register the collection twice with paired categories X and Y that collide with each other. No engine change is needed.
- Sites: two `AddLayer` calls per type (pattern `SpaceshipsCombat.cpp:105-118`), three new category bits in `uint16_t` (`Collision.h:41-42`; five used, `HealthDamage.h:45-49`), and a layer budget of 16 (`Collision.h:11`).
- Determinism: deterministic, but (X,i,Y,j) and (X,j,Y,i) both pass, so every contact appears twice; the game must read one view, and blasters and missiles must target only X (`HealthDamage.h:57-61`). Only the alignment check drops the self pair (`Collision.cpp:495`).
- Cost: double the tests, candidates, and sort length.
- Risks: high. Duplicate-event discipline is spread across every consumer.

### (c) Stronger pushers plus a hard non-penetration resolve

Raise the pusher radius above the contact distance or raise the cap (`Spaceships.h:29-32`). Optionally add a deterministic position projection that runs in Update in index order.
- Sites: game constants and a resolve pass before PreCollision. PreCollision traces the cell exit from final positions (`SpaceshipsCombat.cpp:101-102`), so the resolve must run first. The terrain response is at `SpaceshipsNavigation.cpp:138`.
- Determinism: single-threaded per cell, and positions enter the CRC; bump `Frame::kiVersion`.
- Cost: O(n·k) per pusher zone.
- Risks: jitter in dense fleets, a residual overlap after one pass, and pushes into terrain. It produces no events, and it covers allies and every type.

### (d) Contact query on the Pushers grid

Add a sibling to `ApplyPush` (`PushersUpdate.cpp:102-169`) that returns pushers within the contact distance, for bump events across every type and both alignments.
- Sites: `PushersPostRender` carries only `pIds` (`Pushers.h:95`), so it would need owner, category, and alignment columns and a `kiVersion` bump (`Pushers.h:36-37`).
- Determinism: deterministic, because zone lists are filled in index order (`PushersUpdate.cpp:57-91`).
- Cost and risks: a second event system outside the global time-of-impact ordering.

## Recommendation

- If the answer is damage events between enemies: (a). It is the smallest change, keeps one event per pair, and preserves the sort invariant.
- If the answer is hard separation: (c), independently of (a).
- If the answer is both: (a) and (c).
- Reject (b) for its duplicates and (d) for duplicating Collision.

A Plan needs the user's answer to the open question, the scope (enemies only, or allies too, which needs an alignment-rule change), and the ordering against the Fighters/Omnis/Battleships split Plan.
