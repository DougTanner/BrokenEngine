<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-20T00:30:37.125Z","dependsOn":[]} -->
# Pin the water normal scroll so harness captures repeat on water

## Context

The before/after pixel baseline rule in
`.agents/skills/agent-harness/references/worker.md` `## Rules` takes paused
captures on fresh client launches. Captures of one build at one tick, each
taken through the full recipe on its own launch, did not match on open water.
At a per-pixel threshold of 8, pairwise agreement over the frame was 0.740,
0.735, and 0.981 across four launches, while terrain, islands, and ship hulls
matched exactly. A before/after check of a water-related change therefore
cannot be judged on open water, and the rule's two-baseline exclusion area drops
most of the water.

The water differs because three client render inputs depend on launch history
or wall-clock time, not on the paused tick:

- Normal-map scroll. `PopulateWaterReducedUv`
  (`Engine/Source/Graphics/Render/WaterUniforms.cpp`) keeps six static
  `sfReducedTime*` accumulators that add size times speed times the
  non-negative elapsed-time delta every rendered frame, with the speed lerped
  by camera framing, and are never reset. They are published as the
  `fWaterReducedNormalTime*` GlobalLayout fields, so the scroll phase at a
  capture depends on every frame and framing since launch. No existing route
  pins them.
- Foam history. `PopulateWaterFoam` in the same file ages foam on a
  `std::chrono::steady_clock` delta between published updates
  (`Engine/Data/Shaders/Water/WaterFoam.comp`), so the foam coverage depends
  on wall time. The existing Debug Tweaks checkbox `Water Foam Enabled`
  (`gWaterFoamEnabled`, `Engine/Source/Ui/WaterWrappersBase.h`; drawn in the
  Water section's Foam subtab, `TweaksScreenWater.cpp`) sets
  `fWaterFoamTextureStrength` to 0, and `Water.frag` then skips foam. Tweaks
  values are not saved (`Engine/Source/Ui/TweaksSettings.cpp` persists only
  layout and visibility), so every launch starts with foam shown.
- Gerstner wave phase. `GameBase::Render` (`Engine/Source/GameBase.cpp`) builds
  `elapsedTime` from the tick's simulation time plus the non-negative tick
  remainder, and `RenderWaterMain` passes it to `PopulateGerstnerWaves`.
  While paused, this time stays at the tick only while the client clock error
  is not negative (`GameBase` time-step handling and
  `Engine/Source/Network/Client/ClientSessionRuntime.cpp`). Whether it holds
  when the clock error is negative is unverified; this is the Plan's open
  part.

All three are client-only presentation state outside PostRender, so none of
them reaches the CRC.

## Design

Recommended, as agreed with the user when this Plan was recorded:

1. Add one Boolean Tweaks wrapper in `Engine/Source/Ui/WaterWrappersBase.h`,
   default `false`, declared just before `gWaterNormalIndexOne` under the
   Specular - Normals block, for example `gWaterNormalScrollPinned`. Its label
   follows the wrapper-label rule in `Engine/Source/Ui/AGENTS.md`
   `## Wrapper Contracts` (`Water Normal Scroll Pinned`).
2. Draw it with `WrapperToggle` in `RenderWaterSection`
   (`Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenWater.cpp`) at the top
   of the Specular subtab, before its `BeginTable`, so the subtab still owns
   one table and the toggle is reachable by harness `click` by window and
   label.
3. In `PopulateWaterReducedUv`, while the wrapper is set, set the six
   `sfReducedTime*` accumulators to zero and skip their integration. Keep
   updating `sPreviousElapsedTime` every frame so clearing the toggle resumes
   from phase zero without a catch-up delta. The published fields then depend
   only on camera position and tunables.
4. Extend the capture recipe in the worker.md `## Rules` baseline paragraph:
   after the last tick advance with the game paused, open Tweaks, show the
   Water section, click the Foam subtab and clear `Water Foam Enabled`, click
   the Specular subtab and set `Water Normal Scroll Pinned`, then set the sun
   angle and close Tweaks as the paragraph already directs, and record both
   values with the recipe. Replace the statement that accumulated render state
   such as the water scroll or foam aging has no pinning route with these two
   routes. Keep the two-baseline agreement area for any input still unpinned.
5. Before relying on the recipe, confirm the Gerstner open part: take repeated
   same-build captures with the recipe and compare them on open water. If water
   still disagrees after the toggle and foam are off, diagnose the remaining
   source with `/external-diagnose-bug` (first candidate: the paused wave time
   while the client clock error is negative) and surface any fix outside
   `## In scope` for re-planning instead of widening scope.

`.agents/skills/agent-harness/references/command-reference.md` documents
`click` and `set_slider` mechanics but no capture recipe, so it needs no change
unless the fix session finds a recipe step there.

## Critical files

- `Engine/Source/Graphics/Render/WaterUniforms.cpp`
- `Engine/Source/Ui/WaterWrappersBase.h`
- `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenWater.cpp`
- `.agents/skills/agent-harness/references/worker.md`
- `Engine/Source/Graphics/Render/AGENTS.md` (`## Precision and History`
  normal-scroll sentence, if `/update-claude-docs` finds the pin needs stating)

## In scope

- One new Boolean wrapper in `WaterWrappersBase.h`.
- One `WrapperToggle` call in the Specular subtab of `RenderWaterSection`.
- The `sfReducedTime*` integration block of `PopulateWaterReducedUv`.
- The before/after pixel baseline paragraph of `worker.md` `## Rules`.
- The normal-scroll sentence of `Engine/Source/Graphics/Render/AGENTS.md`
  `## Precision and History`, only as `/update-claude-docs` requires.
- The Gerstner investigation in `## Design` step 5; its fix region is
  `PopulateGerstnerWaves` and the `elapsedTime` computation in
  `GameBase::Render`, and any fix needing more is surfaced for re-planning.

## Out of scope

- Any new harness command or AgentHarness C++ change.
- Saving the new toggle or `Water Foam Enabled` in `TweaksSettings.bin`.
- Changing the foam aging clock, the foam history, or the Graphics `Water Foam`
  quality setting.
- Pinning any other client visual (wakes, trails, particles, smoke, lighting
  history).
- Simulation, PostRender, CRC, wire, or save changes.
- The within-launch paused-capture difference that
  `Documents/Plans/ChangeWorkflow/AgentHarnessPausedScreenshotComparison.md`
  owns.

## Risk tier and invariants

Tier 2, trigger: one subsystem's client render behavior (water presentation)
plus its Debug Tweaks control and harness guidance; no determinism/CRC,
wire, serialization, save/replay, or threading surface. The state changed is
client-only presentation outside PostRender. The toggle defaults off, so normal
play and default rendering are unchanged. Tweaks is debug-input-gated, so the
recipe runs on a Debug client, as the sun-angle step already requires.

## Acceptance criteria

- With the toggle off, water normal scroll behaves as before (diff
  inspection: the integration path is unchanged when the wrapper is false).
- Live, via `/agent-harness` on a BrokenEngineSandbox Debug client and server:
  at least three fresh client launches of one build, each running the
  extended worker.md recipe (paused at one tick, `Water Foam Enabled` off,
  `Water Normal Scroll Pinned` on, fixed sun angle, fixed sleep), produce
  captures whose pairwise agreement on open water at threshold 8 reaches the
  terrain-level agreement of the same captures. The originating runs recorded
  no exact setup beyond that recipe (the save, camera, and pixel regions are
  not recorded), so the fix session picks a paused scene with open water and
  records its setup with the result.
- If that criterion fails with both pins on, the remaining source is
  diagnosed per `## Design` step 5 and either fixed inside `## In scope` or
  surfaced for re-planning; the Plan does not complete with water still
  disagreeing.
- worker.md no longer says the water scroll and foam have no pinning route.

## Notes

- Related, not a duplicate:
  `Documents/Plans/ChangeWorkflow/AgentHarnessPausedScreenshotComparison.md`
  covers two captures within one launch differing on unit regions, root cause
  unknown, and excludes engine C++. This Plan covers launch-to-launch water
  differences with a known source and an engine fix.
