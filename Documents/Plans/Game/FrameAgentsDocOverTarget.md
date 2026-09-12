<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-12T13:34:52.629Z","dependsOn":[]} -->
# Trim the game Frame AGENTS.md back under its advisory size budget

## Context

`Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md` is over its advisory
code-scaled size budget. Run from the session worktree root,

```
pwsh -NoProfile -File .agents/skills/update-claude-docs/scripts/Get-AffectedAgentsDocs.ps1 Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersNavigation.cpp
```

reports for that document `codeTokens` 13130, `directChildDocumentCount` 1,
`budgetRule` `formula`, `budget` 1610, `tokens` 1670, `verdict` `over-target` —
60 tokens over. Every other document in the same chain reports `ok`, and the
chain total is 6416 tokens, well under the 15,000 effective-chain target, so the
excess is confined to this one document.

The verdict is pre-existing debt: it reproduces at baseline
`230ee75da8ff6dee88d83156cc987d83cb432107`, and the session that observed it
changed only `PlayersNavigation.cpp`, never this document.

## Design

Recommended approach: one documentation-only editing pass over
`Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md`, applying the removal
order in `.agents/skills/update-claude-docs/references/content-rules.md`
`## Removing Text` — navigation pointers, repeated introductions, duplicated
mechanics, and redundant prose first, and never an operative rule or contract.

Trim candidates the author recommends considering first, in this order, with the
rationale for each:

- The `## See Also` pointer to `Collections/AGENTS.md`. It is pure navigation to
  the document's own direct child, which a session reaches through the
  directory it is already reading.
- The `FrameInput` invariant bullet. It is by far the longest bullet and repeats
  its bump rationale across the replay version check, the difference stream's
  post-dispatch channel, and the client-only-member exception. Each of those is
  an operative rule and must survive; the recommendation is to compress the
  shared rationale to one statement that all three cite, not to drop a case.
- The two server-counter bullets (`GetServerCellStats`,
  `PublishServerEntityCounts`). They restate the same "do not add a second
  writer / second way in" shape twice and can share one statement of it while
  each keeps its own ownership claim.

Whether one, two, or all three candidates are needed is decided by re-running
the script above after each edit and stopping as soon as the verdict turns
`ok`. If the remaining text is all operative guidance and the document is still
over budget, keep the guidance and report the advisory excess as a residual —
`content-rules.md` states an over-budget verdict never justifies deleting an
operative rule.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md` — the over-target
  document; the only file this Plan changes.
- `.agents/skills/update-claude-docs/scripts/Get-AffectedAgentsDocs.ps1` — the
  owner of the budget calculation and of the pass/fail verdict this Plan is
  measured by.
- `.agents/skills/update-claude-docs/references/content-rules.md` — the removal
  order and the rule that protects operative rules from a size trim.

## In scope

- Wording and structure of `Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md`
  only: its `## Overview`, its `## Invariants` bullets, and its `## See Also`
  section.
- Removing navigation pointers, duplicated mechanics, and redundant prose from
  that document, and compressing long bullets whose operative content is
  preserved.

## Out of scope

- Any other `AGENTS.md` or `CLAUDE.md`, including the sibling
  `Projects/BrokenEngineSandbox/Source/Frame/CLAUDE.md` stub and the
  `Collections/` documents below it.
- Deleting or weakening any operative rule, invariant, ownership claim, or
  contract in the document, and moving one to another document.
- All C++, GLSL, project-membership, and script changes, including any change to
  `Get-AffectedAgentsDocs.ps1` or its budget formula.

## Acceptance criteria

- `pwsh -NoProfile -File .agents/skills/update-claude-docs/scripts/Get-AffectedAgentsDocs.ps1 Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersNavigation.cpp`
  reports `verdict` `ok` for `Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md`,
  or the change reports the remaining advisory excess as a residual with the
  operative rules that caused it.
- Every invariant, ownership claim, and cross-document contract present in the
  document at this Plan's creation is still stated in it.
- `git status` shows no changed file other than that one document.

## Notes

- Change Workflow Tier 1, by the `risk-tiers.md` documentation trigger: the
  change is documentation only, with no public signature or invariant exposure.
- No build is required and no harness run is required.
