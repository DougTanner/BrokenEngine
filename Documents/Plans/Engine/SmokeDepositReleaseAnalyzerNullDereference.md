<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-17T20:39:39.294Z","dependsOn":[]} -->
# Clear the Release analyzer null-dereference errors in the smoke depositors

Line numbers cite baseline `8ac1d7d683812a4df2cfce2362e103c8c4bf7eab`. Where a statement here and the code disagree, the code wins; report the contradiction instead of matching one side to the other.

## Context

The `BrokenEngineSandbox` client `Release|x64` build fails on main. The Release
configuration compiles with `/analyze` (`EnablePREfast` true,
`Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj:268`)
and treats warnings as errors (`:250`), so two analyzer warnings stop the
build with `error C2220`:

- `Engine/Source/Frame/Collections/Puffs/PuffsRender.cpp(103): warning C6011: Dereferencing NULL pointer 'ppPuffsLayouts[iPlane]'`, analyzer path lines 55-103 of `PuffsInterpolate::Render`.
- `Engine/Source/Frame/Collections/SmokeTrails/SmokeTrailsRender.cpp(130): warning C6011: Dereferencing NULL pointer 'ppTrailLayouts[iPlane]'`, analyzer path lines 56-130 of `SmokeTrailsInterpolate::Render`.

`/compile` agent builds pass `/p:RunCodeAnalysis=false`, which disables only
the post-link code-analysis task (`BrokenEngineSandbox.vcxproj:104-111`); the
compile-time `/analyze` still runs, so agent Release builds fail too. Client
`Debug|x64` and server `Debug|x64` and `Release|x64` build clean.

Both functions have the same shape, introduced together by main commit
`259772c0` ("Give each flight plane its own smoke field"):

- A per-plane layout array is value-initialized to null across all `shaders::kiMaxFlightPlanes` (8) entries (`PuffsRender.cpp:64`, `SmokeTrailsRender.cpp:65`).
- Only the entries `i < gpFlightPlanes->Count()` are filled from `gpBufferManager->GetDynamicStorageBuffer` (`PuffsRender.cpp:65-70`, `SmokeTrailsRender.cpp:66-71`).
- The write index is `iPlane = planeWeights.iLowerPlane + j`, `j` in {0, 1}, with a plane skipped when its weight is exactly `0.0f` (`PuffsRender.cpp:87-95`, `SmokeTrailsRender.cpp:101-109`); the dereference is `PuffsRender.cpp:103` and `SmokeTrailsRender.cpp:130`.
- `ComputeSmokePlaneWeights` (`Engine/Source/Graphics/GraphicsUtils.cpp:104-121`) returns `iLowerPlane` in `[0, Count() - 1]` and an upper weight of `0.0f` when `iLowerPlane` is `Count() - 1`; `SmokePlaneWeights` defaults to `{0, 0.0f}` (`Engine/Source/Graphics/GraphicsUtils.h:37-41`).
- `FlightPlanes::Count()` is `std::ssize(mPlanes)` of the boot-time table (`Engine/Source/Frame/FlightPlanes.cpp:30-33`); `GetDynamicStorageBuffer` returns the buffer's mapped pointer (`Engine/Source/Graphics/Managers/BufferManager.h:45-56`).

Not yet established: why the analyzer considers the entry null on its path
(for example, the fill loop running zero times, or `iPlane` reaching an
unfilled entry) and whether any runtime path reaches a null entry. The fix
depends on that answer, so this Plan does not presume it.

Unmet condition: the client `Release|x64` configuration builds. Its project settings make every `/analyze` warning a build error, and `Common/AGENTS.md` (the `ASSERT` `_Analysis_assume_` bullet) expects code to stay clean under that build.

## Design

1. Prove the analyzer's path and its reachability first, by `/external-diagnose-bug` close code inspection of the two functions and `ComputeSmokePlaneWeights`: which condition on the reported path leaves `pp...Layouts[iPlane]` null, and whether a runtime input (flight-plane count, a height `fZ` including NaN or out-of-range values, a buffer whose mapped pointer is null) reaches it.
2. Then apply the branch the proof selects:
   - Unreachable — author's recommendation: state the invariant the proof established where the code relies on it, with the repository's existing mechanism, `ASSERT` (its `_Analysis_assume_` tail exists so the Release `/analyze` build sees checked conditions as holding, `Common/ErrorUtils.h:20`, `Common/AGENTS.md`). Rationale: it documents a real invariant at no Release cost and is how the codebase already answers C6011. Keep both functions parallel.
   - Reachable — the null dereference is a runtime defect; fix it at its source rather than silencing the analyzer, and return to the user before changing anything outside `## In scope`.

No `#pragma warning` suppression and no project or ruleset change: the analyzer is reporting the code, not misconfigured.

Risk tier: Tier 2 — the highest trigger is a possible runtime behavior change in one subsystem (client smoke deposit rendering) if step 1 finds the path reachable; if it is unreachable the change is Tier 1 local assertion work. Client-only render code: not in the CRC, no determinism, wire, save, replay, or format exposure.

## Critical files

- `Engine/Source/Frame/Collections/Puffs/PuffsRender.cpp`
- `Engine/Source/Frame/Collections/SmokeTrails/SmokeTrailsRender.cpp`
- `Engine/Source/Graphics/GraphicsUtils.cpp` (read for the proof)

## In scope

- `PuffsInterpolate::Render` and `SmokeTrailsInterpolate::Render`: the per-plane layout array, its fill loop, and the per-plane write loop.

## Out of scope

- `ComputeSmokePlaneWeights`, `SmokePlaneWeights`, `ProjectToFlightPlane`, `FlightPlanes`, and `BufferManager::GetDynamicStorageBuffer`, unless step 1 proves the defect lives there; then return to the user first.
- `BrokenEngineSandbox.vcxproj` analysis settings (`EnablePREfast`, `TreatWarningAsError`, `CodeAnalysisTreatWarningsAsErrors`, the ruleset) and the `/compile` build arguments.
- Other collections' render functions and any other analyzer warning.

## Acceptance criteria

- `/compile` builds `BrokenEngineSandbox` client `Release|x64` with no C6011 or C2220 in either file; client `Debug|x64` and server `Debug|x64` and `Release|x64` stay clean.
- The handoff records step 1's verdict (the analyzer's null condition and reachable or unreachable) with `path:line` evidence.
- If step 1 finds a reachable path: `/agent-harness` shows puffs and smoke trails still depositing on every flight plane they span.

## Notes

- The failure predates the session that recorded it and reproduced unchanged on baseline `8ac1d7d6`; neither file was changed after `259772c0`.
