<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-18T17:51:44.420Z","dependsOn":["Documents/Plans/Graphics/CameraUpdateDecompositionCorrection.md"]} -->
# Move camera ownership into the engine

## Context

The camera is currently split between the engine and the game. `Engine/Source/Graphics/CameraBase.{h,cpp}` owns projection, matrices, visible-area snapping, and LOD latching, while `Projects/BrokenEngineSandbox/Source/Graphics/Camera.{h,cpp}` owns the complete top-down camera update, target lookup, extrapolation, jump smoothing, zoom, texel references, shake, vibration, sun behavior, persistence, and the `game::gpCamera` global.

Engine render, audio, profile, frame-render, and boot code therefore depend on a game-owned camera. `CameraInput` is likewise stored in game `Input` even though it is consumed only by the camera. The game camera also still exposes an unused `Update(const Frame&)` forwarding overload.

The current implementation is being corrected first by `Documents/Plans/Graphics/CameraUpdateDecompositionCorrection.md`. That prerequisite owns the last local shape correction: `LogMissingPlayer` is inlined into `ResolveTargetPosition`, and `UpdateJump` is renamed to `UpdatePosition`. This ownership move consumes that landed result; it does not implement the correction concurrently.

The baseline before the prerequisite is commit `12a682dad9ad483fe9b6098a0dd6576d62ba0af2`. Existing camera prerequisites `279c5f9` and `2c607ef` are already landed.

## Design

Move the generic camera into uniquely named engine files:

- `Engine/Source/Graphics/CameraBase.h` becomes `Engine/Source/Graphics/EngineCamera.h`.
- `Engine/Source/Graphics/CameraBase.cpp` becomes `Engine/Source/Graphics/EngineCamera.cpp`.
- `engine::CameraBase` becomes `engine::Camera`.

The unique `EngineCamera` filenames are required because the same Visual Studio project already contains the game `Graphics/Camera.cpp`; two `Camera.cpp` basenames would collide in project/build bookkeeping. The game files remain `Projects/BrokenEngineSandbox/Source/Graphics/Camera.{h,cpp}`.

`engine::Camera` owns the complete generic update and all current generic state. Preserve the current public hot-read fields: matrices, visible-area fields, camera position and eye vectors, shake, frame number, visible-area LOD, live and target eye heights, last-known tracking state, jump state, Hermite zoom state, shadow and lighting texel-eye-height references, and `CameraInput`.

Move the generic camera constants and helpers into the engine camera, preserving values and formulas: minimum eye height, initial/default eye height, release zoom ceiling, wave-fade endpoint, shadow and lighting headroom multipliers, initial raw sun angle, reset sun angle, wheel/zoom/chase/jump constants, texel-ramp logic, and smoothstep logic. The menu center and menu offset remain game-owned.

### Engine setup and lifecycle

Define an engine setup value used only to pass the derived game camera's initial menu pose into the generic base:

```cpp
namespace engine
{

struct CameraSetup
{
    XMVECTOR vecInitialPosition {};
};

}
```

`engine::Camera` has an explicit constructor taking `const CameraSetup&`, an explicit virtual destructor, and the following lifecycle behavior:

- The constructor asserts `engine::gpCamera == nullptr`, stores `vecInitialPosition` in `mVecPosition`, and registers `engine::gpCamera = this`.
- The virtual destructor clears `engine::gpCamera` only when it still points at `this`.
- The game constructor builds the existing menu pose and passes it through `CameraSetup` to the engine constructor.
- No game camera global remains.

### Engine-only frame interface

The engine public camera interface names only `const engine::FrameInterpolateBase&`; it must not name `game::FrameInterpolate`.

The exact callbacks are:

```cpp
virtual bool IsMainMenuFrame(const FrameInterpolateBase& rFrameInterpolate) const = 0;
virtual CameraTarget PullTarget(const FrameInterpolateBase& rFrameInterpolate) = 0;
virtual void OnUpdateComplete() = 0;
```

