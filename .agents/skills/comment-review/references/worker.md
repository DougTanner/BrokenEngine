# Comment Review Worker

The numbered steps and the judgment rules for `/comment-review`. The public
contract main reads is [`../SKILL.md`](../SKILL.md).

## Steps

1. Fix the review scope.
   - When the caller supplies a scope, use exactly those files and directories;
     otherwise take the session-changed C++ and shader files from the read-only
     inventory: `pwsh -NoProfile -File
     .agents/scripts/Get-SessionChangeInventory.ps1 -RepositoryRoot <absolute
     repository toplevel> -Baseline <full 40-character SHA> -Regions`, adding
     `-IncludeUntracked <comma-separated paths>` when the caller supplied
     untracked paths, and keeping the `entries` rows whose `class` is `cpp`,
     `dual-language-header`, or `glsl` plus their `regions`.
   - Only `status` `pass` with `truncated` false is usable; any other status, or
     a true `truncated`, means the ranges are unavailable — report that instead
     of enumerating hunks inline.
   - Done when the scope is fixed and stated as session-changed or
     caller-supplied, or the unavailability is reported.
2. Run the bundled scanner: `pwsh -NoProfile -File
   .agents/skills/comment-review/scripts/Find-CommentBlocks.ps1 -Path <paths>`,
   using `pwsh -NoProfile -Command "&
   '.agents/skills/comment-review/scripts/Find-CommentBlocks.ps1' -Path 'a','b'"`
   for more than one path.
   - One run covers the whole scope by default. Only `status` `pass` (exit 0)
     with `truncated` false is usable; `blocked` (exit 2) or `error` (exit 1)
     means the block list is unavailable — report that rather than hunting
     comment blocks by hand. A true `truncated` means the scope is too large for
     one run: split it into subdirectories, or into groups of files, and rerun
     the scanner per part until every run reports `truncated` false. `Blocks
     scanned` is then the sum over those runs.
   - In session mode keep only the blocks overlapping a changed region.
   - Done when the `broken-engine-comment-blocks/v1` objects with their `blocks`
     rows are in hand, filtered to the scope, or the unavailability is reported.
3. Read `Documents/C++StyleGuide.txt` rule 64 and
   [`comment-classes.md`](comment-classes.md), then classify every scanned block
   by reading the surrounding code. The rows' `kinds` and `lineCount` are a
   starting list, not the finding set: a block with no kind can still be a
   finding, and a kind hit is rejected when the code shows the comment states a
   present constraint. Judge `dense` from `lineCount` and from facts the block
   or its function already states elsewhere.
   - Done when every scanned block is accepted with one class or rejected.
4. Write each finding's replacement: `delete`, or the shortest present-tense
   text that keeps every preserved-class fact `comment-classes.md` lists.
   - Route the block instead of guessing whenever the surviving constraint is
     not evident from the comment and the surrounding code.
   - Done when every accepted finding carries a replacement or a route.
5. Return the handoff `../SKILL.md` `## Handoff` defines. `Status` is `BLOCKED`
   when the inventory or the scanner is unavailable, naming that input;
   `NEEDS_ACTION` when any block became a finding; `PASS` when none did. Done
   when that handoff is the final answer.

## Rules

- Never delegate. Report findings only and never edit a file: the manager routes
  each accepted finding to `/resolve-findings`.
- Review comment bytes only. Code, string literals, comment formatting, and
  comment placement stay outside this review.
- For a comment whose constraint code could and should enforce, keep the
  constraint sentence in the replacement text, drop only the process or
  navigation wording, and report the missing enforcement as a separate row whose
  replacement slot is `route: /repo-code-review`.
- Judge only the scoped bytes. Report a pre-existing comment outside a
  session-changed range as a residual and never as a finding.
- Introduce no severity beyond the `Required` and `Recommended` that
  `comment-classes.md` assigns each class.
