<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-04T21:08:29.023Z","dependsOn":[]} -->
# Codebase comment sweep with `/comment-review`

## Context

`Documents/C++StyleGuide.txt` rule 64 requires comments to describe the present
code only. Until the change that created `/comment-review`, no skill reviewed
comment content beyond the comments a session itself changed: `/code-style-review`
steps 17-18 ran on session-changed ranges only, `/repo-code-review` excludes
comment quality, and GLSL comments were reviewed by nothing. The existing body of
comments therefore never passed a content review, and co-workers respond by
deleting agent-written comments wholesale as "low value" — losing the invariant,
ordering, and threading notes mixed in with the noise.

A survey of `Common`, `Engine`, `Projects`, `Tools`, and `DataPacker` `.cpp`/`.h`
at baseline `205bbd065f35f18a06acd6a2d7cd39f06022b0d5` found about 5,000 `//`
comment blocks: about 550 of four or more lines, 68 of eight or more, and 10 of
twelve or more. The blocks fall into three groups.

- Information-free boilerplate. `Common/WindowsUtils.h:35-43` and `:46-54` carry
  Parameters/Returns/Thread-safety template fields that restate the signature on
  every function. `// ====` banner separators appear at
  `Engine/Source/Frame/Collections/CollectionMemory.h:6` and in six other files
  (`grep -n "^\s*// ={20,}"`).
- Change-history and hypothetical-path narration disguised as rationale.
  `Engine/Source/Graphics/Render/MainUniforms.cpp:458-474` is a 17-line block
  arguing that an early return is unreachable and describing what a hypothetical
  empty path would have to flush. `Engine/Source/Frame/IslandTerrain.cpp:337`
  states "results are identical to the previous per-point form".
- Genuinely valuable but overlong. `Engine/Source/File/PackChunks.cpp:859-879` is
  a 21-line thread-safety precondition whose constraint is real;
  `Engine/Source/Graphics/Managers/BufferManager.cpp:454-462` states a present
  three-point ordering chain;
  `Engine/Source/Graphics/Graphics.cpp:198-214` and
  `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersNavigation.cpp:318-331`
  are the same shape. These are shortened, never deleted.

This Plan applies the new `/comment-review` skill to that existing body. The
skill, its `references/comment-classes.md` taxonomy and preserve list, and its
scanner `.agents/skills/comment-review/scripts/Find-CommentBlocks.ps1` all land
with the change this Plan was authored in, so no dependency edge is needed.

## Design

The recommended shape is a fan-out over disjoint directory scopes, because
`/comment-review` already takes a caller-supplied `Scope` and returns findings
only.

1. Main splits the repository into disjoint directory scopes: `Common`, one scope
   per `Engine/Source/<subsystem>`, `Engine/Data/Shaders`, `Projects`, `Tools`,
   and `DataPacker`. The recommendation is to keep each scope small enough that
   one worker's findings stay reviewable in one pass rather than to minimize the
   number of workers.
2. One Sonnet `mechanic` runs `/comment-review` per scope, in parallel, with
   `Scope` supplied by the caller. The skill is findings only and never edits.
3. Main decides each finding once, applying the root `AGENTS.md` rule that review
   findings are judged rather than followed blindly. A finding whose replacement
   text would drop an invariant, required ordering, consequence, lifetime or
   threading contract, platform or driver workaround, or a relationship to code
   elsewhere is rejected; the preserve list in
   `.agents/skills/comment-review/references/comment-classes.md` is the standard.
4. Accepted findings go to `/resolve-findings`, batched per scope so one fix pass
   covers one scope's accepted rows.
5. Compile every target affected by an edited file after its scope's fixes are
   applied, through `/compile`. Comment-only edits still change preprocessed
   bytes, so the build is the evidence that nothing was truncated mid-token.

The author's recommendation is to process the 68 blocks of eight or more lines
first — they carry nearly all of the narration and the overlong-but-valuable
material — and to treat the roughly 550 blocks of four or more lines as the
second wave, so the highest-value scopes land even if the sweep is interrupted.
A further recommendation is to run the boilerplate classes (`banner`,
`template-field`) as their own quick pass, since those are mechanical deletions
that need no judgment about a surviving constraint.

