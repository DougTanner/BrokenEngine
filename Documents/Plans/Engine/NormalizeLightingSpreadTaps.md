<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-21T21:01:08.411Z","dependsOn":[]} -->
# Normalize lighting spread by its discrete tap weights

## Context

`LightingSpread.frag:101-149` sums weighted taps separately for each EWNS
channel, then divides all four channels by an assumed continuous angular
mean of 0.32 at full directionality. The final default 4- and 5-direction
rings yield gains of about 0.9090 for E/W and 0.8815 for N/S on a constant
field, before decay and compression. Direction count, ring falloff and start
phase all affect the actual gain.

## Design

At each tap accumulate the exact four-component coefficient already used by
the RGB sums, including ring falloff and the mix between isotropic and
aligned cosine. Divide the three results componentwise by those sums before
applying the pass decay. Keep the tap order, interpolation, sample coordinates
and texture operations. Let the later tap preparation work remove the common
factor and move weight math out of the per-pixel loop.

## Critical files

- `Engine/Data/Shaders/Lighting/LightingSpread.frag:93-156`
- `Engine/Source/Ui/LightingWrappersBase.h`, `gSpreadDirectionCount*`,
  `gSpreadRingCount*`, `gSpreadDirectionality*`, `gSpreadDistanceFalloff*`.

## In scope

- `LightingSpread.frag::main` tap coefficient and normalizer between ring
  setup and the existing pass decay.

## Out of scope

- Redistributing angular energy or changing tap counts, ring phases,
  compression, shaders that read lighting, or unit tests.

## Risk triggers and invariants

Tier 2 for scoped lighting shader behavior. At least two rings leave a
positive inner-ring weight at maximum falloff. Four or more evenly spaced
directions cover every lobe, including at full directionality. Preserve the
per-texel low-layer height decay and the distinction between accumulated
outputs and weighted pre-accumulation outputs.

## Acceptance criteria

1. Feed each tested pass a constant field away from the texture border with
   EWNS vectors `R = (1, 2, 3, 4) / 16`, `G = (2, 3, 4, 5) / 16`, and
   `B = (3, 4, 5, 6) / 16`. Set both compression coefficients to zero and
   use deposit threshold `1/32` and per-pass output threshold `1/256`. Use
   pass decay `3/4` with height fades `0` and `1/8`, giving effective decays
   `3/4` and `21/32`. For both fades, the RGB average of every lobe exceeds
   both thresholds and the denominator floor in `CompressHuePreserving` at
   the pass-0 deposit samples and at the per-pass output after decay.

   Each lobe has unit pre-decay gain at directionality `0`, `1/2`, and `1`,
   including the final 4+5 pattern and minimum rings with maximum falloff.
   After decay and compression, the spread value before curve weighting
   and center addition must equal the input field times effective decay.
   `SpreadOnly` applies the per-pass curve to that value. `Spread` carries
   the value directly in pass 0 and adds the center contribution on later
   passes.
2. `/compile` builds the BrokenEngineSandbox client and ModelViewer with
   shader data. Confirm finite `Spread` and `SpreadOnly` dumps in a fixed
   scene through `/agent-harness`. Record the uniform-field calculation
   independently of floating-point texture rounding.
   The supplied review recorded arithmetic and no live capture sequence.

## Notes

An impulse can still spread unevenly even when a constant field has unity
gain. Evaluate the visible halo on its own acceptance criteria.
