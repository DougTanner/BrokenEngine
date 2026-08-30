# Spaceship arrival-grace movement and behavior policy

Status: Open investigation; no implementation decision has been made.

Area: Engine

Record type: Non-executable reference material. This document intentionally has
no scheduler metadata and is not a Plan input.

Audit source: `CPT/shard-0050/002` in the frozen C++ Plan Trace Audit.

Frozen audit commit: `80896f33661aaab99cf180a96db54600099be652`

## Finding under investigation

The cross-cell transfer path supplies a fresh one-second arrival-grace value
(`Projects/BrokenEngineSandbox/Source/SpawnTransfer.cpp:17-27`), and
`SpaceshipsPostRender::Spawn` stores it in the destination row
(`Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/Spaceships.cpp:552-594`).
The documented contract says arriving entities are skipped by targeting and
behavior scans, but remain subject to collision and damage
(`Projects/BrokenEngineSandbox/Source/Frame/HealthDamage.h:31-32`;
`Projects/BrokenEngineSandbox/Source/Frame/Collections/AGENTS.md:12-14`).

The owner-side consumers do not have one unambiguous interpretation of that
contract. The Spaceship PostRender phase hook scans for a nearest player and
can fire an enemy blaster without checking `pfArrivalGracePeriods`
(`Spaceships.cpp:465-510`). In `SpaceshipsPostRender::Update`, the grace timer
is decremented, but the code still scans the previous-frame players, regenerates
health, computes steering, and calls `ApplyMovement` while the timer is
positive (`Spaceships.cpp:629-652`). The later `AvoidTerrain` phase also runs
after the update and can perform a player-proximity terrain-avoidance scan
(`Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/SpaceshipsNavigation.cpp:154-196`).

`ApplyMovement` is not only a target scan: it chooses acceleration from the
return/flee/chase flags and then applies nearby-pusher velocity
(`SpaceshipsNavigation.cpp:119-135`). `ApplyTerrainBounce` always runs after
the behavior branch and can move the ship, reflect velocity, add bounce
velocity, and change its turn (`:137-152`). Thus a proposal to suppress
owner-side behavior during grace must decide whether acceleration, pusher
interaction, terrain bounce, terrain avoidance, and health regeneration are
behavior or physical interaction. The earlier Plan selected no exact policy
for those interactions, so “preserve velocity” and “collision/interaction
remains active” cannot both be evaluated from its text alone.

Inbound target eligibility does filter the arriving ship from other entities
(`Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:497-505,530-546`; player
combat `PlayersCombat.cpp:56-74`), but that does not protect the arriving
ship's own scans, steering, movement, or blaster phase. The durable source
trace proves the owner-side gap and the unresolved movement/interaction
boundary; the ignored shard report is supplementary provenance.

## Controlling contract and invariant

`HealthDamage.h:31-32` and `Projects/BrokenEngineSandbox/Source/Frame/Collections/AGENTS.md:12-14`
define arrival grace as a fresh one-second transfer state that suppresses
targeting and behavior scans while leaving collision and damage active.
`Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/AGENTS.md:5-12`
owns Spaceship steering, terrain interaction, and hysteresis behavior. The
frame phase order and shared PostRender state keep client and server behavior
CRC-visible.

The unresolved invariant is the exact set of owner-side operations allowed
while `pfArrivalGracePeriods[i] > 0`: a transferred ship must not acquire a
target or fire early, while the selected physical interactions and any
preserved state must be explicit, deterministic, and client/server paired.
The policy must also retain the existing countdown boundary and avoid silently
changing a carried velocity or turn state merely because a behavior gate was
added.

## Boundary and impact

The open boundary is Spaceship transfer materialization and the owner-side
PostRender Spawn, Update, `ApplyMovement`, `ApplyTerrainBounce`,
`AvoidTerrain`, and blaster-targeting paths during positive grace. It includes
player scans, acceleration, pusher effects, terrain movement/turn/velocity,
health regeneration, the grace countdown, and enemy-fire timing. It excludes
the one-second duration itself, generic collision and damage ownership,
inbound registry filters, transfer behavior flags, health policy, blaster
collision, random algorithms, and unrelated entity grace handling.

