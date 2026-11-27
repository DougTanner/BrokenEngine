<!-- broken-engine-plan/v2 {"createdUtc":"2026-11-26T14:01:16.172Z","dependsOn":[]} -->
# Find-CommentBlocks.ps1 emits trailing `//` comments as blocks

## Context

`.agents/skills/comment-review/scripts/Find-CommentBlocks.ps1` defines
`$script:CommentPattern = '^\s*//'` (line 26), and `Get-CommentBlock` (lines
85-121) builds blocks only from lines matching it. A trailing comment after
code on the same line never becomes a block, so `/comment-review`
(`references/worker.md` step 2 keeps only scanner blocks overlapping changed
regions) never classifies it.

Observed instance: `Engine/Source/Ui/LightingWrappersBase.h:67` carries the
trailing comment "Fraction of gCombineMaxBrightness; paired with
gCombineMaxBrightness >= 0.1f and gCombineLinearLength <= 0.8f, bounds keep
0 < m < S1 < P, m >= 0.001f, and P - S1 >= 0.004f." It copies values the code
owns, the same copied derived bound the session's comment review raised at
`LightingUniforms.cpp:164`, yet the scanner emitted no block for it. Lines 66
and 70 of the same file hold similar trailing comments.

## Design

`Get-CommentBlock` also emits a one-line block for each line that holds code
followed by a `//` comment, with `startLine` = `endLine` = that line, the
`charCount` and `kinds` computed from the text after `//`, and `firstLine`
holding that comment text. A `//` inside a string or character literal (for
example a URL) is not a comment; the author recommends a simple scan that
skips quoted spans rather than a tokenizer, since a missed or extra hint only
affects reviewer input. A trailing comment stays its own block and never joins
an adjacent full-line block. The block row shape is unchanged, so the
`broken-engine-comment-blocks/v2` schema version stays; the executing session
bumps it only if it adds a field.

Check that whole-repository runs stay under `$script:MaximumBlocks` often
enough; the worker's existing split-and-rerun rule on `truncated` covers the
rest.

## Critical files

- `.agents/skills/comment-review/scripts/Find-CommentBlocks.ps1`

## In scope

- `Find-CommentBlocks.ps1`: `CommentPattern`/`CommentPrefixPattern` and
  `Get-CommentBlock`, plus the comment above `CommentPattern` that defines a
  block.
- `.agents/skills/comment-review/SKILL.md` and `references/worker.md` text that
  defines a block as full-line only, if any.

## Out of scope

- New candidate kinds, the `history` pattern, and `comment-classes.md`
  (another change owns its classes).
- Block comments (`/* */`).

## Acceptance criteria

- Running the scanner on `Engine/Source/Ui/LightingWrappersBase.h` reports
  one-line blocks at lines 66, 67, and 70.
- A line whose only `//` sits inside a string literal produces no block.

## Notes

- Change Workflow tier: Tier 2 (scoped tool behavior: one skill script).
- Run `/external-skill-creator` validation after the skill package change.
