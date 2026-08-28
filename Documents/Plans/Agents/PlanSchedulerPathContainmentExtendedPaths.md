<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-28T01:09:39.999Z","dependsOn":[]} -->
# Use extended paths for the plan-scheduler path-containment check

## Context

`IsPathBelow` in WorktreeCli's plan scheduler canonicalizes both of its
arguments with direct `std::filesystem` calls that never route the path through
`ExtendedLengthPath`:

- `Tools/WorktreeCli/PlanScheduler.cpp:137` —
  `std::filesystem::weakly_canonical(rChild, error)`.
- `Tools/WorktreeCli/PlanScheduler.cpp:142` —
  `std::filesystem::weakly_canonical(rParent, error)`.

Either failure returns `false` from the function.

The only caller is `RemovePlanAtomicTemporarySiblings`
(`Tools/WorktreeCli/PlanScheduler.cpp:167-209`), the terminal `complete`/
`reject` cleanup of the hidden temporary siblings written next to a Plan file.
It calls the check twice on coordination paths:

- `:170` — `IsPathBelow(planPath, rWorktree)`, where `planPath` is
  `rWorktree / rPlanPath` (`:169`). A `false` result returns `false` from the
  cleanup, which is the `orphan-cleanup-failed` outcome.
- `:191` — `IsPathBelow(temporaryPath, rWorktree)` per candidate sibling, where
  `temporaryPath` is `parent / filename` (`:190`). A `false` result skips that
  sibling.

This violates the invariant stated at `Tools/ToolCommon/AGENTS.md` line 10:
every raw Win32 file call and every direct `std::filesystem` operation on a
coordination path passes its path through `ExtendedLengthPath`, because
`std::filesystem` forwards paths to Win32 unchanged. `weakly_canonical` opens
the existing leading portion of the path through Win32, so past the legacy
260-character limit it can fail on a deep checkout. The check then answers
`false` for a path that is genuinely inside the worktree, and terminal
preparation reports `orphan-cleanup-failed` even though the sibling is present
and below the worktree.

The same function is already inconsistent with its own file-local neighbours,
which all convert the identical values: the directory scan at `:177`
(`std::filesystem::directory_iterator(ExtendedLengthPath(parent), error)`), the
attribute probe at `:200`
(`::GetFileAttributesW(ExtendedLengthPath(temporaryPath).c_str())`), and the
delete at `:205` (`::DeleteFileW(ExtendedLengthPath(temporaryPath).c_str())`).

Practical severity is bounded exactly as in the landed
`LandingLockWorktreeExtendedPaths` fix and in the sibling
`PlanSchedulerExtendedPathProbes` Plan: reaching the boundary needs a checkout
deep enough that the total path exceeds the legacy limit. The value is
invariant conformance and correct behavior at the boundary, not a currently
reproducible user-facing outage.

## Design

The recommended fix converts both `weakly_canonical` arguments with the
existing `ExtendedLengthPath` helper (`Tools/ToolCommon/ToolCliCommon.cpp:351-365`,
declared in `ToolCliCommon.h:78-80`) at the argument position inside
`IsPathBelow`, matching the file-local precedent at `:177`, `:200`, and `:205`
and the argument-position wrapping used by the landed
`LandingLockWorktreeExtendedPaths` change.

Rationale for converting inside `IsPathBelow` rather than at its two call
sites: the function's result comes from comparing the two canonicalized paths
component by component (`:147-155`), so the two spellings must stay consistent
with each other. Converting inside the function is the only place that
guarantees both sides receive identical treatment no matter which caller is
added later; converting at one call site and not the other would compare a
`\\?\`-prefixed path against an unprefixed one and make the check answer
`false` for a path that is in fact below the parent.

Consistency of the compared spellings is what makes the wrap safe. Both
arguments go through the same helper and then the same `weakly_canonical`
call, so whatever spelling that pair produces — prefixed or stripped back to a
plain drive path by the implementation's final-path resolution — it is the same
spelling on both sides, and the component-prefix comparison is unchanged.

Two points the implementation should confirm from current source before
editing, because the recommendation above depends on them:

- Both arguments are absolute when they reach `IsPathBelow`.
  `ExtendedLengthPath` deliberately returns a non-absolute or already-prefixed
  path unchanged (`ToolCliCommon.cpp:360-363`), so a relative worktree argument
  would be left unprefixed while the child was prefixed, reintroducing the
  mismatch the paragraph above rules out. Both values derive from the
  `--worktree` argument; confirm the command layer has already made that value
  absolute, and if it has not, make the paths absolute before converting rather
  than widening the change to the argument-parsing layer.
- No prefixed spelling escapes. `IsPathBelow` returns only `bool` and its two
  local canonicalized paths do not leave the function, so nothing prefixed can
  be logged, persisted, hashed, or handed to Git. Confirm this still holds
  after the edit.

No behavior change is intended for ordinary short paths: the helper leaves an
already-prefixed or non-absolute-drive path alone, and today's `complete` and
`reject` results stay identical.

## Critical files

- `Tools/WorktreeCli/PlanScheduler.cpp:134-156` — `IsPathBelow`, the two
  `weakly_canonical` calls and the component comparison.
- `Tools/WorktreeCli/PlanScheduler.cpp:167-209` —
  `RemovePlanAtomicTemporarySiblings`, the only caller, and the file-local
  precedent at `:177`, `:200`, and `:205`.
- `Tools/ToolCommon/ToolCliCommon.cpp:351-365` and `ToolCliCommon.h:78-80` —
  `ExtendedLengthPath` contract (read-only reference).
- `Tools/ToolCommon/AGENTS.md` — the path-conversion invariant this change
  satisfies.

## In scope

- Routing both `std::filesystem::weakly_canonical` arguments inside
  `IsPathBelow` (`Tools/WorktreeCli/PlanScheduler.cpp:134-146`) through
  `ExtendedLengthPath`, keeping the two compared spellings consistent.
- Making those two paths absolute inside `IsPathBelow` only if the confirmation
  step in `## Design` shows an argument can arrive relative.
