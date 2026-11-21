<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-21T21:01:09.893Z","dependsOn":[]} -->
# Keep the lighting tone curve finite and monotonic

## Context

`RenderLightingGlobal` derives Uchimura shoulder constants from independent
sliders (`Engine/Source/Graphics/Render/LightingUniforms.cpp:127-145`). In
`LightCombine.comp:24-66` and `DebugTexture.frag:33-48`, an inactive shoulder
can evaluate `exp(157)` at black with valid P=0.305, m=0.30, a=1.3, l=0.5.
Multiplying infinity by its zero weight can propagate NaN. The wrappers allow
P at or below m. The default toe blend also yields about -0.00133 at 0.05.

## Design

Derive the effective linear start on the CPU from the requested m and P,
reducing m to at most P minus the existing epsilon scale to keep a positive
shoulder length. Compute the effective black offset in [0, m] and the
shoulder constants from the effective values. Keep P, contrast, linear length
and their existing settings ranges. Document the effective parameter ordering
beside the controls, including the cap on black lift when m is small.

Use one shader implementation in the shared lighting math used by combine and
the directional Float16 debug view. For x from zero to m, use
`b + (m - b) * pow(clamp(x / m, 0, 1), c)`. At m and above retain the linear
and shoulder segments. Bound the shoulder exponential argument at zero before
evaluating it, including when evaluated for inactive vector lanes. The three
segments join at m and S0, are nondecreasing, and produce the effective black
lift at zero. Keep per-color and luminance modes and debug input scaling.

## Critical files

- `Engine/Source/Graphics/Render/LightingUniforms.cpp:127-145`
- `Engine/Source/Ui/LightingWrappersBase.h`, `gCombineMaxBrightness` through
  `gCombineBlackTightness`, and the Combine controls in
  `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenLighting.cpp`.
- `Engine/Data/Shaders/Lighting/LightCombine.comp:21-67,94-134`,
  `Engine/Data/Shaders/ShaderFunctions.h`, and
  `Engine/Data/Shaders/DebugTexture.frag:31-53`.

## In scope

- Effective Uchimura parameters in `RenderLightingGlobal`, their explanation
  in the Lighting tweak UI, and the scalar/vector curve and debug mirror.

## Out of scope

- Curve sampling over pass count, ambient packing, angular transport, color
  mode semantics, settings file shape, storage formats, and unit tests.

## Risk triggers and invariants

Tier 2 for client lighting tone behavior across its uniform producer and
shader/UI consumers. Keep the CPU/GPU uniform layout intact. Black at default
lift remains black. The power base and shoulder evaluation remain finite for
finite nonnegative Float16-source lighting, including inactive vector lanes.

## Acceptance criteria

1. Exercise defaults, the near-coincident P/m example, P=m, P<m and the
   extreme slider combinations. Check zero, 0.05, both segment joins, and
   very bright values. Raw scalar and vector output is finite, nonnegative,
   continuous and nondecreasing, with a bounded shoulder and matching debug
   evaluation for equal scaled inputs.
   `kiDebugTextureFormatFloat16LightingDirectional` is unregistered in
   `RegisterDebugTextures`. In a temporary local shader diagnostic, route the
   Float16 deposit view through that fragment branch and feed known samples
   through its existing pass normalization. Feed the combine scalar and
   vector paths the same scaled inputs and effective uniforms, including
   vectors with lanes in different segments. Enable `ENABLE_DEBUG_PRINTF_EXT`
   to log results from selected invocations before directional coloring or
   UNORM storage. Remove the diagnostic before the scene comparison.
2. `/compile` builds the BrokenEngineSandbox Debug client and ModelViewer with
   shader data. Through `/agent-harness`, reproduce the settings above,
   including the supported slider extremes, in the same scene before and
   after. Inspect the rendered scene and registered UNORM combine view for
   output. Use the registered Float16 deposit and spread views to check
   lighting inputs. Cover hue-preserve off, on, and intermediate, including
   zero-energy input. The supplied review recorded arithmetic and no live
   capture sequence.

## Notes

The new power toe can change dim material response. Record that change at
fixed settings so the halo stage can distinguish it from angular behavior.