The game overrides cast the supplied base reference to `const game::FrameInterpolate&`, relying on the existing `GameBase` contract that the concrete render interpolate is the game frame type. No dynamic dispatch, alternate frame type, or new validation path is added.

### Closed camera-target factories

Define a closed `engine::CameraTarget` value with private construction and exactly three factories:

```cpp
class CameraTarget
{
public:

    static CameraTarget Direct(FXMVECTOR vecPosition);
    static CameraTarget Tracked(FXMVECTOR vecRawPosition, FXMVECTOR vecVisualOffset);
    static CameraTarget Extrapolate();

private:

    enum class Kind : uint8_t
    {
        kDirect,
        kTracked,
        kExtrapolate,
    };

    CameraTarget(Kind eKind, XMVECTOR vecPosition, XMVECTOR vecVisualOffset);

    Kind meKind {};
    XMVECTOR mVecPosition {};
    XMVECTOR mVecVisualOffset {};

    friend class Camera;
};
```

The factories are the only construction paths. `Direct` carries a final target position. `Tracked` carries raw player position separately from the reconciliation visual offset. `Extrapolate` carries no current position; the engine resolves it from the last raw tracked position and velocity. The factory and update contract preserve position W=1 and offset W=0.

### Engine update

Change the generic update entry point to:

```cpp
void Update(const FrameInterpolateBase& rFrameInterpolate, float fDeltaTime);
```

`fDeltaTime` is the exact float conversion of the existing sim-scaled render delta (`GameBase::mfLastRenderFrameSeconds`). `GameBase::UpdateRenderInterpolation` passes it explicitly, and the boot path passes the same value after persistent-state load and render-interpolate setup.

Preserve the existing update order:

1. Advance `mfTime`, decay shake, and latch the interpolate tick.
2. Advance sun angle only outside the main menu, using the interpolate frame's `fDeltaTime`.
3. Apply debug free-camera movement only in the main menu.
4. Pull and resolve a `CameraTarget`.
5. Apply `UpdatePosition` jump/chase behavior.
6. Apply Hermite zoom.
7. Update shadow and lighting texel-eye-height references.
8. Compute eye position and eye normal.
9. Set controller vibration from shake.
10. Calculate matrices and visible-area/LOD state.
11. Call `OnUpdateComplete`.

For a tracked target, update velocity from the raw position before adding the visual offset. For an extrapolated target, clamp elapsed time to `[0, 2]` seconds. Preserve all existing jump thresholds, re-anchoring, smoothstep, zoom, texel-ramp, vibration, matrix, visible-area, and LOD behavior.

`Update(const Frame&)` is removed. The update remains before the existing visual-error-offset decay block in regular rendering. The boot update remains before the first render/present.

### Reset, restore, and sun

Add generic engine methods:

```cpp
void ResetForSession();
void RestoreEyeHeight(float fEyeHeight);
float RawSunAngle() const;
void ResetSunAngle();
```

`ResetForSession()` preserves the current `Game::Reset()` subset exactly:

- raw sun resets to `1.8f`;
- last-known position, velocity, and time clear;
- jump-start and previous-target state clear;
- jump state clears;
- shadow and lighting texel-eye-height references return to zero sentinels.

It does not reset camera time, current/target zoom, Hermite zoom state, current position, shake, eye state, or the game diagnostic tracking ID.

`RestoreEyeHeight` assigns both live and target eye height directly to the persisted value. It does not alter the client-state format or add clamping. The raw sun angle starts at `1.4f`; `ResetSunAngle` and `ResetForSession` use the existing `1.8f` reset value.

### Game camera

Make `game::Camera final` and retain only game policy:

- menu target constants;
- constructor setup of the menu pose;
- `IsMainMenuFrame`;
- `PullTarget`;
- `OnUpdateComplete`, which calls `gpGame->CaptureClientStateAndSaveIfChanged()`;
- `SunAngle` UI override;
- the existing tracking/missing-player diagnostic state and inline diagnostic body from the prerequisite correction.

