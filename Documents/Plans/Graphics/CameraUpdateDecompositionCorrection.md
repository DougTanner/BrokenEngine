<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-18T16:19:26.056Z","dependsOn":[]} -->
# Correct Camera Update Decomposition

## Context

The historical executable Plan `Documents/Plans/Graphics/CameraUpdateDecomposition.md` (deleted by commit `279c5f97ab92b6ed1b37b09c5e27a75d97de334d`) treated the change as pure code motion and required every resulting function to remain at cyclomatic complexity 10 or below. Its design made `LogMissingPlayer` mandatory solely to keep `ResolveTargetPosition` below that threshold (historical Plan lines 6, 12, and 39-42). That criterion was satisfied by moving code, but the historical review found metric redistribution with modest orchestration benefit rather than meaningful simplification.

The residual is still present and is outside the separately committed current metrics/deep-analysis implementation. `Projects/BrokenEngineSandbox/Source/Graphics/Camera.cpp` still calls the four historical helpers at lines 119-122 and 188, and defines them at lines 147, 205, 228, and 278; `Camera.h` declares them at lines 81-84. All four helpers remain single-call or stage boundaries. The current 511/511 code-quality snapshot has one target and one corpus result, `structuralErosion: 0` for `Camera.cpp`, no high-complexity target function, and `excessDecisions: 36`; the threshold cleanup did not remove the decision burden.

## Design

Keep `ResolveTargetPosition` and `UpdateEyeHeight` as independently meaningful target-resolution and zoom-stage boundaries. In `ResolveTargetPosition`, make the smallest in-place structural correction:

- Return immediately for the free-camera case.
- Return immediately for the combined main-menu or invalid-client-player case using the existing canonical menu pose.
- Continue with the existing gameplay lookup. Preserve the tracking-history writes and visual-error offset on a found player. In the missing-player branch, inline the existing `LogMissingPlayer` body verbatim, preserving its static one-second throttle, verbose level, exact log text, frame sources, player loop, and position-extrapolation order. Return the found or extrapolated target directly. Do not extract another helper.

Rename the private `UpdateJump` member to `UpdatePosition` at its declaration, definition, and sole call. The function owns both jump handling and ordinary chase blending. Replace its initial compound tests with an outer `if (mbJumping)` containing the target-shift re-anchor test and an `else if (fDistanceToTarget > kfJumpDistanceThreshold)` start test. This removes only the redundant `!mbJumping` decision; preserve every transition, state write, and subsequent jump/normal-position update in its existing order.

Leave `UpdateEyeHeight`, the `Camera::Update` stage order, camera tuning, logging behavior, visual-error handling, focus fallback, extrapolation, zoom, and persistence behavior unchanged. Do not add compatibility aliases or another abstraction.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Graphics/Camera.cpp`
- `Projects/BrokenEngineSandbox/Source/Graphics/Camera.h`

## In scope

- `Camera::ResolveTargetPosition` and its sole call from `Camera::Update(const FrameInterpolate&)`: early-return flattening and direct return of the existing found/missing results.
- Removal of the private `Camera::LogMissingPlayer` declaration/definition and verbatim placement of its diagnostic block in the missing-player branch.
- Rename of private `Camera::UpdateJump` to `Camera::UpdatePosition` at the declaration, definition, and sole call; the initial jump/chase condition structure inside that function.

## Out of scope

- Any camera behavior, tuning, math, logging cadence/text/level/order, focus fallback, missing-player extrapolation, zoom behavior, state visibility, or statement ordering change.
- `UpdateEyeHeight`, either `Camera::Update` overload's other stages, `SunAngle`, file-scope helpers, and all other historical camera decomposition cleanup.
- Metrics implementation/deep-analysis files, metric threshold enforcement, new compatibility surfaces, serialization, wire/replay/CRC behavior, and unit tests.
- Any file outside `Camera.cpp` and `Camera.h`, apart from documentation inspection required by the normal workflow.

## Risk tier and invariants

Expected Change Workflow Tier 1: a local, behavior-preserving client-only private structure/name change with no public signature or simulation/CRC exposure. Escalate to Tier 2 if implementation changes motion, focus fallback or extrapolation, logging cadence/content, zoom, state visibility, or ordering. Preserve the graphics-layer timing contract: camera motion and visual timers use the sim-scaled render delta, and target-history/extrapolation state is updated in the same sequence. Preserve DirectXMath function-form calls and vector W conventions (positions W=1; directions and offsets W=0). Do not introduce heap allocation, containers, or new per-frame logging allocation; retain the existing static throttle and loop. The change remains outside deterministic PostRender state, serialization, networking, threading, and `.pack` data.

## Coordination

No directional dependencies. The active metrics/deep-analysis implementation is separately committed and remains independent; do not modify or fold its six session-owned paths into this camera change. No reciprocal Plan coordination is required.

## Acceptance criteria

- The diff shows exactly the stated private boundary changes: `LogMissingPlayer` is gone, its diagnostic block is inline in the missing-player branch, `UpdateJump` is renamed only at its declaration/definition/sole call, and the redundant `!mbJumping` decision is removed without changing branch or update order.
- Client compilation succeeds through `/compile` for the selected `Camera.cpp` change.
- Fresh C++ correctness/style/documentation/project-membership checks run only as triggered by the changed regions and find no behavior or contract change; code inspection/review confirms all tracking-history mutation, visual-error offset, logging, extrapolation, jump transitions, and call/order behavior are preserved.
- A code-quality `Compare` for `Camera.cpp` against the future implementation baseline parses the complete source and provides source evidence of the redundant-decision removal, with target and corpus `excessDecisions` each reduced by exactly 1 and no offsetting new excess decision. Metrics remain advisory; no cyclomatic-complexity ceiling is an acceptance condition.
- No harness scenario is required unless implementation changes runtime-observable behavior; such a change is a plan conflict requiring escalation.

## Notes

This Plan records the proven historical decomposition residual, not a new feature or a compatibility migration. It has no dependencies and must not alter the already committed metrics/deep-analysis session work.
