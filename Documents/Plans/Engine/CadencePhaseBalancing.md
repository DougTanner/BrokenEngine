<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-07T12:28:30.718Z","dependsOn":["Documents/Plans/Engine/ShadowUpdateCadence.md","Documents/Plans/Engine/SmokeUpdateCadence.md"]} -->
# Balance Render Cadence Phases

## Context

Three client render chains refresh on a cadence of every N render frames, each with its own slider from 1 to 4:
- **Lighting:** `gLightingUpdateCadence`, with the predicate in `Engine/Source/Graphics/Render/LightingUniforms.cpp` `RenderLightingGlobal`.
- **Smoke:** `gSmokeUpdateCadence`, from `Documents/Plans/Engine/SmokeUpdateCadence.md`, predicate in `RenderSmokeGlobal`.
- **Terrain shadows:** `gShadowUpdateCadence`, from `Documents/Plans/Engine/ShadowUpdateCadence.md`, predicate in `GlobalUniforms.cpp` `PopulateShadowArea`.

Each chain keeps its own function-local render-frame counter starting at 0, and refreshes when `counter % cadence == 0`. All counters advance once per `RenderFrameGlobal`, so they stay aligned. With all three at cadence 2, every heavy refresh lands on the even frames and the odd frames do almost nothing. The GPU frame time then alternates between heavy and light, and the heavy frames set the frame pacing.

The user asked for one schedule that offsets each chain's refresh phase so heavy refreshes spread across frames (for example, lighting on even frames and smoke on odd). It should consider every combination of cadences and weight each chain by its cost.

## Chain set

`SmokeUpdateCadence` and `ShadowUpdateCadence` each stop before writing code if their measured GPU share is below 5%. If the user then rejects a stopped Plan, this Plan's dependency edge becomes satisfied but that cadence never exists.

Before implementing, check which of `gSmokeUpdateCadence` and `gShadowUpdateCadence` exist:
- Balance lighting plus each chain that exists.
- For a missing chain, omit its weight, phase global, cached cadence, search loop, and predicate swap.
- In the acceptance criteria, drop the missing chain's element from each cadence tuple and keep the remaining chains' cadences. For example, with shadow missing, (2,2,2) becomes lighting 2, smoke 2. Skip a criterion that measures only the missing chain.
- If neither exists, stop and recommend that the user reject this Plan: lighting alone has nothing to balance against.

## Design

The author recommends an exhaustive phase search, because it is exact and tiny:
- Each chain has a cadence from 1 to 4, so there are at most 4×4×4 = 64 phase combinations.
- The combined pattern repeats within `lcm` of the cadences, at most 12 frames.
- The search runs only when a cadence value changes.

Fixed weights are recommended over live GPU timers, because the timers are compiled out of Release builds (`kbProfiling`), and fixed weights keep the schedule independent of measurement noise.

1. **Shared counter and phases** (`Render.h`). This state crosses files, so it uses the directory's inline-global convention:
   - `inline int64_t giCadenceFrame = 0;`
   - `inline int64_t giLightingCadencePhase = 0;`, `inline int64_t giSmokeCadencePhase = 0;`, `inline int64_t giShadowCadencePhase = 0;`
   - `inline bool IsCadenceRefreshFrame(int64_t iPhase, int64_t iCadence) { return (giCadenceFrame + iPhase) % iCadence == 0; }`
2. **Advance and resolve** (`GlobalUniforms.cpp` `RenderFrameGlobal`). Before `RenderLightingGlobal`, increment `giCadenceFrame` once and call a file-static `ResolveCadencePhases()`.
   - `ResolveCadencePhases()` reads the three cadence wrappers (`Get<int64_t>()`). If they equal the values cached from the last call (function-local statics), it returns immediately.
   - Otherwise it brute-forces every phase triple, with each phase in `[0, cadence)`. For each triple it computes the per-frame load over `std::lcm` of the three cadences: the sum of the weights of the chains that refresh on that frame.
   - It keeps the triple with the lowest maximum per-frame load, then the lowest sum of squared per-frame loads. Remaining ties go to the first triple in loop order, so the result is deterministic.
   - It writes the three phase globals.
3. **Weights** are three `constexpr int64_t` constants in `GlobalUniforms.cpp`: `kiLightingCadenceWeight`, `kiSmokeCadenceWeight`, and `kiShadowCadenceWeight`.
   - Each is the measured per-refresh GPU cost in whole microseconds, from acceptance criterion 1.
   - A one-line comment names the measurement setup.