`PullTarget` casts the engine base frame to `game::FrameInterpolate` and returns:

- `CameraTarget::Direct` for main-menu and invalid-client-player poses;
- `CameraTarget::Tracked` with raw player position and `gpGame->mVecVisualErrorOffset` when the focused player is found;
- `CameraTarget::Extrapolate` after the existing missing-player diagnostic when the focused identity is absent from the current frame.

The game class no longer declares update methods, generic helpers, generic camera state, `CameraInput`, or `game::gpCamera`.

### Input and consumers

Move `CameraInput` from game `Input.h` to `EngineCamera.h`. `Input::UpdateCameraInput()` writes `engine::gpCamera->mCameraInput`; the camera update reads it. Preserve all existing wheel ownership, scroll-delta, and free-camera binding behavior.

Replace all engine and game camera-global consumers with `engine::gpCamera`. Requalify generic constants to `engine::Camera::`. Update engine includes from `Graphics/Camera.h`/`CameraBase.h` to `Graphics/EngineCamera.h`; game `Graphics/Camera.h` remains the concrete game camera header.

Use `engine::gpCamera->RawSunAngle()` directly from `MainMenuScreen.cpp`.

## Critical files

Engine camera and lifecycle:

- `Engine/Source/Graphics/CameraBase.h` → `Engine/Source/Graphics/EngineCamera.h`
- `Engine/Source/Graphics/CameraBase.cpp` → `Engine/Source/Graphics/EngineCamera.cpp`
- `Engine/Source/Engine.h`
- `Engine/Source/GameBase.h`
- `Engine/Source/GameBase.cpp`
- `Engine/Source/Main.cpp`

Engine camera consumers:

- `Engine/Source/Audio/StaticVoices.cpp`
- `Engine/Source/Profile/ProfileScreens.cpp`
- `Engine/Source/Graphics/GraphicsUtils.cpp`
- `Engine/Source/Graphics/Islands.cpp`
- `Engine/Source/Graphics/Managers/ParticleManager.cpp`
- `Engine/Source/Graphics/Managers/RenderTargetTextures.cpp`
- `Engine/Source/Graphics/Managers/TextureManager.cpp`
- `Engine/Source/Graphics/Render/GlobalUniforms.cpp`
- `Engine/Source/Graphics/Render/LightingUniforms.cpp`
- `Engine/Source/Graphics/Render/MainUniforms.cpp`
- `Engine/Source/Graphics/Render/SmokeUniforms.cpp`
- `Engine/Source/Graphics/Render/WaterUniforms.cpp`
- `Engine/Source/Frame/Collections/AreaLights/AreaLightsRender.cpp`
- `Engine/Source/Frame/Collections/Billboards/BillboardsRender.cpp`
- `Engine/Source/Frame/Collections/HexShields/HexShieldsRender.cpp`
- `Engine/Source/Frame/Collections/PointLights/PointLightsRender.cpp`

Game camera and callers:

- `Projects/BrokenEngineSandbox/Source/Graphics/Camera.h`
- `Projects/BrokenEngineSandbox/Source/Graphics/Camera.cpp`
- `Projects/BrokenEngineSandbox/Source/Input/Input.h`
- `Projects/BrokenEngineSandbox/Source/Input/Input.cpp`
- `Projects/BrokenEngineSandbox/Source/Game.h`
- `Projects/BrokenEngineSandbox/Source/Game.cpp`
- `Projects/BrokenEngineSandbox/Source/ClientSettings.cpp`
- `Projects/BrokenEngineSandbox/Source/Agent/AgentScene.h`
- `Projects/BrokenEngineSandbox/Source/Agent/AgentScene.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/MissilesRender.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/SpaceshipsRender.cpp`
- `Projects/BrokenEngineSandbox/Source/Ui/Screens/MainMenuScreen.cpp`

Project membership:

- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj`
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj.filters`

The client project entries move from `CameraBase.{h,cpp}` to `EngineCamera.{h,cpp}`. The server project remains without client camera source membership.

