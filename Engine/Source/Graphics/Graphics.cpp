#if defined(BT_CLIENT)

#include "Graphics.h"

#include "File/PackChunks.h"
#include "Ui/GraphicsSettingsWrappersBase.h"
#include "Ui/LightingWrappersBase.h"
#include "Ui/PbrWrappersBase.h"
#include "Ui/ShadowWrappersBase.h"
#include "Ui/WaterWrappersBase.h"
#include "Ui/WrapperBase.h"

#include "Profile/ProfileManager.h"
#include "Game.h"

namespace engine
{

// Shadow-execution-aligned snap block size. FullDetail and WaterFullDetail both snap their render extents
// to this multiple; single-sourced here so the two snap loops cannot drift apart.
constexpr int64_t kiDetailBlockSize = 8 * static_cast<int64_t>(shaders::kiShadowTextureExecutionSize);

// Reference render width. WaterFullDetail anchors the resolution-independent Gerstner water grid to it, and
// SmokeSimulationPixels scales the smoke-sim resolution relative to it.
constexpr int64_t kiReferenceWidth = 3'840;
// Smoke-sim pixel budget at the reference width (smoke resolution scales by actual/reference width).
constexpr float kfSmokeReferencePixels = 8'192.0f;

static constexpr int64_t SnapToDetailBlock(int64_t iExtent)
{
	int64_t iSnappedExtent = kiDetailBlockSize;
	while ((10 * iSnappedExtent) / 9 < iExtent)
	{
		iSnappedExtent += kiDetailBlockSize;
	}
	return iSnappedExtent;
}

std::tuple<int64_t, int64_t> FullDetail()
{
	int64_t iX = SnapToDetailBlock(gpGraphics->mFramebufferVkExtent2D.width);
	int64_t iY = SnapToDetailBlock(gpGraphics->mFramebufferVkExtent2D.height);

	static int64_t siX = 0;
	static int64_t siY = 0;
	if (iX != siX || iY != siY)
	{
		LOG(kGraphics, kDebug, "FullDetail: {} x {}", iX, iY);
		siX = iX;
		siY = iY;
	}

	return std::make_tuple(iX, iY);
}

std::tuple<int64_t, int64_t> WaterFullDetail()
{
	// Gerstner frequencies are fixed, so the water vertex grid must be fixed too — anchor to a
	// reference 4K resolution instead of the live framebuffer extent. Block-snap matches FullDetail()
	// so the snapped result is deterministic and divides cleanly for shadow-execution-aligned consumers.
	static constexpr int64_t kiReferenceHeight = 2'160;
	int64_t iX = SnapToDetailBlock(kiReferenceWidth);
	int64_t iY = SnapToDetailBlock(kiReferenceHeight);

	return std::make_tuple(iX, iY);
}

float SmokeSimulationPixels()
{
	float fPixels = static_cast<float>(gpGraphics->mFramebufferVkExtent2D.width);
	return (fPixels / static_cast<float>(kiReferenceWidth)) * kfSmokeReferencePixels * gSmokeSimulationPixels.mfCurrent;
}

float SmokeSimulationPixelsY()
{
	// Floor to a multiple of kiComputeTileSize so uiSmokeTilesY divides evenly
	float fWidth = static_cast<float>(gpGraphics->mFramebufferVkExtent2D.width);
	float fHeight = static_cast<float>(gpGraphics->mFramebufferVkExtent2D.height);
	float fY = SmokeSimulationPixels() * (fHeight / fWidth);
	return std::floor(fY / static_cast<float>(shaders::kiComputeTileSize)) * static_cast<float>(shaders::kiComputeTileSize);
}

Graphics::Graphics(HINSTANCE instanceHandle, HWND windowHandle)
{
	mInstanceHandle = instanceHandle;
	mWindowHandle = windowHandle;
	ASSERT(gpGraphics == nullptr);

	gpGraphics = this;

	CHECK_VK(volkInitialize());

	Create();

	LoadAnimationDataFromEagerChunks();

	gpSwapchainManager->AcquireNextImage();

	// Find the monitor refresh rate
	int64_t iDevices = 0;
	DISPLAY_DEVICE displayDevice {.cb = sizeof(DISPLAY_DEVICE)};
	while (EnumDisplayDevices(nullptr, static_cast<DWORD>(iDevices++), &displayDevice, EDD_GET_DEVICE_INTERFACE_NAME) == TRUE)
	{
		DEVMODEA displayMode {};
		if ((displayDevice.StateFlags & DISPLAY_DEVICE_ACTIVE) != 0 && EnumDisplaySettings(displayDevice.DeviceName, ENUM_CURRENT_SETTINGS, &displayMode) == TRUE)
		{
			LOG(kGraphics, kDebug, "Active display device \"{}\" has frequency of {} Hz", displayDevice.DeviceName, displayMode.dmDisplayFrequency);
			miMonitorRefreshRate = displayMode.dmDisplayFrequency;
		}
	}
}

Graphics::~Graphics()
{
	meDestroyType = DestroyType::kSurface;
	Destroy();

	if (gpGraphics == this)
	{
		gpGraphics = nullptr;
	}
}

void Graphics::WaitAllFramebufferFencesIdle()
{
	// Drain ALL in-flight framebuffer fences (RenderGlobal already waited only the current one). This
	// is NOT vkDeviceWaitIdle — only graphics-queue work references the island slots being freed /
	// re-patched, so the present and transfer queues need not stall. The kExecuted guard skips fences
	// that were never submitted (early frames), which are not signalable.
	for (const CommandBuffers& rCommandBuffers : gpCommandBufferManager->mPerFramebufferCommandBuffers)
	{
		if (rCommandBuffers.mFlags & CommandBufferFlags::kExecuted)
		{
			CHECK_VK(vkWaitForFences(gpDeviceManager->mVkDevice, 1, &rCommandBuffers.mVkFence, VK_TRUE, kFenceTimeoutNanoseconds.count()));
		}
	}
}

void Graphics::RenderGlobal(std::chrono::duration<float> currentTime)
{
	int64_t iCommandBuffer = gpSwapchainManager->miFramebufferIndex;

	CommandBuffers& rCommandBuffers = gpCommandBufferManager->mPerFramebufferCommandBuffers.at(iCommandBuffer);
	VkResult vkResult = vkGetFenceStatus(gpDeviceManager->mVkDevice, rCommandBuffers.mVkFence);
	if (vkResult == VK_NOT_READY)
	{
		ScopedCpuProfile scopedCpuProfile(kCpuTimerWaitFence);
		CHECK_VK(vkWaitForFences(gpDeviceManager->mVkDevice, 1, &rCommandBuffers.mVkFence, VK_TRUE, kFenceTimeoutNanoseconds.count()));
	}
	else if (vkResult != VK_SUCCESS)
	{
		CHECK_VK(vkResult);
	}

	mRenderFrameDeltaNanoseconds = mRenderFrameTimer.GetDeltaNs(true);

	// UPDATE_AFTER_BIND permits descriptor writes but does not prevent races with in-flight samplers, so eviction, adoption, lighting
	// reblur, and restoration run only after every framebuffer fence drains. AnyRestorationPending also covers template-owned elevation
	// without a texture-map chunk. Clear the prior acquire publication before testing the predicate so an idle frame cannot resubmit it;
	// ProcessPendingTextures republishes only inside the write epoch. This drain covers Vulkan descriptor and image use only — it is not
	// the PackChunks loader drain (Graphics::Destroy owns that), so unrelated disk loads keep running through this window.
	gpTextureManager->mFlags.Set(TextureManagerFlags::kPendingAcquireBarriers, false);
	bool bDescriptorChurnPending = gpIslandTerrainResidency->AnyEvictionPending() || gpIslandTerrainResidency->AnyRestorationPending()
	                            || (gpTextureUploadManager->miPendingAdoptions.load(std::memory_order_relaxed) != 0)
	                            || (gpTextureManager->mFlags & TextureManagerFlags::kPendingLightingReblur);
	if (bDescriptorChurnPending)
	{
		WaitAllFramebufferFencesIdle();

		// A completion can race the pre-scan; when no epoch opens it waits until the next frame.
		// Every island descriptor mutation in this scope therefore follows the all-fence drain.
		TextureDescriptors::ScopedBindlessWriteEpoch bindlessWriteEpoch(gpTextureManager->mTextureDescriptors);
		gpIslandTerrainResidency->EvictionSweep();
		gpTextureManager->ProcessPendingTextures(iCommandBuffer);
		if (gpTextureManager->mFlags & TextureManagerFlags::kPendingLightingReblur)
		{
			gpTextureManager->ReblurAllLightingTextures();
			gpTextureManager->mFlags.Set(TextureManagerFlags::kPendingLightingReblur, false);
		}
		gpIslandTerrainResidency->RestorationSweep();
		gpTextureManager->mTextureDescriptors.VerifyAllDescriptorGenerations();
	}

	// muiFrameCounter advances every frame, independent of memory-budget support, as the LRU grace clock.
	if (gpDeviceManager->mCapabilities & DeviceCapabilityFlags::kMemoryBudgetAvailable)
	{
		vmaSetCurrentFrameIndex(gpDeviceManager->mpAllocator, static_cast<uint32_t>(muiFrameCounter));
	}
	++muiFrameCounter;

	if (rCommandBuffers.mFlags & CommandBufferFlags::kExecuted)
	{
		gpProfileManager->GpuRead(iCommandBuffer, kGpuTimerGlobal, kGpuTimerCount, true);
	}

	gpProfileManager->CpuStart(kCpuTimerRenderGlobal);
	RenderFrameGlobal(iCommandBuffer, currentTime);
	gpParticleManager->RenderGlobal(iCommandBuffer);
	gpProfileManager->CpuStop(kCpuTimerRenderGlobal);

	gpCommandBufferManager->SubmitGlobalCommandBuffer(iCommandBuffer);
}

void Graphics::RenderMainPresentAcquire(int64_t iCommandBuffer, const std::unordered_map<GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<GridCoord>& rActiveCoords, GridCoord cameraCoord, std::chrono::duration<float> currentTime)
{
	gpProfileManager->CpuStart(kCpuTimerRenderMain);
	RenderFrameMain(iCommandBuffer, rRenderInterpolates, rActiveCoords, cameraCoord, currentTime);
	gpProfileManager->CpuStop(kCpuTimerRenderMain);

	gpImGuiManager->Prepare(iCommandBuffer);

	gpCommandBufferManager->SubmitMainCommandBuffer(iCommandBuffer);

	gpCommandBufferManager->SubmitUiCommandBuffer(iCommandBuffer);

	// Capture after SubmitUiCommandBuffer and before Present: the UI submit has queued mVkFence, so SaveScreenshot waits on a pending or
	// completed signal while the present image remains application-owned.
	// Screenshot capture incurs a full GPU synchronization and is dev-only.
	if constexpr (kbScreenshots)
	{
		if (mScreenshotRequest)
		{
			SaveScreenshot(iCommandBuffer, *mScreenshotRequest);
			mScreenshotRequest.reset();
		}

		if (mDumpRenderTargetRequest)
		{
			DumpRenderTarget(iCommandBuffer, *mDumpRenderTargetRequest);
			mDumpRenderTargetRequest.reset();
		}
	}

	gpSwapchainManager->Present(iCommandBuffer);

	// Signal upload thread to process one upload iteration
	gpTextureUploadManager->SignalFrame();

	// Renders per second
	mRendersInTheLastSecond.Set();

	gpProfileManager->UpdateProfileText();

	if constexpr (kbRenderThread)
	{
		gpProfileManager->CpuStart(kCpuTimerWaitPresentFuture);
		gpSwapchainManager->mPresent.Wait();
		gpProfileManager->CpuStop(kCpuTimerWaitPresentFuture);
	}

	Create();

	// Acquire only if the recreate proceeded. A deferred Create() (window off-screen/minimized) leaves the swapchain
	// retired, and a post-Destroy defer (TOCTOU zero-area re-check below) leaves the swapchain manager torn down —
	// acquiring in either case is at best a wasted OUT_OF_DATE and at worst a null deref. The next frame's render
	// skip (GameBase::HandleDeferredSwapchain) retries the recreate and re-acquires once it lands.
	if (!mbSwapchainRecreateDeferred)
	{
		gpSwapchainManager->AcquireNextImage();
	}
	gpProfileManager->CpuStart(kCpuTimerAcquireToGlobal);
}

bool Graphics::ExtentSettled() const
{
	// Settled = extents equal, no deferred recreate, no pending swapchain-tier teardown. The last term catches the state
	// after a failed tail acquire/present (tier escalated, mbSwapchainRecreateDeferred still false) — a retired swapchain
	// that has not yet been recreated, which extent equality alone would falsely report as settled.
	return mFramebufferVkExtent2D.width == gVkWantedFramebufferExtent2D.width
	    && mFramebufferVkExtent2D.height == gVkWantedFramebufferExtent2D.height && !mbSwapchainRecreateDeferred
	    && meDestroyType < DestroyType::kSwapchain;
}

bool Graphics::SurfaceExtentZeroArea(VkSurfaceCapabilitiesKHR& rVkSurfaceCapabilitiesKHR)
{
	rVkSurfaceCapabilitiesKHR = {};
	CHECK_VK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpInstanceManager->mVkPhysicalDevice, gpInstanceManager->mVkSurfaceKHR, &rVkSurfaceCapabilitiesKHR));
	// Re-check the tier: CHECK_VK above can escalate meDestroyType to kSurface (VK_ERROR_SURFACE_LOST_KHR), and a
	// surface-loss teardown must never be deferred (the zeroed caps struct would read as a zero-area extent).
	return rVkSurfaceCapabilitiesKHR.currentExtent.width != 0xFFFFFFFF
	    && (rVkSurfaceCapabilitiesKHR.currentExtent.width == 0 || rVkSurfaceCapabilitiesKHR.currentExtent.height == 0)
	    && meDestroyType < DestroyType::kSurface;
}

