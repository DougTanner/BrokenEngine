<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-06T19:20:37.969Z","dependsOn":[]} -->
# Fix: /repo-code-review — the targets-file command runs only in PowerShell

## Context

Observed symptom: `.agents/skills/repo-code-review/SKILL.md:38-41` documents the
targets file as `.agents/scripts/Get-SessionChangeInventory.ps1 ... -EmitTargets`
piped to `Set-Content -LiteralPath Temp/code-review-targets.json -NoNewline`.
`Set-Content` is a PowerShell cmdlet, and Git Bash is Claude Code's documented
shell (root `AGENTS.md` `## Environment`). Main ran the pipeline through Bash and
got ParserError `An empty pipe element is not allowed` with no targets file.
Wrapping the script in a nested `pwsh` also failed, and main reran it through
PowerShell.

The script's own file route is closed for this mode:
`Get-SessionChangeInventory.ps1:736` makes `-OutputPath` exclusive with
`-EmitTargets`. A plain stdout redirect does work: at the recording commit, the
same command ending in `> <file>` under Git Bash exited 0 and wrote a valid
`broken-engine-code-quality-targets/v1` document. That form is the only one
checked; `SKILL.md` does not state it.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 4ba81e08-3493-4492-84a8-df446e573f41
- Worktree/branch UUID: aa8d6b01-5a38-4ae9-a02f-fbbcb7611124
- Session branch: claude/aa8d6b01-5a38-4ae9-a02f-fbbcb7611124
- Worktree: .claude\worktrees\BrokenEngine\aa8d6b01-5a38-4ae9-a02f-fbbcb7611124
- Landing ref: claude/aa8d6b01-5a38-4ae9-a02f-fbbcb7611124
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/RunTargetsCommandInEitherShell.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design

First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <landing ref>` in bounded friction mode, supplying the
recorded client and the recorded conversation session ID. Then make the smallest
fix inside the `## In scope` boundary below. If root-causing shows the fix lies
outside that boundary, surface it for re-planning instead of expanding scope.

The author recommends replacing the `Set-Content` pipeline with a `>` redirect to
`Temp/code-review-targets.json`, because `>` is valid in Git Bash and PowerShell 7
alike and needs no script change. The file's one difference is a trailing
newline; confirm that the file's consumers accept it, starting with the
`json.loads` read in `Analyze-CodeQualityMetrics.py` (`targets`). When a consumer
rejects it, surface the alternative of admitting `-OutputPath` with `-EmitTargets`
for re-planning, since that changes the script.

## Critical files

- `.agents/skills/repo-code-review/SKILL.md` — `## Inputs`

## In scope

- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the targets-file command sentence of
  `SKILL.md` `## Inputs`

## Out of scope

- The landed change the session produced
- `Get-SessionChangeInventory.ps1`, including its `-OutputPath` exclusion and the
  `broken-engine-code-quality-targets/v1` format
- The inventory invocations other skills document, none of which pipes to
  `Set-Content`
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants

Expected Tier 1: the highest trigger is documentation with no public signature or
invariant exposure, since one documented command changes and the script and the
targets format do not. A fix that reaches the script is Tier 2 and returns for
re-planning. Determinism, wire, serialization, replay, affinity, threading,
allocation, shader, and build surfaces are not exposed; no live verification
applies. Never embed transcript paths or home paths.

## Acceptance criteria

- The recorded symptom no longer reproduces under the documented invocation: the
  documented command, run unchanged in Git Bash and in PowerShell 7, exits 0 and
  writes a `broken-engine-code-quality-targets/v1` targets file
- `.agents/skills` holds no remaining `Set-Content` pipeline for that file
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
