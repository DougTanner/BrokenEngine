<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-15T22:36:05.471Z","dependsOn":[]} -->
# Fix: Test-DataPackerMaterializeData.ps1 — high-free-space volumes skip low-space cancellation

## Context

The `/next-plan` runtime acceptance for the claimed
`Documents/Plans/DataPacker/FileManagerWorktreeDecomposition.md` Plan used this
fixture invocation (the `<worktree-root>` placeholder keeps the session's
absolute home path out of tracked content):

```text
pwsh -NoProfile -File .agents/scripts/Test-DataPackerMaterializeData.ps1 -DataPackerExecutable '<worktree-root>\DataPacker\Platforms\VisualStudio2026\Output\DataPacker.exe'
```

Five normal cases passed. The host wrapper then reported exit `1` after this
diagnostic:

```text
WARNING: LIMITATION: cancellation setup requires sparse-file support and enough free space; fsutil output: sparse-file setup not attempted because the volume cannot satisfy the low-space warning preconditions
INCOMPLETE DataPacker --materialize-data fixture (low-space cancellation skipped)
```

The same invocation had to be repeated to recover the exact diagnostic. The
user-approved acceptance substitution uses static evidence for the active
low-space invariant, so this is no longer an acceptance blocker in the claimed
Plan.

The fixture's setup computes the sparse length from
`[IO.DriveInfo]::AvailableFreeSpace` and requires
`$conservativeAllocation + $reserveBytes -le $drive.AvailableFreeSpace`
(`.agents/scripts/Test-DataPackerMaterializeData.ps1:184-194`). On a
high-free-space volume the five-percent reserve can exceed the fixed ten
gigabyte warning gap, so the setup is not attempted. The warning, incomplete
result, and fixture exit are emitted at `:213-216`; the source's exit is `2`,
which the `/next-plan` host wrapper surfaced as `1` in this run.

The active Plan's `## In scope` is limited to behavior-preserving extraction in
`FileManager::InitializeWorktreeOutputs` and `FileManager::MaterializeOutput`
(`Documents/Plans/DataPacker/FileManagerWorktreeDecomposition.md:27-34`). The
fixture script is outside that boundary. This is therefore `/next-plan`
tooling friction, not an in-scope FileManager acceptance failure. The exact
root cause is intentionally deferred to `/next-plan-review`.

Session provenance (machine-local; not reproducible after cleanup):
- Client: codex
- Conversation session ID: none
- Worktree/branch UUID: 580d2d34-6de9-4fde-a836-02026d3430dd
- Session branch: codex/580d2d34-6de9-4fde-a836-02026d3430dd
- Worktree: .codex\worktrees\BrokenEngine\580d2d34-6de9-4fde-a836-02026d3430dd
- Landing ref: the session branch above, whose tip is the session's final
  commit and which survives exactly as long as the worktree recorded above.
  Fallback once that branch is gone:
  `git log --diff-filter=A --format=%H -- <this plan path>`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to this session alone
  (its diff limited to this session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes this worktree: Codex
  transcript discovery requires the producing worktree to remain registered.

## Design

In a new session, run `/next-plan-review <landing ref>` supplying the recorded
Codex client and landing ref. Root-cause the fixture friction from the proven
session evidence, then make the smallest fix inside the `## In scope`
boundary below. The fix must make the documented invocation complete on a
high-free-space volume without silently converting the low-space cancellation
check into a false pass, while preserving the existing link and source-length
assertions. If root-causing shows the fix lies outside that boundary, surface it
for re-planning instead of expanding scope.

## Critical files

- `.agents/scripts/Test-DataPackerMaterializeData.ps1` — sparse-file setup,
  low-space precondition, warning, and incomplete-result path

## In scope

- Root-cause investigation via `/next-plan-review`, run with the recorded
  landing ref and Codex client
- The smallest resulting fix confined to the low-space cancellation fixture
  setup and result path in
  `.agents/scripts/Test-DataPackerMaterializeData.ps1:187-216`

## Out of scope

- The landed FileManager extraction and every file named by
  `Documents/Plans/DataPacker/FileManagerWorktreeDecomposition.md`
- DataPacker runtime behavior, materialization semantics, link rollback, and
  the low-disk invariant itself
- Unrelated skills/scripts; any transcript path or transcript text in the
  repository

## Risk tier and invariants

Expected Tier 2 (scoped fixture-tool behavior); escalate if the fix reaches
build/bootstrap coordination. The fixture must still exercise genuine
low-space cancellation, preserve the Data and Attribution links and sparse
source length, retain the five existing normal cases, and never report a false
PASS merely because the host has ample free space. This script does not expose
deterministic simulation, CRC, replay, wire, serialization, shader, or runtime
allocation state. Never embed transcript paths or home paths.

## Acceptance criteria

- The recorded fixture invocation no longer ends with the high-free-space
  `LIMITATION`/`INCOMPLETE` result or require a repeated invocation solely to
  obtain its diagnostic.
- The low-space cancellation case reaches its real link and source assertions
  on the affected host class, and the existing normal cases remain passing.
- `/validate-skill` passes if a skill file changes; WorktreeCli `plan validate`
  exits `0` with `status: valid` and `code: ok`.

## Notes

This Plan is keyed to the pair (`.agents/scripts/Test-DataPackerMaterializeData.ps1`,
high-free-space volume skips the sparse low-space cancellation setup and
returns an incomplete fixture result). A later observation of the same pair is
a duplicate, not a new residual. Existing DataPacker Plans cover runtime
materialization behavior, not this fixture's host-capacity limitation.
