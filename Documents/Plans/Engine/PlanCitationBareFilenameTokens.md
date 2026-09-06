<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-06T01:43:58.136Z","dependsOn":[]} -->
# Fix: Test-PlanCitations.ps1 — a backticked bare filename in prose is scanned as a citation

## Context
During this session's plan work the documented invocation

```powershell
pwsh -NoProfile -File .agents/skills/plan-audit/scripts/Test-PlanCitations.ps1 <plan file>
```

reported unresolved citation rows with `pathExists: false` for backticked prose
tokens that cite no repository path at all: bare filenames written as plurals or
as a category, such as a sentence about every skill package's public file or
about its private worker file. Nothing in the prose claimed a path, so each row
was a lead that could not be chased. The prose was rewritten twice to stop the
rows appearing — changing wording to satisfy the script rather than the reader —
and the false rows also consumed one `/coherence-review` finding and the fix
round that finding triggered.

The emitting grammar was re-read in the current tree and matches the symptom.
`.agents/skills/plan-audit/scripts/Test-PlanCitations.ps1:17` accepts any
backticked token of `[A-Za-z0-9_./-]+` ending in `.h`, `.cpp`, `.md`, `.ps1`,
`.py`, `.txt`, or `.glsl`, with no requirement that the token contain a
directory separator. The scan at `:141-146` applies that pattern to every
backticked run on every line, and `Get-RepositoryRelativePath` at `:68-97`
resolves a bare filename against the repository root and rejects only rooted
tokens and tokens escaping the root — so a bare filename becomes a record rooted
at the repository top level, `pathExists` is computed false at `:170`, and the
row is emitted as unresolved at `:280-296`.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 40d5ea23-9635-484d-99d0-a21b2c0f9667
- Worktree/branch UUID: 75e972a1-4851-444c-b0a0-9d5076f1cf48
- Session branch: claude/75e972a1-4851-444c-b0a0-9d5076f1cf48
- Worktree: .claude\worktrees\BrokenEngine\75e972a1-4851-444c-b0a0-9d5076f1cf48
- Landing ref: claude/75e972a1-4851-444c-b0a0-9d5076f1cf48
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review claude/75e972a1-4851-444c-b0a0-9d5076f1cf48` in bounded
friction mode, supplying the recorded client and conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below.

The author's recommendation is to require a directory separator: a backticked
token is a citation only when it contains at least one `/`, so a bare filename
in prose is never scanned, while every real citation in this repository — which
is always written as a repository-relative path — still is. That is one change
to the pattern at `:17` and is decidable without judging the surrounding prose.
It also drops the two token shapes that are genuinely path-less but currently
resolve: a bare filename and a `./`-free single segment.

The cost of that recommendation is that a relative citation written without a
separator stops being checked. The implementing session should confirm from the
tracked plan and skill files that no such citation form is in use before landing
it, and if one is, keep the separator rule and state the exception it added.

The alternative of treating a token that resolves under more than one skill
package as ambiguous prose is not recommended: it still scans bare filenames,
still emits a row for a name that happens to be unique, and makes the grammar
depend on the current tree's contents.

If root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

## Critical files
- `.agents/skills/plan-audit/scripts/Test-PlanCitations.ps1` — the citation
  grammar at `:17`, the backtick scan at `:141-146`, and the resolver at
  `:68-97`
- `.agents/skills/plan-audit/references/worker.md` — the documented consumption
  (`:17-25`); in scope only if the chosen fix changes what the auditor is told a
  row means

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the citation token grammar and its
  resolution in `.agents/skills/plan-audit/scripts/Test-PlanCitations.ps1`, plus
  the matching consumption wording in the plan-audit worker file named above
  when the fix changes what a row means

## Out of scope
- The success-exit status and code of the same script, including its behavior
  when the result is truncated, which
  `Documents/Plans/Engine/PlanCitationTruncatedStatus.md` owns
- The caps (`MaximumCitations`, `MaximumTextLength`, `MaximumOutputBytes`), the
  shed ordering, and the emitted schema name and fields
- The execution-card completeness payload of the same script
- Any audit judgment rule: the script still renders no verdict on a plan
- Rewriting the prose of any existing plan or skill file to avoid the current
  false rows
- The landed change the session produced
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Coordination
`Documents/Plans/Engine/PlanCitationTruncatedStatus.md` changes the success-exit
status and code of this same script, and names this grammar in its own
`## Out of scope`, so the two edits touch disjoint regions of one file. Neither
Plan requires the other first. Whichever lands second must re-read the other's
landed change and keep it working: this Plan must not alter which status or code
a completed lookup returns, and that Plan must not alter which tokens are
scanned.

## Risk tier and invariants
Expected Tier 2 (scoped behavior of one tool at an existing boundary — which
tokens a review-support script scans), by the root `AGENTS.md` Tier-2 trigger for
one subsystem's tool behavior; escalate if the fix reaches build/bootstrap
coordination. Invariants to preserve: the schema name
`broken-engine-plan-citations/v1` and every existing field; only unresolved
records are emitted; the script stays read-only, reads nothing outside the
worktree root, and writes its whole result to stdout with diagnostics on stderr;
the bundled-script rule's canonical `pwsh -NoProfile -File <repo-relative path>`
invocation from the worktree root. Never embed transcript paths or home paths.

## Acceptance criteria
- A file whose only backticked path-like tokens are bare filenames in prose
  returns no citation rows for them
- A file citing a real repository-relative path that does not exist, or a valid
  path with an out-of-range line number, still returns that unresolved row
- A file whose citations all resolve still returns an empty unresolved list with
  the unchanged status and code
- `/validate-skill` passes wherever the root `AGENTS.md` Apply the triggered
  cleanup step triggers it; plan validate exits 0

## Notes
The citation grammar was outside the `## In scope` boundary of both already
landed citation-script Plans, so this is new scope rather than a re-observation.
