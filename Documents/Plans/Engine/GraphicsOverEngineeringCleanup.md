<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-08T23:17:05.221Z","dependsOn":[]} -->
# Remove over-engineered checks and dead code in Graphics

## Context

A repo-wide over-engineering (YAGNI) sweep looked for useless hashing, excessively defensive checks, fallbacks for cases that cannot happen, ultra-rare edge-case protection, and speculative generality (parameters or functions with one value or no caller). A low-effort finder model flagged candidates file by file, and one validation pass by a stronger model confirmed or rejected each against the code, citing callers and invariants. The main session spot-checked only two of the 27 validation groups. This Plan carries 19 of the 25 confirmed candidates under `Engine/Source/Graphics/**`, with line numbers as of commit `7514890c`, and the one rejected candidate as a do-not-change entry. The `CameraBase.cpp`/`CameraBase.h` references (candidate 2, Critical files, Notes) are as of `main` `9d643e02`, after the `EngineCamera` to `CameraBase` rename. The other six, candidates 10-15 (singleton teardown guards in destructors), moved to `Documents/Plans/Engine/SingletonTeardownGuardFamily.md`; the remaining candidates keep their original numbers. The sweep's working files were session-local and are gone, so every fact the executor needs is restated below.

`.agents/references/cpp-conventions.md` (first bullet) assumes parameters from within the codebase are valid and puts validation only at trust boundaries; each candidate below is a check, fallback, parameter, or function inside our own code that the validator found guarded by an earlier check, fixed by every caller, or unused.

**Every candidate is unverified input, not an approved change.** The validator confirmed about 95% of what it saw, and only two groups were spot-checked. When this Plan was recorded, the user asked that the executing agent not trust these recommendations blindly.

## Design

### Per-candidate verification

For each candidate, in order:

1. Re-locate the code by its function and quoted condition; line numbers will have drifted.
2. Independently prove the claim against current code: cite the invariant (the asserting constructor, the single writer, the Vulkan specification guarantee) or enumerate every caller with a repository-wide search covering `Engine/`, `Projects/`, `Tools/`, and `Common/`. Do not reuse the validator's evidence without re-reading it.
3. Apply the proposed simpler form only when step 2 proves it. Where the proof rests on an invariant held elsewhere, the author recommends the `ASSERT` form over plain deletion. `ASSERT` (`Common/ErrorUtils.h:20`) is active in every build configuration, so it turns a wrong proof into an immediate hard failure.
4. When the proof fails, or the change would touch a boundary listed below, drop the candidate and report it with the reason in the completion summary. A dropped candidate is a normal outcome, not a failure of this Plan.

### Boundaries that must never be removed

- Server validation of every client-to-server record and every grid save, and the rejection of bad values from packets or files under the bad-value rule in `.agents/references/cpp-conventions.md`; owners: `Engine/Source/Network/AGENTS.md` `## Corrupt Input Policy`, `Engine/Source/File/AGENTS.md` `## Grid Saves`.
- The per-tick determinism CRC and everything feeding it. All candidates here are client render state outside the CRC.
- `.pack`, manifest, save, and replay format and version checks, and the texture cache header's magic, version, and texture-shape checks (`TextureCache.cpp:204`).
- Vulkan, VMA, Win32, and other OS or third-party API result checks (`CHECK_VK` and friends), and checks on external input such as the `pipeline.cache` file read.

Converting a check into an `ASSERT` is an acceptable outcome for any candidate.

### Candidates

**Dead code and single-value parameters.**

