<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-18T14:05:17.839Z","dependsOn":[]} -->
# Fix: /compile runtime-data-mode reference lists "absent primary output" as a genuine blocker, which the DataPacker code contradicts

## Context

`.agents/skills/compile/references/runtime-data-mode.md:8` ends with:

> The only thing an agent must supply is generation *authorization*; the genuine
> blockers are validation and environment failures DataPacker reports itself — an
> unrecognized reparse point, absent primary output, or insufficient disk — never
> a user-prepared directory.

"absent primary output" is wrong. DataPacker treats an absent primary output as a
normal cold-start case and succeeds, proven from the current tree along the exact
call path an authorized Local generation takes:

- `DataPacker/Source/Main.cpp:678-684` — `MaterializeData` is the whole
  `--materialize-data` run: it constructs `FileManager` in
  `InitializationMode::kDataOnly` and returns success for anything other than
  `EnsureLocalResult::kCancelled`.
- `DataPacker/Source/FileManager.cpp:195-247` — `DiscoverLinkedWorktreeIdentity`
  never inspects the primary output directory. It checks Git common-directory
  agreement and that the output path equals the expected path; `expectedOutput`
  is a computed path, not a probed one.
- `DataPacker/Source/FileManager.cpp:472` — the primary Data source path is
  assigned unconditionally, existing or not.
- `DataPacker/Source/FileManager.cpp:484-519` — `ReconcileWorktreeOutput` throws
  only for reparse-point cases, and links only when the source
  `IsOrdinaryDirectory`. An absent source with an absent destination falls
  through both branches silently.
- `DataPacker/Source/FileManager.cpp:552-557` — `MaterializeOutput`'s decisive
  line: when the source is empty or is not an ordinary directory (which is
  exactly the absent case), it calls `create_directories` on the local
  destination, sets `OutputRootState::kLocal`, and returns
  `EnsureLocalResult::kMaterialized`. That is success, so the `/compile`
  invocation proceeds into the `RunDataPacker=true` game build, which regenerates
  the data from source assets.

Only two of the three listed blockers survive inspection: the reparse-point
throws (`:487-490`, `:493-496`, `:548-551`) and the insufficient-disk failure
(`:575-578`). The middle item does not.

The consequence is the documented risk that an agent reading this reference stops
an authorized Local generation and reports it as an environment failure, when the
tooling in fact handles the case and would have produced the data.

The clause is pre-existing. It is byte-identical before and after the recording
session's change (verified with `git diff`), which removed the data-oracle receipt
system and never touched this sentence; `/scope-review` passed on that diff, so
correcting the clause there would have been unauthorized scope expansion.

Verify the cited line numbers against the working tree before editing — they may
have moved.

Session provenance (machine-local; not reproducible after cleanup). The
inaccuracy above is proven from tracked source, so no transcript review is
required; these fields only identify where it was observed:
- Client: claude
- Worktree/branch UUID: 43d26cc1-5e1e-4e20-bb75-438923147358
- Session branch: claude/43d26cc1-5e1e-4e20-bb75-438923147358
- Worktree: .claude\worktrees\BrokenEngine\43d26cc1-5e1e-4e20-bb75-438923147358
- Landing ref: the session branch above, whose tip is the recording session's
  final commit.

## Design

Documentation only, one clause.

In `.agents/skills/compile/references/runtime-data-mode.md`, amend the final
sentence of the bullet at `:8` so the list of genuine self-reported blockers no
longer contains "absent primary output". The corrected list is the two conditions
DataPacker actually throws on along this path: an unrecognized reparse point and
insufficient disk. The rest of that bullet — that an absent worktree `Data`
output is the legitimate expected starting state, that authorization is the only
thing an agent must supply, and that a user-prepared directory is never a blocker
— is already correct and stays as written.

An absent *primary* output is, if anything, another instance of the case the
bullet already covers: the generation build creates the local directory and
proceeds. State that only if the corrected sentence would otherwise read as
though the case were unaddressed; do not add a new paragraph for it.

Deliberately not chosen: changing DataPacker. The code's behavior is the correct
one — a cold primary is not a reason to refuse a generation the caller authorized
— so the documentation is the side that is wrong.

## Critical files

- `.agents/skills/compile/references/runtime-data-mode.md` — the bullet at `:8`,
  final sentence; the only file this Plan edits
- `DataPacker/Source/FileManager.cpp` — `MaterializeOutput` (`:541-588`) and
  `ReconcileWorktreeOutput` (`:484-519`), read to confirm the corrected blocker
  list; never edited by this Plan
- `DataPacker/Source/Main.cpp` — `MaterializeData` (`:678-684`), read for the
  call path; never edited by this Plan

## In scope

- The final sentence of the `.agents/skills/compile/references/runtime-data-mode.md`
  bullet at `:8`: remove "absent primary output" from the list of genuine
  self-reported blockers and leave the remaining two conditions accurate

## Out of scope

- Every other sentence and bullet of `runtime-data-mode.md`
- `.agents/skills/compile/SKILL.md` and every other skill or reference file
- All DataPacker source, including `FileManager.cpp` and `Main.cpp`: no behavior
  change, and no change to what DataPacker treats as a blocker
- The Shared/Local mode rules, the deletion-only exception, and the Gaea guard

## Risk tier and invariants

Expected Tier 1 (documentation only, no public signature or invariant exposure).
The reference must stay fail-closed in the direction that matters: the corrected
text may not license an unauthorized generation, and the requirement that an
agent supply explicit generation authorization must survive the edit unchanged.

## Acceptance criteria

- The reference no longer names an absent primary output as a blocker, and the
  blockers it does name each map to a throw or failure reachable in
  `FileManager.cpp` on the `--materialize-data` path
- The authorization requirement and the "never a user-prepared directory" rule
  are still stated
- WorktreeCli `plan validate` exits `0` with `status: valid` and `code: ok`
