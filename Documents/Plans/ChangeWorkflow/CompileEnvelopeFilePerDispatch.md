<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-16T16:50:44.895Z","dependsOn":[]} -->
# Fix: /compile worker — a later build's envelope block lands in another dispatch's envelope file

## Context
Observed symptom: during a `/next-plan` run, a `/compile` builder dispatch
appended its server build's `broken-engine-build-result/v1` envelope to another
dispatch's file, `Temp/AgentBuildEnvelopes/compile-20261016T134801121Z-5164.md`.
Its handoff said this happened "because the PID-based filter failed". The worker
cut that file back to its original length with a guarded byte truncation, and
main had to check the incident and report it.

Where the rule lives today: `.agents/skills/compile/references/worker.md`
`## Steps` step 9 names the envelope file
`compile-<UTC yyyyMMddTHHmmssfffZ>-<process id>.md`, unique to the dispatch. The
first build's PowerShell call creates it, and each later build's call appends
to "the same file". Each build runs in its own shell call, so a later call runs
in a different process. Step 9 does not say how that later call finds the file
the first call created. Several dispatches write into the same
`Temp/AgentBuildEnvelopes/` directory, so a lookup by pattern can match another
dispatch's file.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 9c0d16a4-ef3f-47e9-8c5f-9edb064a3199
- Worktree/branch UUID: 7ee6c7fe-e2a0-4081-a01c-477dd7fa8a34
- Session branch: claude/7ee6c7fe-e2a0-4081-a01c-477dd7fa8a34
- Worktree: .claude\worktrees\BrokenEngine\7ee6c7fe-e2a0-4081-a01c-477dd7fa8a34
- Landing ref: claude/7ee6c7fe-e2a0-4081-a01c-477dd7fa8a34 (the observing
  session records and lands this Plan itself).
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/CompileEnvelopeFilePerDispatch.md`, but a periodic
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
`/next-plan-review <review ref>` in bounded friction mode — the landing ref —
supplying the recorded client and the recorded conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below. If
root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

The author recommends that step 9 make the envelope file's path something every
later call of the same dispatch already holds, rather than something it has to
look up. For example, the worker records the exact relative path the first call
created and passes that literal path to each later call. A lookup by name
pattern or process id would be removed. Rationale: a literal path cannot match
another dispatch's file, and it adds no script.

## Critical files
- `.agents/skills/compile/references/worker.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to
  `.agents/skills/compile/references/worker.md` `## Steps` step 9 (the envelope
  file naming, creation, and append bullets)

## Out of scope
- The landed change the session produced
- `Invoke-CompileBuild.ps1`, WorktreeCli, and the
  `broken-engine-build-result/v1` envelope format
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior: one skill's worker steps); escalate if
the fix reaches build/bootstrap coordination. Never embed transcript paths or
home paths. Each envelope block stays byte-verbatim from its own call's captured
output.

## Acceptance criteria
- The recorded symptom no longer reproduces under the documented invocation: a
  read of step 9 shows that each later build call appends to a path fixed by the
  first call of the same dispatch, with no lookup that can match another
  dispatch's file in `Temp/AgentBuildEnvelopes/`
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