If only the blaster block is gated, the ship can still steer or change velocity
from a player while the contract says behavior scans are skipped. If all of
`ApplyMovement` is gated, nearby pushers no longer affect the ship during a
period when physical interaction may be required. If terrain bounce or
avoidance is retained, position, velocity, or turn can change during a period
that a preserve-velocity interpretation expects to be stable. Each outcome is
observable in shared state and can affect CRC/replay, so the choice cannot be
left implicit in a branch condition.

## Open choices

These alternatives are recorded for a future decision; none is selected here.

1. **Behavior-only suppression with physical interaction.** Suppress target
   acquisition, owner-side steering/AI acceleration, terrain-avoidance scans,
   and enemy blaster fire during grace, while explicitly retaining only the
   collision-class interactions that the authority identifies as required.
   Define whether `ApplyTerrainBounce` and the pusher portion of
   `ApplyMovement` belong to that retained set, and whether health regeneration
   is independent of targeting.
2. **Arrival-state preservation.** Hold the carried velocity, direction/turn,
   and target-derived state during grace, suppressing all operations that can
   alter them, while retaining collision and damage. Define how a pusher or
   terrain contact is represented when it would otherwise change velocity or
   position, and how the first post-grace tick resumes without a skipped or
   duplicated fire.
3. **Explicit physical-grace policy.** Permit pusher and terrain responses
   during grace, but suppress player-target scans, AI acceleration, and firing.
   Define the exact ordering with `ApplyTerrainBounce` and `AvoidTerrain`, the
   allowed changes to velocity/position/turn, and why those changes satisfy
   the arrival contract rather than constituting behavior.

No option may infer transfer state from a zero value or silently reuse the
inbound target filter as an owner-side gate. The selected policy must be
 expressed at each affected phase so future changes cannot make one of the
 movement or terrain interactions drift from the documented rule.

## Decisive questions and acceptance evidence

- Does “behavior scan” include `NearestAlivePlayerPosition`,
  `RegenerateHealth`, `ComputeSteering`, `AvoidTerrain`, and the enemy-fire
  scan, or only operations that choose a player target? Which authority owns
  that classification?
- During positive grace, must the transferred velocity and turn be preserved
  exactly, or may pusher and terrain responses change them? If physical
  interaction remains active, which position, velocity, and delta-rotation
  changes are allowed and in what phase order?
- Is health regeneration allowed while grace is positive when its input is the
  nearest alive player, and does retaining it require a player scan that the
  grace contract otherwise forbids?
- Can a focused transfer scenario place a ship with an in-range player, a
  nearby pusher, and terrain contact, then separately observe target/fire
  suppression, movement/push/terrain outcomes, countdown expiry, and ordinary
  post-grace behavior?
- Do collision and damage remain active, do inbound filters still exclude the
  arriving ship, and do client/server CRC and replay results remain equal for
  every selected grace branch?

The eventual executable Plan must select one operation-by-operation policy,
name the exact phase guards and preserved interactions, state the first
post-grace transition, and bind a focused transfer/grace acceptance scenario.
Expected future work is Tier 3 because the choice changes fixed-tick,
CRC-visible Spaceship behavior across transfer while requiring client/server
parity and phase-order evidence. Until that policy is selected, no source fix
is authorized.

## Provenance

- Frozen source candidate: `CPT/shard-0050/002`.
- Frozen consolidated index: `Temp/CppPlanTraceAudit/80896f33661aaab99cf180a96db54600099be652/consolidated-index.md`.
- Durable source evidence: `Projects/BrokenEngineSandbox/Source/SpawnTransfer.cpp:17-27`, `Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/Spaceships.cpp:465-510,552-594,629-675`, and `Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/SpaceshipsNavigation.cpp:45-196`.
- Existing inbound filters were traced but do not own the arriving ship's
  owner-side policy. The route was reclassified after adversarial review
  because `ApplyMovement`, pusher, and terrain behavior remained undecided.
- No source, wire, replay, collection-layout, or scheduler change is part of
  this investigation.
