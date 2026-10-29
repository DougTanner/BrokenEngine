# Slice 2 handoff detail (Lighting core and progressive capture)

## Changed files

- `Engine/Source/Graphics/Lighting/Lighting.h` — deleted `DrainLightingCaptureCommands`,
  `FrameInterpolateBase` forward declaration, `LightingAllocation`, `uiValidationRequest`;
  `LightingDiagnostics` reduced to `iSources, iInstances, iTraceOnlyInstances, iTriangles, status
  ("not-run"), iCompletedTiles, iTotalTiles, error, directory`; public members per plan `## Public
  interface after the strip` (`ApplyComparisonSettings(bool bScenePaused)`; `AddFrame`,
  `CapturePreparation`, `IsCaptureHeld`, `CancelCapture` gone); private scene-diagnostic,
  hold/release, `CompletePreparation`, manifest, `GetBufferAllocation` declarations gone; new
  private `RecordCapture(VkCommandBuffer)`.
- `Engine/Source/Graphics/Lighting/LightingState.h` — dropped `LightingSceneDiagnostics.h` include;
  `LightingFramebuffer` loses diagnostic buffers/readback/operation
  counters/`uiHeldPublication`/`frameIdentities`, gains `uiPendingPublication`; `State::Comparison`
  keeps only GPU resources plus `eReferenceLayout` and tile constants (`kiTileColumns`,
  `kiTileCount`, `kiTilesPerFrame`=120); `NativeCapture` deleted; `State` holds `diagnostics`
  (latched counts + capture status), `uiServicedCapture`, `captureStart`, `capturePublication`.
- `Engine/Source/Graphics/Lighting/Lighting.cpp` — `GetBufferAllocation` and byte accounting
  deleted; `CreateResources`/`DestroyResources`/`CreateTerrainPipelines`/`DestroyTerrainPipelines`
  drop scene diagnostics and the `completedDiagnostics` reset; `BeginRender` = framebuffer select +
  completed-after-fence latch + range/publication clear; `CompletePreparation`, `AddFrame`,
  `HoldPublication`, `ReleasePublication` deleted; `EndRender` uses `terrain.Prepare(iFramebuffer)`
  and stores serial + four pending counts (incl. `uiTraceOnlyInstances`); `RecordPreparation` calls
  the 3-argument `models->RecordPreparation`, wraps `terrain.RecordPreparation` in
  `kGpuTimerLightingTerrain`, drops operation counters and `RecordSceneDiagnostics`, calls
  `RecordCapture` after `RecordEnvironment`; TEMP block in `BindQueryDescriptors` removed;
  `SetComparison` no longer cancels.
- `Engine/Source/Graphics/Lighting/LightingComparison.cpp` — resume stash removed;
  `IsCaptureRunning` file-local helper; `DestroyComparisonResources` fails a running capture
  ("Graphics recreated during capture") and no longer owns fence/query pool/command pool;
  `ApplyComparisonSettings(bool)` fails on unpause/settings change and starts a capture (reference
  mode + paused + 1920x1080, else `failed` with reason); `PrepareComparisonPipelines` keeps the
  per-frame fixture-draw part, compares publications in tracing frames and binds the reference
  compute set, and in the receivers frame records the publication, creates capture textures/render
  pass/framebuffer/receiver pipelines/reference compute once, and adds missing receiver model
  pipelines (map no longer cleared); `RecordReceivers` loses operation counting.
- `Engine/Source/Graphics/Lighting/LightingCapture.cpp` — rewritten: `LightingWriteFile`,
  `RecordCapture` (receivers stage: reference to GENERAL compute layout + receiver pass; tracing:
  120 tile dispatches; last batch: compute-to-shader-read transition,
  `SetLightingReference(iFramebuffer, &reference, true)`, status `composing`), `FinishCapture`
  (fence wait, finished.png, five `.rgba32f`, `capture.json`, descriptor reset), `WriteProfile`
  (`sources, instances, traceOnlyInstances, triangles, capture{status, completedTiles, totalTiles,
  error, directory}`). All pump/hash/manifest/TEMP code gone.
