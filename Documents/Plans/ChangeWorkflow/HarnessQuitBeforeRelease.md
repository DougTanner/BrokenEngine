<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-19T22:17:19.708Z","dependsOn":[]} -->
# Fix: agent-harness — a deliberate `quit` before release reports a crash finding

## Context
Observed symptom: an acceptance check read the Debug server's `--log-file`
"after `quit`" (the mimalloc shutdown line `Engine/Source/Memory/GlobalAllocator.cpp:284`
formats). The runner sent `{"cmd":"quit"}` to the registered server, which
exited normally — its log ends `Exit main loop` and the shutdown line, and the
process-check exit record shows `exitCode` 0 — then ran
`.agents/skills/agent-harness/scripts/Invoke-HarnessRelease.ps1` with
`-ProcessCheckStatePath`. The release's entry `-Action Check`
(`Invoke-HarnessRelease.ps1:82-113`) found the registered server gone, and
`Invoke-AgentHarnessProcessCheck.ps1:363-408` reports every exited registered
role, whatever its exit code, as an unexpected exit, so the release returned a
`crashFindings` entry for the server. By
`.agents/skills/agent-harness/references/worker.md` step 41, exit `0` with a
non-empty `crashFindings` "still means the run failed", so the runner had to
explain the finding away as its own quit.

`worker.md` covers a deliberate quit only for a relaunch (steps 25-26: "Between
that deliberate quit and re-registration the role is not checked"); no step
covers a check that ends with `quit` before release, and the checker has no
action to unregister a role. Root cause not yet confirmed.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: adf1d3fc-1e95-4b1a-86b9-999666d0017e
- Worktree/branch UUID: 285f5cc6-a302-4d4f-8f47-e7d1b72c0ae0
- Session branch: claude/285f5cc6-a302-4d4f-8f47-e7d1b72c0ae0
- Worktree: .claude\worktrees\BrokenEngine\285f5cc6-a302-4d4f-8f47-e7d1b72c0ae0
- Landing ref: claude/285f5cc6-a302-4d4f-8f47-e7d1b72c0ae0
  The review ref the invoker passes to /next-plan-review: the recording
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
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's
`## Context`. Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <review ref>` in bounded friction mode, with the review ref
the provenance block above defines, supplying the recorded client and the
recorded conversation session ID. Then make the smallest fix inside the
`## In scope` boundary below. If root-causing shows the fix lies outside that
boundary, surface it for re-planning instead of expanding scope.

Candidates for the executing session to choose between: a documented
procedure for a check that ends with `quit` (run `-Action Check` before the
quit and treat the role as deliberately ended, as steps 25-26 do for a
relaunch), or a script-side way to mark a deliberately quit role so the
release check does not report it.

## Critical files
- `.agents/skills/agent-harness/references/worker.md`
- `.agents/skills/agent-harness/scripts/Invoke-HarnessRelease.ps1`
- `.agents/skills/agent-harness/scripts/Invoke-AgentHarnessProcessCheck.ps1`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the files named above: `worker.md`
  steps 25-26 and 39-42, the release entry check, and the checker's `Check`
  action

## Out of scope
- The landed change the session produced
- Unrelated skills/scripts; any transcript path or transcript text in the repo
- Engine quit or shutdown behavior

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior); escalate if the fix reaches
build/bootstrap coordination. A genuine crash of a role that was not
deliberately quit must still be reported. Never embed transcript paths or home
paths.

## Acceptance criteria
- The recorded symptom no longer reproduces under the documented invocation:
  a registered server sent `quit` before release, by the documented procedure,
  yields no `crashFindings` entry and no failed run
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
