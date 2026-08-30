# Deterministic agent dispatch and verification

Status: Open investigation; no implementation decision has been made.

Area: Agents / dispatch, routing, and verification

Record type: Non-executable reference material. This document intentionally
has no scheduler metadata and is not a Plan input.

## Context

The post-landing review of commit
`68d0840d1a0a8b6e692e3d1fcaab27c330ae812c` found a measured duplicate locator
operation costing 157.511s and exposing 832,436 tokens, plus three routing
violations in the session's model-routing evidence. This record owns those
four findings as one open decision area because they all concern who may claim
a concern, how a route is selected, and how the result is typed. The exact
three violation rows must be reconstructed from the authoritative dispatch and
return evidence in their eventual review; this record does not invent labels
or transcript details.

The current repository has separate role/model guidance in root `AGENTS.md`,
delegated handoff requirements in `.agents/references/subagent-reporting.md`,
execution-model measurement and verdict rules in
`.agents/skills/next-plan-review/references/measurement.md`, and headless
review routing in `.codex/codex-review.ps1`. `/verify-changes` publishes a
natural-language result contract, while finalizer artifact revalidation is a
separate concern recorded by
`Documents/Plans/Agents/RemoveFinalizeVerificationArtifactRevalidation.md`.
The finalizer change must not be reintroduced by this investigation.

## Scope and open questions

The investigation should assign stable concern IDs to the duplicate locator
and each of the three routing violations, then prove for each one the parent
dispatch, exclusive ownership claim, requested role/type, canonical role,
configured model/effort, actual executor/model/effort, fallback, result, and
measured cost. A requested role or configured mapping is intent evidence only;
the current measurement contract requires an evidence chain to the actual
executor and outcome.

Investigate a repository/host dispatch broker that issues one route per
concern and rejects a second owner; concern IDs and exclusive ownership that
make duplicate work unrepresentable; one canonical role mapping for planning,
review, implementation, research, locate/build/mechanical work and fallback;
typed route receipts that carry requested, canonical, configured, actual, and
fallback route evidence; and a cross-client typed verification result that
can replace natural-language artifact parsing at the manager boundary.

The verification question is deliberately separate from finalizer
revalidation. A future typed verification result may be consumed by the
manager/verification workflow, but it must not restore prompt/output parsing
in `Show-FinalizeApprovalReview.ps1` or make landing scripts reparse reviewer
prose. A prose-only reminder to search for duplicates is not an acceptable
replacement for an ownership or route mechanism.

## Mechanisms to compare

The comparison must use the same criteria for every mechanism. Removal or
consolidation into an existing owner wins a tie; adding a new rule is the last
choice.

| Mechanism | Invalid-state prevention and enforcement | Latency/tokens | Failure recovery | Added ceremony |
| --- | --- | --- | --- | --- |
| Repository/host dispatch broker | A single broker can reject duplicate concern ownership and emit one canonical typed route; enforcement is strong if all dispatches cross it. | One broker round trip and a small typed receipt; it may save the 157.511s/832,436-token duplicate when it prevents the second route. | A typed unavailable/occupied result can return ownership to the manager without starting work; recovery must define stale-owner release. | New broker ownership and host integration are medium/high; consolidation into an existing dispatch boundary is preferred. |
| Host integration/plugin | The host can enforce role/model/effort selection before starting a child and produce cross-client evidence. | Potentially low per-dispatch cost when already in the host; plugin discovery/version or unavailable-host fallback can add cost. | Host failure needs a deterministic fallback or a block; a silent local fallback would recreate the routing gap. | Plugin lifecycle and deployment are high ceremony unless the host already exposes the required typed seam. |
| Deterministic preflight | A manager-side classifier can reject duplicate IDs and invalid role mappings before dispatch, but every route must be forced through the preflight. | One bounded preflight adds small latency/tokens and avoids starting doomed or duplicate work. | A rejected route is easy to retry after the owner/result is corrected; preflight cannot recover a race unless ownership is reserved atomically. | Low/medium if an existing inventory or dispatch owner can carry it; no new reminder prose. |
| Post-hoc lint | It can detect duplicate or mismatched routes after the fact but cannot prevent the duplicate work or invalid child from starting. | No critical-path cost, but it preserves all wasted agent time and exposed tokens. | It can report a durable finding, but repair requires a new dispatch or manual correction and cannot undo consumed work. | Low ceremony, weak enforcement; admissible only if stronger mechanisms are impossible and its residual is explicit. |

