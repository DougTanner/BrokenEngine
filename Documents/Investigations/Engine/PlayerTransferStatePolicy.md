# Player transfer navigation and aim state policy

Status: Open investigation; no implementation decision has been made.

Area: Engine

Record type: Non-executable reference material. This document intentionally has
no scheduler metadata and is not a Plan input.

Audit source: `CPT/shard-0049/003` in the frozen C++ Plan Trace Audit.

Frozen audit commit: `80896f33661aaab99cf180a96db54600099be652`

## Finding under investigation

The Player transfer producer serializes direction, velocity, timers, flags,
fleet state, identity, and navigation delay but omits wanted direction, cached
AI steering, island destination, navigation mode, and waypoint state
(`Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersNavigation.cpp:249-276`).
`SpawnTransfer` passes the available payload to the destination
(`Projects/BrokenEngineSandbox/Source/SpawnTransfer.cpp:75-97`), and the
destination spawn initializes omitted shared state from fresh defaults:
`pVecWantedDirections` receives hull direction, `pVecAiDirections` and island
destination are zeroed, and navigation mode/waypoint index restart
(`Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/Players.cpp:464-488`).
Those fields feed the next fixed-tick navigation and aim update
(`Players.h:269-320`; `Players.cpp:767-863`; `PlayersNavigation.cpp:293-389`).

The game collection authority says a transfer arrival restores carried gameplay
state verbatim except arrival grace and named client-only/shield exceptions
(`Projects/BrokenEngineSandbox/Source/Frame/Collections/AGENTS.md:13`). The
Player authority requires navigation state and cached steering to remain
shared-state-driven (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/AGENTS.md:6-18`).
The source comment at `Players.cpp:477-480` instead says the waypoint sequence
restarts on cross-frame transfer. This is an intended-behavior contradiction,
not a selected fix.

## Controlling contract and invariant

The controlling contract is the transfer-verbatim rule and the Player
navigation invariants cited above, together with the shared `TransferData`
payload contract (`Projects/BrokenEngineSandbox/Source/Frame/StatusChange.h:90-109`).
The unresolved invariant is whether wanted/AI/destination/nav-mode/waypoint
state is carried across a normal cross-cell Player transfer or intentionally
reinitialized under an explicit documented exception. Any selected rule must
be client/server and replay deterministic because these are shared state.

## Boundary and impact

The open boundary is Player transfer payload construction, serialization, and
destination materialization. It includes the shared CRC-visible navigation and
aim fields and any transfer marker needed to distinguish carried values from
fresh spawn defaults. It excludes genuine Player spawn behavior and unrelated
fire/health/identity validation.

Under a verbatim interpretation, every cross-cell arrival currently retargets
aim, discards cached steering and island destination, and restarts navigation.
That changes shared navigation state and replayed transfer outcomes even when
client and server perform the same defaults. Under an intentional restart
policy, the authority and source comment must be reconciled so the behavior is
not an undocumented exception.

## Open choices

These alternatives are recorded for a future decision; none is selected here.

1. **Carry the omitted gameplay state.** Extend the transfer representation and
   producer/destination mirror for wanted direction, cached AI direction,
   island destination, navigation mode, and waypoint state. Define the wire,
   replay, save, version, and transfer-marker consequences before changing
   shared layout.
2. **Make restart authoritative.** Clarify the transfer contract and Player
   authority to name navigation/aim reset as an intentional exception, then
   define the exact reset values and when fresh defaults are selected. Ensure
   the source comment and all client/server/replay consumers follow that rule.

The decision must not infer transfer-versus-fresh state from zero or a
non-positive carried value; the collection authority requires an explicit
transfer marker for that distinction.

## Decisive questions and acceptance evidence

- Is the broad verbatim transfer rule normative for the omitted Player fields,
  or is the local restart comment an intentional gameplay policy requiring an
  authority update?
- If fields are carried, what current-format payload/version and
  client/server/replay compatibility rules are allowed, and which fields are
  CRC-visible versus client-only?
- If fields restart, what exact mode, direction, waypoint, and destination
  values apply at arrival, and how are they distinguished from a genuine spawn?
- Can a focused cross-cell transfer scenario compare all omitted fields before
  and after materialization and prove identical client/server CRCs and replay
  outcomes under the selected rule?
- Do normal spawn, transfer, reconnect, save/load, and navigation-throttle
  paths retain their independent contracts?

The eventual executable Plan is expected Tier 3 because either choice can
change shared transfer serialization, deterministic navigation state, and
client/server/replay compatibility. Until the intended policy is selected, no
source or wire change is authorized.

## Provenance

- Frozen source candidate: `CPT/shard-0049/003`.
- Frozen consolidated index: `Temp/CppPlanTraceAudit/80896f33661aaab99cf180a96db54600099be652/consolidated-index.md`.
- Existing Player transfer, timer, and Blaster Plans were searched; none owns
  this intended-verbatim-versus-restart navigation/aim policy contradiction.
- No source, wire, replay, or scheduler change is part of this investigation.
