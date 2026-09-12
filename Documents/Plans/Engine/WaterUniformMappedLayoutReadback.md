<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-12T16:16:31.053Z","dependsOn":[]} -->
# Fix: Stop reading water uniform values back out of the mapped Global layout

## Context

`Engine/Source/Graphics/Render/AGENTS.md:15` states: "Write-combined mapped
layouts are write-only. Compute dependent values in CPU staging state and copy
each populated region once."

`WaterUniforms.cpp` violates that rule in three places, all inside the mapped
`shaders::GlobalLayout` that `RenderFrameGlobal` obtains from
`gpBufferManager->mGlobalLayoutUniformBuffers.at(iCommandBuffer).mpMappedMemory`
(`Engine/Source/Graphics/Render/GlobalUniforms.cpp:497`):

- `Engine/Source/Graphics/Render/WaterUniforms.cpp:90-91` — the reduced-time
  accumulator derives its per-frame delta by reading `rGlobalLayout.fElapsedTime`
  back out of the mapped layout
  (`float fDeltaTime = std::max(0.0f, rGlobalLayout.fElapsedTime - sfPrevElapsedTime);`
  and the following latch store). The value read is the one
  `GlobalUniforms.cpp:499` wrote from its own `fCurrentTime` parameter that same
  frame (`RenderFrameGlobal(int64_t iCommandBuffer, float fCurrentTime)`,
  `GlobalUniforms.cpp:488`; declared `Render.h:143`), so a CPU-side source for
  the same number already exists on the call path. That read-back is also why
  `WaterUniforms.cpp:176-178` has to document a cross-translation-unit ordering
  contract ("after publishing `rGlobalLayout.fElapsedTime`") that has no other
  reason to exist.
- `Engine/Source/Graphics/Render/WaterUniforms.cpp:27` — `PopulateWaterSunsetFade`
  writes `rGlobalLayout.fWaterDepthLutSunsetFade` in its branches, then reads it
  back to apply the intensity/power shaping.
- `Engine/Source/Graphics/Render/WaterUniforms.cpp:45` — `PopulateWaterDirectional`
  does the same with `rGlobalLayout.fWaterDirectional`.

Impact: uncached write-combined reads are slow on the client main loop thread,
and — more importantly for maintenance — the `fElapsedTime` read-back makes one
render translation unit's per-frame water scroll depend on another's
publication order, which is exactly the coupling the directory rule exists to
prevent.

This is pre-existing code. The session that recorded this residual added a
read-only `presentation_continuity_probe` agent command
(`Documents/Plans/Engine/PresentationContinuityQuery.md`) and introduced no
mapped-layout read-back of its own; the residual was routed here because that
change was Tier 3 and this code sat outside its implementation boundary.

## Design

Author's recommendation: give each computation a CPU-side value and write the
mapped field exactly once.

- Thread the already-available elapsed time down instead of reading it back.
  Add a `float fCurrentTime` parameter to `PopulateWaterParameters`
  (`Render.h:167`, defined `WaterUniforms.cpp:179`) and pass it from the single
  call site (`GlobalUniforms.cpp:518`), which already holds `RenderFrameGlobal`'s
  `fCurrentTime` parameter. Pass it on to `PopulateWaterReducedUv` and use it
  for both the `fDeltaTime` computation and the `sfPrevElapsedTime` latch store.
  Then delete the now-obsolete "after publishing `rGlobalLayout.fElapsedTime`"
  clause from the ordering comment at `WaterUniforms.cpp:176-178`, keeping the
  once-per-frame latch requirement that comment also carries.
- In `PopulateWaterSunsetFade` and `PopulateWaterDirectional`, compute the
  piecewise value into a local `float`, apply the shaping to that local, and
  assign the mapped field once at the end of each function.

The arithmetic itself is unchanged in every case: the same float values in the
same order, so the rendered result is expected to be identical.

An alternative the author rejected — caching `fElapsedTime` in a file-static in
`WaterUniforms.cpp` — would add a second latch that looks externally resettable,
which `Engine/Source/Graphics/Render/AGENTS.md:17` warns against.

