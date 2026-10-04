<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-04T18:22:55.741Z","dependsOn":[]} -->
# Fix: /sweep — record the Engine style-sweep stage's propagation and cleanup lessons

## Context
Observed symptoms during the `/next-plan` run of
`Documents/Plans/Engine/StyleGuideSweepEngine.md` (the Engine style sweep
stage). Each one forced rework in the main session:

1. The PROPAGATE step rewrote identifiers inside `Documents/C++StyleGuide.txt`
   rule examples (it produced `!(!gpReplay->mReplayWriters.empty())` in a rule
   example), although the stage Plan excluded style-guide changes. The propagate
   prompt `.agents/skills/sweep/references/prompts/Propagate.md` tells the agent
   to update "AGENTS.md / docs that name the identifier" and reads only the
   type's `## Find rules` and `## Fix bound`; neither section of
   `.agents/skills/sweep/references/types/style-guide.md` excludes the style
   guide itself. Main reverted the edit by hand.
2. Two propagation failure modes, each caught by a per-batch build: removing the
   rule-49 forwarding wrappers in DynamicPipelines (`CreatePipeline*`) inlined
   `data::kShaders*Crc` constants into six `*Render.cpp` callers that lacked
   `#include "Data/Shader.h"`; qualifying global `towupper` as `std::towupper`
   in `Common/ExternalHeaders.h` lacked `<cwctype>`. The current
   `.agents/skills/sweep/SKILL.md` `## Rules` list of propagation failure modes
   names neither (it names a rename leaving callers of a removed member, and a
   helper script writing stray text).
3. Inlining a removed boolean accessor (`Replay::IsRecording()`) at negated call
   sites produced double negations `!(!x.empty())` at five sites in
   `Engine/Source/GameBase.cpp` and
   `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationFixtures.cpp`;
   the stage cleanup pass missed them, and the style-guide type's
   `## Cleanup checklist` does not list the pattern.

A fourth candidate — main itself appending the ledger to the deferred-fixes
record — is already resolved on the current tree: `Invoke-Sweep.ps1 -Close`
writes the ledger, so it is not part of this Plan.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 0b08a46b-ef82-4a44-a88e-e0397475294e
- Worktree/branch UUID: eae0234f-5ca6-4a98-9443-3a782dcf90e2
- Session branch: claude/eae0234f-5ca6-4a98-9443-3a782dcf90e2
- Worktree: .claude\worktrees\BrokenEngine\eae0234f-5ca6-4a98-9443-3a782dcf90e2
- Landing ref: claude/eae0234f-5ca6-4a98-9443-3a782dcf90e2, whose tip is that
  session's final commit and which survives exactly as long as the worktree
  recorded above.
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/SweepStyleGuideStageLessons.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review claude/eae0234f-5ca6-4a98-9443-3a782dcf90e2` in bounded
friction mode, supplying client `claude` and the conversation session ID above.
Then make the smallest fix inside the `## In scope` boundary below. If
root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

Recommended edits (author's recommendation): style-sweep-specific lessons go to
the `style-guide` type, general-method lessons to the skill's `## Rules`:
1. `style-guide.md` `## Fix bound`: `Documents/C++StyleGuide.txt` is the rule
   authority and is never edited by a sweep, rule examples included. `## Fix
   bound` is recommended over `Propagate.md` because the propagate, fix, and
   cleanup prompts all read that section.
2. `SKILL.md` `## Rules`, the propagation failure-modes bullet: add a fix that
   moves a symbol or constant into a file, or qualifies a C runtime name with
   `std::`, without adding the header that declares it.
3. `style-guide.md` `## Cleanup checklist`: add a double negation such as
   `!(!x.empty())` left by inlining a removed boolean accessor at a negated call
   site.

## Critical files
- `.agents/skills/sweep/references/types/style-guide.md`
- `.agents/skills/sweep/SKILL.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to `style-guide.md` `## Fix bound` and
  `## Cleanup checklist`, and the propagation failure-modes bullet in
  `SKILL.md` `## Rules`

## Out of scope
- The landed change the session produced (the Engine style sweep stage)
- `Invoke-Sweep.ps1` and the prompt templates under
  `.agents/skills/sweep/references/prompts/`, unless root-causing shows the
  type sections cannot carry a lesson
- The deferred-fixes ledger write, already done by `Invoke-Sweep.ps1 -Close`
- `Documents/C++StyleGuide.txt` itself
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Change Workflow Tier 1. Trigger: documentation-only edits to skill instruction
prose, with no script or behavior change. Never embed transcript paths or home
paths.

## Acceptance criteria
- `style-guide.md` excludes `Documents/C++StyleGuide.txt` from sweep edits and
  lists the double-negation regression; `SKILL.md` `## Rules` names the
  missing-include propagation failure mode
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
