# Repository C++ Review — Worker

Steps and rules for the dispatched reviewer. Inputs, the handoff form, and the
report template live in [`../SKILL.md`](../SKILL.md).

## Steps

1. Read the changed regions in full-function context, their applicable
   `AGENTS.md`, and the producers, consumers, callers, and mirrored paths needed
   to trace the declared contracts. Diff-only inspection is insufficient. Done
   when every declared contract has been read in its own source.
2. Search changed signatures, semantics, enum values, layouts, ownership,
   guards, frame phases, and serialization identities across every affected
   site. Done when every affected site is identified.
3. Check substantial new logic against existing helpers and deliberate mirrored
   patterns. Done when each substantial new block has been checked against
   them.
4. Apply the relevant checks from [`checks.md`](checks.md). Turn a checklist
   concern into a finding only when a concrete changed path makes the failure
   reachable. Done when every check the changed paths reach has been applied.
5. Try to disprove each candidate finding against guards, caller preconditions,
   lifecycle, and current repository contracts. Done when every candidate is
   dropped or survives disproof.
6. Report the smallest correction, without implementing it. Done when every
   surviving finding carries one correction.
7. Emit a single-claim [`/verify-external-claims`](../../verify-external-claims/SKILL.md)
   request (`## Inputs`) for every candidate finding that
   depends on a non-obvious external API, language, specification, OS, or
   library fact. Done when each such candidate carries one request.
8. Withhold every such candidate from the confirmed findings until the caller
   receives `VERIFIED` evidence. Do not browse or present that fact as
   confirmed. Done when the review is `NEEDS_ACTION` while any verdict is
   pending.
9. Measure the changed `.cpp` files in one batched run:
   `pwsh -NoProfile -Command "& '.agents/scripts/Measure-Tokens.ps1' -Path
   'a','b','c' -Json"`. Use `-Command`, not `-File`: under `-File` the
   comma-separated list binds as one filename and the run fails. Done when
   every changed `.cpp` file has one measurement.
10. Record a size observation only when the changed region exposes a concrete
    cohesive split. Return it as a manager follow-up candidate; never reduce the
    file or prescribe an inline reduction during review. Done when each
    qualifying file carries one observation and no reduction is prescribed.
11. Return the report and the conditional `/update-vcxproj` trigger. Never read
    or grep project XML in this review. Done when the report states the trigger
    or `none`.

## Rules

- Do not edit, run commands that change state, or implement fixes; shared
  reviewer conduct is in
  [`../../../references/subagent-reporting.md`](../../../references/subagent-reporting.md).
- For a Tier-2+ change, also run the scope authorization, minimality, and KISS
  passes in [`../../../references/scope-authorization.md`](../../../references/scope-authorization.md)
  over the changed C++ regions this review already covers. Report the result in
  the `Scope:` field.

### Checks index

Full text in [`checks.md`](checks.md); apply only the checks the changed paths
reach.

- General logic and ownership — every changed region.
- Type and domain modeling — changed parameters, discriminators, or value types.
- Changed comments — a comment the change edited.
- Trust boundaries and failure channels — externally controlled data, network,
  file, or `.pack` paths.
- Allocation-tracked paths and logging — Engine/Game code under allocation
  tracking, and changed logs there.
- ASSERT behavior — an added or changed assertion.
- Collections, persistence, and identity — `Collection<T>` columns and other
  persisted or wire-visible layouts.
- Determinism, threading, and frame phases — CRC-fed or replay state,
  dispatched workers, and frame-phase data.
- XMVECTOR W applicability — a changed `XMVECTOR` that represents a position,
  direction, or color.
- Integration, layering, and build affinity — changed signatures, enums, shared
  headers, guards, or file membership.
- Public state and forwarding APIs — a changed interface with a one-line
  accessor or pass-through.
- Repository patterns — new `bool` members or parameters, and new
  standard-library or third-party includes.
- Completeness and duplication — incomplete integration and substantial new
  near-copies.