- `Engine/Source/Graphics/Lighting/LightingFixture.h` / `.cpp` — blob enum, blob identity, `kuiSchema`, `GetBlob`/`GetIdentity`/`GetInputHash`/`FreezeBlob`, `mBlobs`/`mIdentities`, Append*/HashBytes helpers and blob construction deleted; `InitializeAnimation` heap comment reworded.
- `Engine/Data/Shaders/Lighting/LightingCommon.h` — `kfLightingRayOffset` 0.001f -> 0.05f.
- `Engine/Source/Profile/ProfileManager.h` — `kGpuTimerLightingTerrain` after `kGpuTimerWaterDisplacement`; `"    Lighting Terrain"` at the same index.
- Deleted: `Engine/Source/Graphics/Lighting/LightingSceneDiagnostics.cpp`, `Engine/Data/Shaders/Lighting/LightingSceneDiagnostics.comp`, `Engine/Data/Shaders/Lighting/LightingSceneDiagnostics.h`.

## Self-audit

- Claim: composing-frame `SetLightingReference` writes Set 0 of the current framebuffer after its Global submit -> Check: `TextureDescriptors.cpp:60-61` binding flags (UPDATE_AFTER_BIND) and shader users of the lighting bindings (`Terrain.frag`, `Water.frag`, `ModelForward.h`, `LightingFixture.frag`, all Main-pass) -> Result: Main for that framebuffer is not yet submitted; Global shaders do not use the bindings; kept, flagged for reviewer.
- Claim: Main could compose a partially traced reference -> Check: all preparation submits go to `mGraphicsVkQueue`; the composing barrier (src COMPUTE/SHADER_WRITE) covers earlier submissions -> Result: holds.
- Claim: receiver images not visible to later-frame compute -> Check: render pass EXTERNAL dependency dst COMPUTE_SHADER/SHADER_READ, final layout SHADER_READ_ONLY -> Result: holds.
- Claim: reference layout tracking wrong after readback -> Check: `TextureCache.cpp` CopyImageToHostMemory restores `vkCurrentLayout` -> Result: stays SHADER_READ_ONLY; next receivers stage transitions from tracked layout.
- Claim: descriptor reset skipped when export throws before the fence wait -> Fix: explicit fence wait before the try block in `FinishCapture` -> Recheck: reset runs after the wait on every non-device-lost path.
- Claim: device loss in `FinishCapture` leaves status `composing` -> Check: `IsCaptureRunning` includes `composing`; `DestroyResources` -> `DestroyComparisonResources` -> Result: marked failed on recreation.
- Claim: comment in tracing compare described the serial misleadingly -> Fix: reworded -> Recheck: read.

## Affected-site triggers

- signature: `Lighting::ApplyComparisonSettings(bool)` — callers in `Engine/Source`, `Projects/*/Source` (GameBase.cpp already passes the pause flag).
- removed API: `AddFrame`, `CapturePreparation`, `IsCaptureHeld`, `CancelCapture`, `DrainLightingCaptureCommands`, `LightingAllocation`, `LightingDiagnostics` old fields, `uiValidationRequest` — `Engine/Source`, `Projects/*/Source`, `Tools`.
- layout/enum: `GpuTimers` insertion — every `kGpuTimer*` index consumer and `kpGpuTimerNames` (`Engine/Source/Profile`, `Engine/Source/Graphics`).
- removed shader: `LightingSceneDiagnostics.{comp,h}` — vcxproj/filters (slice 5), DataPacker shader list, docs (slice 6).
- `query_profile.lighting` shape — `.agents/skills/agent-harness/references/command-reference.md` (slice 6).

## Reviewer focus

- Verify the composing-frame Set 0 write is legal when the same framebuffer's Global command buffer is still pending (UPDATE_AFTER_BIND without UNUSED_WHILE_PENDING).
- Verify `RecordCapture` after `RecordEnvironment` (receivers and tiles at the end of preparation) keeps the preparation-before-Main chain and water-foam-consistent receivers.
- Verify a failed capture leaves gameplay local diffuse zero (reference only enabled in the composing frame; reset in `FinishCapture` and `DestroyComparisonResources`).
- Verify no per-frame pipeline recreation: compute/receiver pipelines in the `vkRenderPass == VK_NULL_HANDLE` block; model receivers only when missing.

## Residual detail

- `iCompletedTiles` counts tiles recorded into submitted preparation, not GPU-completed tiles.
- A capture request outside `kFixtureReference` mode now ends `failed` ("Lighting capture requires fixture reference mode"); the old zero-reference export for fixture-zero mode is gone (plan sentence ambiguous; failing chosen).
- `LightingFixtureAnimation::path`, `clip`, `iMaterialCount`, `iJointsPerMaterial` are now write-only (their only reader was the blob); plan row keeps `LightingFixture.cpp:233-269`, so they stay.
- Receiver model pipeline map persists until comparison teardown (no per-capture clear) so an earlier capture's pending receiver pass never loses its pipeline.
