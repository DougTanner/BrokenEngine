<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-22T20:14:23.264Z","dependsOn":[]} -->
# Fix: .agents/references/change-workflow.md — delegate reviewed-file reads in finding triage

## Context

Three recorded incidents in `WorkflowIncidents.md` under owner
`.agents/references/change-workflow.md` share the symptom that main reads a
reviewed file directly to judge review findings instead of passing finding IDs
to `/resolve-findings` (or delegating disputed-site lookups to a `locator`):

- 2026-11-22 (`agy/4004df43-5824-4718-822f-007c9e5d0963`): selector
  `.agents/skills/sweep/SKILL.md` lines 80-125 during coherence re-review;
  reviewed prose entered main; bounding: give the `/resolve-findings`
  `implementer` finding IDs and a section selector.
- 2026-11-22 (`agy/690eea56-ca81-49c6-9d35-f7503f68e7e9`): selector
  `Get-TranscriptProjection.ps1` lines 35-150; 7,701 chars entered main;
  bounding: dispatch a `locator` on disputed `CR-F-005`/`CR-F-006` sites for
  citations and reachability.
- 2026-11-22 (`agy/690eea56-ca81-49c6-9d35-f7503f68e7e9`): selector
  `next-plan-checkpoint-review/references/worker.md` lines 1-115; 8,524 chars
  entered main; bounding: pass finding IDs and cited step ranges in fix briefs
  so the `implementer` reads the worker doc.

Current source inspection confirms `.agents/references/change-workflow.md`
(`### Delegation roles` `:51-60` and `#### Step 6 — Review and resolve
correctness` `:331-337`) requires main to judge each finding and cite a
`file:line` when rejecting, and routes external claims to a `locator`, but does
not direct main to dispatch a `locator` for disputed repository sites or to
hand accepted finding IDs and cited ranges directly to `/resolve-findings`
without reading the reviewed files in main. Likewise
`.agents/references/subagent-reporting.md` (`## Handoffs` `:118,123`) delegates
`Residuals` contradictions to a `researcher` but states only "Decided one at a
time" for `Findings`.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:

- Client: agy
- Conversation session ID: b4bc361b-7bec-4239-9390-63aa1cea4e5d
- Worktree/branch UUID: 690eea56-ca81-49c6-9d35-f7503f68e7e9
- Session branch: agy/690eea56-ca81-49c6-9d35-f7503f68e7e9
- Worktree: `.agy\worktrees\BrokenEngine\690eea56-ca81-49c6-9d35-f7503f68e7e9`
- Landing ref: `agy/690eea56-ca81-49c6-9d35-f7503f68e7e9`
  The review ref the invoker passes to `/next-plan-review` has two cases. When
  the `Observed in an earlier session` line is present, it is that line's
  landed commit hash, with no lookup. When that line is absent, the recording
  session is the observing session, and worktree reuse moves its branch past
  its landing, so the invoker never reviews the ref itself: it resolves the ref
  to the commit that added this Plan with
  `git log --follow --diff-filter=A -n 1 --format=%H <ref> -- <this plan path>`
  and passes that commit. When the recorded ref is gone or that command prints
  nothing (a later session moved this Plan), it runs the same command with
  `<ref>` omitted. A periodic Plan-history squash can make either command
  return an unrelated aggregate commit, and a landing that also deletes a
  similar file can make it return another session's commit, so review the
  result only when the commit is attributable to the observing session recorded
  above alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before `/cleanup-worktrees` removes the worktree recorded
  above: Codex discovery requires a registered worktree, and Claude or `agy`
  review requires the conversation session ID above.

## Design

First root-cause the friction from the current tree and this Plan's
`## Context`. Only when the transcript is genuinely needed, in a new session
run `/next-plan-review <review ref>` in bounded friction mode, with the review
ref the provenance block above defines, supplying the recorded client and
conversation session ID. Then make the smallest fix inside the `## In scope`
boundary below so main does not read reviewed files directly during Step 6
finding triage — passing accepted finding IDs and cited selectors to
`/resolve-findings` and dispatching a `locator` when a disputed finding needs
repository-site evidence. If root-causing shows the fix lies outside that
boundary, surface it for re-planning instead of expanding scope.

## Critical files

- `.agents/references/change-workflow.md` — `### Delegation roles` (`:51-60`)
  and `#### Step 6 — Review and resolve correctness` (`:326-373`).
- `.agents/references/subagent-reporting.md` — `## Handoffs` (`:113-124`), if
  root-cause analysis shows the `Findings` row should mirror the rule.

## In scope

- Root-cause investigation as `## Design` states.
- The smallest resulting fix, confined to
  `.agents/references/change-workflow.md` (`### Delegation roles` and
  `#### Step 6 — Review and resolve correctness`) and
  `.agents/references/subagent-reporting.md` (`## Handoffs`).

## Out of scope

- The landed change the session produced.
- Unrelated skills/scripts; any transcript path or transcript text in the repo.
- Any C++, shader, Visual Studio project, or `WorktreeCli` file.

## Risk tier and invariants

Expected Tier 2 (scoped tool/workflow behavior: Step 6 finding triage and
delegation isolation in `.agents/references/change-workflow.md` and
`.agents/references/subagent-reporting.md`); escalate if the fix reaches
build/bootstrap coordination. No determinism/CRC, wire, serialization,
`.pack`/`kiVersion`, replay, affinity, threading, allocation, shader, build,
trust-boundary, or live `/agent-harness` exposure. Never embed transcript paths
or home paths.

## Acceptance criteria

- The recorded symptom no longer reproduces under the documented invocation:
  Step 6 finding triage directs main to dispatch a `locator` for disputed
  repository evidence and pass accepted finding IDs and selectors to
  `/resolve-findings` instead of reading reviewed files directly.
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing.

## Notes

- No live Plan dependencies or reciprocal `## Coordination` constraints apply.
- Live `/agent-harness` verification does not apply.
