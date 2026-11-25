<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-21T21:04:04.470Z","dependsOn":["Documents/Plans/Engine/RemoveFlightPlanes.md"]} -->
# Remove the compass-shaped world lighting halo

## Context

The reported in-game halo has bright compass arms and responds little to
repeated experiments with the lighting sliders. The review supplied no
game capture, scene setup, or recorded harness sequence. Source and
synthetic arithmetic show two plausible contributors, recorded here for
the execution session to reproduce:

- `LightingSpread.frag:101-149` starts each ring at +X and applies aligned
  cosine to each EWNS lobe independently. The final default rings have four
  and five taps at 10 and 20 m. A unit radial `cos²` deposit from
  `PointLight.frag:59-64` sums to one, but a matched full-directionality
  transport step yields 1 on an axis and about 0.707 on a diagonal before
  normalization. With the original 64-pass synthetic probe, axis/diagonal
  was 1.106 at 140 m and 0.961 at 80 m. Denser rotated rings still gave
  1.105 at 140 m. A one-pass probe cannot establish the final scene result.
- In the original `LightCombine.comp:94-133`, one achromatic 0.2 lobe maps
  to approximately 0.1447 while two 0.1 lobes map to approximately 0.0302
  in total with default settings. With the ambient divide by four, those
  are about 9/255 and 2/255 after storage. Equal pre-map energy can create
  an axis advantage even if spread preserves the sum.

## Design

Reproduce the cross on the original tree at
`4ebddbe4eebf63133ae450a5f50ed925e9846556` through `/agent-harness` and
`/external-diagnose-bug`. Record the scene or replay recipe, emitter pose,
camera basis and framing, lighting settings, resolution, temporal reset or
settle sequence and raw targets. Re-run it on the immediate predecessor to
isolate remaining artifacts. If the predecessor already meets all visual
criteria against the original, retain that evidence and complete this stage
without extra production math changes.

Use the user-reported scene when available or a controlled equivalent that
reproduces the cross. Hold interpolation during capture of paused gameplay
with `pin_graphics`, where applicable. This does not freeze refresh cadence
or animated counters. Identify an actual populated refresh for `Lighting`,
`Spread`, and `SpreadOnly`, which clear on skips. Compare one frame capture
or separately repeated equivalent refreshes because separate dump calls do
not establish a common frame. Request `raw:true` for native Float16 samples
and capture resolved `Combine` and `AmbientCombine` UNORM8 separately.

Trace matched world-space radii and azimuths through deposit, the accumulated
and snapshot outputs, combine before history when possible, resolved
directional/ambient targets, and the lit surface. Sum or reconstruct energy
in linear data for the field comparison. Keep separate directional and
receiver comparisons. Isolate an early pass, the terminal field and a flat
receiver before assigning the effect to a stage. Vary the emitter plane or
receive on a flat surface to separate terrain fade and material weighting.
Use temporary diagnostics during diagnosis if a final dump hides a stage.

For a confirmed combine contribution, sum the nonnegative input EWNS lobes
per color before tone mapping. Use a concentrated lobe as the reference
for the tone curve and for how much mapped light fits in the UNORM range.
In per-color mode keep the curve's black lift as a separate base in each
lobe. Subtract that lift from the mapped total, cap the remaining light at
one minus the lift, and distribute it in proportion to the input lobes.
At zero input, write the base alone. In hue-preserving mode divide the
tone-mapped aggregate Rec.709 energy by its input, with the existing
near-black denominator floor. Limit this gain by the reciprocal of the
largest input color total for nonzero input. Apply it to every color and
lobe to preserve RGB ratios where differently colored lights overlap. At
zero input, keep the hue-preserving result black. Apply the existing mode
blend to these bounded results. Derive ambient from the same clamped
directional values using the existing average.

For a confirmed spread contribution, use the same RGB samples to accumulate
an angularly balanced scalar energy and directionally weighted lobes. Scale
the weighted lobes to that energy, preserving their relative directions.
Use that tap set's unweighted lobe distribution where directional support
vanishes and retain zero for zero energy. Make ring phases/tap coverage
rotationally balanced only where the recorded scalar field shows it is
needed. Check the effects of the pass compressors on the reconstructed
energy before applying the smallest correction at a proven causal site.
Treat material math as a diagnostic control. Escalate a confirmed cause
outside the scoped shader regions for re-planning with the stage evidence.

## Critical files

