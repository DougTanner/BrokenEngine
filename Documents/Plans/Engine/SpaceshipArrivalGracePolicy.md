<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-30T23:51:08.659Z","dependsOn":[]} -->
# Fix spaceship owner behavior during arrival grace

## Context

Cross-cell Spaceship transfer assigns a fresh one-second arrival grace period in
`Projects/BrokenEngineSandbox/Source/SpawnTransfer.cpp`, and
`SpaceshipsPostRender::Spawn` stores it on the destination row. The collection
contract says this period suppresses targeting and behavior scans while leaving
collision and damage active.

The destination ship's own phases do not currently honor that contract.
`SpaceshipsPostRender::Update` still scans Players, regenerates health, computes
steering, and applies the combined AI-movement/pusher path; `AvoidTerrain` still
performs a player-proximity scan; and the PostRender Spawn phase can target a
Player and fire an enemy blaster. The existing `ApplyMovement` combines AI
acceleration and drag with pusher response, while `ApplyTerrainBounce` changes
position, velocity, and turn, so a single broad guard would also suppress
physical interactions that arrival grace is meant to retain.

The user selected the physical-response policy: while grace is positive, the
ship continues its carried motion and turn and retains pusher response, terrain
bounce, collision, and damage, but performs no player scan, health regeneration,
AI steering or acceleration/drag, terrain avoidance, or enemy firing. Grace and
weapon cooldowns continue counting down, and ordinary behavior resumes on the
tick whose decremented grace value reaches zero.

## Design

- In `SpaceshipsPostRender::Update`, decrement and clamp the arrival-grace timer
  before selecting behavior for the row. Use that current value consistently:
  a positive value suppresses the nearest-player scan, health regeneration,
  `ComputeSteering`, and the AI acceleration/drag portion of movement; zero
  permits the ordinary path on that tick.
- Separate the existing pusher-response portion of `ApplyMovement` from its AI
  acceleration/drag portion so the normal path still performs both in the same
  order, while the positive-grace path performs only pusher response. Do not add
  a new collection member, transfer field, or configuration surface.
- Preserve the carried velocity and delta rotation when AI behavior is
  suppressed. Position and direction continue to integrate from those carried
  values in the existing Interpolate phase. Retained pusher response may change
  velocity, and retained `ApplyTerrainBounce` may change position, velocity, and
  delta rotation in its existing order.
- Keep `ApplyTerrainBounce` active for non-exploding grace rows. In
  `AvoidTerrain`, skip positive-grace rows before its nearest-player scan and
  terrain-avoidance turn calculation.
- In the PostRender Spawn phase hook, skip positive-grace non-exploding rows
  before nearest-player targeting and enemy-blaster firing. Continue decrementing
  the carried weapon cooldown during grace; when grace reaches zero, firing uses
  the ordinary cooldown and facing checks without an extra transition tick.
- Keep collision, direct and area damage, death handling, inbound registry
  eligibility filters, and the one-second grace duration unchanged. Update the
  owning collection documentation to state this operation-by-operation policy
  and the zero-boundary transition.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/Spaceships.cpp` — grace countdown, owner-side player scan, health regeneration, steering/movement selection, and Spawn-phase firing guard
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/SpaceshipsNavigation.cpp` — separate AI movement from retained pusher response; skip grace rows in terrain avoidance while retaining terrain bounce
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/Spaceships.h` — helper declarations only if required by the smallest split
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/AGENTS.md` — Spaceship arrival-grace behavior and transition invariant
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/AGENTS.md` — shared arrival-grace contract, only if the existing general wording needs a reference to the Spaceship-specific policy

## In scope

- `SpaceshipsPostRender::Update` grace gating for nearest-player lookup,
  `RegenerateHealth`, `ComputeSteering`, AI acceleration/drag, retained pusher
  response, retained `ApplyTerrainBounce`, timer countdown, and the first
  zero-grace tick
- `SpaceshipsPostRender::ApplyMovement` and/or the smallest helper split needed
  to call AI movement and pusher response independently without changing their
  normal-path order or results
- `SpaceshipsPostRender::AvoidTerrain` positive-grace skip before player scans
  and avoidance steering
- The PostRender `SpaceshipsPostRender::Spawn` phase hook's positive-grace skip
  before player targeting and enemy-blaster firing
- Spaceship collection documentation for the selected physical-response policy
- Focused transfer/grace runtime verification covering each retained and
  suppressed operation plus deterministic client/server results

## Out of scope

- Changing the one-second arrival-grace duration or transfer materialization
- Collection layout, serialization, wire format, replay format, or transfer
  behavior flags
- Generic collision, damage, health values, blaster collision, registry-window
  eligibility, pusher mechanics, terrain algorithms, or unrelated entity grace
- Swept terrain collision, new test infrastructure, unit tests, configuration,
  compatibility paths, or cleanup outside the affected grace branches

## Risk tier and invariants

Expected Change Workflow Tier 3: this changes fixed-tick, CRC-visible Spaceship
behavior across cell transfer and must remain paired between client and server.
Preserve the phase ordering and prior/current Player-frame sources on the normal
path. During positive grace, no owner-side Player targeting or behavior scan may
run; carried motion and turn remain unchanged except for the existing pusher and
terrain-bounce responses. Collision and damage remain active. The transition at
zero must neither add an extra suppressed tick nor duplicate steering or firing.
No collection, serialization, or wire layout changes are authorized.

## Acceptance criteria

- A focused `/agent-harness` transfer scenario proves that a grace-positive ship
  near an alive Player does not scan-derived steer, regenerate health, perform
  terrain-avoidance steering, or fire, while its grace and weapon cooldowns
  continue to decrement.
- Focused variations prove carried velocity/delta rotation continue, a nearby
  pusher can change velocity, terrain contact can apply the existing bounce
  position/velocity/turn response, and collision and damage remain active during
  grace.
- On the tick whose decremented grace value reaches zero, ordinary player scan,
  steering, health regeneration eligibility, terrain avoidance, and firing
  eligibility resume exactly once under their existing conditions.
- The unchanged normal path produces the same ordering and results outside
  grace, and inbound target filters continue to exclude grace-positive ships.
- Client and server Debug x64 builds pass. The focused scenario passes with
  matching client/server CRC, and replay determinism remains equal across the
  grace interval and first ordinary tick.

## Notes

The investigation was validated against baseline
`74aab67cb8ebc43a79c9860a90da706767d52836`. No existing executable Plan owns
this owner-side arrival-grace policy. The separate Spaceship transfer
behavior-flag investigation remains independent and is not a prerequisite for
this change.
