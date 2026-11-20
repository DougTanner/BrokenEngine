<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-20T00:35:11.361Z","dependsOn":[]} -->
# Fix: Get-SessionChangeInventory.ps1 — -EmitTargets redirect prints no pass receipt

## Context
`.agents/skills/repo-code-review/SKILL.md` `## Inputs` documents the targets
file as produced by
`pwsh -NoProfile -File .agents/scripts/Get-SessionChangeInventory.ps1 -RepositoryRoot '<absolute repository toplevel>' -Baseline <full 40-character SHA> -EmitTargets > Temp/code-review-targets.json`.
A `/next-plan` run's main session ran that exact invocation. With stdout
redirected to the file, the pass run printed nothing at all, so the session had
no receipt that the file was written or what the run's status was. Main
appended `Get-Item ... | Select-Object Length` to confirm the file existed,
which departs from the documented invocation.

Script behavior at the recording baseline:

- `Complete-SessionChangeInventory`
  (`.agents/scripts/Get-SessionChangeInventory.ps1:106-111`): in `-EmitTargets`
  mode a pass writes only the targets bytes to stdout; only a non-pass run
  writes its structured status envelope to stderr. A pass therefore leaves no
  status line on any stream once stdout is redirected.
- `-OutputPath`, which in other modes writes the result file and prints a
  one-line `<status> <code> <message> -> <path>` receipt (lines 95-98), is
  rejected with `-EmitTargets` (line 737; message at line 739: "The -OutputPath
  result file is exclusive with -EmitTargets and -EmitManifest.").
- A second consumer, `Get-PromptTargetsText` in
  `.agents/skills/codex-review/scripts/New-CodexReviewPrompt.ps1:269-291`,
  runs `-EmitTargets` and captures stdout and stderr separately; any fix must
  keep its stdout bytes unchanged and must not break how it reads stderr.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 4c7b61a4-ec03-4553-9ab1-e23cc3ac71f1
- Worktree/branch UUID: 2c24c0a5-8dd2-4c11-9afc-991d104208bc
- Session branch: claude/2c24c0a5-8dd2-4c11-9afc-991d104208bc
- Worktree: .claude\worktrees\BrokenEngine\2c24c0a5-8dd2-4c11-9afc-991d104208bc
- Landing ref: claude/2c24c0a5-8dd2-4c11-9afc-991d104208bc
  The review ref the invoker passes to /next-plan-review has two cases. When
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
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
First root-cause the friction from the current tree and this Plan's
`## Context`. Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <review ref>` in bounded friction mode, with the review ref
the provenance block above defines, supplying the recorded client and the
recorded conversation session ID. Then make the smallest fix inside the
`## In scope` boundary below. If root-causing shows the fix lies outside that
boundary, surface it for re-planning instead of expanding scope.

The author's recommendation, for the fix session to confirm: on a pass in
`-EmitTargets` mode, write the same one-line `<status> <code> <message>`
receipt the `-OutputPath` path prints, but to stderr, so the documented
redirect leaves stdout as the exact targets bytes and the session still sees
its status. This changes no invocation and no stdout byte. The alternative,
admitting `-OutputPath` with `-EmitTargets` and changing the documented
invocation in `/repo-code-review`, touches more files; prefer it only if the
fix session finds a stderr line on a pass breaks `Get-PromptTargetsText`.

## Critical files
- `.agents/scripts/Get-SessionChangeInventory.ps1`
  (`Complete-SessionChangeInventory`; the mode-conflict checks near line 737)
- `.agents/skills/repo-code-review/SKILL.md` (`## Inputs` invocation, only if
  the invocation changes)
- `.agents/skills/codex-review/scripts/New-CodexReviewPrompt.ps1`
  (`Get-PromptTargetsText`, only if its stderr handling must change)

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the `-EmitTargets` branch of
  `Complete-SessionChangeInventory` and, only if the alternative is taken, the
  `-OutputPath` conflict check, the `/repo-code-review` `## Inputs` invocation,
  and `Get-PromptTargetsText`

## Out of scope
- The landed change the session produced
- The `-EmitManifest` mode and every other inventory mode
- The targets file format (`broken-engine-code-quality-targets/v1`) and its
  stdout bytes
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior: one script's output contract with two
consumers); escalate if the fix reaches build/bootstrap coordination. Never
embed transcript paths or home paths. The targets bytes on stdout stay
byte-identical for both consumers.

## Acceptance criteria
- The recorded symptom no longer reproduces under the documented invocation:
  running the `/repo-code-review` `## Inputs` command on a pass shows a status
  receipt in the session without any added command, and the targets file bytes
  match those of the unfixed script for the same baseline
- `New-CodexReviewPrompt.ps1` still embeds the same targets text for the same
  baseline
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing

## Coordination
No directional dependencies or mandatory nondirectional coordination.
