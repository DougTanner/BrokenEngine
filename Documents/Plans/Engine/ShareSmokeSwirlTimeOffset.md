<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:50:33.037Z","dependsOn":[]} -->
# Define the Smoke Swirl Time Offset Once

## Context

Found by the architecture review phase of a deep analysis of
`Engine/Source/Graphics/Render` (the directory, not its subdirectories). Line
numbers are from commit `c675b6b2`; locate by symbol if they moved. `R/`
abbreviates `Engine/Source/Graphics/Render/` and `S/` abbreviates
`Engine/Data/Shaders/`.

The smoke swirl noise's circular time offset exists twice with no shared
constant:

- GLSL, `S/Smoke/SmokeSpreadCommon.h:20`:
  `2 * (-1 + 2*sin(0.01 t), -1 + 2*cos(0.01 t))` with `t` =
  `globalLayout.fElapsedTime`.
- CPU, `RenderFrameGlobal` at `R/GlobalUniforms.cpp:613-617`:
  `gSmokeCurlSpeed * kfSmokeCurlTimeScale * 2 * (-1 + 2*sin(0.01 t),
  -1 + 2*cos(0.01 t))`, published as `f2SmokeCurlTimeOffset`
  (`S/ShaderGlobalLayout.h:86`) and read at `S/Smoke/SmokeSpreadCommon.h:63`.

The CPU comment at `R/GlobalUniforms.cpp:611-612` names the shader's number
("0.04 noise UV/s in SmokeSpread") and scales it, so the CPU term is by its
own comment the shader term times a factor: the rate 0.01, the amplitude 2,
and the form are coupled and written in two languages.

The GLSL term's only non-literal operand is `fElapsedTime`, so it is
invocation-invariant, and it runs per invocation in both spread passes
(`S/Smoke/SmokeSpreadOne.comp`, `S/Smoke/SmokeSpreadTwo.comp`).

Authority: root `AGENTS.md` `## Directives` (a value lives once at its owner).
`S/AGENTS.md` `## Shared Contract` routes uniform-only terms to the Render
populators but lists "products, reciprocals, and pre-normalized vectors" and
speaks of new math, so it supports the direction only; the existing shader is
not a violation of that rule.

Unmet criterion: the swirl circular time offset's rate, amplitude, and form
are defined once, and both the swirl and curl offsets derive from it.

## Design

Recommended: add one `vec2` to `GlobalLayout` holding the swirl time offset,
written in `RenderFrameGlobal` beside the existing curl computation; read it
at `S/Smoke/SmokeSpreadCommon.h:20` in place of the inline expression; and
compute `f2SmokeCurlTimeOffset` from the same CPU value times
`gSmokeCurlSpeed * kfSmokeCurlTimeScale`. Update the comment at
`R/GlobalUniforms.cpp:611-612` so it no longer restates the shader's number.

The swirl result changes only by CPU-versus-GPU sine and cosine rounding.

Shader repack step: the Local generation build through `/compile`, since
`GlobalLayout` is a shared CPU/GLSL header and every shader is re-exported.

## Critical files

- `Engine/Data/Shaders/ShaderGlobalLayout.h`
- `Engine/Data/Shaders/Smoke/SmokeSpreadCommon.h`
- `Engine/Source/Graphics/Render/GlobalUniforms.cpp`

## In scope

- One new `vec2` field in `GlobalLayout`.
- `RenderFrameGlobal`: the smoke curl time offset block at
  `R/GlobalUniforms.cpp:611-617`.
- `SmokeSpread`: the `f2TimeNoise` expression at
  `S/Smoke/SmokeSpreadCommon.h:20`.

## Out of scope

- The wind-noise `sin`/`cos` pair with rate 0.5 at
  `S/Smoke/SmokeSpreadCommon.h:16`. It is not duplicated on the CPU; folding
  it would need a second field and is the rule applied to old math, not a fix
  for proven duplication.
- Any other smoke tunable, the curl scale, and `kfSmokeCurlTimeScale`'s value.

## Risk tier

Tier 3, per `.agents/references/risk-tiers.md`. Trigger: data layout — the
shared CPU/GLSL scalar layout of `GlobalLayout` gains a field. Smoke is
client-only visual state outside the CRC: no determinism, serialization,
wire, save, or replay exposure, and no `.pack` format or `kiVersion` change.
The server includes the layout header and is rebuilt.

## Acceptance criteria

- The literal rate 0.01 and the circular form appear once, in
  `RenderFrameGlobal`; `SmokeSpreadCommon.h` reads the published field.
- The Local generation build through `/compile` exports every shader with no
  error and builds the BrokenEngineSandbox client and server and ModelViewer.
- `/glsl-review` reports no CPU/GLSL layout finding for the change.
- Through `/agent-harness`, smoke with swirl and curl enabled looks the same
  before and after in screenshots of one scene. The evidence behind this Plan
  records no harness setup.

## Coordination

No directional dependencies or mandatory nondirectional coordination.

## Notes

- `Documents/Plans/Engine/PublishAreaToUvInverseExtents.md` and
  `Documents/Plans/Engine/RemoveUnreadSunMoonPbrSunLayoutFields.md` also
  change `GlobalLayout` fields, at other positions. No ordering is needed;
  whichever lands later applies its edit to the layout then in the tree.
- The choice of one new field is the author's recommendation, not a user
  decision.
