# Deep Analysis Pipeline Worker

The phase steps and judgment rules the pipeline runs. Triggers, the target
input, and the report form live in [`../SKILL.md`](../SKILL.md).

## Contents

- [Steps](#steps)
  - [Execution context](#execution-context)
  - [Phase 0: Scoped Metric Evidence](#phase-0-scoped-metric-evidence)
    - [Triage Gate](#triage-gate)
  - [Phase 1: Architecture Shape](#phase-1-architecture-shape)
  - [Phase 2: In-Function Mechanics](#phase-2-in-function-mechanics)
  - [Phase 3: Findings Verification](#phase-3-findings-verification)
  - [Phase 4: Route, Verify, and Land](#phase-4-route-verify-and-land)
- [Rules](#rules)

## Steps

### Execution context

1. Run from the main invoking context. If repository policy prevents the current
   context from dispatching, return a main-context dispatch requirement instead
   of approximating either analysis inline. Done when the run holds a context
   that may dispatch, or the dispatch requirement has been returned.
2. Invoke every skill and dispatch every role from the main context. Give each
   role a scoped manifest or report slice and forbid child delegation; schedule
   rounds within the host's available concurrency. Done when every dispatch
   carries its slice and that prohibition.
3. Read root and target-applicable `AGENTS.md` files before dispatch. Plan
   authoring needs the checkout for `create-follow-up-plans`. Done when those
   files are read.

### Phase 0: Scoped Metric Evidence

4. Before architecture analysis, invoke `code-quality-metrics` Snapshot once
   with the resolved relative-POSIX target, the same `Exact`, `Directory`, or
   `Recursive` scope mode, and the absolute checkout root:

   ```powershell
   pwsh -NoProfile -File .agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetrics.ps1 -Mode Snapshot -Target <resolved-relative-POSIX-path> -Scope <resolved-mode> -RepositoryRoot <absolute-checkout-root> -Phase0Hints -OutputPath <absolute-ignored-Temp-path>
   ```

   Done when exactly one Snapshot run has covered both.
5. Consume the digest's `profile`, `targetSelection`, `coverage`, and `hints`
   fields. Done when those four fields are in hand.
6. Read the retained full report's `current.targetMetrics` and
   `current.corpusMetrics` `structuralErosion`, `verbosity`, and
   `excessDecisions` values, because the digest carries no metric values, and
   keep all six for the final summary. Done when all six values are in hand.
7. Treat `excessDecisions` as net scope evidence: unchanged means only no net
   decision removal, never redistribution without source-diff evidence. Done
   when every `excessDecisions` reading is judged that way.
8. Treat an operational failure as a pipeline blocker: it exits `2` with
   diagnostics on stderr and no digest on stdout. Done when the run either
   produced a digest or its failure is reported as a blocker.
9. Treat every reported parse omission as an explicit metric residual, not a
   pipeline failure. Done when every reported omission is recorded as a
   residual.
10. Forward exactly the digest's `hints` to both native analysis phases as
    received; never reconstruct their selection, ordering, or truncation inline.
    Done when the hints are forwarded unchanged.
11. Retain corpus coverage and all parse-omission residuals for the final
    summary even when only target omissions are forwarded. Done when both are
    retained.

#### Triage Gate

12. When the run's stated trigger is solely a `structuralErosion` outlier, stop
    after Phase 0 unless at least one corroborating signal holds for the
    resolved target:

    1. `hints.targetOutliers.items` contains a bucket whose `metric` is
       `verbosity` with `totalCount` above zero (verbosity buckets exist even
       when empty);
    2. the digest's clone-group hints carry at least thirty SLOC summed over
       their `targetInstances[].sloc`;
    3. `pwsh -NoProfile -File .agents/scripts/Get-AnalysisManifest.ps1 -Path <resolved-target> -Extension .h,.cpp`
       — adding `-Recurse` only for Recursive scope — flags any in-target file
       `reduceFileCandidate`;
    4. the retained report's
       `current.targetMetrics.structuralErosion.numerator` is at least one
       thousand.

    Done when every signal has been evaluated and the stop-or-continue outcome
    is settled.
13. Judge those signals in context: a target reading above the corpus is not by
    itself evidence, because the corpus value is about 0.553 and a
    single-function file saturates at 1.0. Done when each evaluated signal
    carries that judgment.
14. Treat an explicit user direction to run the full pipeline regardless of
    metrics as overriding this gate. Done when such a direction, where given,
    has continued the run.
15. When that manifest command cannot run for the resolved target, treat that
    signal as failed and record its blocker as a residual. Done when the failed
    signal carries that residual.
16. On a stop, report the [`../SKILL.md`](../SKILL.md) `## Handoff` target and
    metric bullets, record the outcome as `triage skip` naming each signal
    evaluated, and run none of Phases 1-4, plan authoring, or the Debt Score.
    Done when the `triage skip` report is returned and no later phase has run.

### Phase 1: Architecture Shape

17. Invoke `external-architecture-review` natively through the normal skill
    surface with the resolved target and scope mode. Do not open a
    client-specific skill installation or reproduce its workflow here. Done when
    that review has returned.
18. Include the Phase-0 scoped hints as investigation evidence without expanding
    the target. Done when those hints reached that review and the target is
    unchanged.
19. Retain its scoped manifest, authorities, verified findings, external-claim
    residuals, and handoff note for Phase 2. Done when each of those results is
    retained.
20. Do not choose plan locations, filenames, groups, collisions, dependencies,
    or duplicate decisions at this stage. Done when no such choice has been made
    at this stage.

### Phase 2: In-Function Mechanics

21. Invoke `external-refactor-clean` natively through the normal skill surface
    with the same target and scope mode. Done when that analysis has returned.
22. Include the Phase-0 scoped hints and Phase-1 investigation paths as evidence
    to inspect, without expanding the original finding boundary. Done when that
    evidence reached the analysis and the boundary is unchanged.
23. Treat every forwarded `hints.highComplexityFunctions` item whose `path` lies
    in the resolved target as navigation evidence to inspect. Do not require a
    decomposition, a cyclomatic-complexity-ten outcome, or a threshold verdict
    from that hint. Done when every in-target item has been inspected as
    navigation evidence.
24. Keep hints outside the resolved target evidence-only; they never become
    checklist items. Done when no out-of-target hint became a checklist item.
25. For excess-decision evidence, prefer in-place deletion, merging, flattening,
    or existing dispatch. Extract only for independently meaningful
    responsibility, reuse, or a genuinely separate abstraction. Done when every
    excess-decision candidate takes one of those forms.
26. Treat a target decrease as proof of simplification only when the diff shows
    decision removal and the corpus shows no attributable offset elsewhere, or a
    separately evidenced structural benefit independently justifies extraction.
    Done when every claimed simplification meets one of those conditions.
27. Keep its file-size triage separate from ordinary findings. Every oversized
    file retains the explicit instruction `run /reduce-file <path>`; do not
    analyze it inline or silently convert it into a generic refactor item. Done
    when every oversized file carries that instruction.

### Phase 3: Findings Verification

28. After both reports complete, have the main context dispatch fresh `reviewer`
    roles in scoped slices. Done when every slice has been dispatched and has
    returned.
29. Keep reviewers findings-only: they may read the scoped source, authorities,
    and reports supplied by the main context, but must not write, rewrite,
    delete, score, or relocate a plan and must not delegate. Done when no
    reviewer wrote, scored, or relocated a plan, and none delegated.
30. For every candidate, require a verdict and evidence for:

    1. Correctness — cited path, symbol, behavior, and repository authority exist
       and support the claimed root cause.
    2. Benefit and safety — the proposed outcome is functional or structural,
       not cosmetic, pattern-breaking, or more risky than the proven debt.
    3. Actionability — the unmet acceptance criterion and smallest correction
       boundary are concrete enough for a plan author.
    4. External claims — non-obvious API, specification, license, maintenance,
       or ThirdParty propositions become verification requests for the main
       context to route through `verify-external-claims`; unresolved claims
       remain residuals.

    Done when every candidate carries all four verdicts with evidence.
31. Decide on reviewer evidence once in the main context: drop disproven
    candidates and retain verified caveats. Done when every candidate is dropped
    or accepted.
32. Keep oversized-file items labeled as `/reduce-file` follow-ups. Done when
    each such item carries that label.

### Phase 4: Route, Verify, and Land

33. Route every accepted candidate through `create-follow-up-plans`, passing
    oversized files with their required `/reduce-file <path>` instruction
    intact. Done when every accepted candidate is routed.
34. Follow the root `AGENTS.md` Change Workflow Verify the acceptance table and
    Verify and land steps through `finalize-changes` for acceptance and
    landing. Done when the applicable finalization outcome is known.

## Rules

- The Phase-0 Snapshot is a single capture analyzing the complete corpus and the
  resolved target together; never substitute separate corpus and target runs.
- `hints` arrives already ordered, truncated, and counted in four categories —
  target file and area outliers, target-intersecting clone groups, target
  high-complexity functions, and target skips — each stating its `total` and
  `emitted` counts, including zeroes.
- The forwarded hints are scoped evidence to inspect, never findings: they do
  not expand the original target, create Plans, alter the Debt Score, or replace
  source inspection.
- `CC > 10` remains navigation only, and structural erosion alone is navigation.
- One-shot sequential or threshold-driven extraction is metric-neutral.
- An accepted finding preserves its originating phase, symbols, evidence,
  invariant exposure, and unmet acceptance criterion.
- Analysis findings are pre-existing or out-of-scope debt, never permission to
  fix code in this workflow.
