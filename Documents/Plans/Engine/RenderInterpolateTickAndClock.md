<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-16T14:39:03.364Z","dependsOn":[]} -->
# Give each render interpolate its frame's tick

## Context

Every client render reads the per-coord render interpolates in
`GameBase::mRenderInterpolates`, which `GameBase::UpdateRenderInterpolation`
rebuilds each render from the coord's ring frame. Its `InterpolateFrame` lambda
(`Engine/Source/GameBase.cpp:764-781`) calls
`game::FrameInterpolate::AllocateAndCopy` (`:775`), which copies only the
collections (`FrameInterpolateBase::AllocateAndCopy`,
`Engine/Source/Frame/FrameBase.cpp:179-184`), then
`game::FrameInterpolate::Update` (`:776`), whose engine part
`FrameInterpolateBase::Update` (`FrameBase.cpp:186-198`) sets `fDeltaTime` and
the phase flags but never `iTick`. The client boot render in `MainThread`
(`Engine/Source/Main.cpp:313-317`) runs `AllocateAndCopy` alone. The entry is
default-constructed by `try_emplace` and nothing else writes `iTick`, so on
every render interpolate `iTick` is `0`.

The simulation clock is derived from the tick, not stored: no frame or `GameBase` field stores the tick's instant, and every reader computes the instant from its frame's `iTick` through `TickToSimulationTime` (`Engine/Source/Frame/TimeStep.h:17`). The render-side readers below therefore read the instant of tick 0.

The fixed tick does not hit this: `RunFrameTick` assigns `iTick` right after the same `Update` call (`FrameBase.cpp:289`). Only render-side readers of the copy see tick 0:

- Camera shake: `MainUniforms.cpp:592` samples three Perlin noise channels at 8 times the seconds of the instant of the camera coord's render interpolate `iTick` (`:496,579`). The input is constant, so the shake direction never changes and only its magnitude scales with `fCameraShake`.
- Smoke trails: `SmokeTrailsInterpolate::Render` (`Engine/Source/Frame/Collections/SmokeTrails/SmokeTrailsRender.cpp:98`) zeroes the trail extension while the trail is younger than 50 ms, measured as the render interpolate's instant minus `pStartTimes[i]`. At tick 0 that age is never positive, so every trail renders without its `gSmokeTrailsLength` extension.
- Frame number: `CameraBase::Update` (`Engine/Source/Graphics/CameraBase.cpp:129`) copies `iTick` into `miFrame`, which `MainUniforms.cpp:581` writes to `MainLayout::iFrameNumber`; the only shader reader is the debug print in `Engine/Data/Shaders/Log.vert:35`.

This was found during the `/next-plan` run that derived the simulation clock from the tick.

## Design

- Write `iTick` where the render copy is made, never in `FrameInterpolateBase::Update`. `Update` also runs inside `RunFrameTick`, and keeping the fix out of the tick keeps it away from CRC'd state.
- In the `InterpolateFrame` lambda, after `Update` and next to the `renderBasis` stamp, set the render interpolate's `iTick` to `rFrame.interpolate.iTick`.
- In the `MainThread` boot render, after `AllocateAndCopy`, set the render interpolate's `iTick` to the same source frame's `iTick`.
- With the derived clock, that one field fixes all three readers: each computes its instant from the copy's `iTick`, so the camera shake and the smoke-trail age read the window-start tick's instant, and `miFrame` reads the window-start tick.
- Recommendation, not yet chosen by the user: keep window-start tick granularity. The integer `iTick` cannot carry a sub-tick render instant, and the render interpolate copy has no field for one. Rationale:
  - The fix stays two writes in the render-copy path, and the readers stay unchanged.
  - The cost: the camera-shake noise steps at the 32 Hz tick rate instead of varying smoothly, and the smoke-trail age is measured from the window start, so the extension appears up to one tick later than the drawn time.
  - The alternative is a sub-tick instant computed in the two time readers,
    `MainUniforms.cpp:592` and `SmokeTrailsRender.cpp:98`: the instant of the
    copy's `iTick` plus its `fDeltaTime`, which `Update` sets to the coord's
    render delta. That keeps the render-clock handoff
    `Update(N, kfDt) ≡ Update(N+1, 0)` (`GameBase.cpp:716-722`) seamless,
    because window start N plus `kfDt` equals window start N+1 plus zero. It
    edits both readers, and `SmokeTrailsRender.cpp` is also edited by
    `Documents/Plans/Engine/SmokePerFlightPlane.md`. Confirm the choice at claim
    time.

## Critical files

- `Engine/Source/GameBase.cpp` — `GameBase::UpdateRenderInterpolation`, `InterpolateFrame` lambda (`:764-781`)
- `Engine/Source/Main.cpp` — `MainThread` boot render interpolate (`:313-317`)
- Read-only consumers to confirm: `Engine/Source/Graphics/Render/MainUniforms.cpp:579-592`, `Engine/Source/Frame/Collections/SmokeTrails/SmokeTrailsRender.cpp:98`, `Engine/Source/Graphics/CameraBase.cpp:129`

## In scope

- The `InterpolateFrame` lambda in `GameBase::UpdateRenderInterpolation`: assign the render interpolate's `iTick` as `## Design` states.
- The `MainThread` boot block that fills `mRenderInterpolates.at(kOriginCoordinate)`: assign `iTick` from the source frame.

## Out of scope

- `FrameInterpolateBase::Update`, `FrameInterpolateBase::AllocateAndCopy`, `game::FrameInterpolate::Update`, `RunFrameTick`, and any CRC'd, serialized, or `Values()` state.
- The readers (`MainUniforms.cpp`, `SmokeTrailsRender.cpp`, `CameraBase.cpp`) and their tuning constants: their behavior changes only through the corrected input, unless the claim chooses the sub-tick alternative in `## Design`.
- The render time passed to `RenderGlobal`/`RenderMainPresentAcquire` (`GameBase.cpp:903,907`, `Main.cpp:333-334`), the clock representation, and `TickToSimulationTime`.
- Any other client-only render state, such as `renderBasis` or `fDeltaTime`.

## Risk and invariants

- Change Workflow tier: Tier 2. The trigger is scoped client-only render behavior in one subsystem, the render interpolation path. The copies written are never CRC'd, serialized, transferred, or sent over the wire, and the fix stays out of `RunFrameTick`, so determinism and CRC are untouched.
- Invariant: shared PostRender/CRC state must not derive from a render-interpolated frame (`Engine/Source/Frame/AGENTS.md` `## Architecture`). This change only writes the render copies.
- No allocation: both writes go to an existing entry.

## Acceptance criteria

1. The diff assigns `iTick` on every render interpolate the `InterpolateFrame` lambda and the boot block produce, and touches no file under `Engine/Source/Frame/` or the game project. Under the sub-tick alternative, the only other files it edits are the two time readers `## Design` names.
2. Live (`/agent-harness`), in a scenario with projectiles emitting smoke trails: a client screenshot shows trails extending past their smoothed position once they are older than 50 ms. On the baseline build, the same scenario shows no such extension.
3. Live (`/agent-harness`), with camera shake active, such as nearby explosions: client screenshots taken more than one tick apart show the shake direction varying between them.
4. A replay determinism check through `/agent-harness` still passes, with no CRC mismatch.

## Notes

- `Documents/Plans/Engine/SmokePerFlightPlane.md` edits `SmokeTrailsInterpolate::Render` in `SmokeTrailsRender.cpp`; under the recommended window-start granularity this Plan leaves that file unchanged, so the two do not overlap. The sub-tick alternative edits the same function.