void Graphics::Create()
{
	// Heap: make_unique for each manager (~12 objects that live for the app's lifetime or until device loss).
	// Can't use workbuffer (temporary) or pre-allocate (managers have complex internal state built in constructors)
	ScopedSuppressAllocationTracking suppress;

	Refresh();

	// Defer swapchain-tier recreation before Destroy when a defined surface extent is zero, leaving meDestroyType pending for retry as
	// Refresh's zero-extent guard does. Lost-surface capabilities are invalid, so surface-loss recovery must proceed with teardown and
	// recreation. Zero-area extents during surface loss or fresh device recovery bypass this gate and reach CreateSwapchain; the extent clamp
	// cannot help when minImageExtent is also zero. That case has no recovery-retry handling.
	if (gpInstanceManager != nullptr && meDestroyType >= DestroyType::kSwapchain && meDestroyType < DestroyType::kSurface)
	{
		VkSurfaceCapabilitiesKHR vkSurfaceCapabilitiesKHR {};
		if (SurfaceExtentZeroArea(vkSurfaceCapabilitiesKHR))
		{
			if (!mbSwapchainRecreateDeferred)
			{
				LOG(kGraphics, kInfo, "Swapchain recreate deferred: surface extent {} x {} (window off-screen/minimized)", vkSurfaceCapabilitiesKHR.currentExtent.width, vkSurfaceCapabilitiesKHR.currentExtent.height);
				mbSwapchainRecreateDeferred = true;
			}
			return;
		}
	}

	// Any recreate that reaches here proceeds (including the kSurface tier the gate skips), so clear the once-per-
	// transition latch: the next genuine defer must re-log and re-arm.
	mbSwapchainRecreateDeferred = false;

	// Capture the deferrable-tier flag before Destroy() zeroes meDestroyType — the post-Destroy zero-area re-check
	// below only applies to a swapchain-tier recreate (kSurface/device-loss are deliberately unguarded; see the gate).
	bool bSwapchainTierRecreate = gpInstanceManager != nullptr && meDestroyType >= DestroyType::kSwapchain && meDestroyType < DestroyType::kSurface;

	Destroy();

	// Minimize can zero the surface extent during Destroy's device wait, before CreateSwapchain re-queries it.
	// Re-query after Destroy and defer a defined zero-area extent; Vulkan requires a nonzero imageExtent.
	// With the swapchain manager absent, GameBase::HandleDeferredSwapchain skips rendering and retries Create;
	// its pre-Destroy gate defers until the extent is valid, and teardown tolerates the repeated Destroy on retry.
	// CHECK_VK can escalate surface loss to kSurface, which bypasses this deferral.
	if (bSwapchainTierRecreate)
	{
		VkSurfaceCapabilitiesKHR vkSurfaceCapabilitiesKHR {};
		if (SurfaceExtentZeroArea(vkSurfaceCapabilitiesKHR))
		{
			if (!mbSwapchainRecreateDeferred)
			{
				LOG(kGraphics, kInfo, "Swapchain recreate deferred post-Destroy: surface extent {} x {} (window off-screen/minimized)", vkSurfaceCapabilitiesKHR.currentExtent.width, vkSurfaceCapabilitiesKHR.currentExtent.height);
				mbSwapchainRecreateDeferred = true;
			}
			meDestroyType = std::max(DestroyType::kSwapchain, meDestroyType);
			return;
		}
	}

	if (mpInstanceManager == nullptr)
	{
		mpInstanceManager = std::make_unique<InstanceManager>(mInstanceHandle, mWindowHandle);
	}
	if (mpDeviceManager == nullptr)
	{
		mpDeviceManager = std::make_unique<DeviceManager>();
		gpTextureUploadManager->InitializeTransferResources();
	}
	bool bSwapchainRecreated = (mpSwapchainManager == nullptr);
	if (mpSwapchainManager == nullptr)
	{
		mpSwapchainManager = std::make_unique<SwapchainManager>(mOldVkSwapchainKHR);
		mOldVkSwapchainKHR = VK_NULL_HANDLE;
	}
	if constexpr (kbProfiling)
	{
		gpProfileManager->Create();
	}
	bool bRecordCommandBuffers = false;
	if (mpCommandBufferManager == nullptr)
	{
		mpCommandBufferManager = std::make_unique<CommandBufferManager>();
		bRecordCommandBuffers = true;
	}
	if (mpBufferManager == nullptr)
	{
		mpBufferManager = std::make_unique<BufferManager>();
	}
	else if (bSwapchainRecreated)
	{
		gpBufferManager->CreateSwapchainDependentBuffers();
	}
	if (mpIslands == nullptr)
	{
		mpIslands = std::make_unique<Islands>();
	}
	bool bInitializeBootTextures = false;
	if (mpTextureManager == nullptr)
	{
		mpTextureManager = std::make_unique<TextureManager>();
		bInitializeBootTextures = true;
	}
	else if (bSwapchainRecreated)
	{
		gpTextureManager->CreateScreenDependentResources();
	}
	if (mpPipelineManager == nullptr)
	{
		mpPipelineManager = std::make_unique<PipelineManager>();
	}
	// After the pipeline manager, so the lighting blur pipelines exist before any adoption the boot waits drive can
	// reach TextureManager::BlurLightingTexture. The texture manager is still constructed first, which the pipeline
	// manager's constructor requires.
	if (bInitializeBootTextures)
	{
		try
		{
			mpTextureManager->InitializeBootTextures();
		}
		catch (...)
		{
			meDestroyType = DestroyType::kSurface;
			Destroy();
			if (gpGraphics == this)
			{
				gpGraphics = nullptr;
			}
			throw;
		}
	}
	if (mpParticleManager == nullptr)
	{
		mpParticleManager = std::make_unique<ParticleManager>();
	}
	if (mpImGuiManager == nullptr)
	{
		mpImGuiManager = std::make_unique<ImGuiManager>(mWindowHandle);

		if constexpr (kbDebugInput)
		{
			if (game::gpGame != nullptr)
			{
				game::LoadTweaksSettings();
			}
		}
	}

	if (bRecordCommandBuffers)
	{
		gpCommandBufferManager->RecordCommandBuffers();
	}

}

