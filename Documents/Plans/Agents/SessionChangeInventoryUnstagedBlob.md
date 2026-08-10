<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-09T23:39:45.182Z","dependsOn":[]} -->
# Fix: Get-SessionChangeInventory.ps1 — unstaged current blob lookup fails during scope review

## Context

During the mandated focused `/scope-review` invocation, this documented command was run:

```powershell
pwsh -NoProfile -File .agents/scripts/Get-SessionChangeInventory.ps1 -RepositoryRoot <this worktree> -Baseline 388ed4033795f5ec14912fe6a887aeea2ab4e6bb -Regions -IncludeUntracked Engine/Source/Frame/Collections/CollectionLifecycle.h
```

The inventory returned an error envelope with `status:error`, `code:internal.error`, and the observed failure `git cat-file blob c1af53f932c330f484e16abd4db8c98dbad32733 failed with exit 128` / `bad file`. The state was a newly added Plan staged by `New-PlanFile.ps1` at blob `be91f403...`, followed by an accepted worktree-only correction whose current hash was `c1af53f...`; the inventory apparently hashed the current bytes and then required that hash as a Git object. This blocked the mandatory scope review and forced the manager to stage the already-reviewed correction with `git add`; after staging, `git cat-file -t c1af53f...` returned `blob` and the inventory retry succeeded.

The claimed `Documents/Plans/Frame/ReduceCollectionHeader.md` Plan was already prepared complete, and this script behavior is outside its approved `## In scope`. It is also outside the implementation scope of `Documents/Plans/Frame/ReplayDelayedCoordActivation.md`. This is tooling friction from the `/next-plan` workflow, not an active change failure. The exact root cause is intentionally deferred to `/next-plan-review`.

Session provenance (machine-local; not reproducible after cleanup):

- Client: codex
- Session: a33bd3d5-c7bf-4511-8961-b95de575b6ea
- Session branch: codex/a33bd3d5-c7bf-4511-8961-b95de575b6ea
- Worktree: .codex\worktrees\BrokenEnginePublic\a33bd3d5-c7bf-4511-8961-b95de575b6ea
- Landing commit: `git log --diff-filter=A --format=%H -- <this plan path>`
- Run the review before /cleanup-worktrees removes this worktree: Codex transcript discovery requires the producing worktree to remain registered, and Claude review requires the exact session id above.

## Design

In a new session, run `/next-plan-review <landing commit>` supplying the recorded client and session id, root-cause the friction from the proven transcript, then make the smallest fix inside the `## In scope` boundary below. If root-causing shows the fix lies outside that boundary, surface it for re-planning instead of expanding scope.

## Critical files

- `.agents/scripts/Get-SessionChangeInventory.ps1` — current/baseline identity hashing and the structured error path around `Get-InventoryBlobSha256`, `Get-InventoryFileSha256`, and `Get-InventoryIdentity` (`:112-161`).
- `.agents/scripts/Test-SessionChangeInventoryFixtures.ps1` — throwaway Git fixtures and read-only assertions for the inventory contract.

## In scope

- Root-cause investigation via `/next-plan-review` with the recorded provenance.
- The smallest resulting fix confined to `.agents/scripts/Get-SessionChangeInventory.ps1` and its fixture coverage in `.agents/scripts/Test-SessionChangeInventoryFixtures.ps1`, naming the exact identity-handling region selected by the review.
- A fixture reproducing a staged newly added Plan followed by an accepted worktree-only correction whose current content is not yet a Git object, proving the documented `-Regions -IncludeUntracked` invocation succeeds without a staging workaround.

## Out of scope

- The landed `ReduceCollectionHeader` relocation, its completed Plan, and the replay activation follow-up Plan.
- `.agents/skills/scope-review/SKILL.md`, other skills, WorktreeCli scheduler/claim behavior, and unrelated scripts unless `/next-plan-review` proves the direct caller contract is the source and returns a re-planning pivot.
- Any transcript path or transcript text in the repository; no change to staging policy, landing state, or Plan claims.

## Risk tier and invariants

Expected Tier 2 (scoped tool behavior); escalate if the fix reaches WorktreeCli scheduler or build/bootstrap coordination. Preserve the inventory's read-only guarantee, the `broken-engine-session-change-inventory/v1` envelope and pass/blocked/error exit semantics, correct byte identity for baseline blobs and current worktree/untracked files, deterministic path/region coverage, and the documented focused scope-review invocation. Never embed transcript paths or home paths.

## Acceptance criteria

- The recorded `Get-SessionChangeInventory.ps1` invocation no longer reproduces the `status:error`, `code:internal.error`, `git cat-file blob ... failed with exit 128`, or `bad file` result for the staged-then-worktree-corrected fixture; it returns a usable versioned `status:pass` inventory without requiring `git add`.
- `.agents/scripts/Test-SessionChangeInventoryFixtures.ps1` passes with coverage for the staged Plan plus worktree-only correction case, while proving the inventory writes no repository or Git metadata.
- The focused `/scope-review` workflow consumes the corrected inventory on its first documented invocation, with unchanged `entries`, `regions`, classification, and truncation contracts for existing staged, committed, and untracked cases.
- `/next-plan-review` records the proven root cause and smallest fix within the named script/test boundary; if the root cause is outside it, the issue is surfaced for re-planning instead of expanding scope.
- `/validate-skill` runs if any skill file is changed; WorktreeCli `plan validate` exits `0` with `status:valid` and `code:ok`.

## Notes

This Plan is keyed to the concrete script-plus-symptom pair (`.agents/scripts/Get-SessionChangeInventory.ps1` returning `status:error`, `code:internal.error` for `git cat-file blob c1af53f...` with exit 128 / `bad file`). A later observation of the same pair is a duplicate, not a new residual. The root cause is intentionally deferred to `/next-plan-review`; this body records the exact command, output, staging workaround, and provenance without embedding transcript material.
