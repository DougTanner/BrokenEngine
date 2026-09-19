<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-19T15:08:23.492Z","dependsOn":[]} -->
# Fix: /update-claude-docs — stub-pair sweep reports `stub.missing` for every AGENTS.md

## Context
Observed symptom: `pwsh -NoProfile -File .agents/skills/update-claude-docs/scripts/Get-AffectedAgentsDocs.ps1 Engine/Source/Graphics/Managers/TextureManager.cpp` returned a `pass` payload whose `stubPairs` listed `stub.missing` ("No sibling CLAUDE.md at ...") for every directory `AGENTS.md` in the repository — 78 items. The sweep at `.agents/skills/update-claude-docs/scripts/Get-AffectedAgentsDocs.ps1:492-514` requires a sibling `CLAUDE.md` containing `@AGENTS.md` beside each directory `AGENTS.md`, but commit 8786cb6e ("Remove AGENTS.md", an ancestor of the session baseline 82a480ac) deleted every `CLAUDE.md` stub, and `git ls-files -- '*CLAUDE.md'` returns no tracked file. Under `references/worker.md` step 2 (`stubPairs`: "Fix a reported defect inside the authorized scope in the same edit; report one outside that scope as a residual") the `/update-claude-docs` implementer had to investigate Git history to explain the 78 items and returned them as a Residuals row. The same repository-wide noise recurs on every sync dispatch, because the contract contradicts the repository state on every run.

No document outside the skill asserts that stubs exist: the root `AGENTS.md` names no stub contract, and the remaining `CLAUDE.md` mentions in `.agents/scripts/Get-SessionChangeInventory.ps1`, `.agents/scripts/Test-CitationSupport.ps1`, `.agents/skills/compile/scripts/Resolve-CompileContext.ps1`, `Documents/Plans/AGENTS.md`, `Tools/WorktreeCli/PlanMetadata.cpp`, `/progressive-disclosure-review`, `/comment-review`, and `.agents/references/change-workflow.md` only classify or exempt the filename and stay correct with no stubs present. The stub contract lives solely in the `/update-claude-docs` package: `SKILL.md` (description and `## Purpose`), `references/worker.md` (step 2 `stubPairs`, step 5 "create its sibling `CLAUDE.md` stub", step 6 "confirm stub bytes" / "stub integrity" / "AGENTS.md and stub as a proposed deletion"), `references/content-rules.md` `## Stub Contract`, and `references/audit-mode.md` (stub discovery, bidirectional check, "Stub findings" report row, and stub rule in audit-and-fix). Authority order: the repository state (stubs deliberately removed by 8786cb6e) is trusted over the skill's stale stub contract.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: d185b1a1-7d07-4fa5-91d7-783006468425
- Worktree/branch UUID: 38c38a05-226e-46a8-bc3f-574867481389
- Session branch: claude/38c38a05-226e-46a8-bc3f-574867481389
- Worktree: .claude\worktrees\BrokenEngine\38c38a05-226e-46a8-bc3f-574867481389
- Landing ref: claude/38c38a05-226e-46a8-bc3f-574867481389
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/RemoveClaudeStubSweep.md`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to one session alone
  (its diff limited to that session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`; the cause is already identified there (the stub contract outlived the stubs), so the transcript should not be needed. Only when it genuinely is, in a new session run `/next-plan-review claude/38c38a05-226e-46a8-bc3f-574867481389` in bounded friction mode, supplying the recorded client and conversation session ID.

The author's recommendation is to remove the stub contract from the skill package so it matches the no-stub repository, rather than recreating 78 stubs or adding an exclusion: in `Get-AffectedAgentsDocs.ps1` drop the `stubPairs` result field, `$script:StubSweepExclusions`, `$script:StubBody`, `Test-StubSweepExcluded`, the stub sweep loop, the stub arguments of `Complete-AffectedAgentsDocs` and their trimming in its output-cap loop, and the header comment's sweep mention, keeping `CLAUDE.md` out of `$script:DocumentFileNames` only if nothing else in the script depends on it; in `worker.md` delete the `stubPairs` bullet and the stub clauses of steps 5 and 6 (and the "sweeping stub pairs" example in step 2); in `SKILL.md` remove "and sibling CLAUDE.md import stubs" from the description and `## Purpose` and the `CLAUDE.md` path option from `## Handoff`; delete `content-rules.md` `## Stub Contract`; and remove the stub discovery, bidirectional check, "Stub findings" row, and stub rule from `audit-mode.md`. If root-causing shows the fix lies outside the `## In scope` boundary below, surface it for re-planning instead of expanding scope.

## Critical files
- `.agents/skills/update-claude-docs/scripts/Get-AffectedAgentsDocs.ps1`
- `.agents/skills/update-claude-docs/SKILL.md`
- `.agents/skills/update-claude-docs/references/worker.md`
- `.agents/skills/update-claude-docs/references/content-rules.md`
- `.agents/skills/update-claude-docs/references/audit-mode.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the files named above: the stub sweep and its supporting variables, functions, result field, and comment in `Get-AffectedAgentsDocs.ps1`; the stub clauses of `worker.md` steps 2, 5, and 6; the stub wording in `SKILL.md` description, `## Purpose`, and `## Handoff`; `content-rules.md` `## Stub Contract`; and the stub checks and report row in `audit-mode.md`

## Out of scope
- The landed change the session produced (`Engine/Source/Graphics/Managers/TextureManager.cpp`)
- Recreating `CLAUDE.md` stubs; the `chains` and `sizes` discovery and token-budget logic of `Get-AffectedAgentsDocs.ps1`
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior of one skill's script and prose); escalate if the fix reaches build/bootstrap coordination. Never embed transcript paths or home paths.

## Acceptance criteria
- `pwsh -NoProfile -File .agents/skills/update-claude-docs/scripts/Get-AffectedAgentsDocs.ps1 Engine/Source/Graphics/Managers/TextureManager.cpp` returns a `pass` payload with no stub items and no `stubPairs` field
- No file under `.agents/skills/update-claude-docs/` mentions a `CLAUDE.md` stub
- The static-checks runner, invoked as `.agents/references/static-checks.md` documents it, reports every row the change triggers passing