Current documentation and comments requiring ownership/path updates:

- `AGENTS.md`
- `Engine/Source/Graphics/AGENTS.md`
- `Projects/BrokenEngineSandbox/Source/Graphics/AGENTS.md`
- `Projects/BrokenEngineSandbox/Source/Input/AGENTS.md`
- `Engine/Data/Shaders/Lighting/AGENTS.md`
- `Engine/Source/Graphics/Managers/BufferManager.h`
- `Projects/BrokenEngineSandbox/Source/Agent/AgentScene.h`
- `Documents/Features/Frame/FrameRelativePositions.md`
- `Documents/Features/Graphics/SkyboxRenderPass.txt`
- `Documents/Features/Graphics/HeatDistortionAndShockwave.txt`

Historical investigation packets remain historical material and are not rewritten as part of this implementation.

## In scope

- The engine camera file move and `engine::CameraBase` → `engine::Camera` rename.
- `CameraSetup`, constructor/destructor registration, `engine::gpCamera`, and the closed `CameraTarget` factories in `EngineCamera.h`/`.cpp`.
- The generic camera update loop and all current generic state/helpers/constants from the post-prerequisite game camera result.
- The `FrameInterpolateBase` update/callback signatures and explicit sim-scaled render delta at the regular and boot call sites.
- `ResetForSession`, `RestoreEyeHeight`, `RawSunAngle`, and `ResetSunAngle` plus their exact reset/load call sites.
- `game::Camera` final callback implementations, menu-pose setup, UI sun override, and missing-player/tracking diagnostics.
- Removal of game camera update/state/global declarations made obsolete by the move.
- Moving `CameraInput` to the engine camera and rewiring its existing producer/consumer.
- Every listed engine/game camera consumer and generic constant requalification.
- Client project/filter membership for `EngineCamera.{h,cpp}` and preservation of server exclusion.
- The listed current-state AGENTS, comments, feature documentation, and source citations.
- The minimum mechanical includes/declarations required by the named changes.

## Out of scope

- Any camera behavior, constant, formula, threshold, duration, clamp, logging cadence/text, focus fallback, extrapolation window, W-lane convention, or statement ordering change.
- Reimplementing or concurrently editing `CameraUpdateDecompositionCorrection.md`; its landed result is a prerequisite.
- Menu-input ownership, `gpInput` removal, `FrameInput`, or the broader D9 split.
- Moving or changing visual-error-offset decay or reconciliation behavior.
- Moving menu target constants into the engine.
- Moving menu screens or changing D5 screen ownership.
- Changing client-state file format, version, fields, path, or persistence policy.
- Changing simulation state, frame CRC, replay format, wire protocol, or server behavior.
- Adding compatibility aliases for `game::gpCamera`.
- Adding harness commands, runtime instrumentation, or unit tests.
- Rewriting historical investigation wording.

## Risk tier and invariants

Tier 3 — engine/game ownership integration with public camera API and global lifetime, client render-phase ordering, persistent client-state restoration, project/filter affinity, and cross-subsystem consumers.

Preserve these invariants:

- `engine::gpCamera` has one definition, is registered by the engine constructor, and is cleared by the virtual engine destructor; `game::gpCamera` has no definition or reference.
- The engine camera public interface names only `engine::FrameInterpolateBase`; game-specific frame knowledge stays in game overrides.
- The game camera is the only concrete derived camera and is `final`.
- `CameraTarget` can only be created by Direct, Tracked, or Extrapolate factories; tracked raw position and visual offset remain distinct.
- Raw tracked positions update velocity before visual offset is added.
- Missing-player extrapolation remains clamped to two seconds.
- Explicit camera `fDeltaTime` is the same float value formerly read from `mfLastRenderFrameSeconds`.
- Regular camera update remains before visual-error-offset decay; boot update remains before first render.
- Initial raw sun is `1.4f`; reset raw sun is `1.8f`; main-menu pause and UI override behavior remain unchanged.
- Jump, chase, Hermite zoom, texel references, shake, vibration, matrix, visible-area, and LOD behavior remain unchanged.
- `ResetForSession()` clears exactly the existing reset subset and preserves zoom/current pose/diagnostic state as before.
- `RestoreEyeHeight()` sets live and target eye height directly without changing serialized layout.
- `OnUpdateComplete()` persists at the existing point after matrix/visible-area calculation.
- Camera code remains client-only; no client camera source enters the server project.
- No simulation, CRC, replay, wire, or serialized client-state layout changes occur.
- The update path remains allocation-free and uses no new per-frame containers or logging allocations.

