<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-12T23:35:54.633Z","dependsOn":[]} -->
# Fix: /compile — builder `Changed files` attributes concurrent session edits to the build

## Context
Observed symptom: in a `/next-plan` run, main dispatched a `builder` running
`/compile` while an `/update-claude-docs` `implementer` was dispatched
concurrently in the same session worktree. The `implementer` edited
`Engine/Source/Graphics/AGENTS.md` during the build window. The `builder`'s
handoff then listed `Engine/Source/Graphics/AGENTS.md` under `Changed files` as
rewritten by the build run and needing a look before landing. No build rewrites
that file. Main had to trace the edit back to the concurrent dispatch before it
could trust the handoff.

Mechanism, from the current tree:

- `.agents/skills/compile/references/worker.md` step 9 has the `builder` run
  `.agents/skills/compile/scripts/Get-CompileRewrittenPaths.ps1 -SnapshotPath`
  before the first build and the same command with `-Compare` after the last
  one. Its printed lines become the handoff's `Changed files`.
- `Get-CompileRewrittenPaths.ps1` (`Get-ModifiedTrackedPaths` and the
  `-Compare` loop) takes both reads from worktree-wide
  `git status --porcelain=v1 -z --untracked-files=no`. It prints every tracked
  path in the second read that the snapshot lacks. Nothing in either read
  records who modified a path.
- `.agents/skills/compile/SKILL.md` `## Handoff` defines `Changed files` as
  "the tracked paths the builds newly modified", which is accurate only when
  nothing else edits the worktree during the build window.

So any tracked path that another dispatch first modifies during the build
window is reported as a build rewrite. Still open: which tracked paths a build
can actually rewrite (for example export outputs or project files), and so
whether the script or the skill text can tell build rewrites from concurrent
session edits at all.

`Changed files` has a second consumer. `.agents/skills/finalize-changes/SKILL.md`
(the paragraph on `builder` handoffs since the meaning-change handoff) adds
every path a `builder` lists under `Changed files` to the landing preparation's
path set. A fix must keep every real build rewrite in that set.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 7d71d984-5bf8-474c-bb31-78e055a9350f
- Worktree/branch UUID: cef52823-047e-4c99-bc75-ed082c596293
- Session branch: claude/cef52823-047e-4c99-bc75-ed082c596293
- Worktree: .claude\worktrees\BrokenEngine\cef52823-047e-4c99-bc75-ed082c596293
- Landing ref: 46a66aa33fbdaa0b0b9dc5d1a31823ff9ce28e5e, the observing
  session's landing commit, which added this Plan.
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/CompileRewrittenPathsConcurrentEdits.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First confirm from the current tree and this Plan's `## Context` which tracked
paths a `/compile` build can rewrite, and whether that set is closed and known.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review 46a66aa33fbdaa0b0b9dc5d1a31823ff9ce28e5e` in bounded
friction mode, supplying client `claude` and the conversation session ID above.
Then make the smallest fix inside the `## In scope` boundary below. If the
investigation shows the fix lies outside that boundary, for example in how main
schedules dispatches that run alongside a build, surface it for re-planning
instead of widening scope.

Choices for the fix session. The author recommends the first one if the
investigation finds a closed set:

- Limit the `-Compare` output to the paths a build can rewrite, so a
  concurrent edit elsewhere is never listed.
- If the set is not closed, keep the current output but correct the
  `## Handoff` wording. The rows would then be tracked paths first modified
  during the build window, possibly including concurrent session edits, rather
  than paths the build rewrote.

## Critical files
- `.agents/skills/compile/scripts/Get-CompileRewrittenPaths.ps1`
- `.agents/skills/compile/SKILL.md` `## Handoff` (the `Changed files` paragraph)
- `.agents/skills/compile/references/worker.md` step 9 (the snapshot and
  `-Compare` bullet)

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the files named above: the
  `Get-ModifiedTrackedPaths` function and `-Compare` loop of
  `Get-CompileRewrittenPaths.ps1`, the `Changed files` paragraph of
  `.agents/skills/compile/SKILL.md` `## Handoff`, and the step 9
  snapshot/`-Compare` bullet of `.agents/skills/compile/references/worker.md`

## Out of scope
- The landed change the session produced
- `.agents/skills/finalize-changes/` and its `-RequireExportOutputsRewritten`
  handling; main's dispatch scheduling in `.agents/references/change-workflow.md`
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2, trigger: scoped tool behavior of one skill script and its
handoff text. Escalate if the fix reaches build/bootstrap coordination. Every
path a build actually rewrites must still reach `Changed files`, because
`/finalize-changes` adds those paths to its landing preparation path set. A
failed script run must still return `Status: BLOCKED`. Never embed transcript
paths or home paths.

## Acceptance criteria
- The recorded symptom no longer reproduces under the documented invocation. A
  tracked file edited in the session worktree by another process during a
  `builder` dispatch is not reported as a build rewrite in that dispatch's
  `Changed files`. If the wording option is taken, the handoff instead labels
  it accurately as a path modified during the build window.
- A tracked path the build itself rewrites is still listed under
  `Changed files`.
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
