<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-13T23:06:16.120Z","dependsOn":[]} -->
# Keep Water Color Noise Frequencies Positive

## Context

`PopulatePatchLattice` (`Engine/Source/Graphics/Render/GlobalUniforms.cpp`)
derives a lattice exponent from `log2` of the frequency it is passed, and wraps
that in an `if (fFrequency > 0.0)` branch that leaves the exponent at 0 for a
zero frequency. Its declaration comment in
`Engine/Source/Graphics/Render/Render.h` carries the matching "or 0 for zero
frequency" case.

`PopulateWaterReducedUv` in `Engine/Source/Graphics/Render/WaterUniforms.cpp`
holds the only calls, eight in all. Six can never pass zero:

- the three water normal reads pass a positive
  `kfWaterNormal*OctaveOneSizeMultiplier` constant times a size base built from
  `gLightingSampledNormals*Size` (minimums 0.05, 0.025, 0.02) and `gWaterScale`
  (minimum 0.3);
- the three foam reads pass `1 / (tile size * gWaterScale)`, and every foam tile
  size wrapper has a positive minimum.

Only the two color-noise reads can pass zero. They pass
`gWaterColorNoiseMultiplierOne` or `gWaterColorNoiseMultiplierTwo` times
`gWaterColorNoiseFrequency / gWaterScale`, and all three wrappers
(`Engine/Source/Ui/WaterWrappersBase.h`) have a minimum of 0.

Every write to those three wrappers goes through `Wrapper::Set(float)`
(`Engine/Source/Ui/WrapperBase.h`), which snaps and clamps to the wrapper's
range: the only writer is `TweaksScreen::WrapperSlider`
(`Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenWater.cpp`), reached by
mouse or by the agent `set_slider` command, which types into the same slider.
No settings file stores them (`TweaksSettings.bin` holds only Tweaks layout),
no game or agent code assigns them, and their defaults (0.0013, 0.2, 1.0) are
above zero. So a positive minimum on each makes the zero-frequency branch
unreachable.

Zero is not a useful tuning value for any of the three: it collapses the noise
read to one constant texel, and `gWaterColorNoiseAmount` or the two
`gWaterColorNoiseWeight*` wrappers already turn the noise off.

Originating gap: a logged refactor candidate, not an unmet acceptance criterion
of another change.

## Design

Recommended shape (the author's recommendation; rationale given):

- `gWaterColorNoiseFrequency`: minimum `0.0001f` in place of `0.0f`. Rationale:
  it is one hundredth of the slider's maximum and well under the default, so no
  useful tuning range is lost.
- `gWaterColorNoiseMultiplierOne` and `gWaterColorNoiseMultiplierTwo`: minimum
  `0.1f` in place of `0.0f`. Rationale: `0.1` is their existing step, so it is
  the smallest positive value on the grid the `Water.frag` `fract()` wrap
  needs.
- One comment at those wrappers stating that the positive minimums keep every
  `PopulatePatchLattice` frequency positive, since
  `Engine/Source/Ui/AGENTS.md` `## Wrapper Contracts` treats annotated bounds
  as correctness constraints.
- `PopulatePatchLattice`: remove the `if (fFrequency > 0.0)` branch and compute
  the exponent unconditionally.
- `Render.h`: drop "or 0 for zero frequency" from the `PopulatePatchLattice`
  comment and state that `fFrequency` is positive.
- `Engine/Data/Shaders/Water/Water.frag`: the color-noise comment lists the
  multiplier grid as `(0.0, 0.1, 0.2, ...)`; drop `0.0` from that list.
  Comment only.

Risk tier: Tier 2. The trigger is scoped runtime behavior of one client-only
visual path: the range of three Tweaks sliders and the lattice exponent
computation they feed. Render sits outside the CRC and nothing here feeds
simulation state, so there is no determinism, wire, serialization, `.pack`,
`kiVersion`, save, replay, threading, or trust-boundary exposure. No layout
field changes. `WaterWrappersBase.h` compiles into both client and server, so
both build. Shader exposure: one GLSL comment.

## Critical files

- `Engine/Source/Ui/WaterWrappersBase.h`
- `Engine/Source/Graphics/Render/GlobalUniforms.cpp`
- `Engine/Source/Graphics/Render/Render.h`
- `Engine/Data/Shaders/Water/Water.frag`

## In scope

- The minimum argument of `gWaterColorNoiseFrequency`,
  `gWaterColorNoiseMultiplierOne`, and `gWaterColorNoiseMultiplierTwo`, and the
  one comment beside them.
- `PopulatePatchLattice`: the `fFrequency > 0.0` branch around the
  `report.iExponent` assignment.
- The `PopulatePatchLattice` declaration comment in `Render.h`.
- The multiplier-grid list in the `Water.frag` color-noise comment.

## Out of scope

- The defaults, maximums, and step of the three wrappers, and every other
  wrapper.
- The exponent range (`kfMinimumExponent`, `kfMaximumExponent`), the anchor and
  remainder split, and the seed in `PopulatePatchLattice`.
- `PopulateWaterReducedUv` and its call sites.
- An `ASSERT` or other check on `fFrequency`.
- Any shader code, layout field, or `Wrapper` class change.

## Acceptance criteria

- The diff shows the three raised minimums and the removed branch, and the
  client and server build.
- With default settings, the agent presentation continuity probe reports the
  same `exponent` for `waterColorNoiseOne` and `waterColorNoiseTwo` as before
  the change. The supplied evidence records no harness setup; any in-game view
  serves, with the same view before and after.

## Notes

- Before this change a zero frequency reports exponent 0. The branch does not
  guard undefined behavior: without it `log2(0)` is negative infinity, which
  the existing clamp turns into `kfMinimumExponent`. Deleting the branch alone
  would therefore also be safe; raising the minimums additionally removes the
  degenerate slider values and lets the contract say the frequency is positive.
