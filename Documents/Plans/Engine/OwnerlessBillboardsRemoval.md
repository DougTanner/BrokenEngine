<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-30T12:56:24.825Z","dependsOn":[]} -->
# Remove ownerless Billboards capability

## Context

The false required condition is that the client must keep a complete
Billboards collection and render pipeline without a game owner. The leaf
authority states that no types are registered and no game-side `Add`, `Sync`,
or `Remove` callers exist (`Engine/Source/Frame/Collections/Billboards/AGENTS.md:1-4`).
Nevertheless, the paired collection is stored in every client frame and its
graphics hook creates a dynamic buffer and pipeline
(`Engine/Source/Frame/Collections/Billboards/Billboards.h:20-87`;
`BillboardsRender.cpp:12-38`), while the render walk and profile paths run with
zero rows. The exact frozen source/caller evidence is recorded for
`CPS/shard-0005/001`.
The user explicitly directs complete deletion with no
replacement feature. The concern is pre-existing at session baseline
`80896f33661aaab99cf180a96db54600099be652`.

## Design

The author's recommendation is to delete the ownerless collection as one
client Frame/Graphics/GLSL/project-membership change. Remove the three
collection source files and leaf instructions, the client Frame include,
members, tuple entries, and client collection count adjustment; remove the
Billboards dynamic pipeline enum/API/implementation, dynamic buffer creation,
main render draw loop, CPU/GPU profile counters and name rows, and the two
Particles Billboards shaders. Remove `BillboardLayout` from the shared shader
header because exact searches show those shaders were its only consumers. Also
remove now-dead `GlobalLayout::fAspectRatioInv` from
`Engine/Data/Shaders/ShaderGlobalLayout.h` and its CPU producer in
`Engine/Source/Graphics/Render/GlobalUniforms.cpp`, then remove the stale
`BillboardsRender` example from the `BufferManager.cpp` resize comment while
retaining the live `PlayersRender` example. Verify the remaining CPU/GLSL
`GlobalLayout` fields and offsets as part of the shared-layout deletion.

Run the normal project-membership reconciliation for every removed source and
shader. Preserve the unrelated `DebugRenderBillboard.vert`, `MainLayout`'s
camera-facing debug-billboard basis, debug-circle pipeline, generic particle
paths, and all live Frame collection ordering/counts. Do not create a lazy
gate, synthetic type registration, forwarding API, or replacement indicator.

## Critical files