- `Engine/Data/Shaders/Lighting/PointLight.frag:48-88`, `AreaLight.frag`,
  `LightingSpread.frag:93-169`, `LightCombine.comp:78-143`, and
  `LightingTemporal.comp`.
- `Engine/Data/Shaders/ShaderFunctions.h:115-174`, `Terrain/Terrain.frag`,
  `Water/Water.frag`, and `Model/Model.frag` for receiver diagnosis.

## In scope

- Root-cause isolation in the deposit, spread, combine, and receiver path.
- The proven shader cause in the spread angular gather or per-lobe tone-map
  application, including energy/range handling at those sites.

## Out of scope

- Tuning slider presets or changing visible sprites, depositor art, receiver
  material math, lighting layer count, texture formats, history or cadence.
- Gaussian or radiance-cascade redesigns and unit tests.

## Risk triggers and invariants

Tier 2 for scoped client lighting shader behavior. Preserve EWNS meaning,
orientations of area lights and beams, asset color behavior, lighting on
directional materials, and the no-light result. Preserve spread-only versus
accumulated flow and ambient, temporal, and history publication contracts.
Work in the active lighting layers at execution time.

## Acceptance criteria

1. At identical saved control values, emitter, camera, scene and temporal
   state, the original capture shows the cross and the final in-game
   capture shows the halo without the compass pattern. Compare both against
   the immediate predecessor and retain the recorded reproduction steps.
   A synthetic kernel experiment or a shader diff alone cannot settle this.
2. On a flat reference receiver with a circular emitter, overlay azimuthal
   profiles at matched world radii on the same exposure. Report axis versus
   diagonal and the fourth angular harmonic across the visible halo,
   including inner and outer bands. Record radial falloff, peak and
   radius-integrated brightness before and after. A pass retains a visibly
   lit halo and falloff without an axis-locked shape. Do not dim, clip, or
   flatten the halo to hide the pattern.
3. Repeat with a rotated or diagonal emitter, a colored overlap, and a
   sloped or otherwise directional receiver. Verify that the deliberate
   beam orientation and receiver-facing highlights follow the light and
   surface, and smoke, visible sprites and retained frames keep their intended
   responses. Compare real overlapping bright emitters and each emitter alone
   against the immediate predecessor with the same exposure and saved control
   values. Compare the overlap and the surrounding unsaturated halo using
   screenshots, absolute peak and integrated brightness, radial falloff, and
   RGB and directional lobe ratios. Check that brightness loss tracks the
   diagnosed angular bias or the bounded saturation response, while bright
   overlaps and the surrounding falloff remain visibly lit. Inspect Float16 and
   resolved targets for zero-energy and saturated cases. Keep screenshots and
   data beside the acceptance record at a location subsequent sessions can
   retrieve.
4. If the fix changes combine, exercise the real combine stores with identical
   RGB input totals concentrated in one lobe and split across lobes. Set fixed
   shader diagnostic inputs after pass summation and scaling. Read the first
   populated refresh after a history reset. Test dim and saturated levels,
   mixed colors, zero input and nonzero black lift, with hue preservation off,
   on and at an intermediate blend. Read the actual `Combine` and
   `AmbientCombine` bytes. For each pair, compare the sum of the stored EWNS
   lobes per color and the stored ambient against each other and against the
   predicted bounded totals and their directional means, within the combined
   UNORM8 rounding error. An achromatic mapped light total of 1.25 with zero
   lift must store a sum of about 1.0 in both cases and ambient of about 0.25.
   A concentrated 1.25 clamped to 1.0 and two stored lobes near 0.625 fail
   this check. Compare directional proportions after accounting for the
   mode-weighted black base and check RGB ratios in hue-preserving mode.
5. `/compile` builds the BrokenEngineSandbox client and ModelViewer with
   shader data. `/agent-harness` captures the before and after frames and
   target dumps through the real render path. Record baseline setup even if
   the defect initially cannot be reproduced and keep the criterion pending
   until it can be settled.

## Notes

The one-step synthetic result follows directly from the matching cosine
weights. On an axis it is `1² * 1`. At 45 degrees it is
`2 * (1/sqrt(2))² * (1/sqrt(2))`. Recompute the combine example by applying
the original default curve to 0.2 and 0.1. The recorded 64-pass probe values
are background observations. Reproduce a radial deposit from the original
source and record any sampling, border, resolution and precision assumptions
when repeating a synthetic comparison. The scene and its measured profiles
drive acceptance.
