# Execution Card

The field template every execution card carries. What the card is and when it
is drafted belong to the Change Workflow Definitions in
[`change-workflow.md`](change-workflow.md); `Invoke-PlanCitationsCheck.ps1` in
`/plan-audit` reports each absent field as `card.missingFields`.

## Template

```text
Execution card:
### What does this plan do?
<2-4 plain sentences>
### Why this is good for the codebase
<2-4 plain sentences: what prompted it, who uses the result, and how>
- Goal: <result>
- Out of scope: <boundary>
- Tier trigger: <trigger or none>
- Interfaces and invariants: <contracts>
- Acceptance checks: <check and expected observation; a criterion for new or
  fixed behavior names a check the unedited tree could not pass, and a bug-fix
  criterion (one stating a defect no longer occurs) also names its pre-change
  red run: an /external-diagnose-bug Reproducing signal, a run on a build of
  the unedited tree (a run needing a Local build takes the route in
  `.agents/skills/compile/references/runtime-data-mode.md` `## Mode selection`),
  or, for a defect in the instruction prose
  /progressive-disclosure-review reviews, a read at the session baseline commit
  that shows the defect unambiguously, citing the defective rule by path and
  line and the case it mishandles; a criterion compared against the unedited
  tree, such as a result that must stay unchanged, names that pre-change
  baseline run, which for that instruction prose may be a read at the session
  baseline commit; the card marks each red run on a build of the unedited tree
  and each pre-change baseline run other than a read at the session baseline
  commit `before the first edit`, the literal text the Change Workflow
  Implement and propagate step's pre-edit bullets key on to record that run
  before the first slice, kept whole on one line and placed on the first line
  of the check it marks so a line-based search finds every marked run; within
  a card the literal appears only as that mark, so a card with no marked check
  says so without it; when any file a slice may edit is a data input as
  `Test-AgentDataInputPath` in `.agents/scripts/AgentScriptCommon.psm1`
  reports it for the game project the check targets, counting as a data input
  a scope entry broader than a data-input root that can match one, such as
  `Engine/**` or `**/*.frag`, the marked line also carries the word Local and
  the form, no-effect or export, it takes under
  `.agents/skills/compile/references/runtime-data-mode.md` `## Mode selection`;
  the card cites a Reproducing signal or a read at the session baseline commit
  as-is, unmarked; when
  the proven cause is a general rule, the bug-fix check also covers one input
  beyond the reported case that a patch for only that case would still fail,
  settled by a code read of the general fix when the agent settling it is
  confident the change is correct because it is small and self-contained, and
  otherwise by a run; a criterion for a performance gain the change exists to
  make covers one independent optimization and states that the optimization is
  reverted when the criterion fails, and when `query_profile` measures the gain
  it also names its baseline cohorts and expects a gap outside the
  cohort-to-cohort range the `query_profile` caveat in
  `.agents/skills/agent-harness/references/command-reference.md`
  `## Durable caveats` defines; an `/agent-harness` check names a command that
  reaches the path its criterion observes, from that command reference or from
  a command reference the selected project's
  `Projects/<Project>/Documents/AgentHarness.md` routes, and when none does,
  the check is instead the code reading the Change Workflow Run targeted
  pre-review checks step in `.agents/references/change-workflow.md` prescribes
  for a path nothing reaches today>
- Roles: <required and conditional assignments>
```
