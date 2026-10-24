<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-24T13:52:40.382Z","dependsOn":[]} -->
# Fix: Invoke-StaticChecks.ps1 — line-length failures do not say whether the change added the line

## Context
Observed symptom: during a `/next-plan` run, main ran
`pwsh -NoProfile -File .agents/scripts/Invoke-StaticChecks.ps1 -RepositoryRoot <worktree> -Baseline <full baseline SHA> -IncludeUntracked`.
The `line-length` row failed on
`.agents/skills/agent-harness/references/command-reference.md` line 177
(length 596). The failure entry carries only `path`, `line`, and `length`
(`Get-LineLengthFailure`, `.agents/scripts/Invoke-StaticChecks.ps1:339-357`),
because the check deliberately scans every line of each changed file, not only
the added lines (header comment, lines 6-10). The row therefore cannot tell
main whether the session added the overlong line (an in-scope fix) or the line
already existed at the baseline (a residual). Main worked around it by running
its own `git diff -U0 <baseline> -- <file>` and reading the `@@` hunk headers
to place line 177 inside or outside an added range.

The same script already computes added new-side line numbers from a
`git diff -U0` hunk walk for the GLSL indentation check
(`Add-AddedGlslIndentFailure`, `.agents/scripts/Invoke-StaticChecks.ps1:393`).

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: c4130ec1-498c-4711-bb9d-66a87c7b9776
- Worktree/branch UUID: 0787cd30-7a64-4520-a5f1-f5cb541c9821
- Session branch: claude/0787cd30-7a64-4520-a5f1-f5cb541c9821
- Worktree: .claude\worktrees\BrokenEngine\0787cd30-7a64-4520-a5f1-f5cb541c9821
- Landing ref: claude/0787cd30-7a64-4520-a5f1-f5cb541c9821 (the observing
  session records and lands this Plan itself).
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/StaticChecksLineLengthAddedLine.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review claude/0787cd30-7a64-4520-a5f1-f5cb541c9821` in bounded
friction mode, supplying client `claude` and conversation session ID
`c4130ec1-498c-4711-bb9d-66a87c7b9776`. Then make the smallest fix inside the
`## In scope` boundary below. If root-causing shows the fix lies outside that
boundary, surface it for re-planning instead of expanding scope.

The author recommends marking each `line-length` failure entry with whether
its line is added relative to the baseline, derived from the same `-U0` hunk
walk `Add-AddedGlslIndentFailure` already performs, keeping the whole-file scan
and the pass/fail outcome unchanged; the rationale is that the whole-file scan
is intentional and only the attribution is missing.

## Critical files
- `.agents/scripts/Invoke-StaticChecks.ps1` — `Get-LineLengthFailure`,
  `Invoke-LineLengthCheck`, the header comment describing the `line-length`
  row, and `Add-AddedGlslIndentFailure` if its hunk walk is shared
- `.agents/references/change-workflow.md` — only the
  `#### Step 5 — Run targeted pre-review checks` text, and only if it
  describes the `line-length` failure fields

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the functions, comment, and section
  named under `## Critical files`

## Out of scope
- The landed change the session produced
- The 500-character limit, the markdown exemptions, and the whole-file scan
  itself
- The `markdown-links`, `file-hygiene`, and `validate-skill` rows, except a
  shared added-line helper they reuse unchanged
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior: a script result shape changes); escalate
if the fix reaches build/bootstrap coordination. The run stays read-only and
writes nothing to disk. Never embed transcript paths or home paths.

## Acceptance criteria
- A `line-length` failure on a line the change added and one on a line present
  at the baseline are distinguishable from the row alone, without a separate
  `git diff`
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
