# Plan Audit Worker

The audit steps and the judgment rules the dispatched reviewer runs. Triggers,
inputs, and the handoff form live in [`../SKILL.md`](../SKILL.md).

## Steps

1. Read the complete plan, applicable `AGENTS.md` files, and every cited code
   region. Done when each of those has been read.
2. Resolve the plan's citations with the bundled
   `scripts/Test-PlanCitations.ps1`; never reconstruct its lookups inline:

   ```powershell
   pwsh -NoProfile -File .agents/skills/plan-audit/scripts/Test-PlanCitations.ps1 <plan path>
   ```

   It writes nothing and returns one `broken-engine-plan-citations/v1` JSON
   object on stdout: a record per backtick citation matching its grammar, with
   path existence, line existence, and a short excerpt. Done when that JSON
   object is in hand.
3. The result caps how many records it returns, so read `truncated` and
   `omittedCount` and resolve any omitted citation through the direct reads step
   1 already requires; the prohibition above is against re-deriving what the
   script already returned, not against your own reading. Done when no omitted
   citation is left unresolved.
4. It renders no verdict, and a token it ignores is never a finding. An
   unresolved citation is a lead you must investigate — the plan may create that
   file, or the line may have moved — and never an automatic finding. Done when
   every unresolved citation has been investigated as a lead.
5. Anchor every search to a plan claim: callers, mirrors, cited-type headers, or
   affected-site hunts. Do not explore unrelated plans/subsystems. Done when
   every search traces to a plan claim.
6. Verify structural assumptions, call sites, mirrored client/server paths,
   ownership, data layout, frame phase, threading, determinism, serialization,
   build wiring, and runtime verification where relevant. Done when each
   relevant item is verified against the repository.
7. When the plan puts concurrent writers on one target, first make it prove the
   requirement truly needs a single canonical target; when it does not, require
   partitioned ownership with independently published results; when it does,
   require a structural coordination mechanism the plan proves already exists or
   explicitly introduces and verifies, and trace ordering, visibility, progress
   after a failure, serialization/CRC effects, and client/server publication. An
   instruction that writers take turns is not synchronization evidence. Done
   when the single-target requirement is proven or partitioned ownership is
   required, and, where it is proven, that mechanism and each traced effect are
   settled.
8. Hunt unresolved options, hidden behavior changes, contradictions, magic
   defaults, undeclared invariant exposure, missing affected locations,
   ungrounded requirements or checks, scope that duplicates an existing
   mechanism, and any file the plan's own `## Coordination` section obliges the
   implementer to edit yet its scope contract omits. Done when each of those has
   been hunted.
9. Require `## In scope` and `## Out of scope` headings that are each concrete
   enough to test a diff region against; scope naming a file without its regions
   is a finding. Done when both headings are demanded of the plan.
10. Take heading presence from step 2's citation check's heading-presence result
    and judge concreteness yourself. Done when each heading is either verified
    or reported as a finding.
11. Ground corrections in user intent, repository contract, or necessary
    integration. Done when every correction names its grounding.
12. Prefer reuse, narrower scope, missing propagation/verification, or
    replacement of an invalid step; otherwise report the user decision. Done
    when each correction takes one of those forms or reports that decision.
13. Scale depth to the plan. Done when the depth matches the plan's size.
14. Keep a one-file refactor light; inspect a new subsystem across its full
    integration surface. Done when the inspected surface matches the plan's
    kind.
15. Build bidirectional traceability. Every requirement and exposed invariant
    must map to concrete implementation steps/sites and decisive checks; every
    implementation step/site and check must map back to a stated requirement or
    invariant. Done when both mappings are built.
16. Report missing links, orphan work, and checks that cannot observe their
    claimed outcome. Done when each is reported.
17. For a `/next-plan` invocation, compare the complete current plan to the
    execution card and session baseline. Done when that comparison is made.
18. Verify that every field of the card template in
    [`../../next-plan/SKILL.md`](../../next-plan/SKILL.md) `## Handoff`, plus
    the plan's unresolved decisions, agrees with current repository evidence.
    Done when each of those is checked against that evidence.
19. Report any mismatch as a finding for the manager; do not manufacture
    authority artifacts or another approval gate. Done when every mismatch is
    reported.
20. Audit the proposed execution card against the risk-tier definitions in root
    `AGENTS.md`, classifying at the highest applicable tier. Done when that
    comparison is made and any mismatch reported.
21. Judge these card fields: every trigger named concretely, the
    out-of-scope boundary enforceable, required and conditional roles fitting
    the actual file types and risks, and each acceptance criterion carrying an
    initially decisive check and expected result, with a named independent
    signal for any duplicate check. Done when each of those is verified.
22. If evidence proves a criterion unverifiable in the available environment,
    report a must-fix finding with an achievable replacement or a meaningful
    user decision; do not defer it to an end-of-session waiver. Done when every
    such criterion carries that finding.
23. Route changed C++ to `/repo-code-review`, changed GLSL to `/glsl-review`,
    and a shared CPU/GLSL dual-language header to both. Do not route
    documentation, workflow, style-only, or project-membership-only bytes to
    `/repo-code-review`. A reviewer may recommend escalation with evidence but
    may not silently lower the tier. Done when changed C++, changed GLSL, and
    any dual-language header are routed as stated.

## Rules

- Assume the supplied plan is flawed and verify it against the current
  repository. Report only a concrete problem or improvement the manager
  actually has to resolve.
- A plan's own decision declarations are hypotheses to verify like any other
  statement it makes, except as
  [`../../../references/authority-order.md`](../../../references/authority-order.md)
  provides.
- Load the file or claimed-plan bytes once and retain that immutable snapshot
  for the whole audit. Do not follow later edits. If an input the `## Inputs`
  section of [`../SKILL.md`](../SKILL.md) lists is missing, return `BLOCKED`
  with the exact missing input rather than auditing a moving draft.
- Return the audit inline as the next role's decision input, not final evidence.
  If constrained, narrowed, or interrupted, return findings gathered so far in
  the standard format immediately; never return silence.
- When a proposed finding depends on a non-obvious external API, language,
  specification, or library fact, do not treat it as established: request it per
  [`../../verify-external-claims/SKILL.md`](../../verify-external-claims/SKILL.md),
  `## Inputs`, naming the dependent `PA-F-###`. A pending
  verdict makes the audit `NEEDS_ACTION`, not a confirmed finding.
- Do not edit any repository file, run `/agent-harness`, or interview the user.
  The manager owns all judgment; delegation conduct is in
  [`../../../references/subagent-reporting.md`](../../../references/subagent-reporting.md).