- Keeping the component comparison at `:147-155` and both call sites at `:170`
  and `:191` on their existing values and semantics.

## Out of scope

- The two `std::filesystem::exists` probes owned by
  `Documents/Plans/Agents/PlanSchedulerExtendedPathProbes.md`
  (`Tools/WorktreeCli/PlanMetadata.cpp:138` and
  `Tools/WorktreeCli/PlanScheduler.cpp:996`).
- `CanonicalizeDirectoryPath` and its long-path or UNC handling, owned by the
  landed coordination canonicalization work.
- `Tools/WorktreeCli/LandingLockLifecycle.cpp`, converted by
  `LandingLockWorktreeExtendedPaths`.
- Replacing the canonicalizing containment check with a purely lexical one, or
  otherwise changing what the check defends against (see `## Notes`).
- Any other scheduler behavior: selection order, claim lifecycle, dependency
  rewriting, the temporary-filename shape, diagnostic wording, the JSON
  contract, or the `orphan-cleanup-failed` classification itself.
- Argument parsing and validation of `--worktree`, manifest changes,
  `longPathAware`, or global Windows long-path policy.
- Auditing call sites outside `Tools/WorktreeCli/PlanScheduler.cpp`.

## Risk tier and invariants

Expected Change Workflow Tier 2 (scoped tool behavior). Trigger: WorktreeCli
runtime behavior at a Windows filesystem boundary, confined to one function in
one subsystem. It is not Tier 3 because it changes no shared ToolCommon code,
no persisted schema, no build/bootstrap coordination, and no independently
owned second subsystem.

Preserve these invariants:

- Extended-length spellings are arguments to the OS only, never logged,
  persisted, hashed, or passed to Git.
- The two spellings `IsPathBelow` compares are always produced the same way, so
  the containment answer never depends on prefixing.
- A path outside the worktree still answers `false`, and terminal cleanup still
  refuses to delete anything outside it.
- Scheduler results for existing short-path worktrees are byte-for-byte the
  same as today.
- No Frame CRC, wire, serialization, or `.pack` behavior is involved.

## Acceptance criteria

- Source inspection shows both `weakly_canonical` calls receiving an
  `ExtendedLengthPath`-converted path, produced identically for child and
  parent, with no prefixed value leaving the function.
- The scheduler fixture scenarios pass against the built candidate with
  `pwsh -NoProfile -File .agents/scripts/Test-WorktreeCliPlanScheduler.ps1 -WorktreeCliExecutable <candidate path>`.
- WorktreeCli Release|x64 builds pass through `/compile`.

## Notes

Found as a proven out-of-scope residual while preparing
`Documents/Plans/Agents/PlanSchedulerExtendedPathProbes.md`, and independently
confirmed by that Plan's audit (finding PA-F-001). It is a separate change
because wrapping the `weakly_canonical` arguments alters the spellings the
containment comparison consumes, which needed its own decision, whereas the
sibling Plan's two `exists` probes are self-contained boolean answers.

An alternative considered and not recommended: drop `weakly_canonical` and do a
purely lexical containment check on `lexically_normal` paths, which would
remove the OS call and the long-path exposure entirely. It is not recommended
because it silently narrows what the check defends against — `weakly_canonical`
resolves symlinks and junctions in intermediate directories, and the cleanup's
own reparse-point rejection at `:201` covers only the final temporary file, not
the directory chain above it. Changing that defense is a separate decision, so
it is listed under `## Out of scope`.

No dependency edge is recorded. This function is independent of the sibling
Plan's two probes, of `CanonicalizeDirectoryPath`, and of the landed
`LandingLockWorktreeExtendedPaths` change; the sibling Plan edits
`PlanScheduler.cpp` around line 996, far from this function, so the two can
land in either order.
