<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T23:21:05.442Z","dependsOn":[]} -->
# Clamp the smoke trail gradient distance to the unit circle

## Context

Lines are those at commit `401e4b75`.

The `SmokeTrailGradient` texture is filled by a loop at
`Engine/Source/Graphics/Managers/RenderTargetTextures.cpp:221-235`. For each
texel of the 128 by 128 image it computes `fDistance`, the distance from the
centre divided by the half size (`:231`), and then an intensity from
`std::pow(fDistance, fAlpha)` and `std::pow(1.0f - fDistance, fAlpha)`
(`:232`), which it converts to `int64_t` (`:234`) and narrows to `uint16_t`
(`:235`).

The formula is only meaningful for `fDistance` in [0, 1], but the loop
evaluates it over the whole square:

- `fDistance` ranges from 0.01105 at the four centre texels to 1.40317 at
  the four corners. 3,492 of 16,384 texels (21.31%) have `fDistance` above
  1; no texel has exactly 1.
- For those texels `1.0f - fDistance` is negative, between -0.00104 and
  -0.40317.
- At `gSmokeTrailAlpha` 1.0 (the wrapper's maximum; its range is 0 to 1) the
  formula reduces to `1 - fDistance`, a negative intensity. The `int64_t` is
  negative and `:235` narrows it modulo 65,536: the texel just outside the
  circle gets 65,469 (full intensity) and the corner gets 39,115, so the
  outside region is a bright ring fading toward the corners instead of zero.
  This needs no external fact.
- At a fractional alpha, including the default, `:232` raises a negative
  base to a non-integer power. Converting a NaN `float` to an integer type
  is undefined behavior in C++ (verified against the ISO C++ draft,
  [conv.fpint] paragraph 1: the truncated value cannot be represented in
  the destination type). Whether MSVC's `std::pow` returns NaN for that
  input was not settled, so this Plan does not state what the default alpha
  writes today.
- The outside texels are sampled: `Engine/Data/Shaders/Smoke/Smoke.frag:34`
  reads the gradient at the quad texcoord rotated about the centre, with a
  clamp sampler
  (`Engine/Source/Frame/Collections/SmokeTrails/SmokeTrailsRender.cpp:26`),
  so quad corners and clamped edges land on them.
- An alpha change recreates the texture
  (`Engine/Source/Graphics/Graphics.cpp:650-658`).

Originating gap: the in-function phase of an analysis of
`Engine/Source/Graphics/Managers` at that commit. Unmet criterion: every
texel outside the inscribed circle holds 0 for alpha above 0.

## Design

Recommended: clamp `fDistance` to at most 1 at `:231`. With `fDistance` 1
the intensity is 0 for every alpha above 0, and every texel inside the
circle is unchanged. This is a domain bound on a computed value, not the
file or packet bad-value rule.

## Critical files

- `Engine/Source/Graphics/Managers/RenderTargetTextures.cpp`

## In scope

- The `fDistance` computation in the `SmokeTrailGradient` fill loop.

## Out of scope

- The intensity formula for texels inside the circle, the texture size and
  format, and `gSmokeTrailAlpha`'s range.
- Alpha 0, where every `pow` term is `pow(x, 0)` and the texture is 0.5
  everywhere, inside and outside alike.
- `Smoke.frag` and the sampler.

## Risk tier

Tier 2 — scoped behavior. Trigger: the contents of one generated texture
change, which is rendering behavior of one subsystem. The texture is client
render state outside the simulation CRC; no determinism, layout, wire, or
threading exposure.

## Acceptance criteria

1. `fDistance` never exceeds 1 when the intensity is computed.
2. The BrokenEngineSandbox client builds through `/compile`.
3. Through `/agent-harness`, a render-target dump of `SmokeTrailGradient`
   (the texture is created with transfer-source usage for that readback) has
   0 in every texel outside the inscribed circle, at the default alpha and
   at alpha 1.0, and the in-circle texels equal a dump taken before the
   change. The supplied evidence records no harness setup.

## Coordination

`Documents/Plans/Engine/RenderTargetTextureCreationCleanup.md` deletes the
plane-count ASSERT at `RenderTargetTextures.cpp:240`, a few lines below this
loop, and edits other functions of the file. Neither Plan depends on the
other; whichever lands later re-derives its lines by symbol.
