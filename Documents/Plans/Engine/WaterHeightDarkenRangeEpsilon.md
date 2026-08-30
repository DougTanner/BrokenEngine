<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-30T12:47:42.319Z","dependsOn":[]} -->
# Fix: Keep the water height-darken reciprocal finite

## Context

The frozen C++/GLSL shared-header audit retained `CSG/ShaderMainLayout/001` as
a MEDIUM/HIGH advisory with a credible allowed-input exposure
(`Temp/CppSharedHeaderGlslAudit/80896f33661aaab99cf180a96db54600099be652/ShaderMainLayout.md:51-64`;
the final disposition is also recorded in that directory's `triage.md:34` and
`final-ledger.md:59`).  The false required condition is that the
shader-facing `fWaterHeightDarkenRangeInv` stays finite for every allowed
top/bottom pair while preserving endpoint-order semantics.  The independent
wrappers at `Engine/Source/Ui/WaterWrappersBase.cpp:72-77` allow top in
`[-0.1f, 0.05f]` and bottom in `[-0.5f, 0.0f]`; inclusive clamping in
`Engine/Source/Ui/WrapperBase.h:150-153` therefore permits equality, including
top=bottom=0.  `Engine/Source/Graphics/Render/LightingUniforms.cpp:275-283`
currently uploads the bottom and divides by top-bottom at line 280 without a
guard.  The field is `MainLayout::fWaterHeightDarkenRangeInv` at
`Engine/Data/Shaders/ShaderMainLayout.h:36-38`, and
`Engine/Data/Shaders/Water/Water.frag:141-149` consumes it in the height
interpolation.  At equality, the numerator at the bottom can also be zero, so
the existing comment that surrounding clamping absorbs infinity does not
establish a finite shader input.  This is pre-existing render/UI behavior
outside the completed findings-only audit and is not a layout mismatch.

## Design

Author's recommendation: keep both independent wrapper ranges and the existing
Main field, but make only the local denominator in
`Engine/Source/Graphics/Render/LightingUniforms.cpp:275-283` safe.  Compute
`fWaterHeightDarkenRange = fWaterHeightDarkenTop - fWaterHeightDarkenBottom`,
then use the existing shared `shaders::kfEpsilon` (`1e-6f`,
`Engine/Data/Shaders/ShaderLayoutsBase.h:121-122`) as the minimum magnitude:

```cpp
float fRangeMagnitude = std::max(std::abs(fWaterHeightDarkenRange), shaders::kfEpsilon);
fWaterHeightDarkenRange = fWaterHeightDarkenRange < 0.0f ? -fRangeMagnitude : fRangeMagnitude;
```

Upload `1.0f / fWaterHeightDarkenRange`.  This local expression changes only
an exactly/near-zero magnitude; every range with magnitude at least epsilon
retains its exact reciprocal, negative ranges remain negative, and equality
selects the positive epsilon orientation.  Do not couple the wrappers, narrow
their tuning ranges, add a field, or change Water.frag math.  Update only the
stale local comments that currently call the reciprocal unguarded.

## Critical files

- `Engine/Source/Graphics/Render/LightingUniforms.cpp:275-283` — local range
  calculation and Main-field upload; the only behavior change.
- `Engine/Data/Shaders/ShaderLayoutsBase.h:121-122` — existing epsilon
  contract used to choose the exact floor.
- `Engine/Source/Ui/WaterWrappersBase.cpp:72-77` and
  `Engine/Source/Ui/WrapperBase.h:150-153` — independent inclusive domains
  that must remain unchanged.
- `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenWater.cpp:58-63` — two
  independent slider registrations, unchanged.
- `Engine/Data/Shaders/ShaderMainLayout.h:36-38` and
  `Engine/Data/Shaders/Water/Water.frag:141-149` — field and consumer to
  verify without changing layout or shader math.
- `Engine/Source/Graphics/Managers/WorldLightingShadowPipelines.cpp:356-383`
  — existing Water/Main UBO binding to verify unchanged.

## In scope

- Replacing the unguarded top-minus-bottom reciprocal in
  `RenderLightingMain` with the specified local signed epsilon-magnitude
  expression using `shaders::kfEpsilon`.
- Preserving the existing bottom upload, wrapper independence and inclusive
  ranges, exact reciprocal for all ranges with magnitude at least epsilon,
  sign for inverted ranges, and positive orientation for equality.
- Updating only local comments needed to describe the finite reciprocal.

## Out of scope

- Coupling or narrowing `gWaterHeightDarkenTop`/`gWaterHeightDarkenBottom`,
  changing any other water wrapper, or adding a helper or layout field.
- Changing Water.frag interpolation/clamp math, Main layout order/size,
  descriptor bindings, pipeline publication, or water mesh/displacement
  behavior.
- Other reciprocal advisories, simulation CRC, wire, save/replay, `.pack`,
  threading, compatibility, or unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 2 (scoped client render/UI behavior).  The
trigger is a client-only render populator correction inside an existing UBO
field, with no layout shape, descriptor, threading, simulation, or persistence
change.  Render data remains outside the simulation CRC.

Preserve these invariants:

- For `|top-bottom| >= shaders::kfEpsilon`, the uploaded reciprocal is exactly
  `1.0f / (top-bottom)`, including a negative result for inverted endpoints.
- For a nonzero range with smaller magnitude, the epsilon floor keeps the
  original sign; for equal endpoints, the safe range is positive
  `shaders::kfEpsilon`.
- Every allowed finite wrapper pair produces a finite
  `fWaterHeightDarkenRangeInv`; the existing bottom value and Water.frag field
  interpretation remain unchanged.
- The Main field remains the same 32-bit declaration, order, binding, and
  upload path, and no value reaches Frame/PostRender CRC, wire, save, replay,
  or `.pack` data.

## Acceptance criteria

- Source inspection confirms the exact local expression and these cases:
  top=bottom=0 yields `+shaders::kfEpsilon` and a finite reciprocal;
  top=-0.1/bottom=0 remains a negative exact reciprocal; and the default
  top=0.05/bottom=-0.1 remains unchanged.  No wrapper coupling or shader
  formula change is present.
- Client Debug and Release builds pass `/compile` for the render populator and
  its consumers.
- A client `/agent-harness` Tweaks scenario first uses `describe_ui`, opens the
  F3 Tweaks overlay if needed, uses `describe_ui` again, sets both `Height
  Darken Top` and `Height Darken Bottom` to `0` with `set_slider`, and captures
  a water screenshot.  The commands are accepted, the equality edge produces
  a defined water frame, and the client reports no GPU validation or NaN/Inf
  error; no source instrumentation is added.
- The source trace confirms existing positive and inverted nonzero ranges retain
  their water darkening orientation and exact reciprocal behavior.

## Notes

The audit's external `CSG-EXT-001` request was dropped because this advisory
did not affect routing; the Plan relies on the local zero-denominator,
wrapper-range, and consumer trace.  The user explicitly authorized this
separate durable Plan.  It has no dependency on the independent PBR Gamma
Plan.
