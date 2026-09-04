# Reconstruct and assess

Build a chronological evidence table covering the objective, tier/card/approval,
implementation, propagation, checks, domain and conditional reviews, fix loops,
build/harness work, reconciliation, landing approval, and landing. Include Plan
selection, final preparation, and claim/release only when they occurred.
Locate the governing user objective and, when present, the latest approved plan
from transcript evidence. The latest approved plan governs conformance. Without an
approved plan, the governing scope is the user objective plus recorded acceptance
statements. Execution cards corroborate process and timeline only in that
fallback; they cannot impose scope. When a claimed Plan exists, also inspect its
executable metadata. Map the governing scope and acceptance criteria to the final
diff and verification. Source every event to a session/time or Git/WorktreeCli
result.

Report wall-clock span, explicit user/external pauses, and approximate active
elapsed time. Builds, harness work, debugging, and review are active work.
Required implementation and landing approvals are not waste; flag only extra
loops or unexplained waits.

Before assessing concerns 4, 5, and 6 below, read
[`measurement.md`](measurement.md) for the main-session token-efficiency
measurement, the control-work classification and measurement calculus, and the
execution-model routing rules those three concerns apply.

This audit does not code-review the implementation, infer defects or failure
modes, claim correctness, or request extra testing to establish correctness.
It assesses factual governing-scope conformance, solution minimality, and
workflow process evidence only.

Assess in this order:

1. Governing-scope conformance: `aligned` when all governing work is represented
   with no meaningful unauthorized scope; `partial` when only part is implemented
   without meaningful contradiction; `divergent` when landed scope meaningfully
   contradicts, substitutes for, or exceeds governing scope; or `unverified`
   when evidence is insufficient. This is factual scope mapping, not a
   correctness assessment. Treat required affected-site changes as propagation,
   not scope expansion.
2. Solution minimality: `minimal` when no concrete simpler complete alternative
   is identified; `mixed` when localized removable complexity exists while the
   core approach still matches the size of the change; `overengineered` when the core approach
   or a meaningful portion is more complex than a concrete scope-conforming
   alternative; or `unverified` when evidence is insufficient. Report an issue
   only when it names landed complexity and a concrete simpler alternative that
   preserves governing scope, fixed decisions, and required invariants. Candidate
   signals include needless abstraction, indirection, generalization,
   configuration or extension points, duplicate mechanisms, compatibility paths,
   new subsystems, or scope expansion. Do not count required affected-site
   propagation, invariant preservation, or mandated workflow controls as
   overengineering; do not make taste-only findings.
3. Workflow coverage: report each required review or testing step only as
   `occurred`, `missing`, or `unverified`. This is process compliance evidence,
   not an assessment of adequacy or implementation correctness.
4. Token efficiency: what entered the main session's context and what each
   entry bought, measured and judged by the six checks in `measurement.md`
   `## Measure main-session token efficiency` — script-able instructions, public
   skill surface, brief assembly, handoff volume, direct main work, and
   repeats. Every finding carries the emitter, measured chars, and replacement
   that section requires.
5. Execution-model routing: inventory and verify every direct child/headless
   attempt using the concern-first classification, allowed evidence chains,
   and verdict rules in `measurement.md`.
6. Control-work share: classify and measure active agent-time using the rules in
   `measurement.md`; distinguish required controls from removable
   candidates before treating the burden as waste.
7. Process overhead: reconcile count, landing-phase active time, duplicate
   validations, and unchanged-input rebuild/review/verification.
8. Isolation and landing: wrapper/claim evidence when applicable,
   meta-tool failures, linear-history and parent proofs, conflicts, and
   claim release when applicable.
9. Speed: complexity-adjusted active time, productive costs, and avoidable
   approval or external waits.

Never label repetition from identical landed bytes alone. A repetition or
control-removal recommendation requires proof that code, external state,
evidence inputs, and governing contract were unchanged, plus measured cost,
signal gained, and safety risk of removal. A control not firing once is not
removal evidence. Prioritize from demonstrated impact and risk; no repetition,
extra reconcile, or elapsed-time threshold is automatically P0.

Carry an improvement forward only when this landing's evidence supports it
directly, the signal is durable rather than one session's stylistic preference,
and it would plausibly change a future decision. A correction also names the
existing owning skill, script, or document and the concrete decision rule that
owner is missing or leaves ambiguous; an owner that states the broad principle
but does not decide the case still qualifies, and a candidate with no such gap
does not. Recommend a genuinely unowned capability as a new capability rather
than forcing it into a correction to an owner that does not own it.

When the evidence proves a recurring failure, propose the highest enforcement
that evidence supports, in this order: make the invalid state unrepresentable;
add deterministic validation, a lint rule, or a banned-API rule; centralize the
behavior in one canonical helper; add a runtime check; state it in prose only
when the judgment cannot be encoded.