1. `Engine/Source/Graphics/AnimationData.cpp:221-231`, `AnimationData.h:17` — `AnimationData::FindAnimation` has no caller. Simpler form: delete both.
2. `Engine/Source/Graphics/CameraBase.cpp:344-360`, `CameraBase.h:173` — `CameraBase::ScreenToWorld` and its parallel-ray NaN fallback have no caller; the only other reference is the comment in `WorldToScreen` (`:364`). Simpler form: delete both, and reword that comment so it no longer cites `ScreenToWorld` (for example "X/Y are screen pixels and Z is projected depth.").
3. `Engine/Source/Graphics/Graphics.cpp:657, 661, 769`, `Graphics.h:106` — `Graphics::Destroy` returns a `bool` no caller reads; all three callers (`Graphics.cpp:116, 321, 433`) are bare `Destroy();`. Simpler form: `void Destroy();`, `return;` for `return false;`, and delete the trailing `return true;`.
4. `Engine/Source/Graphics/Managers/TextureCache.h:21, 30-31`, `TextureCache.cpp:185, 204, 240, 250` — the `sourceCrc` parameter, header field, and conditional compare only ever carry the default 0; the only callers (`TextureCache.cpp:132` and `:182`, `BrdfLut.cache`) omit it, so `sourceCrc != 0 && ...` is always false. Local client cache, not a save, wire, or replay format. Simpler form: drop the parameter from both declarations and definitions, the header field, the `|| (sourceCrc != 0 && ...)` term, and the `header.sourceCrc = sourceCrc;` write, and bump `TextureFileCacheHeader::kiVersion` (`TextureCache.h:11`, currently 2) to 3 so older cache files are rejected and regenerated, per the root `AGENTS.md` directive against backward compatibility.
5. `Engine/Source/Graphics/Objects/Buffer.h:80`, `Buffer.cpp:308` — `Buffer::RecordCopy`'s defaulted `vkStageFlags` only ever takes its default; callers `CommandBufferRecordGlobal.cpp:35` and `CommandBufferRecordMain.cpp:44` omit it. Simpler form: drop the parameter and use `VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT`, held in one local constant, at its uses in the definition.
6. `Engine/Source/Graphics/Objects/Texture.cpp:68`, `Texture.h:82` — the static `Texture::RecordBeginRenderPass`'s `vkSubpassContents` is always `VK_SUBPASS_CONTENTS_INLINE`: `Texture.cpp:416` omits it and `CommandBufferRecordMain.cpp:275, 323, 433` pass that value. Simpler form: drop the parameter, use `VK_SUBPASS_CONTENTS_INLINE` in the `vkCmdBeginRenderPass` call (`Texture.cpp:102`), and drop the trailing argument at the three call sites.
7. `Engine/Source/Graphics/Managers/TextureDescriptors.cpp:567-585`, `TextureDescriptors.h:105` — `WriteSingleTextureBinding`'s CRC-keyed `mTextureMap` fallback for a null `pTexture` never runs. Its only caller (`:616`) handles bindings with `iArrayIndex < 0` and empty `textures`; each comes from `RegisterTextureBinding` with `ppTextures == nullptr` (`PipelineDescriptorWriter.cpp:71` `.pTexture = &rTexture`, `:286-287` `&mTextureMap.at(...)`, `:291` guarded by `pTexture != nullptr`); the other callers (`TextureDescriptors.cpp:338, 348`, `PipelineDescriptorWriter.cpp:311`) produce array bindings; the later write at `:444` is for array elements (asserted at `:443`). Simpler form: `ASSERT(rBinding.pTexture != nullptr);`, read the image view and `miGeneration` from `rBinding.pTexture`, and drop the `crc` parameter from definition, declaration, and the call at `:616`. Keep the `vkImageView != VK_NULL_HANDLE` guard: a lazy texture can still be unloaded.

**Destructor guards on objects that always hold their resource.**

8. `Engine/Source/Graphics/Islands.cpp:84-90` — `~Islands`: `if (mIslandMeshVirtualBlock != VK_NULL_HANDLE)`. The only write is `CHECK_VK(vmaCreateVirtualBlock(...))` in the constructor (`:31`), and `CHECK_VK` throws on failure (`GraphicsUtils.h:53`, `GraphicsUtils.cpp:33-40`). The `++miMeshArenaCapacityGeneration` and the null reset write only to an object being destroyed. Simpler form: unconditional `vmaClearVirtualBlock` and `vmaDestroyVirtualBlock`, dropping the increment and reset.
9. `Islands.cpp:96-102` — `~Islands` loop: `if (mIslandsIndirectVkBuffers.at(i) != VK_NULL_HANDLE)`. The constructor loop (`:47-79`) fills every index through `Buffer::CreateBuffer`, which wraps `vmaCreateBuffer` in `CHECK_VK`. Simpler form: the loop body is only `vmaDestroyBuffer(...)`.

Candidates 10-15 (singleton global guards in destructors) moved to `Documents/Plans/Engine/SingletonTeardownGuardFamily.md`.

**Other redundant checks.**

