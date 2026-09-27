<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-27T14:35:37.781Z","dependsOn":[]} -->
# Fix: change-workflow Step 5 — static-pass `implementer` dispatched for C++ with no runnable row

## Context
Observed symptom: in a `/next-plan` run whose change touched C++ plus markdown
and Plan files, main followed `.agents/references/change-workflow.md`
`#### Step 5 — Run targeted pre-review checks` (~:113), which says an
`implementer` "runs the full applicable static pass in
`.agents/references/static-checks.md` after propagation — every tier, when the
change touches C++ or GLSL", and "Otherwise main runs the one documented command
in that reference itself". Main dispatched that static-pass `implementer` for
the C++ change. The `implementer` found nothing to run and ran nothing, which
cost about 28k subagent tokens. Main then ran
`pwsh -NoProfile -File .agents/scripts/Invoke-StaticChecks.ps1` itself for the
changed markdown and Plan files.

The two documents contradict each other. The C++ row of the table in
`.agents/references/static-checks.md` (~:14) reads "focused reads, searches,
and traces inside each implementation slice; no full runner row", and no row
names GLSL. The only runnable row is the markdown, Plan, and skill-file row
(`Invoke-StaticChecks.ps1`). So the C++/GLSL trigger in Step 5 always
dispatches a worker with nothing to run, and the one real command is left to
main's "otherwise" branch. The intro of `static-checks.md` (~:3-6) also defers
to Step 5 for "an `implementer` or main".

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 9ef7534d-32c6-48fd-8c48-a916d8512807
- Worktree/branch UUID: 39941a9e-cd36-4a69-9148-2d03cd9ef7cf
- Session branch: claude/39941a9e-cd36-4a69-9148-2d03cd9ef7cf
- Worktree: .claude\worktrees\BrokenEngine\39941a9e-cd36-4a69-9148-2d03cd9ef7cf
- Landing ref: claude/39941a9e-cd36-4a69-9148-2d03cd9ef7cf
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/StaticPassImplementerDispatch.md`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to one session alone
  (its diff limited to that session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above. OpenCode transcript review remains unsupported regardless of worktree
  retention.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <review ref>` in bounded friction mode, using the landing
ref and supplying client `claude` and the recorded conversation session ID.
Then make the smallest fix inside the `## In scope` boundary below. If
root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

The author recommends this fix, assuming root-causing confirms the Context. The
Step 5 static-pass bullet becomes a main-run step: main runs the one documented
command in `.agents/references/static-checks.md` whenever the change triggers
its row, and no `implementer` is dispatched for the static pass. The rationale
is that `static-checks.md` has no runnable C++ or GLSL row. The C++ checks it
lists already run inside each implementation slice, so a worker dispatched for
C++ or GLSL can only return empty. The recommendation removes the C++/GLSL
trigger outright. It does not keep a conditional "dispatch when a runnable
C++/GLSL row exists", because no such row exists and adding a branch for one
would be a speculative extension point. The intro sentence of
`static-checks.md` that names "an `implementer` or main" is aligned to say main
runs the pass. The brief-citation clause ("main cites that reference in the
brief's `Governing paths`…") goes away with the dispatch.

## Critical files
- `.agents/references/change-workflow.md`
- `.agents/references/static-checks.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the static-pass bullet (~:113) of
  `.agents/references/change-workflow.md` `#### Step 5 — Run targeted
  pre-review checks`, and the intro paragraph (~:3-6) of
  `.agents/references/static-checks.md` that assigns the pass to "an
  `implementer` or main"

## Out of scope
- The landed change the session produced
- The `static-checks.md` table rows and `Invoke-StaticChecks.ps1` behavior
- Step 5's `mechanic` (`/code-style-review`) and `builder` (`/compile`) bullets
  and its `Order:` line
- `.agents/skills/implement-plan/references/worker.md`, whose steps defer to
  `static-checks.md` and need no change
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2, because the fix changes which role runs one Change Workflow
step, which is scoped tool behavior. Escalate if the fix reaches build/bootstrap
coordination. Never embed transcript paths or home paths.

## Acceptance criteria
- For a change that touches C++ or GLSL, Step 5 no longer directs main to
  dispatch a static-pass `implementer` when `static-checks.md` has no runnable
  row for it. Main runs `Invoke-StaticChecks.ps1` itself whenever its row is
  triggered.
- `static-checks.md` and Step 5 agree on who runs the static pass
- The static-checks runner, invoked as `.agents/references/static-checks.md`
  documents it, reports every row the change triggers passing