template <typename T>
void Graphics::PollSetting(Wrapper& rWrapper, const char* pcLabel, DestroyType eTier, std::optional<DestroyFlags> oeFlag, bool bGate)
{
	auto [vCurrent, vPrevious, bChanged] = rWrapper.Changed<T>();
	if (bChanged && bGate) [[unlikely]]
	{
		if (pcLabel != nullptr)
		{
			if constexpr (std::is_floating_point_v<T>)
			{
				LOG(kGraphics, kDebug, "{}: {} -> {}", pcLabel, common::Wb(vPrevious, 3), common::Wb(vCurrent, 3));
			}
			else if constexpr (std::is_enum_v<T>)
			{
				LOG(kGraphics, kDebug, "{}: {} -> {}", pcLabel, static_cast<int64_t>(vPrevious), static_cast<int64_t>(vCurrent));
			}
			else
			{
				LOG(kGraphics, kDebug, "{}: {} -> {}", pcLabel, vPrevious, vCurrent);
			}
		}

		if (oeFlag.has_value())
		{
			mDestroyFlags.Set(oeFlag.value());
		}

		meDestroyType = std::max(eTier, meDestroyType);
	}
}

void Graphics::Refresh()
{
	PollSetting<bool>(gMultisampling, "Multisampling", DestroyType::kSwapchain);

	if (gpInstanceManager != nullptr)
	{
		VkSampleCountFlagBits vkSampleCount = gpInstanceManager->SelectSupportedSampleCount(gSampleCount.Get<VkSampleCountFlagBits>());
		if (vkSampleCount != gSampleCount.Get<VkSampleCountFlagBits>())
		{
			gSampleCount.Set<VkSampleCountFlagBits>(vkSampleCount);
		}
	}

	PollSetting<VkSampleCountFlagBits>(gSampleCount, "Sample count", DestroyType::kSwapchain);

	auto [vkPresentMode, vkPreviousPresentMode, bPresentModeChanged] = gPresentMode.Changed<VkPresentModeKHR>();
	if (bPresentModeChanged) [[unlikely]]
	{
		LOG(kGraphics, kDebug, "{} -> {}", string_VkPresentModeKHR(vkPreviousPresentMode), string_VkPresentModeKHR(vkPresentMode));
		meDestroyType = std::max(DestroyType::kSwapchain, meDestroyType);
	}

	if (gVkWantedFramebufferExtent2D.width != mFramebufferVkExtent2D.width || gVkWantedFramebufferExtent2D.height != mFramebufferVkExtent2D.height) [[unlikely]]
	{
		// Zero-dimension early-return before the LOG: while minimized Create() re-enters every deferred frame with a
		// 0x0 wanted extent, so logging here would spam per frame (the defer gate's own once-per-transition log covers it).
		if (gVkWantedFramebufferExtent2D.width == 0 || gVkWantedFramebufferExtent2D.height == 0)
		{
			return;
		}
		LOG(kGraphics, kDebug, "{} x {} -> {} x {}", mFramebufferVkExtent2D.width, mFramebufferVkExtent2D.height, gVkWantedFramebufferExtent2D.width, gVkWantedFramebufferExtent2D.height);
		mFramebufferVkExtent2D = gVkWantedFramebufferExtent2D;
		meDestroyType = std::max(DestroyType::kSwapchain, meDestroyType);
	}

	PollSetting<bool>(gAnisotropy, "Anisotropy", DestroyType::kSamplers);
	PollSetting<float>(gMaximumAnisotropy, "Max anisotropy", DestroyType::kSamplers);
	PollSetting<bool>(gSampleShading, "Sample shading", DestroyType::kPipelines);
	PollSetting<float>(gMinimumSampleShading, "Min sample shading", DestroyType::kPipelines);
	PollSetting<float>(gMipmapLevelOfDetailBias, "Mip lod bias", DestroyType::kSamplers);
	PollSetting<float>(gPhysicallyBasedRenderingModelDataMipmapLevelOfDetailBias, "Model data mip bias", DestroyType::kSamplers);
	PollSetting<float>(gWaterNormalMipmapBias, "Water normal mip bias", DestroyType::kSamplers);
	PollSetting<bool>(gWireframe, "Wireframe", DestroyType::kPipelines);
	PollSetting<bool>(gDebugTexture, nullptr, DestroyType::kCommandBuffers);

	PollSetting<float>(gWaterShapeDetail, "Water shape detail", DestroyType::kPipelines, DestroyFlags::kWaterMesh, gpBufferManager != nullptr);

	PollSetting<float>(gShadowRenderMultiplier, "Shadow render multiplier", DestroyType::kPipelines, DestroyFlags::kShadowTextures, gpTextureManager != nullptr);

	[[maybe_unused]] auto [fLightingMultiplier, fLightingMultiplierPrevious, bLightingMultiplierChanged] = gLightingDepositTextureMultiplier.Changed<float>();
	[[maybe_unused]] auto [fSpreadTextureMultiplierStart, fSpreadTextureMultiplierStartPrevious, bSpreadTextureMultiplierStartChanged] = gSpreadTextureMultiplierStart.Changed<float>();
	[[maybe_unused]] auto [fSpreadTextureMultiplierEnd, fSpreadTextureMultiplierEndPrevious, bSpreadTextureMultiplierEndChanged] = gSpreadTextureMultiplierEnd.Changed<float>();
	[[maybe_unused]] auto [fSpreadPassCount, fSpreadPassCountPrevious, bSpreadPassCountChanged] = gSpreadPassCount.Changed<float>();
	if ((bLightingMultiplierChanged || bSpreadTextureMultiplierStartChanged || bSpreadTextureMultiplierEndChanged || bSpreadPassCountChanged) && gpTextureManager != nullptr) [[unlikely]]
	{
		mDestroyFlags.Set(DestroyFlags::kLightingTextures);

		meDestroyType = std::max(DestroyType::kPipelines, meDestroyType);
	}

	[[maybe_unused]] auto [fLightingBlurSigma, fLightingBlurSigmaPrevious, bLightingBlurSigmaChanged] = gLightingBlurSigma.Changed<float>();
	[[maybe_unused]] auto [fLightingBlurSampleCount, fLightingBlurSampleCountPrevious, bLightingBlurSampleCountChanged] = gLightingBlurSampleCount.Changed<float>();
	[[maybe_unused]] auto [fLightingBlurEdgeFalloff, fLightingBlurEdgeFalloffPrevious, bLightingBlurEdgeFalloffChanged] = gLightingBlurEdgeFalloff.Changed<float>();
	if ((bLightingBlurSigmaChanged || bLightingBlurSampleCountChanged || bLightingBlurEdgeFalloffChanged) && gpTextureManager != nullptr) [[unlikely]]
	{
		// Earlier frames may still sample the blur results, so RenderGlobal reblurs after its all-fence drain
		gpTextureManager->mFlags.Set(TextureManagerFlags::kPendingLightingReblur);
	}

	[[maybe_unused]] auto [fObjectShadowsRenderMultiplier, fObjectShadowsRenderMultiplierPrevious, bObjectShadowsRenderMultiplierChanged] = gObjectShadowsRenderMultiplier.Changed<float>();
	[[maybe_unused]] auto [fObjectShadowsBlurMultiplier, fObjectShadowsBlurMultiplierPrevious, bObjectShadowsBlurMultiplierChanged] = gObjectShadowsBlurMultiplier.Changed<float>();
	if ((bObjectShadowsRenderMultiplierChanged || bObjectShadowsBlurMultiplierChanged) && gpTextureManager != nullptr) [[unlikely]]
	{
		mDestroyFlags.Set(DestroyFlags::kObjectShadows);

		meDestroyType = std::max(DestroyType::kPipelines, meDestroyType);
	}

	if (gpInstanceManager != nullptr) [[likely]]
	{
		PollSetting<float>(gTerrainElevationTextureMultiplier, "TerrainElevationTexture multiplier", DestroyType::kPipelines, DestroyFlags::kTerrainElevation);

		[[maybe_unused]] auto [fSmokeTrailPower, fSmokeTrailPowerPrevious, bSmokeTrailPowerChanged] = gSmokeTrailPower.Changed<float>();
		[[maybe_unused]] auto [fSmokeTrailAlpha, fSmokeTrailAlphaPrevious, bSmokeTrailAlphaChanged] = gSmokeTrailAlpha.Changed<float>();
		auto [fSmokeSimulationPixels, fPreviousSmokeSimulationPixels, bSmokeSimulationPixelsChanged] = gSmokeSimulationPixels.Changed<float>();
		auto [fSmokeSimulationArea, fPreviousSmokeSimulationArea, bSmokeSimulationAreaChanged] = gSmokeSimulationArea.Changed<float>();
		if (bSmokeTrailPowerChanged || bSmokeTrailAlphaChanged || bSmokeSimulationPixelsChanged || bSmokeSimulationAreaChanged) [[unlikely]]
		{
			LOG(kGraphics, kDebug, "SmokeSimulationPixels: {} -> {} ({} -> {})", common::Wb(fPreviousSmokeSimulationPixels, 3), common::Wb(fSmokeSimulationPixels, 3), common::Wb(gSmokeSimulationPixels.mfCurrent, 3), common::Wb(SmokeSimulationPixels(), 3));
			LOG(kGraphics, kDebug, "SmokeSimulationArea: {} -> {}", common::Wb(fPreviousSmokeSimulationArea, 3), common::Wb(fSmokeSimulationArea, 3));

			mDestroyFlags.Set(DestroyFlags::kSmokeTextures);
			meDestroyType = std::max(DestroyType::kPipelines, meDestroyType);
		}
	}
}