The investigation must also determine whether these mechanisms can share one
typed route/verification receipt across Claude children and headless Codex
reviewers without making a transcript path, home path, or raw prose part of a
tracked contract.

## Critical files

- `AGENTS.md:33-46,78-85` — role/model mapping, delegation depth, review
  routing, and landing verification ownership.
- `.agents/references/subagent-reporting.md:1-80` — task brief, typed/known
  handoff fields, executor evidence, and independent-review boundary.
- `.agents/skills/next-plan-review/SKILL.md:100-153,211-258` — routing
  inventory, allowed evidence chain, and enforcement priority.
- `.agents/skills/next-plan-review/references/measurement.md:47-104` —
  duplicate-work and requested/configured/actual route verdict rules.
- `.codex/codex-review.ps1:1-31,75-180` — current headless review launch,
  model/effort pins, retry, and structured receipt seam.
- `.agents/skills/verify-changes/SKILL.md:86-168` — current typed acceptance
  table boundary and natural-language result contract to be evaluated.
- `.agents/skills/finalize-changes/scripts/Show-FinalizeApprovalReview.ps1`
  and its receipt gates — explicit negative boundary: do not restore removed
  artifact revalidation.

## Open decisions and acceptance evidence

- Which existing repository or host owner can issue an exclusive concern ID
  without adding a second dispatch system? The answer must identify the exact
  call boundary, owner, lifecycle, and stale-owner recovery.
- What canonical mapping determines role, model, effort, and fallback before
  a child starts, and what typed receipt proves the actual route rather than
  merely requested intent? Each of the four finding IDs must map to one route
  and one owner with no duplicate claim.
- Can the same typed verification result represent a Claude child, a headless
  Codex review, and the manager's final `/verify-changes` PASS while retaining
  full baseline/head binding and residuals without parsing natural-language
  prompt/output artifacts?
- Which mechanism wins the comparison on invalid-state prevention,
  host integration/plugin cost, deterministic preflight cost, post-hoc lint
  recovery, latency/tokens, and added ceremony? Removal/consolidation wins
  ties, and a reminder-only rule is not a route.
- What exact future interface and repository/host ownership should one
  executable Plan implement, and where is its Tier-3 boundary if it changes
  dispatch trust, cross-client verification, or landing controls?

Acceptance for this investigation is a durable evidence matrix with the four
finding IDs, source route evidence and costs, a mechanism comparison using all
criteria above, one selected owner/interface for the eventual Plan, and an
explicit Tier-3 boundary. No source, script, skill, receipt, scheduler, or
landing behavior is authorized by this record.

## Risk tier and invariants

This record is non-executable and remains outside scheduler state. Any future
implementation is Tier 3 because it changes manager/worker dispatch ownership,
cross-client verification trust, or landing controls. Until exact interfaces,
ownership, failure recovery, and acceptance evidence are selected, no Plan is
decision-complete.

The eventual design must preserve one concern owner, authoritative actual-route
evidence, full baseline/head verification binding, independent review context,
and the finalizer artifact-removal boundary. It must not rely on transcript or
home paths, raw reviewer prose, or a prose-only duplicate-search obligation.

## Notes

The user priority for this investigation is fixed as removal, then
consolidation/determinization, then adding a rule. Later work should map the
four finding IDs to this record rather than create a second owner for the same
evidence.
