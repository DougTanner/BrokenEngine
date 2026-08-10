<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-10T14:55:44.812Z","dependsOn":[]} -->
# Fix: New-CodexReviewPrompt.ps1 — reviewed paths glob as pathspecs in the diff evidence

## Context
`.agents/skills/codex-review/scripts/New-CodexReviewPrompt.ps1` assembles the
reviewer prompt's diff evidence by handing the reviewed paths to `git` after a
`--` separator as raw pathspecs:

- `Write-DiffEvidence`, line 411:
  `$arguments = @('diff', '-M', '--no-color', '--no-ext-diff') + $script:DiffRange + @('--') + $script:DiffPath`
- `Get-PromptContributor`, line 177:
  `$arguments = @('diff', '--numstat', '-M', '--no-color', '--no-ext-diff') + $script:DiffRange + @('--') + $script:DiffPath`

Git treats an argument after `--` as a pathspec with its default `glob` magic,
so `[`, `*`, and `?` in a reviewed file's own name are matched as wildcards
rather than as literal characters. A reviewed path containing one of those
characters can therefore be silently absent from the prompt's `## Diff` section,
or can match a different path whose diff is included instead. The failure is
silent: `git diff` exits `0` with no error when a pathspec matches nothing, so
the reviewer receives a prompt that omits changed bytes it was dispatched to
review, and `Get-PromptContributor` reports the wrong largest contributors in
the `prompt.diff-too-large` message.

The same class of defect was already fixed in this file for the HEAD-tree
lookup: `Get-PromptHeadEntry` (line 337) passes `":(literal)$RelativePath"` to
`git ls-tree`, and its comment states the reason. The two `git diff` call sites
above were not converted and remain raw. This is pre-existing behavior; it was
not introduced by the change that added the `:(literal)` HEAD lookup, which was
scoped to the head-required working-tree comparison and did not cover diff
evidence assembly.

## Design
Apply the fix shape already established in `Get-PromptHeadEntry`: prefix each
reviewed path with the `:(literal)` pathspec magic before appending it to the
`git diff` argument list, at both call sites. Prefix at the point of use rather
than mutating `$script:DiffPath` itself, because that variable is also used for
non-pathspec purposes (per-path HEAD and working-tree lookups, which already
apply their own magic, and the `$seen`/`$divergent` path bookkeeping that must
keep reporting bare repository-relative paths in operator-facing messages).

Keep the existing `--` separator: `:(literal)` governs wildcard interpretation,
while `--` is what keeps a path from being read as a revision.

## Critical files
- `.agents/skills/codex-review/scripts/New-CodexReviewPrompt.ps1` — the two
  `git diff` argument constructions in `Write-DiffEvidence` and
  `Get-PromptContributor`
- `.agents/skills/codex-review/scripts/Test-CodexReviewPromptFixtures.ps1` — the
  fixture harness that exercises prompt assembly

## In scope
- The `$arguments` construction in `Write-DiffEvidence`
- The `$arguments` construction in `Get-PromptContributor`
- Any fixture in `Test-CodexReviewPromptFixtures.ps1` needed to demonstrate that
  a reviewed path whose name contains `[` appears in the assembled diff evidence

## Out of scope
- `Get-PromptHeadEntry` and `Get-PromptWorkingEntry`, which already pass literal
  pathspecs
- Untracked-file evidence, which reads files directly and never goes through a
  pathspec
- The prompt template, the byte budget, exit codes, and the `/codex-review`
  dispatch contract
- Renaming, normalizing, or restricting which paths a caller may review

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior in one script). No determinism, CRC,
serialization, wire, or build-coordination surface. The invariant to preserve is
that evidence is never truncated or substituted: every reviewed path's diff must
appear in the prompt exactly once, and operator-facing messages must keep
printing bare repository-relative paths, not pathspec-magic-prefixed ones.

## Acceptance criteria
- A reviewed file whose name contains a pathspec wildcard character (`[`)
  appears in the assembled prompt's `## Diff` section with its own diff
- No reviewed path pulls in a different path's diff
- `pwsh -NoProfile -File .agents/skills/codex-review/scripts/Test-CodexReviewPromptFixtures.ps1`
  passes
- `plan validate` exits `0`

## Notes
Recorded as a pre-existing, out-of-scope residual from a primary-checkout
session on branch `2.0.0` whose approved boundary covered only the
`prompt.head-required` verify binding.
