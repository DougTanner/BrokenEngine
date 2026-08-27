<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-27T22:13:01.831Z","dependsOn":[]} -->
# Use extended paths for the two plan-scheduler existence probes

## Context

Two direct `std::filesystem::exists` calls on coordination paths inside
WorktreeCli's plan scheduler never route their path through
`ExtendedLengthPath`:

- `Tools/WorktreeCli/PlanMetadata.cpp:138` —
  `std::filesystem::exists(rPlan.diskPath, error)`, which decides whether an
  unreadable Plan is reported as `missing` or as
  `could not read plan bytes`.
- `Tools/WorktreeCli/PlanScheduler.cpp:996` —
  `std::filesystem::exists(targetDiskPath, targetError)`, the terminal
  `complete`/`reject` presence probe whose result drives the `plan-untracked`
  state conflict and the later delete.

This violates the invariant stated at `Tools/ToolCommon/AGENTS.md` line 10:
every raw Win32 file call and every direct `std::filesystem` operation on a
coordination path passes its path through `ExtendedLengthPath`, because
`std::filesystem` forwards paths to Win32 unchanged. Past the legacy
260-character Win32 limit an unprefixed `exists` answers `false` without
setting a meaningful error, which would misreport a present Plan as `missing`
and would let terminal preparation skip the untracked-target guard.

Both sites are provably inconsistent with the file-local code that handles the
same value:

- `PlanMetadata.cpp:23` — `ReadBytes` opens the very same `rPlan.diskPath`
  through `std::ifstream input(ExtendedLengthPath(rPath), std::ios::binary)`.
  Line 135 hands that value to `ReadBytes` three lines before line 138 probes
  the identical value raw, so the converted open and the unconverted probe sit
  on the same failure path.
- `PlanScheduler.cpp:1056` — the delete of the same `targetDiskPath` uses
  `::DeleteFileW(ExtendedLengthPath(targetDiskPath).c_str())`, while line 996
  probes it raw.

These are the only two remaining unprefixed `std::filesystem` coordination
probes found in `Tools/WorktreeCli`; every other call site in that directory
and in `Tools/ToolCommon` already converts.

Practical severity is bounded in the same way the landed
`LandingLockWorktreeExtendedPaths` fix was: reaching the boundary needs a
checkout deep enough that the total path exceeds the legacy limit. The value of
the fix is invariant conformance and correct behavior at the boundary, not a
currently reproducible user-facing outage.

## Design

Each OS-facing path is converted with the existing `ExtendedLengthPath` helper
(`Tools/ToolCommon/ToolCliCommon.cpp:351-365`, declared in
`ToolCliCommon.h:78-80`) at the argument position of the two `exists` calls,
exactly as the file-local precedent already does at `PlanMetadata.cpp:23` and
`PlanScheduler.cpp:1056`. The stored `diskPath` and `targetDiskPath` values
themselves stay unprefixed, so no other consumer changes. That local wrap is
the complete fix; nothing beyond the two call sites changes.

Current source settles both questions the wrap depends on:

- Both values reach Win32 unprefixed today. `rPlan.diskPath`
  (`PlanMetadata.cpp:196`, built as `rWorktree / path`) and `targetDiskPath`
  (`PlanScheduler.cpp:994`, built as `worktree / target`) are plain joins with
  no upstream canonicalization or prefixing, and each already has a sibling
  call on the identical value that must convert it to be correct —
  `PlanMetadata.cpp:23` and `PlanScheduler.cpp:1056`.
- Neither value, nor anything derived from it, is logged, persisted, hashed, or
  passed to Git, so the prefixed spelling cannot escape the OS call. The four
  `diskPath` references are the declaration (`PlanMetadata.h:15`), `ReadBytes`
  (`:135`), this `exists` (`:138`), and the assignment (`:196`); the three
  `targetDiskPath` references are the assignment (`PlanScheduler.cpp:994`),
  this `exists` (`:996`), and the prefixed `DeleteFileW` (`:1056`). The
  reported JSON uses the normalized repo-relative `plan.path` instead.

No behavior change is intended for ordinary short paths: the helper leaves an
already-prefixed or non-absolute-drive path alone, so today's `validate`,
`list`, `claim-next`, `complete`, and `reject` results stay identical.

## Critical files

- `Tools/WorktreeCli/PlanMetadata.cpp:133-142` — `ParsePlan`, and `:23` and
  `:196` for the same value's prefixed read and its construction.
- `Tools/WorktreeCli/PlanScheduler.cpp:994-1004` and `:1056` — the terminal
  presence probe and the prefixed delete of the same path.
- `Tools/ToolCommon/ToolCliCommon.cpp:351-365` and `ToolCliCommon.h:78-80` —
  `ExtendedLengthPath` contract (read-only reference).
- `Tools/ToolCommon/AGENTS.md` — the path-conversion invariant this change
  satisfies.

## In scope

- Routing `std::filesystem::exists` at `PlanMetadata.cpp:138` and at
  `PlanScheduler.cpp:996` through `ExtendedLengthPath` at the argument
  position.
- Keeping every other consumer of `rPlan.diskPath` and `targetDiskPath` on its
  existing spelling.

## Out of scope

- `CanonicalizeDirectoryPath` and its long-path or UNC handling, owned by the
  landed coordination canonicalization work.
- `Tools/WorktreeCli/LandingLockLifecycle.cpp`, whose three probes were
  converted by `LandingLockWorktreeExtendedPaths`.
- Any other scheduler behavior: selection order, claim lifecycle, dependency
  rewriting, diagnostic wording, JSON contract, or the `plan-untracked` and
  `missing` classifications themselves.
- Manifest changes, `longPathAware`, or global Windows long-path policy.
- Auditing call sites outside `Tools/WorktreeCli/PlanMetadata.cpp` and
  `Tools/WorktreeCli/PlanScheduler.cpp`.

## Risk tier and invariants

Expected Change Workflow Tier 2 (scoped tool behavior). Trigger: WorktreeCli
runtime behavior at a Windows filesystem boundary, confined to two call sites
in one subsystem. It is not Tier 3 because it changes no shared ToolCommon
code, no persisted schema, no build/bootstrap coordination, and no
independently owned second subsystem.

Preserve these invariants:

- Extended-length spellings are arguments to the OS only, never logged,
  persisted, hashed, or passed to Git.
- Scheduler results for existing short-path worktrees are byte-for-byte the
  same as today.
- No Frame CRC, wire, serialization, or `.pack` behavior is involved.

## Acceptance criteria

- Source inspection shows both `std::filesystem::exists` calls receiving an
  `ExtendedLengthPath`-converted path, with no prefixed value reaching a
  reported, persisted, hashed, or Git-bound field.
- The scheduler fixture scenarios pass against the built candidate with
  `pwsh -NoProfile -File .agents/scripts/Test-WorktreeCliPlanScheduler.ps1 -WorktreeCliExecutable <candidate path>`.
- WorktreeCli Release|x64 builds pass through `/compile`.

## Notes

Found as a pre-existing, out-of-scope residual during the
`LandingLockWorktreeExtendedPaths` session, which converted the three
`std::filesystem` calls in `Tools/WorktreeCli/LandingLockLifecycle.cpp` and was
deleted at completion. No dependency edge is recorded: these two call sites are
independent of that change and of `CanonicalizeDirectoryPath`.
