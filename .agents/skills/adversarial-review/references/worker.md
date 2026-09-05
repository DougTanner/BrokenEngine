# Adversarial Review Worker

The review steps the dispatched reviewer runs. Triggers, required inputs, and
the handoff form live in [`../SKILL.md`](../SKILL.md).

## Steps

1. Read, for each authorized hypothesis, the callers, consumers, schemas,
   instructions, generated outputs, or sibling paths it needs; diff-only reading
   is insufficient. Done when every hypothesis has its dependent sites read.
2. Test the contract appropriate to the artifact. For code and shaders, trace
   logic, integration, lifetime, threading, determinism, edge states, and build
   reachability. For scripts, project metadata, schemas, and data, trace inputs,
   state changes, failure handling, compatibility, and consumers. For skills,
   plans, workflow, and documentation, trace discovery and invocation policy,
   executable instructions, authority boundaries, acceptance semantics, links,
   and contradictions with governing instructions. Done when every changed
   artifact type in the list has had its own contract traced.
3. Attempt to refute each candidate finding against guards, established
   preconditions, and governing invariants, and keep only those that survive.
   Done when each kept finding cites a `file:line` read in this review and names
   a reachable in-scope failure the change introduced or newly exposed — input
   or state leads to a wrong outcome, a violated governing contract, or a failed
   approved acceptance criterion.
4. Emit one single-claim request per
   [`/verify-external-claims`](../../verify-external-claims/SKILL.md),
   `## Inputs`, for every kept finding that depends on a
   non-obvious external API, language, specification, or library claim. Done
   when each such finding has its request row, or no finding depends on one.

## Rules

- Do not edit files, run state-changing commands, or implement fixes; shared
  reviewer conduct is in
  [`../../../references/subagent-reporting.md`](../../../references/subagent-reporting.md).
- Review logic and correctness; leave style to the artifact's domain review.
- Report proven pre-existing or out-of-scope defects in `Residuals` instead of
  fixing or expanding into them, while in-scope structural acceptance failures
  stay findings.
- Exclude style issues and diagnostics a prescribed compiler, validator, or
  static check directly catches.
- A pending external verdict makes the review `NEEDS_ACTION`, and the claim is
  not confirmed until that verdict returns.