16. `Graphics.cpp:600-652` — `Graphics::RecreateResources`: every `if (gpTextureManager != nullptr)` / `if (gpBufferManager != nullptr)` wrapper. The sole caller is `Graphics::Create` (`:393`) inside the `else` of `if (mpTextureManager == nullptr)` (`:381`); the globals are set in their constructors and cleared only in destructors; `mpBufferManager` is non-null by `:368-371`; the same `else` dereferences `gpTextureManager` unchecked at `:398`. Simpler form: remove the wrappers, keeping the bodies. The `!= nullptr` checks in `Destroy` (`:714`, `:718`) and in the `PollSetting` calls (`:541-567`) are not candidates.
17. `Managers/DeviceManager.cpp:411-415` — `DeviceManager::LoadPipelineCache`: `if (!fileStream)` after `seekg(0, beg)`. A failed `seekg` sets failbit, so the following `read` extracts nothing and the existing `gcount() == 0` check (`:420-423`) logs the same and leaves the create info empty. The external-input check stays covered. Simpler form: keep the `seekg`, delete the wrapper, unindent its body.
18. `Managers/InstanceManager.cpp:690-704` — `InstanceManager::SelectDepthFormat`: the linear-tiling fallback pass. The Vulkan specification's mandatory format support requires `VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT` in `optimalTilingFeatures` for `VK_FORMAT_D16_UNORM`, which is in the candidate list (`:675`); a linear-only result would also be wrong for the optimal-tiling depth image the swapchain creates (`SwapchainManager.cpp:54`, `:350`). Simpler form: delete the fallback; keep the throw at `:706-709` or make it `ASSERT(mDepthVkFormat != VK_FORMAT_UNDEFINED);`. The executor re-checks the mandatory-support claim with `/verify-external-claims`.
19. `Managers/ParticleManager.cpp:64` — `ParticleManager::RenderGlobal`: `std::max(..., shaders::kfEpsilon)` on the stretch range; both operands are local constants 1.0f and 10.0f (`:58-59`). Simpler form: `1.0f / (fStretchVelocityEnd - fStretchVelocityStart)`.
20. `Managers/TextureManager.cpp:655-658` — `TextureManager::WaitForTextures`: early `continue` on `>= kReady` duplicates the `while (... < kReady)` right after it, and nothing in the `for` body follows the `while`. Simpler form: delete the `if`.
21. `Managers/TextureUploadManager.cpp:377-385` — `TextureUploadManager::HandleUploadEarlyOut`: null `mTransferVkCommandPool` early-out that already has `DEBUG_BREAK()`. The thread starts only from `StartThread` (`:143`), called only from `TextureManager::InitializeBootTextures` (`TextureManager.cpp:236`), which runs after `Graphics.cpp:348-350` creates the device and calls `InitializeTransferResources`; the pool is cleared only in `DestroyTransferResources`, which joins the thread first (`:80-86`). Simpler form: `ASSERT(mTransferVkCommandPool != VK_NULL_HANDLE);`.
22. `TextureUploadManager.cpp:332-338` — `TextureUploadManager::UploadThread` device-lost catch: `if (mCurrentCrc != 0)`. A device-lost `std::system_error` comes only from `CHECK_VK` calls at `:265, :287, :295` or inside `SubmitChunkUpload`, all after `DequeueNextUpload` set a nonzero `mCurrentCrc` (`:368`) and before `ResetUploadProgress` (`:603`); `HandleUploadEarlyOut` zeroes the CRC and `continue`s without throwing. Simpler form: `ASSERT(mCurrentCrc != 0);` and the four body lines unconditionally.
23. `Engine/Source/Graphics/Objects/Buffer.cpp:239` — `Buffer::Create`: the `mpMappedMemory != nullptr` half of the condition. Both branches (`:229`, `:233`) require `VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT`, which sets `VMA_ALLOCATION_CREATE_MAPPED_BIT` (`:29-42`); `CHECK_VK` at `:46` throws on failure, and with that bit VMA fills `pMappedData` on success. Simpler form: `if (rDataFunction != nullptr)`, optionally with `ASSERT(mpMappedMemory != nullptr);`.
24. `Engine/Source/Graphics/Render/GlobalUniforms.cpp:387` — `PopulateShadowParameters`: `std::max(gShadowBlurSigma.mfCurrent, 1.0e-6f)`. `gShadowBlurSigma` is `Wrapper(1.0f, 0.1f, 5.0f)` (`ShadowWrappersBase.cpp:16`); its only writer is the tweak slider (`TweaksScreenBase.cpp:240-242`) through the clamping `Set(float)` (`WrapperBase.h:141-144`), which the agent `kSetSlider` command also drives; it is not loaded from settings (`ClientSettings.cpp:43-60`). Simpler form: `float fShadowBlurSigma = gShadowBlurSigma.mfCurrent;`.
25. `Engine/Source/Graphics/Render/LightingUniforms.cpp:95` — `PopulateLightingParameters`: `|| !sTemporalAreaLatch.bInitialized` is implied by `|| !sbHeldVisibleArea`. `sTemporalAreaLatch` is written only through `Update` (`:99`), which always leaves `bInitialized == true` (its reset at `:35` is followed by `:43` in the same call); before the first refresh `sbHeldVisibleArea` is also false (`:79`). Simpler form: delete that term.