## Coordination

This Plan has a directional prerequisite on `Documents/Plans/Graphics/CameraUpdateDecompositionCorrection.md`. Its future metadata must carry that exact dependency path.

Before this Plan's implementation begins, the prerequisite must have landed and its result must be consumed exactly: `LogMissingPlayer` is already inlined in `ResolveTargetPosition`, and `UpdateJump` is already renamed to `UpdatePosition`. Do not implement these two corrections concurrently, do not reintroduce the removed helper, and do not resolve the ownership move against a moving prerequisite worktree.

After the prerequisite lands, re-read the resulting `Camera.cpp`/`Camera.h` symbols and move the corrected implementation. Any conflict or missing prerequisite result is a plan conflict to return to the manager, not an invitation to improvise.

## Acceptance criteria

1. Client Debug, client Release, and server Debug compile and link through `/compile`.

2. Project/filter coherence passes through `/update-vcxproj`: client contains `EngineCamera.{h,cpp}` exactly once, old engine `CameraBase.{h,cpp}` entries are absent, game `Camera.cpp` remains present, and the server project contains no client camera source.

3. Static ownership sweep passes:

   - zero tracked-code references to `game::gpCamera`;
   - zero `game::Camera::` camera-consumer references under `Engine/Source`;
   - zero `game::FrameInterpolate` names in the engine camera public interface;
   - exactly one `engine::gpCamera` definition;
   - `CameraInput` is defined/stored only by `engine::Camera`;
   - `game::Camera` is `final`;
   - the unused `Update(const Frame&)` overload is absent;
   - `ResetForSession()` and `RestoreEyeHeight(float)` replace the old reset/load field writes.

4. Fresh-AppData runtime acceptance uses only observable terminal state. Before each scenario, record that the per-scenario AppData root did not exist. Define a camera state as settled when two consecutive successful `describe_scene` samples have monotonic ticks and identical parsed `camera.eye`, `camera.visibleArea`, and `camera.lod`.

   - Boot reaches a settled camera state with finite eye, visible-area, and LOD fields.
   - Background wheel input changes the settled eye state smoothly and does not alter simulation state.
   - Existing movement/coord controls exercise the large-target jump path; terminal settled camera state follows the target without a teleport. If an intermediate jump trajectory is not externally observable, use the static order/formula trace rather than claiming it from pixels.
   - Attempt the missing-player path using only existing harness commands. If the focused identity cannot be kept absent long enough for a camera sample, accept the static `PullTarget`/extrapolation trace instead; do not add instrumentation or a command.
   - Settle the camera after restoring the focused player/coordinate and observe recovery to a valid camera state.
   - Exercise the existing Time of Day UI path; raw-sun initial `1.4f`, reset `1.8f`, UI override, menu pause, and update ordering are settled by static source trace because `describe_scene` does not expose sun angle.
   - Issue reset and verify terminal camera state remains valid, saved zoom behavior is preserved, and reset uses only the specified subset.

5. Persistence acceptance uses both terminal scene state and direct versioned-file evidence where produced:

   - after a settled zoom change, retain the client `ClientState.bin` from the scenario root;
   - verify the existing versioned-file header (`version == 3`, payload size `32`) and the persisted target float at the current POD payload offset (`24`, file offset `40`) without changing runtime code;
   - release and relaunch with the same AppData root, then require a settled camera state showing the saved zoom effect;
   - launch a separate absent fresh root and require the initial zoom target rather than the saved target;
   - do not claim separate-run wall-time or intermediate-frame bit identity.

