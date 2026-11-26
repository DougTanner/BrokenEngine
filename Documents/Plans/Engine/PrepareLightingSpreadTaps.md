<!-- broken-engine-plan/v2 {"createdUtc":"2026-11-21T21:04:09.185Z","dependsOn":["Documents/Plans/Engine/CollapseZeroRadiusLightingSpread.md","Documents/Plans/Engine/SkipEmptyLightingSpread.md"]} -->
# Prepare lighting spread taps once per frame

## Context

`LightingSpread.frag:54-67,93-149` derives pass and ring parameters, rotates
each ring's direction vector, computes offsets and direction factors, and
accumulates the normalizer in every output pixel. These values depend on
mutable pass settings and camera framing. Only terrain fade, sampling, and
image-dependent lighting math depend on the output pixel. The uniform
per-sample `fInvSqrtDistance` factor cancels in the normalized output.

## Design

Prepare ordered taps and per-pass normalizers in CPU staging during Global
lighting population, using the active pass count and the current settings.
Publish a bounded per-framebuffer storage buffer with a pass offset/count
table, normalized ring offsets and the directional weights required by the
accepted spread operator. Reuse the per-command-buffer storage descriptor
pattern. Keep the same pass/ring/tap order and use the same quantized tap
counts, angular phases, ring falloff and per-lobe normalization. The fixed
normalizer can be folded into the coefficient for a gather that divides by
the exact sum. If the accepted operator also gathers scalar energy, prepare
its ring weights and fixed normalizer in the same records. Remove the
canceled `fInvSqrtDistance` scale.

Bound the buffer from supported wrappers and `kiMaxSpreadPasses` before
upload. The current maximum is 24 rings with up to 32+j taps in ring j,
or 1,044 taps in a pass before any accepted angular changes. Size the
storage from the actual final tap limits and upload only the active prefix.
Do not place an expanded per-tap array in `GlobalLayout`. Preserve the
effective-radius test, UV mapping and per-pixel low-layer elevation scales.
Per-pixel RGB sampling, compression, directional energy reconstruction,
decay and accumulation remain in the shader.

## Critical files

- `Engine/Data/Shaders/Lighting/LightingSpread.frag:52-169`,
  `Engine/Data/Shaders/ShaderLayoutsBase.h` and the spread layout headers.
- `Engine/Source/Graphics/Render/LightingUniforms.cpp`, `Render.h`, and
  `Engine/Source/Ui/LightingWrappersBase.h`.
- `Engine/Source/Graphics/Managers/BufferManager.cpp`, `BufferManager.h`,
  `WorldLightingShadowPipelines.cpp::CreateLightingPipelines`, and the
  `kPerCommandBufferStorageBuffers` descriptor writer in
  `Engine/Source/Graphics/Objects/PipelineDescriptorWriter.cpp`.

## In scope

- `RenderLightingGlobal` pass/tap preparation, spread-specific bounded
  storage, descriptors, and the GLSL gather setup and coefficient math.

## Out of scope

- Changing the accepted spread operator, ranges or controls, adding more
  persistent lighting images, changing cadence, and unit tests.

## Risk triggers and invariants

Tier 3 for a new CPU/GPU storage data layout and binding. Match array stride
and binding, verify the storage range against device limits, and update
only the acquired and waited framebuffer's storage. Changing a control
must take effect on its next applicable frame without stale tap indices.
Preserve zero-radius sampling and per-texel height behavior.

## Acceptance criteria

1. Compare fixed-scene `Spread`, `SpreadOnly`, directional and ambient
   outputs against the immediate predecessor. Cover one, default and max
   passes, minimum and maximum rings/directions, full falloff/directionality,
   nonzero and zero reach, tilted camera, height fade and live setting edits.
   Report Float16 and UNORM differences and preserve the accepted radial
   shape and directional result within rounding tolerance.
2. Show the resulting shader loop no longer calculates tap rotations,
   offsets from angles, directional factors or fixed normalizers per pixel.
   Record storage size and bounds, CPU preparation/upload cost and GPU
   Lighting Spread profile against the same fixed workload. If the added
   reads or CPU work outweigh the removed work on the intended workload,
   resolve that trade-off before landing.
3. `/compile` builds the BrokenEngineSandbox client and ModelViewer with
   shader data. Use `/agent-harness` for the live setting edits, dumps and
   profiles. No live capture sequence was supplied with the source observation.

## Notes

The shader's camera-space UV scale and the terrain height scale need neither
new settings nor a baked tap table for each camera or terrain height.