void Graphics::RecreateResources()
{
	if (std::to_underlying(mDestroyFlags.meFlags) == 0)
	{
		return;
	}

	if (mDestroyFlags & DestroyFlags::kShadowTextures)
	{
		if (gpTextureManager != nullptr)
		{
			gpTextureManager->mRenderTargetTextures.CreateShadowTextures();
		}
	}

	if (mDestroyFlags & DestroyFlags::kObjectShadows)
	{
		if (gpTextureManager != nullptr)
		{
			gpTextureManager->mRenderTargetTextures.CreateObjectShadowsTextures();
		}
	}

	if (mDestroyFlags & DestroyFlags::kLightingTextures)
	{
		if (gpTextureManager != nullptr)
		{
			gpTextureManager->mRenderTargetTextures.CreateLightingTextures();
		}
	}

	if (mDestroyFlags & DestroyFlags::kWaterMesh)
	{
		if (gpBufferManager != nullptr)
		{
			gpBufferManager->CreateWaterMesh();
		}
		if (gpTextureManager != nullptr)
		{
			// Texel grid must match the water-mesh vertex grid (see RenderTargetTextures.h comment).
			gpTextureManager->mRenderTargetTextures.CreateWaterDisplacementTextures();
		}
	}

	if (mDestroyFlags & DestroyFlags::kTerrainElevation)
	{
		if (gpTextureManager != nullptr)
		{
			gpTextureManager->mRenderTargetTextures.CreateTerrainTextures();
		}
	}

	if (mDestroyFlags & DestroyFlags::kSmokeTextures)
	{
		if (gpTextureManager != nullptr)
		{
			gpTextureManager->mRenderTargetTextures.CreateSmokeTextures();
			gpTextureManager->mRenderTargetTextures.CreateWindTextures();
		}
	}

	mDestroyFlags.meFlags = static_cast<decltype(mDestroyFlags.meFlags)>(0);
}

