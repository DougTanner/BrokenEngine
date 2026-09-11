<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-11T22:19:44.724Z","dependsOn":[]} -->
# Fix: repo-code-review — the documented targets-file run saves no file

## Context
`.agents/skills/repo-code-review/SKILL.md` `## Inputs` lines 50-61 tell the
dispatching manager that "the dispatching manager saves one read-only run to a
file:" and then give the run as
`pwsh -NoProfile -File .agents/scripts/Get-SessionChangeInventory.ps1
-RepositoryRoot <absolute repository toplevel> -Baseline <full 40-character SHA>
-EmitTargets`. That form saves nothing. `Get-SessionChangeInventory.ps1` states
at lines 1-5 that it "writes no file and no repository metadata ... so it is safe
under a read-only sandbox. Diagnostics go to stderr; stdout carries only the
result document", its `param` block at lines 7-16 exposes no output-path
parameter, and `-EmitTargets` reaches `Write-SessionTargets` (lines 668-698,
dispatched at lines 786-789), which ends in `Write-InventoryStream $false $text`
— stdout only.

Observed symptom in this session: the first run of the documented form produced
no file and placed the whole `broken-engine-code-quality-targets/v1` JSON in the
main session's context. Main then had to repeat the identical run as
`... -EmitTargets | Set-Content -LiteralPath Temp/NextPlan/code-review-targets.json -NoNewline`
to obtain the file path that this skill's own `## Inputs` requires as "the
authoritative supplied input", so the targets bytes were paid for twice and the
documented one-run form never yielded a path.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 02dde971-a960-4eeb-899d-899d465abfd3
- Worktree/branch UUID: 2e2c84f5-a093-4c3c-a388-b0318257ea49
- Session branch: claude/2e2c84f5-a093-4c3c-a388-b0318257ea49
- Worktree: .claude\worktrees\BrokenEngine\2e2c84f5-a093-4c3c-a388-b0318257ea49
- Landing ref: claude/2e2c84f5-a093-4c3c-a388-b0318257ea49, the session branch
  above, whose tip is this session's final commit and which survives exactly as
  long as the worktree recorded above.
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- <this plan path>`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to one session alone
  (its diff limited to that session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`;
the cited lines should be sufficient, so the transcript is not expected to be
needed. Only when it genuinely is, in a new session run
`/next-plan-review <landing ref above>` in bounded friction mode, supplying
client `claude` and the recorded conversation session ID. Then make the smallest
fix inside the `## In scope` boundary below. If root-causing shows the fix lies
outside that boundary, surface it for re-planning instead of expanding scope.

Two fixes were considered. The author recommends the documentation-only one:
have `## Inputs` state the save form explicitly — the documented run piped to
`Set-Content -LiteralPath <a stated Temp/ path> -NoNewline`, which the root
`AGENTS.md` bundled-script rule permits under its "using that call's own output"
clause for a PowerShell-tool call — and state that the manager supplies that
path, not the bytes, to the review worker. The rejected alternative adds a
`-TargetsPath <file>` parameter to `Get-SessionChangeInventory.ps1` that writes
the targets file and prints only the path; the author's rationale for rejecting
it is that the script's header comment makes "writes no file" a deliberate
read-only-sandbox property of every mode, so the parameter would trade a
one-line documentation fix for a behavioral exception to that property. An
implementer who disagrees after reading lines 1-5 may take the parameter route
instead, updating `## Inputs` to that form.

## Critical files
- `.agents/skills/repo-code-review/SKILL.md` — `## Inputs`, the paragraph at
  lines 50-61
- `.agents/scripts/Get-SessionChangeInventory.ps1` — only if the parameter route
  is taken

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the `## Inputs` paragraph of
  `.agents/skills/repo-code-review/SKILL.md` named above, plus
  `Get-SessionChangeInventory.ps1`'s `param` block, `Write-SessionTargets`, and
  its header comment only if the parameter route is taken
- The one-line pointer to that paragraph in
  `.agents/references/subagent-reporting.md` line 87, only if the fix makes its
  wording wrong

## Out of scope
- The landed change the session produced
- `/codex-review`'s own receipt-supplied `targetsPath` route, which already
  yields a file
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 1 (mechanical: skill documentation, no public signature or
invariant exposure); escalate to Tier 2 if the implementer takes the
`-TargetsPath` parameter route, which changes one script's tool behavior. Never
embed transcript paths or home paths.

## Acceptance criteria
- A main session following the `## Inputs` form obtains the targets file in one
  run, with no targets JSON in its own context
- The recorded symptom no longer reproduces under the documented invocation
- The static-checks runner, invoked as `.agents/references/static-checks.md`
  documents it, reports every row the change triggers passing
