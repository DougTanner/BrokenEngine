<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-26T16:53:09.696Z","dependsOn":[]} -->
# Fix: /code-style-review — style-rule judgment result is not kept, so the worker reruns the script

## Context
Observed during a `/next-plan` run for the Engine Plan
`GameBaseBackReferenceRemoval.md`. The `/code-style-review` worker ran the
step-6 style-rule judgment
(`pwsh -NoProfile -File .agents/scripts/Test-StyleRuleJudgment.ps1 -RepositoryRoot <toplevel> -Baseline <sha>`)
twice instead of once, because it needed the result again for later parsing
and had not kept it. The two runs disagreed: the first judged 72 blocks, the
second 73, with one more flagged entry. The rerun cost a second paid Jev pass
and left two differing results for the same review.

Current-tree evidence:
- `.agents/skills/code-style-review/references/worker.md` step 6 says to run
  the judgment "once" and gives the invocation, but it gives no way to keep the
  result document for step 10's adjudication of flagged entries.
- `.agents/scripts/Test-StyleRuleJudgment.ps1` already takes an `-OutputPath`
  parameter (the `param` block and the result-writing branch near line 110):
  given one, it writes the full JSON result to that file and prints a one-line
  summary. Step 6 never mentions it.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 5c6a9eef-1279-4356-8749-17c9c4d4bae1
- Worktree/branch UUID: b7e73f28-c654-42c5-aa6b-01ae9633e1eb
- Session branch: claude/b7e73f28-c654-42c5-aa6b-01ae9633e1eb
- Worktree: .claude\worktrees\BrokenEngine\b7e73f28-c654-42c5-aa6b-01ae9633e1eb
- Landing ref: claude/b7e73f28-c654-42c5-aa6b-01ae9633e1eb (the observing
  session records and lands this Plan itself; the branch tip is that session's
  final commit and survives exactly as long as the worktree recorded above).
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/CodeStyleReviewJudgmentResultCapture.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <review ref>` in bounded friction mode — the landing ref
above — supplying client `claude` and the recorded conversation session ID.
Then make the smallest fix inside the `## In scope` boundary below. If
root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

The author recommends that step 6's invocation pass `-OutputPath` with a
gitignored `Temp/` file addressed from the worktree root, and that later steps
read the flagged entries from that file instead of rerunning the script.
Rationale: the script already supports it, so the fix is a wording change with
no new mechanism, and a kept file also gives a long `Judgment` field an
existing file to cite under `Evidence`.

## Critical files
- `.agents/skills/code-style-review/references/worker.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to
  `.agents/skills/code-style-review/references/worker.md` step 6 (the judgment
  invocation and its result-usability bullets) and, only if needed, the step 10
  bullet that consumes step 6's flagged entries

## Out of scope
- The landed change the session produced
- `.agents/scripts/Test-StyleRuleJudgment.ps1` and `Invoke-Jev.ps1`
- The `Judgment` field definition in `.agents/skills/code-style-review/SKILL.md`
  `## Handoff`
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Change Workflow Tier 2 — trigger: scoped tool behavior (one skill worker's
script-invocation step). Escalate if the fix reaches build/bootstrap
coordination. Never embed transcript paths or home paths.

## Acceptance criteria
- A `/code-style-review` run over a session-changed scope invokes
  `Test-StyleRuleJudgment.ps1` exactly once and adjudicates from the kept result
- The static-checks runner, invoked as `.agents/references/static-checks.md`
  documents it, reports every row the change triggers passing

## Notes
- The `/external-skill-creator` validation applies to the fix because it
  changes a `.agents/skills/*/` package.
