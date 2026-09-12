<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-12T21:31:18.284Z","dependsOn":["Documents/Plans/Engine/GlobalWaterUniformPublicationOrder.md"]} -->
# Fix: Stop reading elapsed time back out of the mapped Global layout for the Gerstner wave phase

## Context

`Engine/Source/Graphics/Render/AGENTS.md:15` states: "Write-combined mapped
layouts are write-only. Compute dependent values in CPU staging state and copy
each populated region once."

The Gerstner wave path violates that rule in one place.
`PopulateGerstnerWaves` (`Engine/Source/Graphics/Render/MainUniforms.cpp:412`)
derives the whole Gerstner wave phase from a read-back of the mapped
`shaders::GlobalLayout`:

`double dWaveTime = static_cast<double>(rGlobalLayout.fElapsedTime);`
(`MainUniforms.cpp:421`)

`rGlobalLayout` is the write-combined mapped uniform memory the Global phase
obtained from
`gpBufferManager->mGlobalLayoutUniformBuffers.at(iCommandBuffer).mpMappedMemory`,
and `fElapsedTime` was written that same frame at
`Engine/Source/Graphics/Render/GlobalUniforms.cpp:498` from `RenderFrameGlobal`'s
`fCurrentTime` parameter. The read-back forces the comment at
`MainUniforms.cpp:409` ("Wave phase reduction: read elapsed time from
already-populated global layout") to record a cross-phase dependency that exists
for no other reason, and `dWaveTime` flows on into
`PopulateGerstnerLowWaves` and `PopulateGerstnerMediumWaves`
(`MainUniforms.cpp:440,448`), so the entire low and medium wave phase depends on
it.

Impact: uncached write-combined reads are slow on the client main loop thread,
and the wave phase silently depends on `fElapsedTime` having been published
first — the coupling the directory rule exists to prevent, with nothing in the
signature expressing it.

This is pre-existing code. The change that removed the three equivalent
read-backs in `WaterUniforms.cpp`
(`Documents/Plans/Engine/WaterUniformMappedLayoutReadback.md`) named this site in
its `## Out of scope` because, in the tree it was written against, the Main
phase had no elapsed-time parameter on the call path.

That is no longer the shape this Plan runs against. This Plan's `dependsOn`
prerequisite, `Documents/Plans/Engine/GlobalWaterUniformPublicationOrder.md`,
moves the Gerstner work out of the Main phase: its `## Design` (lines 31-38)
creates "one water-publication routine that runs from `RenderFrameGlobal` before
`SubmitGlobalCommandBuffer`" and moves the "`PopulateGerstnerWaves` count/array
writes into that routine", its `## In scope` (lines 53-61) covers "Moving the
current active-LOD dimensions, displacement indirect dispatch, and wave
count/array generation into the pre-submit Global phase without changing their
formulas", and its critical files name the `GlobalUniforms.cpp:480-511`
insertion point (line 42) and the `MainUniforms.cpp:317-436,501-535,591-626`
regions to move (line 44). The scheduler always runs that prerequisite first, so
by the time this Plan executes the Gerstner routine is called from
`RenderFrameGlobal` (`GlobalUniforms.cpp:487`), whose `float fCurrentTime`
parameter is already the exact value written to `fElapsedTime` at
`GlobalUniforms.cpp:498`. The remaining defect is only the read-back itself.

## Design

Author's recommendation: pass `RenderFrameGlobal`'s existing `fCurrentTime` into
the Gerstner routine the prerequisite moved into the Global phase, and delete
the read-back. No signature on the Main call chain changes.

- Add a `float fCurrentTime` parameter to `PopulateGerstnerWaves` — the routine
  the prerequisite's `## Design` moves into the pre-submit Global phase, and
  which that Plan does not rename; if the prerequisite as landed calls the
  moved wave work by another name, this parameter goes on that routine instead.
- Pass `RenderFrameGlobal`'s `fCurrentTime` parameter
  (`GlobalUniforms.cpp:487`) to it from the single post-move call site, the way
  `PopulateWaterParameters` is already called at `GlobalUniforms.cpp:517`.
- Replace `double dWaveTime = static_cast<double>(rGlobalLayout.fElapsedTime);`
  (`MainUniforms.cpp:421`) with the same cast of that parameter, leaving the
  rest of the routine's float operations and their order untouched.
- Rewrite the `MainUniforms.cpp:409` comment line so it no longer claims the
  elapsed time comes from the populated global layout, keeping the non-const
  rationale the rest of that comment block carries.

Because the passed value is the identical `float` the Global phase writes to
`fElapsedTime` that frame (`GlobalUniforms.cpp:498`), `dWaveTime` is
bit-identical and the rendered wave phase is unchanged. This mirrors the
established pattern: `PopulateWaterParameters` gained a `float fCurrentTime`
parameter fed from `RenderFrameGlobal` for exactly this reason, and
`PopulateWaterReducedUv` derives its per-frame delta from that parameter instead
of from `rGlobalLayout.fElapsedTime`
(`Engine/Source/Graphics/Render/WaterUniforms.cpp`, `Render.h:163`).

One alternative the author rejected: publishing the elapsed time into an
`inline` global in `Render.h` from `RenderFrameGlobal` and reading it back in
`MainUniforms.cpp`. That moves the read out of write-combined memory but keeps
an implicit ordering dependency unexpressed in any signature; a per-frame input
the caller already holds belongs in the call, not in cross-file state.

## Critical files

- The Gerstner wave routine, `PopulateGerstnerWaves` — its
  `rGlobalLayout.fElapsedTime` read, the comment recording it, its signature,
  and the consumers of `dWaveTime` — in whichever file the prerequisite leaves
  it, `MainUniforms.cpp` or `GlobalUniforms.cpp`. Pre-move location:
  `Engine/Source/Graphics/Render/MainUniforms.cpp:409-421,440-448`.
- `Engine/Source/Graphics/Render/GlobalUniforms.cpp:487-517` —
  `RenderFrameGlobal`, its `fCurrentTime` parameter, the single mapped write of
  `fElapsedTime` that the passed value must match, and the
  `PopulateWaterParameters` call that shows the pattern.
- `Documents/Plans/Engine/GlobalWaterUniformPublicationOrder.md:31-38,53-61` —
  the prerequisite that puts the Gerstner routine in the Global phase, whose
  landed shape fixes the routine's name and its call site.
- `Engine/Source/Graphics/Render/AGENTS.md:15` — the write-only rule this
  restores.

## In scope

- Adding a `float fCurrentTime` parameter to `PopulateGerstnerWaves` (the
  routine the prerequisite moved into the Global phase), including its
  declaration in `Render.h` if the prerequisite left it cross-translation-unit.
- Passing `RenderFrameGlobal`'s `fCurrentTime` parameter to it at its single
  call site in the pre-submit Global phase.
- Replacing that routine's `rGlobalLayout.fElapsedTime` read with that
  parameter as its time source, in whichever file the prerequisite leaves the
  routine — `MainUniforms.cpp` or `GlobalUniforms.cpp` (pre-move:
  `MainUniforms.cpp:421`).
- Correcting the comment above that read to match the removed dependency
  (pre-move: `MainUniforms.cpp:409`).

## Out of scope

- Any signature change to `RenderFrameMain` (`Render.h:140`) or
  `Graphics::RenderMainPresentAcquire`, and any change to their callers.
- The remaining `rGlobalLayout` writes in the Gerstner routine (pre-move:
  `MainUniforms.cpp:416-417,444`), wherever the prerequisite leaves it, and the
  rest of `MainUniforms.cpp`, which are writes and already conform.
- Any change to the wave math, amplitude fades, LOD or quality behavior, water
  shaders, tunables, or the `gPresentationContinuity` snapshot contents.
- Moving water population between the Global and Main phases, Global/Main
  command-buffer submission ordering, and buffer allocation or mapping policy —
  all owned by the prerequisite.
- The rest of the Render directory's mapped-layout usage.
- Simulation CRC, wire, save, replay, and `.pack` bytes; unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 2. Trigger: it changes one render routine's
signature and the source of the per-frame Gerstner wave phase — scoped runtime
behavior of one client subsystem, outside the CRC, with no determinism, wire,
serialization, threading, or trust-boundary exposure. Caller and callee sit in
the same Global phase after the prerequisite lands, so no subsystem outside
`Graphics/Render/` is touched.

Preserve these invariants:

- No read of the mapped `shaders::GlobalLayout` remains in the Gerstner routine
  or its band helpers; their `rGlobalLayout` accesses are writes only.
- The elapsed time used for the wave phase is the same value
  `RenderFrameGlobal` publishes to `fElapsedTime` that frame — the sub-tick
  remainder is included, not the tick-quantized interpolate time.
- `RenderFrameMain` and `Graphics::RenderMainPresentAcquire` keep their
  signatures.
- Rendering stays outside the CRC; no simulation, wire, save, replay, or `.pack`
  bytes change.

## Acceptance criteria

A diff settles this change. No harness query exposes the Gerstner wave phase, so
rendered pixels are not evidence for it, and none of these criteria is an
agent-harness scenario in the sense the compile skill's `## Inputs` defines.

- A scoped search of the Gerstner routine and its `PopulateGerstnerLowWaves` /
  `PopulateGerstnerMediumWaves` helpers shows no read of any `rGlobalLayout`
  member; every remaining `rGlobalLayout` access there is an assignment.
- Diff inspection of that routine shows the same float operations in the same
  order as before, with the new `fCurrentTime` parameter as the sole time
  source: the only changed expression is `dWaveTime`'s initializer, and the
  low-quality bypass, the amplitude-fade computations, and the count zeroing are
  byte-for-byte unchanged.
- The diff contains no change to the `RenderFrameMain` or
  `Graphics::RenderMainPresentAcquire` declarations, definitions, or call sites.
- Client Debug and Release builds pass `/compile`.

## Notes

This Plan's `dependsOn` edge to
`Documents/Plans/Engine/GlobalWaterUniformPublicationOrder.md` is directional
for a reason: doing this fix first would have to thread a `fCurrentTime`
parameter through `Graphics::RenderMainPresentAcquire` and `RenderFrameMain`
that the prerequisite then removes when it moves the Gerstner work into
`RenderFrameGlobal`. Sequenced after it, the fix is one parameter on one
routine.

`rFrameInterpolate.fCurrentTime`, available inside `RenderFrameMain`, remains
the wrong source and is rejected whichever phase the routine ends up in:
`GameBase::Render` publishes `rFrame.interpolate.fCurrentTime = mfCurrentTime`
(`Engine/Source/GameBase.cpp:929`), the tick-quantized time, while the value
`RenderFrameGlobal` receives adds the sub-tick remainder
(`GameBase.cpp:870`, passed at `GameBase.cpp:884`). Substituting it would
quantize the wave phase to the sim tick and change how the water looks, which
this Plan must not do.

`Documents/Plans/Engine/HostVisibleBufferMappingContract.md` touches the same
mapped Global layout but owns the VMA mapping guarantee, not reading the mapped
layout back; no edge is required in either direction.
