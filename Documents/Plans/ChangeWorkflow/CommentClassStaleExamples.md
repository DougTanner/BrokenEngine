<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-18T00:25:00.273Z","dependsOn":[]} -->
# Comment class examples cite lines that no longer hold their comments

## Context

`.agents/skills/comment-review/references/comment-classes.md` `## Examples`
teaches each comment class with a worked example that cites a repository
`path:line` range. Commit `65255669` ("Clarify codebase comments and record pack
reset race") rewrote or deleted the comments those citations point at, and the
examples were not updated with it. Every cited range is now stale: the named
lines hold either different prose or plain code, so a reviewer who follows a
citation to learn the class finds nothing that demonstrates it.

Verified against the current tip at `2028ccd5`:

- `Common/WindowsUtils.h:35-43` (boilerplate) — the file no longer contains
  `Parameters:`, `Returns:`, or `Thread-safety:` anywhere; the comments nearest
  the cited range, at 28 and 31-32, are plain present-tense sentences, and the
  file is 35 lines, so the cited range runs past its end.
- `Engine/Source/Frame/Collections/CollectionMemory.h:6-15` (boilerplate) — no
  `// ====` banner remains in the file; lines 6-9 are a plain prose block about
  `ForEachMemberPointer`.
- `Engine/Source/Frame/IslandTerrain.cpp:337` (history) — the file no longer
  contains the word "previous"; line 337 is code inside
  `NormalFromElevation()`.
- `Engine/Source/Graphics/Render/MainUniforms.cpp:458-474` (speculative) — that
  range is the body of `PopulateHexShield()` and the head of
  `RenderFrameMain()`; the eleven hypothetical-path lines are gone, and the
  surviving invariant note is the single line at 471.
- `Common/Log/Log.h:214` (navigation) — the file no longer contains
  `AGENTS.md`; line 214 is inside the `LOG` macro definition.
- `Engine/Source/File/PackChunks.cpp:859-879` (dense) — that range is bounds-check
  code in the audio read path with no comment in it.

Evidence for the cause: `git log --oneline -S"Thread-safety:" --
Common/WindowsUtils.h` and `git log --oneline -S"results are identical to the
previous" -- Engine/Source/Frame/IslandTerrain.cpp` both return `65255669`, and
that commit's diffstat touches `Common/WindowsUtils.h`, `Common/Log/Log.h`, and
the other cited files.

The defect is documentation-only. The class table at
`comment-classes.md:7-14`, the `## Preserve list`, and every rule 64 clause the
classes enforce are unaffected; only the `## Examples` citations are wrong.

## Design

Recommended approach, with rationale — the implementing session owns the final
call:

1. For each of the five classes that carries a citation today (boilerplate,
   history, speculative, navigation, dense), search the current tree for a live
   comment that demonstrates the class, and replace the stale citation with the
   located `path:line` range plus a short quotation of the offending text.
   Searching fresh is the reason this was deferred rather than fixed in place:
   the replacements cannot be derived from the old citations.
   `.agents/skills/comment-review/scripts/Find-CommentBlocks.ps1` already
   enumerates comment blocks with `startLine`, `lineCount`, and `firstLine` and
   is the cheapest way to shortlist `dense` and banner candidates; the other
   classes are better found with `Grep`: the class table supplies the terms for
   two of them — the history row's trigger words "now", "no longer",
   "previously", "instead of", and the navigation row's `AGENTS.md`/`CLAUDE.md`
   — while the boilerplate row (`comment-classes.md:9`) names only "template
   fields" and "banner separators", so its search terms (`Parameters:`,
   `Returns:`, `====`) have to be chosen by the implementing session.
2. Where no live instance of a class exists, follow the precedent the `false`
   bullet already sets at `comment-classes.md:34-38`: state that no repository
   instance was located and give the reader the test to apply instead of a
   citation. That keeps the file honest without inventing an example, and it is
   preferable to citing a comment that only weakly fits the class.
3. Keep each bullet's teaching content — the preserved-facts guidance in the
   boilerplate bullet, the "first two lines are preserved" split in the
   speculative bullet, the delete-versus-trim rule in the navigation bullet, and
   the shorten-never-delete rule in the dense bullet. Only the citation and any
   quoted text that belongs to the old comment change; the class semantics do
   not.

Recommended against: pinning citations to commit SHAs or removing the examples
outright. The examples are the file's only concrete teaching material, and a
SHA-pinned citation points at code a reviewer cannot read in the worktree.

## Critical files

- `.agents/skills/comment-review/references/comment-classes.md` — lines 18-40,
  the `## Examples` list. The only file this Plan changes.
- `.agents/skills/comment-review/scripts/Find-CommentBlocks.ps1` — read-only, a
  search aid for locating replacement examples.
- `Documents/C++StyleGuide.txt` rule 64 — read-only, the authority the classes
  enforce; a replacement example must still be a violation of the clause its
  class names.

## In scope

- The `## Examples` section of
  `.agents/skills/comment-review/references/comment-classes.md`: the six stale
  `path:line` citations, the text quoted from those comments, and any wording a
  replacement citation forces to change within the same bullet.

## Out of scope

- Editing any C++ or GLSL comment. This Plan corrects citations; it does not
  fix, reword, or delete the comments it ends up citing, and finding a fresh
  violation is not authorization to fix it.
- The class table at `comment-classes.md:7-14`, the `## Preserve list`, rule 64
  itself, `.agents/skills/comment-review/SKILL.md`, and
  `.agents/skills/comment-review/references/worker.md`.
- Any change to `Find-CommentBlocks.ps1`, including new output fields.
- The separate question of whether `/comment-review` should call an external
  model to shortlist blocks, recorded in
  `Documents/Investigations/JevDecisionModelWorkflowUses.md`. That
  investigation's decision 7 is what deferred this defect here; the two changes
  are independent and neither blocks the other.

## Acceptance criteria

The diff alone does not settle these, so verify each:

- Every `path:line` citation surviving in `## Examples` resolves, at the
  implementing session's tip, to a comment that exhibits the class its bullet
  names. Check by reading each cited range.
- No bullet cites a range that holds only code.
- Each of the six classes still states the same rule 64 clause and the same
  preserve/trim guidance it states today.

## Notes

- Change Workflow tier: Tier 1 (mechanical documentation). No public signature,
  invariant, determinism/CRC, serialization, wire, replay, threading,
  allocation, shader, build, or project-membership exposure; no C++ or GLSL byte
  changes, so no compile and no live verification are required.
- Risk trigger from `.agents/references/risk-tiers.md`: documentation-only
  change with no invariant exposure.
- Pre-existing: the stale citations were produced by `65255669`, which predates
  the session that recorded this follow-up.
