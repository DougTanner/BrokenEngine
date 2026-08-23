# Model transition evaluation

## Question

Should Broken Engine change the model or host used for planning, implementation,
review, or headless execution, and what evidence would distinguish a semantic
improvement from a loss of deterministic workflow parity?

## Current evidence

The root `AGENTS.md` role table names the planner, reviewer, implementer,
locator, and builder routes. `.codex/agents/*.toml` records Codex role pins,
and `.codex/codex-review.ps1` pins the headless review route. The
`/next-plan-review` measurement contract requires requested/configured/actual
model and effort evidence and states that config or CLI pins are intent rather
than runtime proof. No transition corpus, semantic oracle, saturation rule, or
cost comparison is owned by these documents today.

## Options to compare

1. Keep the current route and improve only provenance. This minimizes change,
   but leaves any model-quality question unanswered.
2. Evaluate one candidate transition with a fixed cross-role corpus and an
   explicit semantic oracle, then adopt it only after saturation and cost
   evidence. This yields a focused comparison but requires labeling and
   host-identity capture.
3. Run old and candidate routes in parallel for a bounded sample, comparing
   semantic outcomes and workflow-contract conformance separately. This gives
   direct parity evidence but doubles execution cost and may expose host-only
   differences.

## Evidence to collect

- A representative corpus covering planning, implementation, review, build,
  diagnosis, and handoff tasks, with a versioned boundary and no secret or
  transcript content.
- A semantic oracle that defines acceptable outcomes and a saturation rule
  that says when more samples are unlikely to change the decision.
- Authoritative actual model/effort/host identity from launch or service
  receipts, separately from requested and configured pins.
- Cost and latency evidence using the same workload, configuration, service
  tier, and retry policy; report active work, wall time, and excluded waits
  separately.
- A second, deterministic-parity record for routing, receipt shape, privacy,
  and handoff contracts. Do not use semantic scores as proof of parity.

## Promotion criteria

Promote to a Tier 3 implementation Plan only after the corpus, oracle,
saturation rule, actual-identity source, and cost method are selected and
reproducible. Write one Plan for the semantic model transition and a separate
Plan for deterministic routing/receipt parity when their critical files or
acceptance evidence differ. Do not promote a transition from a config-only
comparison or an unbounded anecdotal sample.

## Non-goals

- No model switch, role-table edit, receipt change, score, or scheduler gate is
  authorized by this investigation.
- No credential, transcript, home-path, or external-service mutation belongs in
  the corpus or evidence.

## Notes

This document remains an option comparison until the promotion criteria are
settled with evidence. A semantic improvement and deterministic workflow parity
answer different questions and must not be collapsed into one verdict.
