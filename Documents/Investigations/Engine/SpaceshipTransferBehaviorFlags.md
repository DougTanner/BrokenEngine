# Spaceship transfer behavior-flag policy

Status: Open investigation; no implementation decision has been made.

Area: Engine

Record type: Non-executable reference material. This document intentionally has
no scheduler metadata and is not a Plan input.

Audit source: `CPT/shard-0050/001` in the frozen C++ Plan Trace Audit.

Frozen audit commit: `80896f33661aaab99cf180a96db54600099be652`

## Finding under investigation

`SpaceshipsPostRender::Transfer` serializes position, direction, velocity,
alignment, health, blaster timer, and delta rotation, but no Spaceship behavior
flags (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/Spaceships.cpp:369-397`).
`TransferData::SharedMembers` likewise has no flag field
(`Projects/BrokenEngineSandbox/Source/Frame/StatusChange.h:90-109`), and
`SpawnTransfer` passes no flags to the destination
(`Projects/BrokenEngineSandbox/Source/SpawnTransfer.cpp:17-27`).
`SpaceshipsPostRender::Spawn` initializes every arriving row's flags to zero
(`Spaceships.cpp:552-594`, especially `:587`).

The omitted flags include `kFleePlayer` and `kReturnToIslandCenter`, which
`ComputeSteering` sets/clears with distance hysteresis and which `Update` uses
to choose acceleration and steering
(`Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/SpaceshipsNavigation.cpp:45-117`;
`Spaceships.cpp:629-652`). They are shared PostRender members included in the
frame CRC (`Spaceships.h:140-152`; `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:702-714`).
The game collection authority says transfer arrivals restore carried gameplay
state verbatim except named arrival-grace and client-only/shield exceptions
(`Projects/BrokenEngineSandbox/Source/Frame/Collections/AGENTS.md:13`), but it
does not state whether these behavior flags are an exception.

## Controlling contract and invariant

The controlling contracts are the transfer-verbatim rule,
`Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/AGENTS.md:5-12`
for hysteresis steering, and the shared transfer payload/CRC contract. The
unresolved invariant is whether non-transient flee/return flags cross a cell
boundary, or whether the destination is required to recompute them before its
first behavior decision. Any selected policy must preserve client/server
parity and the existing transient `kTransfer`/`kExploding` semantics.

## Boundary and impact

The open boundary is Spaceship transfer payload construction and destination
spawn initialization. It includes CRC-visible behavior flags, any explicit
transfer marker, and the first post-arrival steering decision. It excludes
arrival-grace targeting (recorded separately), health validation, and genuine
fresh-spawn defaults.

When a ship transfers inside a hysteresis band, zeroing the flags can change
acceleration or steering on the arrival tick relative to the source state.
Because the flags and resulting behavior are shared PostRender state, the
choice can alter CRC and replay outcomes even if both endpoints currently
apply the same reset.

## Open choices

These alternatives are recorded for a future decision; none is selected here.

1. **Carry non-transient behavior flags.** Add the required transfer
   representation and producer/destination mirror for `kFleePlayer` and
   `kReturnToIslandCenter`, preserving transient flags and append-only wire or
   replay rules. Define whether a transfer marker is needed to distinguish
   carried flags from fresh-spawn zero defaults.
2. **Recompute behavior on arrival.** Document the two hysteresis flags as an
   explicit transfer exception and define the exact ordering that recomputes
   them before the first owner-side behavior decision. Keep transient transfer
   and explosion flags out of the carried state.

The choice must explain why arrival grace, hysteresis, transfer publication,
and destination first-dispatch timing do not produce an unintended one-tick
behavior discontinuity.

## Decisive questions and acceptance evidence

- Does “verbatim gameplay state” include the two hysteresis flags, or are they
  derived state that must be recomputed on arrival?
- If flags are carried, what payload/version and transfer-marker changes are
  allowed, and how are CRC/replay/client/server compatibility preserved?
- If flags are recomputed, which source fields and phase order determine the
  first destination decision, and how is the transfer exception documented?
- Can a focused transfer scenario place a ship in both hysteresis bands,
  compare source/destination flags and first steering output, and prove the
  selected client/server CRC and replay result?
- Do `kTransfer`, `kExploding`, arrival grace, delta rotation, and fresh spawn
  defaults retain their current independent behavior?

The eventual executable Plan is expected Tier 3 because the decision crosses
shared transfer representation, deterministic steering/CRC state, replay, and
client/server parity. Until the intended policy is selected, no source, wire,
or replay change is authorized.

## Provenance

- Frozen source candidate: `CPT/shard-0050/001`.
- Frozen consolidated index: `Temp/CppPlanTraceAudit/80896f33661aaab99cf180a96db54600099be652/consolidated-index.md`.
- Existing Spaceship health and transfer-related Plans were searched; none
  owns this behavior-flag carry-versus-recompute decision.
- No source, wire, replay, or scheduler change is part of this investigation.
