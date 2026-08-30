<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-30T12:47:35.048Z","dependsOn":[]} -->
# Fix: Keep the PBR gamma inverse finite

## Context

The frozen C++/GLSL shared-header audit retained `CSG/ShaderMainLayout/002` as
a MEDIUM/HIGH advisory with an observed allowed-input exposure
(`Temp/CppSharedHeaderGlslAudit/80896f33661aaab99cf180a96db54600099be652/ShaderMainLayout.md:66-79`;
the final disposition is also recorded in that directory's `triage.md:35` and
`final-ledger.md:60`).  The false required condition is that every allowed
Gamma tweak value keeps the shader-facing reciprocal finite.  The current
wrapper accepts zero at `Engine/Source/Ui/PbrWrappersBase.cpp:15-20`, and
`Wrapper::Set(float)` clamps inclusively at `Engine/Source/Ui/WrapperBase.h:150-153`.
`Engine/Source/Graphics/Render/LightingUniforms.cpp:354-360` uploads
`1.0f / gPbrGamma.Get()` into `MainLayout::fPbrGammaInv`, declared at
`Engine/Data/Shaders/ShaderMainLayout.h:108-110`; the final fullscreen resolve
uses that value as the exponent at `Engine/Data/Shaders/HdrResolve.frag:39-60`.
Therefore Gamma=0 is a reachable user-controlled input that publishes a
non-finite exponent.  This is pre-existing render/UI behavior outside the
completed findings-only audit and is not a layout or binding mismatch.

## Design

Author's recommendation: change only the `gPbrGamma` wrapper's lower bound
from `0.0f` to `shaders::kfEpsilon` in
`Engine/Source/Ui/PbrWrappersBase.cpp:17`.  `shaders::kfEpsilon` is the
existing shared shader-domain constant `1e-6f` at
`Engine/Data/Shaders/ShaderLayoutsBase.h:121-122`, and the same constant
already floors shader-facing reciprocal inputs in
`Engine/Source/Graphics/Render/LightingUniforms.cpp:274`.  This chooses the
smallest repository-established positive domain floor rather than product
tuning or a second internal clamp.  Keep the default `0.9f`, maximum `2.0f`,
wrapper step, slider registration, reciprocal upload, layout declaration and
HDR resolve expression unchanged.  The resulting allowed finite Gamma domain
is inclusive `[shaders::kfEpsilon, 2.0f]`, so the existing upload remains
`1.0f / gPbrGamma.Get()` and is finite for every finite value the wrapper can
accept.

## Critical files

- `Engine/Source/Ui/PbrWrappersBase.cpp:15-20` — owning Gamma range; change
  only its lower bound.
- `Engine/Data/Shaders/ShaderLayoutsBase.h:121-122` — existing shared epsilon
  that fixes the exact positive bound.
- `Engine/Source/Ui/WrapperBase.h:150-153` — inclusive float clamping that
  enforces the allowed range.
- `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenPbr.cpp:24-27` — Gamma
  slider exposure, which remains unchanged.
- `Engine/Source/Graphics/Render/LightingUniforms.cpp:354-360` — reciprocal
  upload and Main-layout field population, which remain unchanged.
- `Engine/Data/Shaders/ShaderMainLayout.h:108-110` and
  `Engine/Data/Shaders/HdrResolve.frag:39-60` — CPU/GPU field and final
  consumer to verify without changing layout or shader math.

## In scope

- Replacing the `gPbrGamma` minimum `0.0f` with the existing
  `shaders::kfEpsilon` (`1e-6f`) while retaining default `0.9f` and maximum
  `2.0f`.
- Preserving `Wrapper::Set`'s inclusive clamp, the Gamma Tweaks slider, the
  `fPbrGammaInv` field and upload shape, and HdrResolve's existing `pow`
  expression.
- Updating only local wording needed if a touched comment still describes
  zero as an allowed Gamma value.

## Out of scope

- Adding an internal reciprocal clamp or finite fallback in
  `LightingUniforms.cpp`, changing `fPbrGammaInv`, changing any UBO layout,
  descriptor/binding, or shader expression.
- Changing Gamma's default or maximum, other PBR ranges, slider layout, or
  persisted settings behavior.
- Any simulation, CRC, wire, save/replay, `.pack`, threading, or compatibility
  change; unit tests are not part of this Plan.

## Risk tier and invariants

Expected Change Workflow Tier 2 (scoped client render/UI behavior).  The
trigger is a user-controlled Tweaks wrapper domain feeding a client-only
shader-facing reciprocal; no shared layout shape, descriptor, threading,
simulation, or persistence surface changes.  The wrapper source is compiled
for both targets, but the corrected value is presentation-only and does not
enter simulation state.

Preserve these invariants:

- The finite Gamma domain is inclusive `[shaders::kfEpsilon, 2.0f]`; default
  Gamma remains `0.9f`.
- Every finite value accepted by the wrapper produces a finite positive
  `fPbrGammaInv`; zero is no longer an allowed wrapper value.
- The Main layout field remains a single 32-bit float at its current order and
  offset, with the same descriptor/binding and upload path.
- HdrResolve keeps its existing mapped-color and `pow` math, and no value
  computed here reaches Frame/PostRender CRC, wire, save, replay, or `.pack`
  data.

## Acceptance criteria

- Source inspection shows `gPbrGamma` uses exactly `shaders::kfEpsilon` as its
  minimum while default/max remain `0.9f`/`2.0f`, and confirms no reciprocal
  clamp or layout/shader change was added.
- Client Debug and Release builds pass `/compile` for the wrapper and render
  consumers.
- A client `/agent-harness` Tweaks scenario first uses `describe_ui`, opens the
  F3 Tweaks overlay if needed, uses `describe_ui` again, then runs
  `set_slider` for `Gamma` with value `0` and captures a screenshot.  The
  command is accepted and clamps through the finite lower bound; the captured
  frame is defined and the client reports no GPU validation or NaN/Inf error.
- Source comparison confirms the unchanged default `0.9f` and nonzero-value
  reciprocal/upload path, and no simulation/CRC, wire/save/replay, or `.pack`
  bytes change.

## Notes

The audit's external `CSG-EXT-002` request was dropped because this advisory
did not affect routing; the Plan relies on the local zero-denominator and
consumer trace.  The user explicitly authorized this separate durable Plan.
It has no dependency on the independent water height-darkening Plan.