## Critical files

- `Engine/Source/Graphics/Render/WaterUniforms.cpp:12-45,48-91,176-179` — the
  three read-back sites, the reduced-time accumulator, and the ordering comment.
- `Engine/Source/Graphics/Render/Render.h:167` — the `PopulateWaterParameters`
  declaration whose signature gains the elapsed-time parameter.
- `Engine/Source/Graphics/Render/GlobalUniforms.cpp:488-499,518` — the
  `RenderFrameGlobal` parameter that already carries the CPU-side elapsed time,
  the single mapped write of `fElapsedTime`, and the call site to update.
- `Engine/Source/Graphics/Render/AGENTS.md:15` — the write-only rule this
  restores.

## In scope

- Replacing the `rGlobalLayout.fElapsedTime` reads at `WaterUniforms.cpp:90-91`
  with the CPU-side `fCurrentTime` value threaded through
  `PopulateWaterParameters` and `PopulateWaterReducedUv`.
- Updating the `PopulateWaterParameters` declaration in `Render.h` and its
  single call site in `GlobalUniforms.cpp`.
- Converting the read-modify-write of `fWaterDepthLutSunsetFade`
  (`WaterUniforms.cpp:13-27`) and `fWaterDirectional`
  (`WaterUniforms.cpp:31-45`) to a local float with one mapped store each.
- Correcting the `WaterUniforms.cpp:176-178` ordering comment to match the
  removed dependency.

## Out of scope

- `Engine/Source/Graphics/Render/MainUniforms.cpp:421`, which reads
  `rGlobalLayout.fElapsedTime` back for the Gerstner wave phase. That site sits
  in the Main phase, which has no elapsed-time parameter on the call path
  (`RenderFrameMain`, `Render.h:144`), so removing its read-back is a separate
  change with its own boundary.
- Any change to the values computed, the water shaders, tunables, LOD or quality
  behavior, or the `gPresentationContinuity` snapshot contents.
- Global/Main command-buffer submission ordering, buffer allocation or mapping
  policy, and the rest of the Render directory's mapped-layout usage.
- Simulation CRC, wire, save, replay, and `.pack` bytes; unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 2. Trigger: it changes a cross-translation-unit
render function signature (`PopulateWaterParameters` in `Render.h`) and the
source of the per-frame water phase delta, which is scoped runtime behavior of
one client subsystem with no determinism/CRC, wire, serialization, threading, or
trust-boundary exposure.

Preserve these invariants:

- Every mapped `GlobalLayout` field this change touches is written and never
  read; no read of write-combined mapped memory remains in `WaterUniforms.cpp`.
- `PopulateWaterReducedUv` still runs exactly once per frame; the reduced-time
  accumulators integrate once per frame and the previous-elapsed latch stays
  function-local.
- The elapsed time used for the water delta is the same value
  `RenderFrameGlobal` publishes to `fElapsedTime` that frame.
- Rendering stays outside the CRC; no simulation, wire, save, replay, or `.pack`
  bytes change.

## Acceptance criteria

- A scoped search of `Engine/Source/Graphics/Render/WaterUniforms.cpp` shows no
  read of any `rGlobalLayout` member.
- Water scroll speed and appearance are unchanged for a fixed camera and fixed
  tunables: a harness screenshot comparison across a short fixed run shows no
  water phase drift or speed change versus the pre-change build.
- The `gPresentationContinuity` snapshot's published water origins and reduced
  normal origins are unchanged for the same fixed run.
- Client Debug and Release builds pass `/compile`.

## Notes

`Documents/Plans/Engine/GlobalWaterUniformPublicationOrder.md` and
`Documents/Plans/Engine/HostVisibleBufferMappingContract.md` both touch the same
mapped Global layout but own different root causes — host publication ordering
and the VMA mapping guarantee respectively — and neither addresses reading the
mapped layout back. No dependency edge is required in either direction: this
change does not depend on their outcome, and applying it first does not block
them.