### Rejected candidate — do not change

- `Engine/Source/Graphics/AnimationData.cpp:172-175` — replacing `mAnimatedNodes.at(...)` with `operator[]` because the indices are validated. Rejected: `Documents/C++StyleGuide.txt` rule 16 requires `.at()` for `std::vector` access; it is mandated style, not a defensive check.

## Critical files

- `Engine/Source/Graphics/AnimationData.cpp`, `AnimationData.h`
- `Engine/Source/Graphics/CameraBase.cpp`, `CameraBase.h`
- `Engine/Source/Graphics/Graphics.cpp`, `Graphics.h`
- `Engine/Source/Graphics/Islands.cpp`
- `Engine/Source/Graphics/Managers/DeviceManager.cpp`, `InstanceManager.cpp`, `ParticleManager.cpp`, `TextureCache.cpp`, `TextureCache.h`, `TextureDescriptors.cpp`, `TextureDescriptors.h`, `TextureManager.cpp`, `TextureUploadManager.cpp`
- `Engine/Source/Graphics/Objects/Buffer.cpp`, `Buffer.h`, `Texture.cpp`, `Texture.h`
- `Engine/Source/Graphics/Render/GlobalUniforms.cpp`, `LightingUniforms.cpp`
- Call sites only: `Engine/Source/Graphics/Managers/CommandBufferRecordMain.cpp` (candidate 6), plus any further call site the executor's search finds

## In scope

Only the exact checks, branches, parameters, functions, and statements named in candidates 1-9 and 16-25, their declarations, the call-site argument drops those candidates name, and the `TextureFileCacheHeader::kiVersion` bump of candidate 4 — and only the candidates the executor proves. Comment edits that a removed check or function makes false are in scope.

## Out of scope

- The rejected `AnimationData.cpp:172-175` `.at()` access above.
- Every boundary listed under `### Boundaries that must never be removed`.
- Every `if (gpX == this)` singleton teardown guard, including the six former candidates 10-15 and the `Graphics.cpp` ones: `Documents/Plans/Engine/SingletonTeardownGuardFamily.md` handles the whole family once.
- The `!= nullptr` checks in `Graphics::Destroy` and in the `PollSetting` calls of `Graphics.cpp`.
- Any other check, branch, or file not listed as a candidate; report similar-looking sites as residuals instead.
- Shaders and GPU layouts: no `shaders::GlobalLayout` member changes.

## Acceptance criteria

- The completion summary lists every candidate 1-9 and 16-25 as applied, with the executor's own proof (invariant, specification guarantee, or every caller cited against current code), or dropped, with the reason.
- No boundary from `### Boundaries that must never be removed` is weakened, and the rejected candidate is unchanged.
- The client builds clean in Debug and Release through `/compile`, and the server build is unaffected.
- A live `/agent-harness` client run boots, regenerates `BrdfLut.cache` under the new cache version on first launch and reuses it on the next, renders islands, lighting, shadows, and particles with a screenshot, and shuts down with no `ASSERT` and no Vulkan validation error.

## Notes

- Risk tier: Tier 3 (`.agents/references/risk-tiers.md`). Triggers: serialization — candidate 4 changes the `TextureFileCacheHeader` layout and bumps its `kiVersion`; threading — candidates 21-22 change the texture upload thread's early-out and device-lost handling. All candidates are client render state outside the determinism CRC.
- `Documents/Plans/Engine/RemoveEngineRuntimeOverEngineering.md` (candidate 18) also edits `CameraBase.cpp`: only the `SetVibration` call (~`:195`), a region disjoint from candidate 2, so there is no required ordering.
- `Documents/Plans/Engine/ShadowUpdateCadence.md` moves `LightingTemporalAreaLatch` out of `LightingUniforms.cpp` into `Render.h`, and `Documents/Plans/Engine/CadencePhaseBalancing.md` edits `RenderLightingGlobal` and `GlobalUniforms.cpp`. If either has landed first, re-prove candidates 24-25 against the moved latch type and the new cadence code; there is no required ordering.
