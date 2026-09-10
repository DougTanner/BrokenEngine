This file, `.agents/references/risk-tiers.md`, defines the Change Workflow risk tiers; a worker that must classify a change reads it.

### Risk tiers

Classify the whole change at the highest applicable tier before implementing:

- Tier 1 — mechanical: documentation, style, project membership, or local behavior-preserving work with no public signature or invariant exposure.
- Tier 2 — scoped behavior: one subsystem's runtime or tool behavior, excluding changes to determinism/CRC, wire/protocol, serialization or data layout, save/replay compatibility, threading, or trust boundaries — a change to one of those surfaces alters what it computes, carries, or trusts; adding or tightening checks inside one unit at an existing boundary, with the format and the trust unchanged, is that unit's scoped behavior — build/bootstrap coordination, and independently owned cross-subsystem integration.
- Tier 3 — invariant/integration: any surface excluded from Tier 2 above, build/bootstrap coordination that can block other sessions, or a change spanning independently owned subsystems.

A reviewer may escalate the tier when the changed bytes expose a higher-risk surface. Resolve meaningful ambiguity with the user before proceeding.