Splitting the sweep across several sessions is expected and acceptable; each
directory scope is independently landable because no scope's edits can affect
another's.

## Critical files

- `.agents/skills/comment-review/SKILL.md` and
  `.agents/skills/comment-review/references/worker.md` — the dispatch contract
  and the worker steps for each scope.
- `.agents/skills/comment-review/references/comment-classes.md` — the class
  definitions, severities, length threshold, and the preserve list that decides
  what is never a finding.
- `.agents/skills/comment-review/scripts/Find-CommentBlocks.ps1` — the read-only
  candidate scanner, invoked
  `pwsh -NoProfile -File .agents/skills/comment-review/scripts/Find-CommentBlocks.ps1 -Path <root>`.
- `Documents/C++StyleGuide.txt` rule 64 — the single authority on comment
  content; the sweep applies it and never amends it.
- `Common/WindowsUtils.h:35-43`, `:46-54`;
  `Engine/Source/Frame/Collections/CollectionMemory.h:6` — the boilerplate
  exemplars.
- `Engine/Source/Graphics/Render/MainUniforms.cpp:458-474`,
  `Engine/Source/Frame/IslandTerrain.cpp:337` — the narration exemplars.
- `Engine/Source/File/PackChunks.cpp:859-879`,
  `Engine/Source/Graphics/Managers/BufferManager.cpp:454-462`,
  `Engine/Source/Graphics/Graphics.cpp:198-214`,
  `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersNavigation.cpp:318-331`
  — the valuable-but-overlong exemplars, shortened rather than deleted.

## In scope

- Comment bytes only, in `.cpp`, `.h`, and `.inl` files under `Common/`,
  `Engine/Source/`, `Projects/`, `Tools/`, and `DataPacker/`, and in the shader
  files under `Engine/Data/Shaders/`.
- Deleting a comment block entirely, or replacing it with shorter present-tense
  text that keeps every preserved-class fact.
- Whitespace-only lines that exist solely as part of a deleted comment block.

## Out of scope

- Any code byte: no declaration, statement, expression, string literal, include,
  or formatting change. A comment finding that reveals a missing runtime check is
  recorded and routed to `/repo-code-review`, never fixed here.
- `ThirdParty/`.
- `.agents/skills/comment-review/` itself — its taxonomy, thresholds, scanner
  regexes, and dispatch wiring are fixed inputs to this sweep, not targets of it.
- `Documents/C++StyleGuide.txt` and every other style-guide or instruction
  document; rule 64 is applied, not changed.
- Comments in `Documents/`, `.agents/`, and other non-source trees.
- Adding new comments where none exist today.

## Risk tier

Tier 1 (mechanical). Trigger: comment-only, behavior-preserving edits with no
public signature or invariant exposure — no changed code byte, so no
determinism/CRC, serialization or `.pack`/`kiVersion`, replay, wire, threading,
allocation, shader-binding, or project-membership surface is touched.
Compile evidence is still required for every affected target, since a truncated
or malformed comment edit is a build error.

## Acceptance criteria

- `pwsh -NoProfile -File .agents/skills/comment-review/scripts/Find-CommentBlocks.ps1 -Path <root>`
  run over each of `Common`, `Engine/Source`, `Engine/Data/Shaders`, `Projects`,
  `Tools`, and `DataPacker` reports zero blocks of kind `banner` and zero of kind
  `template-field`.
- Every finding main accepted has been applied, and every finding main rejected
  is recorded with the preserved-class fact that rejected it.
- Every target containing an edited file builds through `/compile`.
- `git diff` over the sweep shows changed bytes inside comment text only — no
  code line differs — so replay determinism and the per-tick CRC are untouched by
  construction and need no harness run.

## Notes

Origin: authored as the deferred follow-up of the session that added the
`/comment-review` skill, moved the duplicated comment checks out of
`/code-style-review`, and extended `C++StyleGuide.txt` rule 64. Baseline for
every count and citation above: `205bbd065f35f18a06acd6a2d7cd39f06022b0d5`. The
user directed this deferral explicitly; that change creates the skill only and
runs no sweep.

No `dependsOn`: the skill, its references, and its scanner land in the same
change that created this Plan, so the prerequisite is satisfied before this Plan
is ever selectable.
