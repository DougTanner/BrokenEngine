<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-10T17:32:42.972Z","dependsOn":[]} -->
# Fix: Change Workflow Steps 5-7 — a Step 7 style rename invalidates the Step 5 build and the acceptance runs

## Context
Observed during a `/next-plan` run on an engine C++ Plan.

`.agents/references/change-workflow.md:107-114` (Step 5, Run targeted pre-review
checks) dispatches `builder` `/compile` for every `Build required` handoff, and
the acceptance-table runtime/harness scenarios named at line 114 run on the
binaries that build produces. `.agents/references/change-workflow.md:132-140`
(Step 7, Apply the triggered cleanup) is where `mechanic` `/code-style-review`
runs for changed C++, two steps later.

Observed symptom. In that run, `/code-style-review` applied a style-guide rule-56
rename of the local `tickRemainderWallNs` in
`engine::ServerSessionRuntime::WaitForTick` only at Step 7 — after the Step 5
`/compile` build and after the `/agent-harness` acceptance runs had already
completed against the pre-rename binary. The dispatch returned
`Build required: Server Debug|x64`.

Cost. Main had to issue a second full `builder` `/compile` dispatch (124.7 s of
build time), and had to hold that dispatch until the harness released the running
server executable, because the first build no longer matched the final source
bytes. One build would have covered the final bytes had the mechanical style pass
run before the pre-review build.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the session
`/next-plan-review` must reach — while the `Landing ref` line names a ref whose
tree actually contains this Plan:
- Client: claude
- Conversation session ID: a06b85a1-692a-49b9-917b-1565c31365f4
- Worktree/branch UUID: 31d26f5f-0202-44d3-a438-1ca3857ffb79
- Session branch: claude/31d26f5f-0202-44d3-a438-1ca3857ffb79
- Worktree: .claude\worktrees\BrokenEngine\31d26f5f-0202-44d3-a438-1ca3857ffb79
- Landing ref: the session branch above, whose tip is that session's final commit
  and which survives exactly as long as the worktree recorded above.
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/CodeStyleReviewBeforePreReviewBuild.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <review ref>` in bounded friction mode — the landing ref above
— supplying the recorded client and the recorded conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below. If root-causing
shows the fix lies outside that boundary, surface it for re-planning instead of
expanding scope.

The author's recommendation, offered as a starting point rather than a settled
decision, is to move the mechanical `/code-style-review` dispatch for changed C++
so it runs before the Step 5 pre-review build, keeping its semantic-candidate
routing where it is; the alternative the fix session should weigh is leaving the
dispatch at Step 7 and stating there that a style fix which changes emitted bytes
re-triggers the Step 5 build and any acceptance runs already taken. The concrete
ordering is the fix session's decision, made from the root cause and from what
`/code-style-review` and `/compile` actually require of each other. Whichever is
chosen, the ordering lines of both steps must agree, and the parallelism the
existing Order lines grant must not be silently narrowed.

## Critical files
- `.agents/references/change-workflow.md` (Step 5 and Step 7, including their
  Order lines)
- `.agents/skills/code-style-review/SKILL.md` — only if the chosen ordering
  changes what that skill states about when it is dispatched

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the Step 5 and Step 7 sections of
  `.agents/references/change-workflow.md` and, only if the chosen ordering
  requires it, the dispatch-timing statement in
  `.agents/skills/code-style-review/SKILL.md`

## Out of scope
- The landed change the session produced
- The dispatch list, ordering, or triggers of Steps 1-4, 6, 8, and 9
- `/compile` build mechanics, data-mode selection, and harness invocation
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2: scoped tool behavior, because the fix changes when a dispatch
runs rather than only how a document reads. Escalate if the fix reaches
build/bootstrap coordination. Never embed transcript paths or home paths.

## Acceptance criteria
- The recorded symptom no longer reproduces: a change whose C++ receives a
  mechanical style fix needs only one pre-review build to cover its final bytes,
  or the workflow states plainly that the style fix re-triggers the build and the
  acceptance runs
- Step 5 and Step 7 state one consistent ordering with no contradiction between
  them
- /external-skill-creator validate mode passes wherever the Change Workflow Apply
  the triggered cleanup step triggers it; plan validate exits 0
