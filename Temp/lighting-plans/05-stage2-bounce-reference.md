# 05 Stage 2: one-bounce indirect reference

This file replaces the earlier prepare-only `Temp/lighting-plans/05-stage2-bounce-reference.md`.
It is a working note, not an executable Plan: it carries no scheduler metadata and authorizes no
source change by itself. The session that runs it drafts the decision-complete plan through the
Change Workflow and gets the user's approval before editing source.

## Objective

Add the shared one-diffuse-bounce indirect reference to the lighting foundation as new commits on
top of pinned foundation `18f7f603964f033794488ad36b92ba100ef2b8da`, so DDGI and Split Radiance
Cascades compare against one reference instead of each building its own. Owning requirement:
`Documents/Investigations/Engine/LightingFoundationHandoff.md`
`## Stage 2: one-bounce indirect reference`.

## Starting point and branch

- Start: `18f7f603` on `origin/lighting-foundation-work`. The `lighting-foundation` branch the
  documents name was never created, and plan 04 was never run.
- Work in a fresh wrapper session worktree moved to `18f7f603` (setup steps are in the session
  handoff). Use `18f7f603` as the baseline for every diff, script and review brief.
- Clustered, Directional Cache and ReGIR/ReSTIR candidates stay on `18f7f603`. Stage 2 must not
  change the direct reference, the fixture, or the existing capture files, so those candidates'
  inputs stay identical.
- The finished stage 2 commit becomes the start commit for DDGI and Split Radiance Cascades.
  Publishing it is a fast-forward push to `origin/lighting-foundation-work`, only after the user
  confirms; never force-push. The original foundation worktree
  (`C:/Users/dougt/.claude/worktrees/BrokenEngine/0cb8b7b1-9695-4b87-ab78-8bf06eceacad`) keeps the
  local `lighting-foundation-work` branch checked out at `18f7f603`; do not touch it.

## What exists at 18f7f603

- `LightingClosestHit` (`Engine/Data/Shaders/Lighting/LightingQuery.h:48`) returns the hit and its
  material. Nothing calls it, so the material lookup for models, terrain and fixture solids is
  untested. Stage 2 is its first consumer.
- The direct reference (`Engine/Data/Shaders/Lighting/LightingReference.comp`) evaluates every
  source with 256 or 512 Halton area samples, shadowed through `LightingOccluded`
  (`LightingRadiometry.h`). Shared ray offset: `kfLightingRayOffset` = 0.05
  (`LightingCommon.h:21`).
- `LightingComparisonSettings` (`Engine/Source/Graphics/Lighting/Lighting.h`): modes Disabled,
  FixtureZero and FixtureReference; scenarios Density (0/100/300/1000 sources, every fifth a 4 m
  rectangle), Point and Rectangle (one isolated source each); cameras Lowest, Default and Event;
  256/512 area samples; event index 0-119. Controls: Tweaks Lighting `Comparison` tab.
- Capture (`LightingCapture.cpp`, `LightingComparison.cpp`) runs progressively in paused frames,
  `kiTilesPerFrame` = 120 (`LightingState.h:55`), and writes the position, shading-normal,
  geometric-normal, albedo and direct `.rgba32f` raws, `finished.png` and `capture.json`.
  Progress is in `query_profile.lighting.capture`.
- Fixture materials: albedo 0.5, colored bounce wall 0.8/0.1/0.1 (`LightingFixture.cpp:159`).

## What DDGI and Split Radiance Cascades need from it

From `LightingPrototypeDDGI.md:68` and `LightingPrototypeSplitRadianceCascades.md:74` and their
`Technique quality` and `Honest radiometry` rows:

- One diffuse bounce: hemisphere rays from each receiver; each hit shaded with direct radiance
  only (no further bounce), using the same sources, materials, radiometry and ray offset as the
  direct reference.
- Indirect output kept separate from direct, so candidates compare indirect alone with identical
  direct lighting and materials.
- A visible colored bounce from the red wall across receiver heights.
- Converged enough to serve as the "converged reference" in their screenshot comparisons.

## Decisions the plan must put to the user