- `Engine/Source/Frame/Collections/Billboards/Billboards.h`, `Billboards.cpp`, `BillboardsUpdate.cpp`, `BillboardsRender.cpp`, and their leaf `AGENTS.md`/`CLAUDE.md` — deleted collection and hooks.
- `Engine/Source/Frame/FrameBase.h:1-7,67-123,195-230` — include, storage, tuples, and client count.
- `Engine/Source/Graphics/Managers/DynamicPipelines.h:15-31,41-60` and `DynamicPipelines.cpp:234-260` — removed pipeline type/API.
- `Engine/Source/Graphics/Managers/CommandBufferRecordMain.cpp:420-425` — removed draw/timer loop.
- `Engine/Source/Profile/ProfileManagerBase.h:85-125,195-256` — removed profile enum/name rows.
- `Engine/Data/Shaders/ShaderLayoutsBase.h:202-209` — unused `BillboardLayout` declaration.
- `Engine/Data/Shaders/ShaderGlobalLayout.h:3-15` — now-dead `fAspectRatioInv` field in the shared CPU/GLSL GlobalLayout.
- `Engine/Source/Graphics/Render/GlobalUniforms.cpp:491-500` — now-dead GlobalLayout producer.
- `Engine/Source/Graphics/Managers/BufferManager.cpp:454-463` — stale Billboards resize example.
- `Engine/Data/Shaders/Particles/Billboards.vert`, `Billboards.frag`, and `Particles/AGENTS.md` — deleted feature shaders and stale authority prose.
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj` and `BrokenEngineSandbox.vcxproj.filters` — remove client Billboards source/header/shader membership; `BrokenEngineSandboxServer.vcxproj.filters` — remove only its obsolete empty Billboards filter; leave `BrokenEngineSandboxServer.vcxproj` untouched.

## In scope

- Deleting the Billboards collection, its empty Update/Add/Remove/Sync hooks,
  resource/render hooks, dynamic buffer/pipeline, profile rows, shared
  `BillboardLayout`, feature shaders, and project/filter membership.
- Removing the now-dead `GlobalLayout::fAspectRatioInv` declaration and
  `GlobalUniforms.cpp` assignment, and checking the remaining CPU/GLSL
  GlobalLayout layout and all remaining shader consumers.
- Removing every now-dead Frame tuple registration and graphics traversal hook,
  while updating client collection count and preserving server/client parity.
- Removing only Billboards-specific authority prose and validating the
  remaining shader/project references with `/update-vcxproj` and shader review.
- Proving no game-side type registration, `Add`, `Sync`, `Remove`, or current
  renderer consumer exists before deletion.

## Out of scope

- `DebugRenderBillboard.vert`, `MainLayout` billboard basis fields, debug
  circles, visible-light billboards, additive particles, or any other generic
  debug/render path.
- Remaining `GlobalLayout` fields, CPU populators, shader bindings, and layout
  consumers beyond the removed `fAspectRatioInv` field.
- Adding a replacement offscreen indicator, registry, type, UI feature, or
  synthetic caller.
- Changing unrelated Frame collection order, SOA layout, deterministic state,
  wire/save/replay formats, textures, or profile semantics beyond removing
  the Billboards rows.
- The separate future-only `ObjectLayout`/`ModelCustomLayout` cleanup, except
  for reciprocal coordination in the shared shader header.

## Risk tier and invariants

Expected Change Workflow Tier 3. Trigger: deletion crosses client Frame
collection registration, Graphics resource/pipeline ownership, dual-language
shader assets including the shared CPU/GLSL `GlobalLayout`, profiling index
tables, and Visual Studio project membership.

Preserve these invariants:

- Client and server `Frame::Collections()` tuples remain ordered and their
  static counts/names remain consistent after the client-only removal.
- No Billboards GPU resource, descriptor, pipeline, shader, draw, or profile
  row remains reachable; unrelated debug billboard resources remain intact.
- The remaining CPU and GLSL `GlobalLayout` declarations, producers, field
  order, offsets, and bindings agree after removing only `fAspectRatioInv`.
- No collection ID/CRC, deterministic frame state, wire/save/replay format, or
  server behavior is changed by the client-only deletion.
- The project files contain every remaining source/shader exactly once and no
  deleted Billboards member.

## Acceptance criteria

- Exact repository searches over first-party C++/GLSL/project files prove no
  current `BillboardsInterpolate`, `BillboardsPostRender`, `BillboardFlags`,
  `billboard_t`, `kDynamicPipelineBillboards`, `kCpuCounterBillboards`,
  `kGpuTimerBillboards`, `BillboardLayout`, or `fAspectRatioInv` consumer
  remains, while the explicitly unrelated `DebugRenderBillboard.vert` path
  remains.
- `GlobalUniforms.cpp` has no removed-field producer, `ShaderGlobalLayout.h`
  has a verified remaining layout, and `BufferManager.cpp` retains only the
  live `PlayersRender` resize example.
- Client Frame tuple/count compilation, Graphics pipeline/profile tables,
  CPU/GLSL GlobalLayout layout checks, shader compilation, and project/filter
  membership validation all pass; `/update-vcxproj` reports the intended
  removals.
- Client Debug and Release builds pass `/compile`; startup/render tracing or
  an existing harness scenario shows no Billboards buffer/pipeline creation or
  draw, while normal rendering and debug billboard paths remain available.
- The Billboards leaf docs/shaders are removed or no longer referenced, and no
  replacement capability is added.
- No unit tests are added.

## Coordination

`Documents/Plans/Engine/UnusedShaderLayoutDeclarations.md` also changes
`Engine/Data/Shaders/ShaderLayoutsBase.h`. Neither Plan depends on the other.
Keep the `BillboardLayout` deletion in this Plan and the two future-only
declarations in that Plan disjoint, then review the shared header once so all
live Model/HexShield layouts and bindings remain untouched.

## Notes

The leaf Billboards authority is part of the deletion boundary and should be
removed with the empty directory; `DebugRenderBillboard.vert` is a distinct
debug primitive despite the shared word “billboard.”
