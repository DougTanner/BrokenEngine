<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:29:46.197Z","dependsOn":[]} -->
# Reject duplicate coordinates in staged grid saves

## Context

The accepted finding `CAI/shard-0013/006` identifies a whole-grid identity gap.
`ReadGridSave` inserts each serialized coordinate with `try_emplace` but ignores
the insertion result, then overwrites the existing `CoordFrames`
(`Engine/Source/File/GridSave.cpp:104-113`). It checks only that the followed
cell exists after the loop (`:135-141`), not that the staged map size equals
`iFrameCount`. Two complete records with one duplicated coordinate therefore
load successfully while the later row silently discards an entire cell.

The source shard's frozen/live target hashes match audit baseline
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`; current status contains only the
six pre-existing `Documents/Plans/Agents/Cpp*Audit.md` edits. The duplicate-key
acceptance is unresolved, pre-existing, and outside the approved audit work.

## Design

The author's recommendation is to require every `try_emplace` during staged
read to report insertion success and throw `CorruptStreamException` when a
coordinate repeats. After the loop, also require
`coordFrames.size() == iFrameCount` before the existing followed-cell and
adoption gate completes. Preserve the sorted writer format and isolated staged
snapshot; do not attempt to merge duplicate rows.

## Critical files

- `Engine/Source/File/GridSave.cpp:104-166` — coordinate insertion, final semantic checks, and adoption.
- `Engine/Source/File/AGENTS.md` — one-row-per-coordinate and two-stage save contract.

## In scope

- Checking the `try_emplace` insertion result for each serialized grid row.
- Verifying final staged row count against `iFrameCount` before adoption.
- Routing duplicate coordinates through the existing post-header corrupt-save
  failure path.

## Out of scope

- Island-template membership, frame-area geometry, frame payload identity,
  save layout/version, coordinate sorting in the valid writer, or network
  static-data adoption.
- Repairing or merging duplicate records, changing game save state, or adding a
  compatibility format.

## Risk tier and invariants

Expected Change Workflow Tier 2 (downgraded from Tier 3). This is serialized
save input that controls the complete deterministic grid and its adoption
boundary.

Tier rationale: the fix is mechanical and pre-specified — check the
`try_emplace` result already being discarded, compare the staged row count with
`iFrameCount`, and throw the existing `CorruptStreamException`. Valid saves keep
their exact layout, ordering, and adoption behavior.

Preserve these invariants:

- A successful load has exactly one staged `CoordFrames` entry per serialized
  frame record.
- Duplicate or missing rows fail before any live-grid replacement, using the
  existing post-header clear/reset behavior.
- Valid save ordering, frame clocks, followed-cell validation, and atomic
  adoption remain unchanged.

## Acceptance criteria

- A save with two complete rows carrying the same coordinate returns failure and
  does not adopt the later row or shrink the live grid silently.
- A valid save with `iFrameCount` unique rows still stages/adopts every row and
  passes the existing followed-cell check.
- Server `Debug|x64` builds clean through `/compile`.

## Coordination

`Documents/Plans/Engine/GridSaveIslandReference.md` and
`Documents/Plans/Engine/FrameAreaValidation.md` share the staged-save boundary
but own template membership and static-area geometry. Keep the duplicate-key
check independent, preserve the common rejection path, and re-derive line
citations before editing. No dependency is required.

## Notes

The consolidated index records no duplicate-family hint or external claim for
this candidate.
