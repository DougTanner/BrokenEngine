# Comment Review Worker

The numbered steps and the judgment rules for `/comment-review`. The public
contract main reads is [`../SKILL.md`](../SKILL.md).

## Steps

1. Fix the review scope and scanner mode.
   - Caller-supplied file or directory scope uses `WholeFile` mode (`-Path`).
   - Session-changed scope uses `Session` mode (`-RepositoryRoot '<absolute
     repository toplevel>' -Baseline <baseline SHA>`, adding `-Head <SHA>`
     with the review row's `Head` when the brief names one, else
     `-IncludeUntracked` when the caller supplied untracked files).
   - Done when the scope is fixed and stated as session-changed or
     caller-supplied.
2. Run the bundled scanner directly (`Get-ScannedFile` and `Get-SessionFile` own
   path deduplication):
   - Session mode:
     `pwsh -NoProfile -NonInteractive -File
     .agents/skills/comment-review/scripts/Find-CommentBlocks.ps1` with the
     step-1 `Session` parameters.
   - `WholeFile` mode (caller-supplied scope): pass `-Path <path>`, or
     `pwsh -NoProfile -NonInteractive -Command "&
     '.agents/skills/comment-review/scripts/Find-CommentBlocks.ps1' -Path
     'a','b'"` for multiple paths.
   - Require `status` `pass`, `schemaVersion` `broken-engine-comment-blocks/v3`
     (reject older versions), and `truncated` false; any other `status` means
     the block list is unavailable — report `BLOCKED` instead of hand-hunting.
   - Take `Blocks scanned` from `counts.total` for each original scan scope;
     when `truncated` is true, split by `-PathPrefix` (`Session` mode) or into
     subdirectories or file groups (`WholeFile` mode) to retrieve the `blocks`
     rows until every part reports `truncated` false. If a single file still
     reports `truncated` true, report `BLOCKED` naming that file instead of
     looping or hand-hunting.
   - Filter the returned blocks to the scope: for caller-supplied scope, keep
     all returned blocks; for session-changed scope, keep blocks with
     `overlapsRegion` true.
   - Done when the filtered `blocks` rows are in hand, or `BLOCKED` is reported.
3. Read `Documents/C++StyleGuide.txt` rule 64 and
   [`comment-classes.md`](comment-classes.md), then classify every scanned block
   by reading the surrounding code. The rows' `kinds` and `charCount` are a
   starting list, not the finding set: a block with no kind can still be a
   finding, and a kind hit is rejected when the code shows the comment states a
   present constraint. Judge `dense` from `charCount` and from facts the block
   or its function already states elsewhere.
   - Done when every scanned block is accepted with one class or rejected.
4. Write each finding's replacement: `delete`, or the shortest present-tense
   text that keeps every preserved-class fact `comment-classes.md` lists and
   itself fails no class there.
   - Route the block instead of guessing whenever the surviving constraint is
     not evident from the comment and the surrounding code.
   - Done when every accepted finding carries a replacement or a route.
5. Return the handoff `../SKILL.md` `## Handoff` defines. `Status` is `BLOCKED`
   when the inventory or the scanner is unavailable, naming the missing input;
   `NEEDS_ACTION` when any block became a finding; `PASS` otherwise. Done when
   that handoff is the final answer.

## Rules

- Never delegate. Report findings only and never edit a file.
- Review comment bytes only. Code, string literals, comment formatting, and
  comment placement stay outside this review.
- For a comment whose constraint code could and should enforce, keep the
  constraint sentence in the replacement text, drop only the process or
  navigation wording, and report the missing enforcement as a separate row whose
  replacement slot is `route: /repo-code-review`.
- Raise fresh findings only on the blocks step 2 kept. Report a pre-existing
  comment outside them as a residual and never as a finding.
- Give each finding the severity the `comment-classes.md` `severity` column
  assigns its class.
