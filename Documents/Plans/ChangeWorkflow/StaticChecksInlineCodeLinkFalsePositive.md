<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-27T22:28:32.197Z","dependsOn":[]} -->
# Skip inline code spans in the static-check markdown link scan

## Context

The `markdown-links` check in `.agents/scripts/Invoke-StaticChecks.ps1` reports link-shaped text inside markdown inline code spans as a broken link. `Get-MarkdownLinkFailure` skips fenced code blocks (the `^\s*(?:```|~~~)` toggle) but runs `$script:LinkPattern` (`'\[(?:[^\[\]]*)\]\(\s*([^)\s]+)'`, line 38) over the whole of every other line, backtick spans included.

Observed evidence: running `pwsh -NoProfile -File .agents/scripts/Invoke-StaticChecks.ps1 -RepositoryRoot <worktree root> -Baseline ff516591e28721633e5cda255edc03c3020b7919` in a session that edited `Documents/Plans/ChangeWorkflow/CodeStyleReviewPermittedForms.md` reported the `markdown-links` failure `{"path":"Documents/Plans/ChangeWorkflow/CodeStyleReviewPermittedForms.md","line":16,"target":"const","resolves":false}`. Line 16 is prose whose inline code `` `[](const auto& rSource)` `` is a C++ lambda, not a link; that line is identical at baseline `ff516591`, so the defect predates the session and lies outside its docs-only change.

Effect: any session that edits a markdown file containing a C++ lambda (or any `[...](...)` text) in inline code gets a false `markdown-links` failure from the Run targeted pre-review checks step.

Change Workflow tier: Tier 2 — scoped behavior of one tool script (the static-check runner); no determinism, wire, serialization, threading, trust, or build-coordination surface.

## Design

Recommended: in `Get-MarkdownLinkFailure`, remove inline code spans from each non-fenced line before matching `$script:LinkPattern`, so only text outside code spans is scanned. A code span is a run of one or more backticks closed by the next backtick run of the same length on the same line (CommonMark rule); replacing each such span with an empty string suffices, since failures report only the line number. Rationale: this is the smallest change at the point where the false match arises and mirrors the existing fenced-block skip; an unmatched lone backtick is left in place and scanned as ordinary text, as CommonMark treats it.

Code spans that wrap across lines are not handled; the author judges them rare enough in this repository's prose not to justify a multi-line parser.

## Critical files

- `.agents/scripts/Invoke-StaticChecks.ps1`

## In scope

- `Get-MarkdownLinkFailure` in `.agents/scripts/Invoke-StaticChecks.ps1`: strip inline code spans from each scanned line before the `[regex]::Matches($line, $script:LinkPattern)` loop.
- A script-level pattern variable next to `$script:LinkPattern` for the code-span regex, if the implementation adds one.
- The header comment of the same script, only if its description of the link check would otherwise be wrong.

## Out of scope

- `$script:LinkPattern` itself, `Get-HeadingSlug`, `Resolve-RepositoryPath`, the fenced-block toggle, and the `validate-skill` check.
- Multi-line inline code spans, reference-style links, and HTML links.
- The result document shape, exit codes, and any caller skill or reference prose.
- Editing any markdown file to work around the false positive.

## Acceptance criteria

- Running the documented `Invoke-StaticChecks.ps1` invocation on a change that edits a markdown file containing `` `[](const auto& rSource)` `` in inline code reports no `markdown-links` failure for that line.
- A genuinely broken link outside inline code (for example `[x](Missing/Path.md)` in prose) in a changed markdown file still reports a `markdown-links` failure with `resolves: false`.
- Valid links outside inline code on the same line as a code span are still counted in `linkCount` and still resolve.

## Notes

- Found as a pre-existing residual during a docs-only style-workflow session at baseline `ff516591e28721633e5cda255edc03c3020b7919`.
- Verification is a direct run of the script as its skill documents it; no build or live harness run applies.