4. **Predicates.** Replace each chain's private counter and modulo with the shared check:
   - lighting: `siLightingRefreshFrame` and its `%` in `RenderLightingGlobal` become `IsCadenceRefreshFrame(giLightingCadencePhase, iLightingUpdateCadence)`;
   - smoke: the render-frame counter added by `SmokeUpdateCadence` becomes `IsCadenceRefreshFrame(giSmokeCadencePhase, ...)`;
   - shadow: the counter added by `ShadowUpdateCadence` becomes `IsCadenceRefreshFrame(giShadowCadencePhase, ...)`.

   Forced refreshes (reset, crop safety, smoke clear and disabled paths, first use) are unchanged. They are extra refreshes and do not move the schedule. Smoke's frames-since-refresh step count is unchanged.
5. **Effect of a phase change.** When a slider changes the phases, one chain's next refresh can come earlier or later by up to N-1 frames once. This matches what changing a cadence slider already does today.

## Critical files

- `Engine/Source/Graphics/Render/Render.h`
- `Engine/Source/Graphics/Render/GlobalUniforms.cpp`
- `Engine/Source/Graphics/Render/LightingUniforms.cpp`
- `Engine/Source/Graphics/Render/SmokeUniforms.cpp`

## In scope

- **`Render.h`:** the four inline globals and `IsCadenceRefreshFrame`, placed beside the other cross-file render state.
- **`GlobalUniforms.cpp`:**
  - the three weight constants;
  - the file-static `ResolveCadencePhases()`;
  - the increment and call at the top of `RenderFrameGlobal`;
  - the shadow refresh predicate in `PopulateShadowArea`, where only its counter and modulo are replaced.
- **`LightingUniforms.cpp` `RenderLightingGlobal`:** the `siLightingRefreshFrame` counter and the modulo in `bScheduledLightingRefresh`.
- **`SmokeUniforms.cpp` `RenderSmokeGlobal`:** the render-frame counter and its modulo only.
- **Documentation:** `Engine/Source/Graphics/Render/AGENTS.md` `## Ordering and Publication`. One bullet states that cadenced chains share `giCadenceFrame`, that `RenderFrameGlobal` resolves weighted phases when a cadence changes, and that forced refreshes stay outside the schedule. The lighting update-interval bullet stays as it is.

## Out of scope

- Live or runtime-measured weights, and weights that change with quality settings or enable state.
- Adding wind or any other pass to the cadence set.
- Any change to what a refresh does, to the crop-safety tests, or to forced-refresh triggers.
- New sliders or persisted settings: phases are derived, not saved.
- Any new harness command, profile counter, or GPU timer.

## Risk tier and invariants

**Tier 2.** The change is limited to one subsystem's client render scheduling, with no serialization, CRC, wire, or threading surface.

Invariants:
- `giCadenceFrame` advances exactly once per `RenderFrameGlobal`, before any cadenced predicate reads it.
- Each chain still refreshes exactly once per `cadence` frames on its schedule, plus forced refreshes.
- At cadence 1, a chain refreshes every frame, because its phase is always 0.
- Render state stays outside the CRC.

## Acceptance criteria

1. **Weights.** Use a Debug client, fullscreen 3840x2160, Immediate present mode, default quality, all three cadences at 1, in a heavy-smoke scene. Take `query_profile` cohorts as the `commands-client.md` `query_profile` entry requires. Set the three weights from these `averageUs` values:
   - lighting: Lighting Spread + Lighting Combine + Lighting Temporal;
   - smoke: Smoke Spread;
   - shadow: Shadow.
2. The client and server build cleanly.
3. **Resolved phases.** A code reading of `ResolveCadencePhases` with the measured weights confirms these triples:
   - (2,2,2): the heaviest chain is alone on one parity.
   - (2,2,1): lighting and smoke are on opposite parities.
   - (4,4,4): the three chains are on three different phases.
4. **Frame-time balance.** The profiler reports per-root maxima only, and lighting runs in Main while smoke and shadow run in Global. So only a split between two Global chains is measurable.
   - **If both smoke and shadow exist:**
     - With the camera static and cadences (lighting 1, smoke 2, shadow 2), compare pre-change and post-change builds in the same scene and protocol.
     - Before the change, smoke and shadow refresh on the same frame. After it, they are on opposite parities.
     - The Global root timer's `maxUs` must be lower after the change, and its `averageUs` must stay within cohort noise.
     - Report both cohorts.
   - **Otherwise:** criterion 3 alone verifies the balance.
5. **No visual change.** Screenshots at cadence (2,2,2) match the pre-change look apart from refresh timing.