1. Reference scene size. Cost per pixel is about B bounce rays x (one closest-hit query plus a
   full direct evaluation at the hit). A direct evaluation costs one shadow ray per point source
   plus S rays per rectangle: 52,000 for 1000 sources at S = 256 (800 + 200 x 256), about 5,200
   for 100 sources. At 1920x1080, B = 256 and 1000 sources that is about 2.8 x 10^13 queries per
   capture, which is impractical. (The old plan's 10^16 figure was wrong; the conclusion stands.)
   Levers: scenario (Point or Rectangle with one source, or Density at 100), B, area samples at
   the hit (may be lower than at the receiver), image resolution or a pixel subset, and camera
   preset. First measure the current direct capture's wall time at Lowest and Default; the
   presets were never benchmarked after the camera change. Record the choice in the handoff
   document and in `capture.json`.
2. Integration shape: a new comparison mode next to FixtureReference, or an option on it; and the
   name of the new raw output (for example `indirect.rgba32f`).
3. Bounce sampling: cosine-weighted hemisphere directions from a fixed sequence, like the Halton
   area samples, so the commit alone reproduces a capture.

## Constraints

- Reuse the fixture, sources, `LightingClosestHit`/`LightingOccluded`, the capture lifecycle, the
  comparison controls and the composition seam. No second scene or capture framework.
- Direct output must not change for the same settings.
- Client only: no simulation, save, wire or CRC change. No new harness commands. No unit tests.
- `LightingFoundationHandoff.md` `## Known omissions` and `## Decisions` still bind: off-screen
  terrain is not traced, MASK is opaque, BLEND/water/smoke/sprites/shields do not occlude, one
  shared 0.05 m offset.
- One current format: if `capture.json` gains fields, there is no compatibility path for old
  captures.
- These prototypes may be discarded; build the smallest reference that meets the needs above.

## Steps

1. Set up the worktree per the session handoff. Copy this file over
   `Temp/lighting-plans/05-stage2-bounce-reference.md` (LF line endings, no BOM) as the
   session's first change; the static checks' `line-length` row settles any rewrap.
2. Change Workflow: `/prepare-change`, `/plan-alternatives`, then `/plan-audit` and
   `/plan-simplicity-review`. The old 05 assumed Tier 3, which adds `/external-grill-plan`; the
   preparation classifies.
3. Present the plan with the three decisions above, options and a recommendation. Stop for
   approval.
4. After approval: implement, propagate, and run the review and cleanup steps the tier triggers,
   all against baseline `18f7f603`. Do not run `/finalize-changes`.
5. Update `LightingFoundationHandoff.md`: `## Stage 2` says what now exists and the chosen scene
   size; `## Known omissions` replaces the "`LightingClosestHit` ... is untested" bullet with what
   was verified and what still is not.
6. Commit on the session branch, then ask the user before pushing.

## Acceptance (proposed; the approved plan carries the final table)

| # | Criterion | Pass |
| --- | --- | --- |
| 1 | Builds | `/compile`: DataPacker Release x64 with Local data, Sandbox client and server Debug x64; ModelViewer Debug x64 if code it compiles changed. |
| 2 | Direct unchanged | The direct evaluation and its dispatch are unchanged in the diff; if they had to change, a `direct.rgba32f` from an `18f7f603` build and one from the stage 2 build at the same settings are byte-identical. |
| 3 | Indirect capture | The chosen scene completes; it writes the new indirect raw plus the existing seven files; `capture.json` records the bounce parameters; wall time is reported. |
| 4 | Material lookup | Receivers facing the red wall show indirect red clearly above green and blue; hits on terrain, a rigid model and the skinned mesh return their albedo (method set in the plan; `// TEMP START` blocks allowed). |
| 5 | Zero path kept | Disabled and FixtureZero behave as at `18f7f603`; gameplay local diffuse is still zero. |
| 6 | Vulkan clean | Client log has no `DebugUtilsCallback`, `CheckVk failed`, `VK_ERROR_DEVICE_LOST` or `DEBUG_BREAK at`. |
| 7 | Simulation/CRC | `git diff 18f7f603` shows no simulation, save, wire or CRC change; neither log has desync lines. |
| 8 | Docs and checks | Handoff document updated per step 5; static checks pass with `-Baseline 18f7f603`. |

## Finish

- Report the foundation and stage 2 commit IDs, the decisions taken, capture wall times, and
  which checks ran and which did not.
- After the user confirms: `git push origin HEAD:lighting-foundation-work` (fast-forward only).
- The user then pins the new commit in the DDGI and Split Radiance Cascades launch text.
