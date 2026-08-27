<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:29:41.298Z","dependsOn":[]} -->
# Reject grid saves that reference missing island templates

## Context

The accepted finding `CAI/shard-0013/005` identifies a semantic gap in the
two-stage grid-save boundary. `ReadGridSave` validates placement counts and
bytes but accepts each raw `IslandPlacement::islandCrc`
(`Engine/Source/File/GridSave.cpp:104-141`; `FrameStaticData::Read` at
`Engine/Source/Frame/FrameStaticData.cpp:24-37`). The staged grid is then
adopted, and the first server tick resolves the placement through
`gpIslandTerrain->mIslands.at` while rebuilding nav data
(`Engine/Source/Frame/FrameBase.cpp:300-309`,
`Engine/Source/Frame/NavCellData.cpp:316-321`). A stale or damaged save can
therefore report load success and terminate the server after adoption, violating
the save contract in `Engine/Source/File/AGENTS.md`.

The source shard's frozen/live target hashes match audit baseline
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`; current status contains only the
six pre-existing `Documents/Plans/Agents/Cpp*Audit.md` edits. This missing
template check is unresolved, pre-existing, and outside the approved audit
work.

## Design

The author's recommendation is to resolve every staged placement CRC against
the currently loaded island-template map during `ReadGridSave`, before
`AdoptGridSave` can publish the snapshot. Throw the existing corrupt-stream
exception on the first missing template so the post-header failure path clears
the grid and resets game save state. Keep derived navigation rebuilding lazy
after a fully valid adoption and leave the network static-data adoption path to
its separate Plan.

## Critical files

- `Engine/Source/File/GridSave.cpp:104-166` — staged read, semantic checks, and adoption.
- `Engine/Source/Frame/FrameStaticData.cpp:24-37` — placement deserialization (read-only reference).
- `Engine/Source/Frame/FrameBase.cpp:300-309` and `Engine/Source/Frame/NavCellData.cpp:316-321` — deferred lookup (read-only evidence).
- `Engine/Source/File/AGENTS.md` — isolated save-adoption and corruption rules.

## In scope

- Island-template membership validation for every staged save placement before
  the save returns success.
- Routing a missing template through the established post-header save failure
  and fresh-game reset path.
- The `ReadGridSave` staging boundary and its direct placement-validation helper.

## Out of scope

- Client queued network static-data adoption (owned by
  `ClientStaticIslandReference.md`), duplicate-coordinate detection, frame-area
  validation, or nav-topology validation.
- Changing save layout/version, island generation, navigation algorithms, or
  the valid writer's output.
- Any client rendering or texture-residency behavior.

## Risk tier and invariants

Expected Change Workflow Tier 2 (downgraded from Tier 3). Grid saves are
serialized trust-boundary inputs that replace deterministic server state and
feed deferred navigation.

Tier rationale: the Design fully specifies one membership lookup per staged
placement CRC against the already-loaded island-template map inside
`ReadGridSave`, throwing the existing corrupt-stream exception. It adds only
corrupt-input rejection; save layout, valid adoption, and navigation
derivation are unchanged.

Preserve these invariants:

- A successful staged save references only currently loaded island templates.
- No partially valid grid is adopted; any post-header semantic failure follows
  the existing clear/reset path.
- Valid save ordering, sorted coordinates, frame clocks, navigation derivation,
  and `Frame::kiVersion` remain unchanged.

## Acceptance criteria

- A structurally valid save containing one absent island CRC returns failure,
  clears the post-header staged/live state as today, and never reaches
  `mIslands.at` during the next tick.
- A valid save with all template CRCs still autoloads/adopts and rebuilds nav
  data on the first server tick.
- Server `Debug|x64` builds clean through `/compile`.

## Coordination

`Documents/Plans/Engine/GridSaveDuplicateCoord.md` and
`Documents/Plans/Engine/FrameAreaValidation.md` also inspect staged save
adoption and `GridSave.cpp`. They own duplicate-key and area-geometry checks;
keep this template-membership check independent, preserve the common failure
path, and re-derive line ranges before implementation. No dependency is
required.

## Notes

The consolidated index labels this and `CAI/shard-0014/001` as probable
`DUP-004`, but the source reports explicitly distinguish save staging from
queued client static-data adoption, so they are separate Plans.