6. Static source review confirms the constructor/destructor global lifetime, `Camera::Update` order, regular pre-offset-decay call, boot call, `OnUpdateComplete` placement, raw-before-offset velocity calculation, reset subset, restore behavior, and unchanged formulas/constants.

7. A fresh non-C++ coherence reviewer independently checks documentation and Visual Studio project/filter XML. The review maps every acceptance criterion to a decisive observation and an independent signal; stale current-state `CameraBase`, old global, old include-path, and game-owned generic-constant references are corrected, while historical investigation wording remains untouched.

8. No unit tests are added, and no simulation, CRC, replay, wire, packed-data, or client-state version bytes change.

## Execution card

### Tier and triggers

- Tier 3.
- Triggers: engine/game ownership inversion, public camera and global lifetime API, engine-only frame interface, client render ordering, persistence timing, project/filter membership, and cross-subsystem consumers.
- Directional prerequisite: `Documents/Plans/Graphics/CameraUpdateDecompositionCorrection.md`; no concurrent implementation.

### Roles

- One `implementer` owns the camera move and returns affected-site notes.
- Fresh `reviewer` for `/plan-audit`.
- Fresh `reviewer` for `/plan-simplicity-review`.
- `/external-grill-plan` after audit decisions.
- `implementer` for `/update-affected-code`.
- Fresh `reviewer` for `/repo-code-review` over changed C++.
- Fresh `reviewer` for `/scope-review` over the whole Tier-3 diff.
- Fresh `reviewer` for `/adversarial-review`.
- `mechanic` for `/update-vcxproj` and `/code-style-review`.
- `implementer` for `/update-claude-docs`.
- Fresh non-C++ coherence `reviewer` for documentation and project/filter XML.
- `builder` for client Debug/Release and server Debug.
- `implementer` for `/agent-harness` runtime acceptance.
- Fresh read-only `/verify-changes` reviewer at the landing gate.

### Criterion-to-check map

- Lifecycle/global ownership → static constructor/destructor and global-definition review → one registered/cleared `engine::gpCamera`, no game global → independent boot and clean process release.
- Engine-only API → header/source grep plus C++ review → only `FrameInterpolateBase` is named publicly; game callbacks perform the cast → client/server compile.
- Closed target contract → C++ review of private constructor/factories and update switch → only Direct/Tracked/Extrapolate values exist; raw position remains separate from offset → missing/recovery runtime attempt or static fallback.
- Timing/order → static trace of `EngineCamera::Update`, `GameBase::UpdateRenderInterpolation`, and boot `Main` → explicit delta and call ordering preserved → settled terminal camera observations.
- Reset/restore → source review plus `ClientSettings.cpp` call-site trace → exact reset subset and direct persisted eye-height restoration → reset and same-root relaunch observations.
- Runtime state → fresh-root harness with settled condition → finite settled eye/visible-area/LOD, zoom effect, jump/recovery behavior → direct `ClientState.bin` target evidence.
- Sun behavior → static trace → initial 1.4, reset 1.8, main-menu pause, UI override, and unchanged formulas → terminal post-reset camera state as an independent runtime health signal.
- Build/affinity → `/compile` plus `/update-vcxproj` → client builds link, server remains free of camera source, XML/filter paths are coherent → fresh non-C++ reviewer.
- Documentation → fresh coherence review plus stale-reference sweep → current docs use EngineCamera/engine ownership and preserve historical packets → final diff scope review.
- Scope/integration → `/scope-review` → only authorized camera ownership/consumer/project/docs regions change → `/adversarial-review` finds no omitted reachable consumer or ordering failure.

## Notes

This is an engine/game ownership refactor, not a new camera capability. The prerequisite correction must land first. The implementation must preserve the current render-only/non-CRC camera behavior and must stop if the prerequisite result or any authority boundary differs from this body.