bool Graphics::Destroy()
{
	if (meDestroyType == DestroyType::kNone)
	{
		return false;
	}

	LOG(kGraphics, kInfo, "Graphics::Destroy() {}", static_cast<int64_t>(meDestroyType));

	// Drain worker threads (Vulkan requires exclusive host access to all queues)
	if (gpSwapchainManager != nullptr)
	{
		gpSwapchainManager->mPresent.Wait();
	}
	if (gpCommandBufferManager != nullptr)
	{
		gpCommandBufferManager->mSubmitMain.Wait();
		gpCommandBufferManager->mSubmitGlobal.Wait();
	}
	if (meDestroyType >= DestroyType::kSurface)
	{
		// Full recovery ends with ResetTextureChunkStates() below, which must find no PackChunks loader running:
		// a loader that popped a whole-texture request before the reset would otherwise publish kUploading after
		// it, leaving a chunk in a state RequestChunkLoad skips forever. Lighter destroy levels keep no such
		// reset, so they keep their current cost.
		gpFileManager->mpPackChunks->mLoader.WaitForLoadersIdle();
	}
	if (gpTextureUploadManager != nullptr)
	{
		// Must follow the loader drain: a whole-texture job finishing inside the drain stores kUploading and posts
		// an upload, and the reset maps kUploading back to kDiskLoaded and re-arms the pending-adoption counter —
		// so without this wait the reset could rewrite a chunk whose upload thread is still writing vkUploadImage and
		// vmaAllocation.
		gpTextureUploadManager->WaitIdle();
	}

	if (gpDeviceManager != nullptr)
	{
		// Teardown must continue even if the device is already lost; result escalation is not actionable here.
		vkDeviceWaitIdle(gpDeviceManager->mVkDevice);
	}

	RecreateResources();

	if (meDestroyType >= DestroyType::kSamplers)
	{
		if (gpTextureManager != nullptr)
		{
			gpTextureManager->DestroySamplers();
			gpTextureManager->CreateSamplers();

			// Global Set 0 survives pipeline recreation, always update it with new sampler handles
			gpTextureManager->mTextureDescriptors.WriteGlobalDescriptorSets();

			// Rewrite per-pipeline sampler descriptors unless all pipelines are being fully rebuilt
			if (meDestroyType < DestroyType::kPipelines)
			{
				gpTextureManager->mTextureDescriptors.RewriteSamplerDescriptors();
			}
		}
	}

	if (meDestroyType >= DestroyType::kCommandBuffers)
	{
		mpCommandBufferManager.reset();
	}

	if (meDestroyType >= DestroyType::kPipelines)
	{
		mpPipelineManager.reset();
	}

	if (meDestroyType >= DestroyType::kSwapchain)
	{
		if (meDestroyType < DestroyType::kSurface)
		{
			// Partial destroy: keep TextureManager and BufferManager alive, only destroy screen-dependent internals
			if (gpTextureManager != nullptr)
			{
				gpTextureManager->DestroyScreenDependentResources();
			}
			if (gpBufferManager != nullptr)
			{
				gpBufferManager->DestroySwapchainDependentBuffers();
			}
		}
		else
		{
			// Full destroy: tear down completely for device recreation
			mpTextureManager.reset();
			mpBufferManager.reset();
		}
		if constexpr (kbProfiling)
		{
			gpProfileManager->Destroy();
		}
		// Save old swapchain handle for seamless transition (only during recreation, not final shutdown)
		if (mpSwapchainManager != nullptr && meDestroyType < DestroyType::kSurface)
		{
			mOldVkSwapchainKHR = mpSwapchainManager->ReleaseHandleForRecreation();
		}
		mpSwapchainManager.reset();
		if constexpr (kbDebugInput)
		{
			// gpImGuiManager guard: SaveTweaksSettings derefs gpImGuiManager->mpTweaksScreen, and a post-Destroy re-defer
			// runs a second Destroy() after mpImGuiManager.reset() below already nulled the global on the first pass.
			if (game::gpGame != nullptr && gpImGuiManager != nullptr)
			{
				game::SaveTweaksSettings();
			}
		}
		mpImGuiManager.reset();
	}

	if (meDestroyType >= DestroyType::kSurface)
	{
		mpIslands.reset();
		gpTextureUploadManager->DestroyTransferResources();
		// Reset lazy-loaded texture chunk states so they reload after device recreation
		gpFileManager->mpPackChunks->ResetTextureChunkStates();
		// IslandTerrainResidency is owned by Main.cpp (outlives Graphics); mesh arena allocations belong to Islands,
		// which was destroyed above before mpDeviceManager.reset(). Release only template-owned resources here.
		if (gpIslandTerrainResidency != nullptr)
		{
			gpIslandTerrainResidency->ReleaseGpuResources();
		}
		mpDeviceManager.reset();
		mpInstanceManager.reset();
	}

	mDestroyFlags = DestroyFlags_t {};
	meDestroyType = DestroyType::kNone;

	return true;
}

} // namespace engine

#endif // defined(BT_CLIENT)
