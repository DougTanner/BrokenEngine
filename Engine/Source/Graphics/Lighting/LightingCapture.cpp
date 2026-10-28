#if defined(BT_CLIENT)

#include "Data/Shader.h"
#include "File/PackChunks.h"
#include "Graphics/Managers/TextureCache.h"
#include "Graphics/Screenshot.h"
#include "LightingState.h"

namespace engine
{

static nlohmann::json LightingOperationJson(const LightingTerrainOperations& rOperations)
{
	return {{"units", "counts"}, {"renderpasses", rOperations.iRenderPasses}, {"draws", rOperations.iDraws}, {"dispatches", rOperations.iDispatches}, {"builds", rOperations.iBuilds}, {"barriers", rOperations.iBarriers}, {"copies", rOperations.iCopies}, {"fills", rOperations.iFills}, {"updates", rOperations.iUpdates}, {"timestamps", rOperations.iTimestamps}, {"queryResets", rOperations.iQueryResets}};
}

static std::string LightingHash(std::span<const std::byte> bytes)
{
	common::Sha256Hasher hasher;
	hasher.Update(std::span(const_cast<std::byte*>(bytes.data()), bytes.size()));
	std::array<uint8_t, 32> uiDigest {};
	if (!hasher.Finish(uiDigest))
	{
		throw std::runtime_error("Lighting SHA256 failed");
	}
	std::string result;
	for (uint8_t uiByte : uiDigest) result += std::format("{:02x}", uiByte);
	return result;
}

static nlohmann::json LightingWriteArtifact(const std::filesystem::path& rDirectory, std::string_view name, std::span<const std::byte> bytes)
{
	std::filesystem::path path = rDirectory / name;
	std::ofstream stream(path, std::ios::binary | std::ios::trunc);
	stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	stream.close();
	if (!stream)
	{
		throw std::runtime_error("Lighting artifact write failed");
	}
	return {{"path", common::ToString(path.native())}, {"bytes", bytes.size()}, {"sha256", LightingHash(bytes)}};
}

static void LightingServiceCapture(Lighting& rLighting)
{
	static std::chrono::steady_clock::time_point sNext;
	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	if (now < sNext)
	{
		return;
	}
	sNext = now + 20ms;
	MSG message {};
	while (PeekMessage(&message, nullptr, 0, 0, PM_NOREMOVE) == TRUE)
	{
		if (message.message == WM_QUIT || message.message == WM_CLOSE || message.message == WM_SIZE || message.message == WM_SYSCOMMAND || message.message == WM_INPUT || (message.message >= WM_KEYFIRST && message.message <= WM_KEYLAST) || (message.message >= WM_MOUSEFIRST && message.message <= WM_MOUSELAST) || message.message >= WM_APP)
		{
			rLighting.CancelCapture();
			break;
		}
		PeekMessage(&message, nullptr, 0, 0, PM_REMOVE);
		TranslateMessage(&message);
		DispatchMessage(&message);
	}
	DrainLightingCaptureCommands();
}

static void LightingWaitFence(VkFence vkFence, Lighting& rLighting)
{
	while (true)
	{
		LightingServiceCapture(rLighting);
		VkResult vkResult = vkWaitForFences(gpDeviceManager->mVkDevice, 1, &vkFence, VK_TRUE, 2'000'000);
		if (vkResult == VK_TIMEOUT)
		{
			continue;
		}
		CHECK_VK(vkResult);
		return;
	}
}

static void LightingBeginCaptureCommand(VkCommandBuffer vkCommandBuffer)
{
	CHECK_VK(vkResetCommandBuffer(vkCommandBuffer, 0));
	VkCommandBufferBeginInfo vkBeginInfo {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
	CHECK_VK(vkBeginCommandBuffer(vkCommandBuffer, &vkBeginInfo));
}

static void LightingSubmitCaptureCommand(VkCommandBuffer vkCommandBuffer, VkFence vkFence, Lighting& rLighting)
{
	CHECK_VK(vkEndCommandBuffer(vkCommandBuffer));
	CHECK_VK(vkResetFences(gpDeviceManager->mVkDevice, 1, &vkFence));
	VkSubmitInfo vkSubmitInfo {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &vkCommandBuffer};
	CHECK_VK(vkQueueSubmit(gpDeviceManager->mGraphicsVkQueue, 1, &vkSubmitInfo, vkFence));
	LightingWaitFence(vkFence, rLighting);
}

bool Lighting::IsCaptureHeld() const
{
	return mpState->comparison != nullptr && !std::cmp_equal(mpState->comparison->uiHeldPublication, 0);
}

void Lighting::CancelCapture()
{
	// TEMP START original native request capture
	mpState->nativeCapture.Abandon();
	// TEMP END original native request capture
	if (!IsCaptureHeld())
	{
		return;
	}
	State::Comparison& rComparison = *mpState->comparison;
	rComparison.flags.Set(State::CaptureFlags::kMutationPending);
	if (rComparison.status != "composing" && rComparison.status != "exporting" && rComparison.status != "complete")
	{
		rComparison.flags.Set(State::CaptureFlags::kCancel);
	}
}

void Lighting::RecordCaptureStage(std::string_view stage)
{
	State::Comparison& rComparison = *mpState->comparison;
	LightingFramebuffer& rFramebuffer = *mpState->framebuffers.at(mpState->iFramebuffer);
	const shaders::GlobalLayout& rGlobal = *reinterpret_cast<const shaders::GlobalLayout*>(gpBufferManager->mGlobalLayoutUniformBuffers.at(mpState->iFramebuffer).mpMappedMemory);
	const shaders::MainLayout& rMain = *reinterpret_cast<const shaders::MainLayout*>(gpBufferManager->mMainLayoutUniformBuffers.at(mpState->iFramebuffer).mpMappedMemory);
	nlohmann::json value
	{
		{"sequence", ++rComparison.uiSequence}, {"publication", rFramebuffer.uiRecordedPublication}, {"generation", rFramebuffer.uiGeneration}, {"framebuffer", mpState->iFramebuffer},
		{"renderTime", rGlobal.fElapsedTime}, {"cameraFrame", gpCamera->miFrame}, {"basis", {gpCamera->mBasisCoordinate.iQ, gpCamera->mBasisCoordinate.iR}},
		{"renderFrame", rMain.iFrameNumber}, {"renderNumber", rMain.iRenderNumber},
		{"tlas", reinterpret_cast<uint64_t>(rFramebuffer.scene.mVkAccelerationStructureKHR)}, {"tlasAddress", rFramebuffer.scene.GetDeviceAddress()},
		{"exposure", rMain.fPhysicallyBasedRenderingExposure}, {"waterNormalTime", {rGlobal.fWaterReducedNormalTimeX, rGlobal.fWaterReducedNormalTimeY, rGlobal.fWaterReducedNormalTimeTwoX, rGlobal.fWaterReducedNormalTimeTwoY, rGlobal.fWaterReducedNormalTimeThreeX, rGlobal.fWaterReducedNormalTimeThreeY}},
		{"sunColor", {rGlobal.f4SunColor.x, rGlobal.f4SunColor.y, rGlobal.f4SunColor.z}}, {"moonColor", {rGlobal.f4MoonColor.x, rGlobal.f4MoonColor.y, rGlobal.f4MoonColor.z}}, {"ambientColor", {rGlobal.f4AmbientColor.x, rGlobal.f4AmbientColor.y, rGlobal.f4AmbientColor.z}},
	};
	int64_t iMeshes = gpBufferManager->mpiMeshDataOffset[mpState->iFramebuffer];
	int64_t iJoints = gpBufferManager->mpiJointMatrixOffset[mpState->iFramebuffer];
	for (const std::pair<std::string_view, const Buffer*>& rBufferIdentity : {std::pair<std::string_view, const Buffer*>("global", &gpBufferManager->mGlobalLayoutUniformBuffers.at(mpState->iFramebuffer)), {"main", &gpBufferManager->mMainLayoutUniformBuffers.at(mpState->iFramebuffer)}, {"sources", &rFramebuffer.sources}, {"samples", &rFramebuffer.samples}, {"publicationBuffer", &rFramebuffer.publication}, {"instances", &rFramebuffer.instances}, {"geometry", &rFramebuffer.geometries}, {"meshData", &gpBufferManager->mMeshDataStorageBuffers.at(mpState->iFramebuffer)}, {"joints", &gpBufferManager->mJointMatrixStorageBuffers.at(mpState->iFramebuffer)}})
	{
		std::string_view name = rBufferIdentity.first;
		const Buffer* pBuffer = rBufferIdentity.second;
		value["buffers"][name] = {{"handle", reinterpret_cast<uint64_t>(pBuffer->GetBuffer())}, {"bytes", pBuffer->mInfo.iDataSize}};
		int64_t iInputBytes = pBuffer->mInfo.iDataSize;
		if (name == "sources")
		{
			iInputBytes = rFramebuffer.pendingDiagnostics.iSources * static_cast<int64_t>(sizeof(shaders::LightingSource));
		}
		if (name == "samples")
		{
			iInputBytes = mpState->settings.iAreaSamples * static_cast<int64_t>(sizeof(XMFLOAT2));
		}
		if (name == "instances")
		{
			iInputBytes = static_cast<int64_t>(rFramebuffer.uiInstanceCount) * static_cast<int64_t>(sizeof(shaders::LightingInstance));
		}
		if (name == "geometry")
		{
			iInputBytes = static_cast<int64_t>(rFramebuffer.uiGeometryCount) * static_cast<int64_t>(sizeof(shaders::LightingGeometry));
		}
		if (name == "meshData")
		{
			iInputBytes = iMeshes * static_cast<int64_t>(sizeof(common::MeshData));
		}
		if (name == "joints")
		{
			iInputBytes = iJoints * static_cast<int64_t>(sizeof(common::JointMatrix));
		}
		value["buffers"][name]["inputBytes"] = iInputBytes;
		value["buffers"][name]["sha256"] = LightingHash(std::as_bytes(std::span(pBuffer->mpMappedMemory, static_cast<size_t>(iInputBytes))));
	}
	for (const std::tuple<GridCoord, int64_t, float>& rFrameIdentity : rFramebuffer.frameIdentities)
	{
		value["frames"].push_back({{"coordinate", {std::get<0>(rFrameIdentity).iQ, std::get<0>(rFrameIdentity).iR}}, {"tick", std::get<1>(rFrameIdentity)}, {"delta", std::get<2>(rFrameIdentity)}});
	}
	for (const LightingModelRange& rRange : rFramebuffer.modelRanges)
	{
		const Buffer& rBuffer = gpBufferManager->mpDynamicStorageBuffers[kBufferMain].at(rRange.instanceBufferCrc).at(mpState->iFramebuffer);
		value["models"].push_back({{"scene", rRange.modelCrc}, {"instanceBufferKey", rRange.instanceBufferCrc}, {"buffer", reinterpret_cast<uint64_t>(rBuffer.GetBuffer())}, {"first", rRange.iFirstInstance}, {"count", rRange.iInstanceCount}, {"sha256", LightingHash(std::as_bytes(std::span(rBuffer.mpMappedMemory + rRange.iFirstInstance * sizeof(shaders::ModelLayout), static_cast<size_t>(rRange.iInstanceCount * sizeof(shaders::ModelLayout)))))}});
	}
	const shaders::LightingGeometry* pGeometries = reinterpret_cast<const shaders::LightingGeometry*>(rFramebuffer.geometries.mpMappedMemory);
	for (int64_t i = 0; i < rFramebuffer.uiGeometryCount; ++i)
	{
		const shaders::LightingGeometry& rGeometry = pGeometries[i];
		value["geometryRanges"].push_back({{"geometry", i}, {"position", rGeometry.uiPositionAddress}, {"attributes", rGeometry.uiAttributeAddress}, {"indices", rGeometry.uiIndexAddress}, {"materials", rGeometry.uiMaterialAddress}, {"firstIndex", rGeometry.uiFirstIndex}, {"triangles", rGeometry.uiPrimitiveCount}, {"kind", rGeometry.uiKind}, {"payload", rGeometry.uiPayload}});
	}
	for (const auto& [uiCrc, rTexture] : gpTextureManager->mTextureMap) value["textures"].push_back({{"crc", uiCrc}, {"image", reinterpret_cast<uint64_t>(rTexture.mVkImage)}, {"generation", rTexture.miGeneration}});
	value["globalDescriptorSet"] = reinterpret_cast<uint64_t>(gpTextureManager->mTextureDescriptors.mGlobalDescriptorSets.at(mpState->iFramebuffer));
	value["referenceEnabled"] = *reinterpret_cast<const uint32_t*>(gpTextureManager->mTextureDescriptors.mLightingControls.at(mpState->iFramebuffer).mpMappedMemory);
	const VkAccelerationStructureInstanceKHR* pVkAccelerationStructureInstances = reinterpret_cast<const VkAccelerationStructureInstanceKHR*>(rFramebuffer.accelerationInstances.mpMappedMemory);
	for (int64_t i = 0; i < rFramebuffer.uiInstanceCount; ++i) value["accelerationInstances"].push_back({{"instance", i}, {"customIndex", static_cast<uint32_t>(pVkAccelerationStructureInstances[i].instanceCustomIndex)}, {"blasAddress", pVkAccelerationStructureInstances[i].accelerationStructureReference}});
	for (const auto& [iSlot, rIsland] : gpTextureManager->mTextureDescriptors.mIslandSlots)
	{
		nlohmann::json identity = {{"slot", iSlot}, {"island", rIsland.islandCrc}};
		for (const std::pair<std::string_view, const Texture*>& rTextureIdentity : {std::pair<std::string_view, const Texture*>("elevation", gpTextureManager->mRenderTargetTextures.mElevationTextures[iSlot]), {"color", gpTextureManager->mRenderTargetTextures.mColorTextures[iSlot]}, {"normal", gpTextureManager->mRenderTargetTextures.mNormalsTextures[iSlot]}, {"mask", gpTextureManager->mRenderTargetTextures.mMasksTextures[iSlot]}})
		{
			const Texture* pTexture = rTextureIdentity.second;
			identity[rTextureIdentity.first] = {{"image", reinterpret_cast<uint64_t>(pTexture->mVkImage)}, {"view", reinterpret_cast<uint64_t>(pTexture->mVkImageView)}, {"generation", pTexture->miGeneration}};
		}
		value["islandSlots"].push_back(std::move(identity));
	}
	for (const std::pair<std::string_view, const Texture*>& rTextureIdentity : {std::pair<std::string_view, const Texture*>("elevation", &gpTextureManager->mRenderTargetTextures.mTerrainElevationTexture), {"waterDisplacement", &gpTextureManager->mRenderTargetTextures.mWaterDisplacementTexture}, {"waterNormal", &gpTextureManager->mRenderTargetTextures.mWaterDisplacementNormalTexture}, {"waterFoam", &gpTextureManager->mRenderTargetTextures.mWaterFoamHistoryTexture}, {"reference", &rComparison.reference}})
	{
		const Texture* pTexture = rTextureIdentity.second;
		value["renderTargets"][rTextureIdentity.first] = {{"image", reinterpret_cast<uint64_t>(pTexture->mVkImage)}, {"generation", pTexture->miGeneration}};
	}
	// TEMP START matched memory cost
	if (stage == "prepared" || stage == "composed" || stage == "exportComplete")
	{
		mpState->terrain.RecordMatchedCostReceipt(mpState->iFramebuffer, rFramebuffer.geometries, value["matchedMemoryCost"]);
		value["matchedMemoryCost"]["cpuTerrainPrepare"] = {{"lastMs", rFramebuffer.fMatchedTerrainPrepareMs}, {"totalMs", rFramebuffer.fMatchedTerrainPrepareTotalMs}, {"calls", rFramebuffer.uiMatchedTerrainPrepareCalls}, {"scope", "exact LightingTerrain::Prepare call; excludes Write, RecordPreparation, receipt hashing/export; cumulative since framebuffer creation; Debug measurements"}};
	}
	// TEMP END matched memory cost
	rComparison.manifest["stages"][stage] = std::move(value);
}

bool Lighting::CapturePreparation(VkCommandBuffer vkPreparation, VkSemaphore vkGlobalFinished, VkSemaphore vkPreparationFinished)
{
	// TEMP START original native request capture
	struct [[nodiscard]] NativePreparationCleanup
	{
		State::NativeCapture& rNative;
		explicit NativePreparationCleanup(State::NativeCapture& rNative) : rNative(rNative)
		{
		}
		NativePreparationCleanup(const NativePreparationCleanup&) = delete;
		NativePreparationCleanup& operator=(const NativePreparationCleanup&) = delete;
		~NativePreparationCleanup() noexcept
		{
			rNative.Abandon();
		}
	} nativeCleanup(mpState->nativeCapture);
	// TEMP END original native request capture
	State::Comparison& rComparison = *mpState->comparison;
	const LightingComparisonSettings& rSettings = mpState->settings;
	bool bReferenceRequest = !std::cmp_equal(rSettings.uiCaptureRequest, 0) && rSettings.uiCaptureRequest != mpState->uiServicedCapture;
	bool bValidationRequest = !std::cmp_equal(rSettings.uiValidationRequest, 0) && rSettings.uiValidationRequest != mpState->uiServicedValidation;
	if (rSettings.eMode == LightingComparisonMode::kDisabled || (!bReferenceRequest && !bValidationRequest))
	{
		return false;
	}
	if (!std::cmp_equal(rComparison.uiResumePublication, 0))
	{
		return false;
	}
	if (mRequestedComparisonSettings != rSettings)
	{
		mpState->uiServicedCapture = rSettings.uiCaptureRequest;
		mpState->uiServicedValidation = rSettings.uiValidationRequest;
		rComparison.uiRequest = bReferenceRequest ? rSettings.uiCaptureRequest : rSettings.uiValidationRequest;
		rComparison.status = "cancelled";
		rComparison.error = "Comparison settings changed before capture preparation";
		rComparison.manifestPath.clear();
		rComparison.manifestHash.clear();
		return false;
	}
	// Heap: this developer capture retains scalar evidence and output artifacts beyond each fenced tile.
	ScopedSuppressAllocationTracking suppress;
	gpCommandBufferManager->mSubmitGlobal.Wait();
	rComparison.flags.Set(State::CaptureFlags::kCancel, false);
	rComparison.flags.Set(State::CaptureFlags::kReferenceValid, false);
	rComparison.flags.Set(State::CaptureFlags::kMutationPending, false);
	rComparison.error.clear();
	rComparison.manifestPath.clear();
	rComparison.manifestHash.clear();
	rComparison.exportReadback = {};
	rComparison.receiverOperations = {};
	rComparison.referenceOperations = {};
	rComparison.exportOperations = {};
	rComparison.fFirstTileGpuMs = 0.0;
	rComparison.status = "preparing";
	rComparison.start = std::chrono::steady_clock::now();
	rComparison.uiRequest = bReferenceRequest ? rSettings.uiCaptureRequest : rSettings.uiValidationRequest;
	rComparison.iCompletedTiles = 0;
	rComparison.iTotalTiles = bReferenceRequest && rSettings.eMode == LightingComparisonMode::kFixtureReference ? 129'600 : 0;
	rComparison.manifest = {{"schema", 1}, {"request", rComparison.uiRequest}, {"captureRequest", rSettings.uiCaptureRequest}, {"validationRequest", rSettings.uiValidationRequest}, {"width", 1'920}, {"height", 1'080}, {"sourceCount", rSettings.iSourceCount}, {"samples", rSettings.iAreaSamples}, {"event", rSettings.iEvent}, {"mode", static_cast<int>(rSettings.eMode)}, {"camera", static_cast<int>(rSettings.eCamera)}, {"scenario", static_cast<int>(rSettings.eScenario)}, {"status", "pending"}, {"exportComplete", false}, {"referenceComplete", false}};
	rComparison.uiHeldPublication = HoldPublication(mpState->iFramebuffer);
	// TEMP START original native request capture
	State::NativeCapture& rNative = mpState->nativeCapture;
	if ((rNative.flags & State::NativeCapture::Flags::kActive) && !(rNative.flags & State::NativeCapture::Flags::kCleanupFailed))
	{
		if (bReferenceRequest || rNative.uiRequest != rSettings.uiValidationRequest)
		{
			rNative.Abandon();
		}
		else
		{
			rNative.uiPublication = rComparison.uiHeldPublication;
			rNative.uiGeneration = mpState->framebuffers.at(mpState->iFramebuffer)->uiGeneration;
			rNative.iFramebuffer = mpState->iFramebuffer;
		}
	}
	// TEMP END original native request capture
	rComparison.manifest["requestedSequence"] = ++rComparison.uiSequence;
	rComparison.manifest["kind"] = bReferenceRequest ? "frame-capture" : "terrain-validation";
	if (bReferenceRequest)
	{
		mpState->uiServicedCapture = rSettings.uiCaptureRequest;
	}
	if (bValidationRequest)
	{
		mpState->uiServicedValidation = rSettings.uiValidationRequest;
	}
	if (!bReferenceRequest)
	{
		VkFenceCreateInfo vkFenceCreateInfo {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
		if (rComparison.vkFence == VK_NULL_HANDLE)
		{
			CHECK_VK(vkCreateFence(gpDeviceManager->mVkDevice, &vkFenceCreateInfo, nullptr, &rComparison.vkFence));
		}
	}
	VkPipelineStageFlags vkWaitStages = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	VkSubmitInfo vkPreparationSubmit {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .waitSemaphoreCount = 1, .pWaitSemaphores = &vkGlobalFinished, .pWaitDstStageMask = &vkWaitStages, .commandBufferCount = 1, .pCommandBuffers = &vkPreparation};
	CHECK_VK(vkResetFences(gpDeviceManager->mVkDevice, 1, &rComparison.vkFence));
	CHECK_VK(vkQueueSubmit(gpDeviceManager->mGraphicsVkQueue, 1, &vkPreparationSubmit, rComparison.vkFence));
	LightingWaitFence(rComparison.vkFence, *this);
	CompletePreparation(mpState->iFramebuffer);
	// TEMP START original native request capture
	if ((rNative.flags & State::NativeCapture::Flags::kActive) && !(rNative.flags & State::NativeCapture::Flags::kCleanupFailed))
	{
		rNative.iEndResult = rNative.pApi->EndFrameCapture(rNative.pDevice, rNative.pWindow);
		rNative.iEndActive = rNative.pApi->IsFrameCapturing();
		if (rNative.iEndActive == 0)
		{
			rNative.flags.Set(State::NativeCapture::Flags::kActive, false);
		}
		rNative.pcStatus = "end-failed";
		rNative.iFinalCount = rNative.pApi->GetNumCaptures();
		if (rNative.iEndResult != 0 && rNative.iEndActive == 0 && rNative.iFinalCount == rNative.iBaselineCount + 1)
		{
			rNative.pcStatus = "path-failed";
			rNative.iSizingResult = rNative.pApi->GetCapture(static_cast<uint32_t>(rNative.iBaselineCount), nullptr, &rNative.uiPathBytes, nullptr);
			if (rNative.iSizingResult != 0 && rNative.uiPathBytes > 1)
			{
				rNative.path.resize(rNative.uiPathBytes);
				rNative.uiReadBytes = rNative.uiPathBytes;
				rNative.iReadResult = rNative.pApi->GetCapture(static_cast<uint32_t>(rNative.iBaselineCount), rNative.path.data(), &rNative.uiReadBytes, nullptr);
				if (rNative.iReadResult != 0 && rNative.uiReadBytes == rNative.uiPathBytes && rNative.path.back() == '\0')
				{
					rNative.path.resize(rNative.uiPathBytes - 1);
					rNative.pcStatus = std::filesystem::is_regular_file(std::filesystem::path(std::u8string(rNative.path.begin(), rNative.path.end()))) ? "complete" : "file-missing";
				}
			}
		}
		rNative.Abandon();
	}
	// TEMP END original native request capture
	RecordCaptureStage("prepared");
	if (bReferenceRequest && !(rComparison.flags & State::CaptureFlags::kCancel))
	{
		if (std::cmp_not_equal(gpGraphics->mFramebufferVkExtent2D.width, 1'920) || std::cmp_not_equal(gpGraphics->mFramebufferVkExtent2D.height, 1'080))
		{
			rComparison.error = "Lighting capture requires the original frame at 1920x1080";
			rComparison.flags.Set(State::CaptureFlags::kCancel);
		}
		else
		{
			rComparison.status = "receivers";
			LightingBeginCaptureCommand(rComparison.vkCommandBuffer);
			RecordReceivers(rComparison.vkCommandBuffer);
			LightingSubmitCaptureCommand(rComparison.vkCommandBuffer, rComparison.vkFence, *this);
			RecordCaptureStage("receiver");
		}
	}
	std::chrono::steady_clock::time_point referenceStart = std::chrono::steady_clock::now();
	rComparison.status = (rComparison.flags & State::CaptureFlags::kCancel) ? "cancelled" : "reference";
	for (int64_t iTile = 0; iTile < rComparison.iTotalTiles && !(rComparison.flags & State::CaptureFlags::kCancel); ++iTile)
	{
		LightingBeginCaptureCommand(rComparison.vkCommandBuffer);
		if (iTile == 0)
		{
			vkCmdResetQueryPool(rComparison.vkCommandBuffer, rComparison.vkQueryPool, 0, 2);
			++rComparison.referenceOperations.iQueryResets;
			vkCmdWriteTimestamp(rComparison.vkCommandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, rComparison.vkQueryPool, 0);
			++rComparison.referenceOperations.iTimestamps;
		}
		rComparison.compute.RecordCompute(mpState->iFramebuffer, rComparison.vkCommandBuffer, 1, 1, 1, {static_cast<float>((iTile % 480) * 4), static_cast<float>((iTile / 480) * 4), static_cast<float>(rSettings.iAreaSamples), 0.0f});
		++rComparison.referenceOperations.iDispatches;
		if (iTile == 0)
		{
			vkCmdWriteTimestamp(rComparison.vkCommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, rComparison.vkQueryPool, 1);
			++rComparison.referenceOperations.iTimestamps;
		}
		LightingSubmitCaptureCommand(rComparison.vkCommandBuffer, rComparison.vkFence, *this);
		++rComparison.iCompletedTiles;
		if (iTile == 0)
		{
			uint64_t puiTimestamps[2] {};
			CHECK_VK(vkGetQueryPoolResults(gpDeviceManager->mVkDevice, rComparison.vkQueryPool, 0, 2, sizeof(puiTimestamps), puiTimestamps, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT));
			uint32_t uiValidBits = gpInstanceManager->mVkQueueFamilyProperties.at(gpInstanceManager->miGraphicsQueueFamilyIndex).timestampValidBits;
			uint64_t uiDelta = puiTimestamps[1] - puiTimestamps[0];
			if (std::cmp_less(uiValidBits, 64))
			{
				uiDelta &= (static_cast<uint64_t>(1) << uiValidBits) - 1;
			}
			rComparison.fFirstTileGpuMs = static_cast<double>(uiDelta) * gpInstanceManager->mVkPhysicalDeviceProperties.limits.timestampPeriod * 1.0e-6;
		}
	}
	rComparison.durationReference = std::chrono::steady_clock::now() - referenceStart;
	if (bReferenceRequest && rComparison.iTotalTiles == 0 && !(rComparison.flags & State::CaptureFlags::kCancel))
	{
		LightingBeginCaptureCommand(rComparison.vkCommandBuffer);
		rComparison.reference.TransitionImageLayout(rComparison.vkCommandBuffer, TextureLayout::kGeneral, TextureLayout::kTransferDestination);
		++rComparison.referenceOperations.iBarriers;
		VkClearColorValue vkClearColor {};
		VkImageSubresourceRange vkRange {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1};
		vkCmdClearColorImage(rComparison.vkCommandBuffer, rComparison.reference.mVkImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &vkClearColor, 1, &vkRange);
		++rComparison.referenceOperations.iFills;
		rComparison.reference.TransitionImageLayout(rComparison.vkCommandBuffer, TextureLayout::kTransferDestination, TextureLayout::kShaderReadOnly);
		++rComparison.referenceOperations.iBarriers;
		VkMemoryBarrier vkReadback {.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER, .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT, .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT};
		vkCmdPipelineBarrier(rComparison.vkCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &vkReadback, 0, nullptr, 0, nullptr);
		++rComparison.referenceOperations.iBarriers;
		rComparison.eReferenceLayout = TextureLayout::kShaderReadOnly;
		LightingSubmitCaptureCommand(rComparison.vkCommandBuffer, rComparison.vkFence, *this);
		rComparison.manifest["zeroDirectComplete"] = true;
	}
	if (rComparison.iTotalTiles != 0 && rComparison.iCompletedTiles == rComparison.iTotalTiles && !(rComparison.flags & State::CaptureFlags::kCancel))
	{
		RecordCaptureStage("lastTile");
		LightingBeginCaptureCommand(rComparison.vkCommandBuffer);
		rComparison.reference.TransitionImageLayout(rComparison.vkCommandBuffer, TextureLayout::kComputeReadWrite, TextureLayout::kShaderReadOnly);
		++rComparison.referenceOperations.iBarriers;
		VkMemoryBarrier vkReadback {.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER, .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT, .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT};
		vkCmdPipelineBarrier(rComparison.vkCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &vkReadback, 0, nullptr, 0, nullptr);
		++rComparison.referenceOperations.iBarriers;
		rComparison.eReferenceLayout = TextureLayout::kShaderReadOnly;
		LightingSubmitCaptureCommand(rComparison.vkCommandBuffer, rComparison.vkFence, *this);
		if (!(rComparison.flags & State::CaptureFlags::kCancel))
		{
			gpTextureManager->mTextureDescriptors.SetLightingReference(mpState->iFramebuffer, &rComparison.reference, true);
			rComparison.flags.Set(State::CaptureFlags::kReferenceValid);
			rComparison.manifest["referenceComplete"] = true;
		}
	}
	rComparison.status = (rComparison.flags & State::CaptureFlags::kCancel) ? "cancelled" : "composing";
	VkSubmitInfo vkCompletion {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .signalSemaphoreCount = 1, .pSignalSemaphores = &vkPreparationFinished};
	CHECK_VK(vkQueueSubmit(gpDeviceManager->mGraphicsVkQueue, 1, &vkCompletion, VK_NULL_HANDLE));
	return true;
}

void Lighting::FinishCapture(int64_t iFramebuffer)
{
	// Heap: raw images and the common manifest are exported while all captured inputs remain parked.
	ScopedSuppressAllocationTracking suppress;
	State::Comparison& rComparison = *mpState->comparison;
	if (!IsCaptureHeld())
	{
		if (!std::cmp_equal(rComparison.uiResumePublication, 0))
		{
			VkFence vkResumedFence = gpCommandBufferManager->mPerFramebufferCommandBuffers.at(iFramebuffer).mVkFence;
			CHECK_VK(vkWaitForFences(gpDeviceManager->mVkDevice, 1, &vkResumedFence, VK_TRUE, kFenceTimeoutNanoseconds.count()));
			RecordCaptureStage("resumed");
			rComparison.manifest["stages"]["resumed"]["referenceValid"] = false;
			rComparison.manifest["stages"]["resumed"]["localDiffuse"] = 0;
			rComparison.uiResumePublication = 0;
			if (!rComparison.manifestHash.empty())
			{
				try
				{
					std::string serialized = rComparison.manifest.dump(2);
					nlohmann::json artifact = LightingWriteArtifact(rComparison.directory, "manifest.json", std::as_bytes(std::span(serialized.data(), serialized.size())));
					rComparison.manifestHash = artifact.at("sha256").get<std::string>();
				}
				catch (const std::exception& rException)
				{
					rComparison.error = rException.what();
					rComparison.status = "failed";
					rComparison.manifestHash.clear();
				}
			}
			WriteProfile(mpState->lastCaptureReport);
		}
		return;
	}
	VkFence vkFence = gpCommandBufferManager->mPerFramebufferCommandBuffers.at(iFramebuffer).mVkFence;
	LightingWaitFence(vkFence, *this);
	RecordCaptureStage("composed");
	if (!(rComparison.flags & State::CaptureFlags::kCancel))
	{
		rComparison.status = "exporting";
	}
	try
	{
		rComparison.directory = gpFileManager->mTempDirectory / "Lighting" / std::format("capture-{}-{}", rComparison.uiHeldPublication, rComparison.start.time_since_epoch().count());
		std::filesystem::create_directories(rComparison.directory);
		if (!(rComparison.flags & State::CaptureFlags::kCancel) && rComparison.manifest.contains("stages") && rComparison.manifest["stages"].contains("receiver"))
		{
			for (int64_t i = 0; i < 4; ++i)
			{
				std::vector<std::byte> bytes;
				Texture& rTexture = rComparison.receivers[i];
				TextureCache::CopyImageToHostMemory(rTexture.mVkImage, State::Comparison::kExtent, VK_FORMAT_R32G32B32A32_SFLOAT, 1, 1, false, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, bytes, &rComparison.exportReadback);
				++rComparison.exportOperations.iCopies;
				rComparison.exportOperations.iBarriers += 2;
				rComparison.manifest["receiver"][i] = LightingWriteArtifact(rComparison.directory, std::array<std::string_view, 4> {"position.rgba32f", "shading-normal.rgba32f", "geometric-normal.rgba32f", "albedo.rgba32f"}[i], bytes);
				if (i == 0)
				{
					std::vector<std::byte> valid(1'920 * 1'080);
					const float* pfPixels = reinterpret_cast<const float*>(bytes.data());
					for (int64_t iPixel = 0; iPixel < std::ssize(valid); ++iPixel)
					{
						valid.at(iPixel) = pfPixels[iPixel * 4 + 3] > 0.0f ? static_cast<std::byte>(1) : static_cast<std::byte>(0);
					}
					rComparison.manifest["validMask"] = LightingWriteArtifact(rComparison.directory, "valid.r8", valid);
				}
				LightingServiceCapture(*this);
			}
			if (rComparison.eReferenceLayout == TextureLayout::kShaderReadOnly)
			{
				std::vector<std::byte> bytes;
				TextureCache::CopyImageToHostMemory(rComparison.reference.mVkImage, State::Comparison::kExtent, VK_FORMAT_R32G32B32A32_SFLOAT, 1, 1, false, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, bytes);
				++rComparison.exportOperations.iCopies;
				rComparison.exportOperations.iBarriers += 2;
				rComparison.manifest["direct"] = LightingWriteArtifact(rComparison.directory, "direct.rgba32f", bytes);
			}
			if (!(rComparison.flags & State::CaptureFlags::kCancel))
			{
				rComparison.manifest["png"] = SaveFinishedPng(iFramebuffer, rComparison.directory / "finished.png");
				++rComparison.exportOperations.iCopies;
				rComparison.exportOperations.iBarriers += 2;
				rComparison.exportReadback.uiPayload = std::max(rComparison.exportReadback.uiPayload, rComparison.manifest["png"]["readbackPayload"].get<uint64_t>());
				rComparison.exportReadback.uiAllocated = std::max(rComparison.exportReadback.uiAllocated, rComparison.manifest["png"]["readbackAllocated"].get<uint64_t>());
			}
		}
		rComparison.status = (rComparison.flags & State::CaptureFlags::kCancel) ? "cancelled" : "exporting";
		WriteCaptureManifest();
		if (!(rComparison.flags & State::CaptureFlags::kCancel))
		{
			rComparison.manifest["exportComplete"] = true;
			RecordCaptureStage("exportComplete");
			rComparison.status = "complete";
		}
	}
	catch (const std::system_error& rException)
	{
		if (rException.code() == VkErrorCode(VK_ERROR_DEVICE_LOST))
		{
			throw;
		}
		rComparison.error = rException.what();
		rComparison.flags.Set(State::CaptureFlags::kCancel);
		rComparison.status = "failed";
	}
	catch (const std::exception& rException)
	{
		rComparison.error = rException.what();
		rComparison.flags.Set(State::CaptureFlags::kCancel);
		rComparison.status = "failed";
	}
	gpTextureManager->mTextureDescriptors.SetLightingReference(iFramebuffer, nullptr, false);
	rComparison.flags.Set(State::CaptureFlags::kReferenceValid, false);
	RecordCaptureStage("invalidated");
	if (rComparison.eReferenceLayout == TextureLayout::kShaderReadOnly)
	{
		LightingBeginCaptureCommand(rComparison.vkCommandBuffer);
		rComparison.reference.TransitionImageLayout(rComparison.vkCommandBuffer, TextureLayout::kShaderReadOnly, TextureLayout::kComputeReadWrite);
		++rComparison.referenceOperations.iBarriers;
		rComparison.eReferenceLayout = TextureLayout::kGeneral;
		LightingSubmitCaptureCommand(rComparison.vkCommandBuffer, rComparison.vkFence, *this);
	}
	rComparison.uiResumePublication = rComparison.uiHeldPublication + 1;
	ReleasePublication(iFramebuffer, rComparison.uiHeldPublication);
	rComparison.uiHeldPublication = 0;
	rComparison.manifest["releaseSequence"] = ++rComparison.uiSequence;
	rComparison.manifest["status"] = rComparison.status;
	rComparison.manifest["error"] = rComparison.error;
	rComparison.manifest["operations"]["reference"] = LightingOperationJson(rComparison.referenceOperations);
	rComparison.manifest["operations"]["export"] = LightingOperationJson(rComparison.exportOperations);
	rComparison.manifest["queuedMutation"] = (rComparison.flags & State::CaptureFlags::kMutationPending);
	rComparison.manifest["mutationOutcome"] = (rComparison.flags & State::CaptureFlags::kCancel) ? "cancelled before Main; mutation waits for release" : "mutation waits for export, invalidation and release";
	rComparison.manifestPath = common::ToString((rComparison.directory / "manifest.json").native());
	std::string serialized = rComparison.manifest.dump(2);
	try
	{
		nlohmann::json artifact = LightingWriteArtifact(rComparison.directory, "manifest.json", std::as_bytes(std::span(serialized.data(), serialized.size())));
		rComparison.manifestHash = artifact.at("sha256").get<std::string>();
	}
	catch (const std::exception& rException)
	{
		rComparison.error = rException.what();
		rComparison.status = "failed";
		rComparison.manifestHash.clear();
		rComparison.manifest["exportComplete"] = false;
	}
	WriteProfile(mpState->lastCaptureReport);
}

void Lighting::WriteProfile(nlohmann::json& rResult) const
{
	const LightingDiagnostics& rDiagnostics = mpState->completedDiagnostics;
	rResult = {{"publication", rDiagnostics.uiPublication}, {"generation", rDiagnostics.uiResourceGeneration}, {"framebuffer", rDiagnostics.iFramebuffer}, {"sources", rDiagnostics.iSources}, {"instances", rDiagnostics.iInstances}, {"triangles", rDiagnostics.iTriangles}, {"materials", rDiagnostics.iMaterials}, {"persistentBytes", rDiagnostics.uiPersistentBytes}, {"peakBytes", rDiagnostics.uiPeakBytes}, {"validationRequested", rDiagnostics.uiValidationRequested}, {"validationCompleted", rDiagnostics.uiValidationCompleted}, {"terrainWrittenOnce", rDiagnostics.iTerrainWrittenOnce}, {"terrainUntouched", rDiagnostics.iTerrainUntouched}, {"terrainDuplicate", rDiagnostics.iTerrainDuplicate}, {"terrainMismatches", rDiagnostics.iTerrainMismatches}, {"terrainMs", rDiagnostics.durationTerrain.count()}, {"worstTerrainTileMs", rDiagnostics.durationWorstTerrainTile.count()}, {"terrainValidationMs", rDiagnostics.durationTerrainValidation.count()}, {"sceneDiagnosticPublication", rDiagnostics.uiSceneDiagnosticPublication}, {"badSources", rDiagnostics.iBadSources}, {"badSamples", rDiagnostics.iBadSamples}, {"residentPolicy", rDiagnostics.residentPolicy}};
	if (mpState->comparison == nullptr)
	{
		rResult["capture"] = mpState->lastCaptureReport;
		return;
	}
	const State::Comparison& rComparison = *mpState->comparison;
	if (rComparison.status == "not-run" && !mpState->lastCaptureReport.empty())
	{
		rResult["capture"] = mpState->lastCaptureReport.at("capture");
		rResult["capture"]["resourcesRecreated"] = true;
		return;
	}
	rResult["capture"] = {{"request", rComparison.uiRequest}, {"status", rComparison.status}, {"heldPublication", rComparison.uiHeldPublication}, {"referenceValid", (rComparison.flags & State::CaptureFlags::kReferenceValid)}, {"completedTiles", rComparison.iCompletedTiles}, {"totalTiles", rComparison.iTotalTiles}, {"firstTileGpuMs", rComparison.fFirstTileGpuMs}, {"referenceWallMs", rComparison.durationReference.count()}, {"error", rComparison.error}, {"manifest", rComparison.manifestPath}, {"manifestSha256", rComparison.manifestHash}};
	rResult["capture"]["exportComplete"] = rComparison.status == "complete" && !rComparison.manifestHash.empty();
	rResult["capture"]["validationCompleted"] = rComparison.status == "complete" && rComparison.manifest.contains("terrain") ? rComparison.manifest["terrain"].value("completed", static_cast<uint64_t>(0)) : 0;
	rResult["capture"]["queuedMutation"] = (rComparison.flags & State::CaptureFlags::kMutationPending);
	rResult["captureRequested"] = mRequestedComparisonSettings.uiCaptureRequest;
	rResult["validationRequestedSetting"] = mRequestedComparisonSettings.uiValidationRequest;
}

static nlohmann::json LightingFileIdentity(const std::filesystem::path& rPath, Lighting& rLighting)
{
	std::future<FileContentDigest> future = std::async(std::launch::async, common::ThreadLocal::Entry([&rPath]()
	{
		// Heap: the file hashing worker owns its OS file and path buffers until completion.
		ScopedSuppressAllocationTracking suppress;
		FileContentDigest digest;
		if (!gpFileManager->ComputeOrdinaryFileSha256({FileFlags::kTempDirectory}, rPath, digest))
		{
			throw std::runtime_error("Lighting build/input file hash failed");
		}
		return digest;
	}, 64 * 1'024));
	while (future.wait_for(2ms) != std::future_status::ready) LightingServiceCapture(rLighting);
	FileContentDigest digest = future.get();
	std::string hash;
	for (uint8_t uiByte : digest.sha256) hash += std::format("{:02x}", uiByte);
	return {{"path", common::ToString(rPath.native())}, {"bytes", digest.iByteCount}, {"sha256", hash}};
}

void Lighting::WriteCaptureManifest()
{
	State::Comparison& rComparison = *mpState->comparison;
	LightingFramebuffer& rFramebuffer = *mpState->framebuffers.at(mpState->iFramebuffer);
	nlohmann::json& rManifest = rComparison.manifest;
	// TEMP START original native request capture
	const State::NativeCapture& rNative = mpState->nativeCapture;
	if (rNative.uiRequest != 0)
	{
		rManifest["nativeRequestCapture"] = {{"status", rNative.pcStatus}, {"request", rNative.uiRequest}, {"publication", rNative.uiPublication}, {"generation", rNative.uiGeneration}, {"framebuffer", rNative.iFramebuffer}, {"baselineCount", rNative.iBaselineCount}, {"finalCount", rNative.iFinalCount}, {"index", rNative.iBaselineCount}, {"beforeActive", rNative.iBeforeActive}, {"startActive", rNative.iStartActive}, {"endResult", rNative.iEndResult}, {"endActive", rNative.iEndActive}, {"discardResult", rNative.iDiscardResult}, {"discardActive", rNative.iDiscardActive}, {"sizingResult", rNative.iSizingResult}, {"readResult", rNative.iReadResult}, {"pathBytesIncludingNul", rNative.uiPathBytes}, {"readBytesIncludingNul", rNative.uiReadBytes}, {"active", (rNative.flags & State::NativeCapture::Flags::kActive)}, {"cleanupFailed", (rNative.flags & State::NativeCapture::Flags::kCleanupFailed)}, {"device", reinterpret_cast<uint64_t>(rNative.pDevice)}, {"window", reinterpret_cast<uint64_t>(rNative.pWindow)}, {"hashEvidence", "external capture-file SHA256 receipt required"}, {"eventIdentity", "requires replay event/resource mapping"}, };
		if (std::string_view(rNative.pcStatus) == "complete")
		{
			rManifest["nativeRequestCapture"]["path"] = rNative.path;
		}
	}
	// TEMP END original native request capture
	rManifest["rowLayout"] = {{"format", "VK_FORMAT_R32G32B32A32_SFLOAT"}, {"byteOrder", "little-endian"}, {"rowBytes", 1'920 * 16}, {"rowOrder", "increasing Vulkan image y"}, {"mask", "position.w > 0; valid.r8 is 1 for valid, 0 otherwise"}, {"direct", "linear reflected diffuse RGB before smoke, tone mapping and sRGB"}};
	rManifest["counts"] = {{"sources", rFramebuffer.pendingDiagnostics.iSources}, {"instances", rFramebuffer.pendingDiagnostics.iInstances}, {"triangles", rFramebuffer.pendingDiagnostics.iTriangles}, {"materials", rFramebuffer.pendingDiagnostics.iMaterials}, {"completedTiles", rComparison.iCompletedTiles}, {"totalTiles", rComparison.iTotalTiles}};
	rManifest["omissions"] = rFramebuffer.pendingDiagnostics.residentPolicy;
	rManifest["timings"] = {{"firstTileGpuMs", rComparison.fFirstTileGpuMs}, {"referenceWallMs", rComparison.durationReference.count()}, {"captureWallMs", std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - rComparison.start).count()}, {"projectedReferenceGpuMs", rComparison.fFirstTileGpuMs * rComparison.iTotalTiles}};
	rManifest["fixtureInputSha256"] = mpState->fixture->GetInputHash(mpState->settings, mpState->eScenario, mpState->settings.eCamera == LightingComparisonCamera::kEvent);
	for (int64_t i = 0; i < 6; ++i)
	{
		LightingFixtureBlob eBlob = static_cast<LightingFixtureBlob>(i);
		rManifest["fixtureBlobs"].push_back(LightingWriteArtifact(rComparison.directory, std::array<std::string_view, 6> {"fixture-sources.bin", "fixture-samples.bin", "fixture-geometry.bin", "fixture-cameras.bin", "fixture-events.bin", "fixture-animation.bin"}[i], mpState->fixture->GetBlob(eBlob)));
	}
	rManifest["publishedSources"] = LightingWriteArtifact(rComparison.directory, "published-sources.bin", std::as_bytes(std::span(reinterpret_cast<const shaders::LightingSource*>(rFramebuffer.sources.mpMappedMemory), static_cast<size_t>(rFramebuffer.pendingDiagnostics.iSources))));
	rManifest["publishedSamples"] = LightingWriteArtifact(rComparison.directory, "published-samples.bin", std::as_bytes(std::span(reinterpret_cast<const XMFLOAT2*>(rFramebuffer.samples.mpMappedMemory), static_cast<size_t>(mpState->settings.iAreaSamples))));
	const LightingTerrainCompletion& rTerrain = mpState->terrain.Complete(mpState->iFramebuffer);
		rManifest["terrain"] = {{"publication", rTerrain.uiPublication}, {"generation", rTerrain.uiResourceGeneration}, {"requested", rTerrain.validation.uiRequestedSerial}, {"completed", rTerrain.validation.uiCompletedSerial}, {"status", !std::cmp_equal(rTerrain.validation.uiCompletedSerial, 0) && rTerrain.validation.uiCompletedSerial == rTerrain.validation.uiRequestedSerial ? "completed" : "not-run"}, {"P", rTerrain.counts.iPlacements}, {"N", rTerrain.counts.iVertices}, {"A", rTerrain.counts.iTriangles}, {"T", rTerrain.counts.iTiles}, {"workgroups", rTerrain.counts.iWorkgroups}, {"productionMs", rTerrain.durationPreparation.count()}, {"worstTileMs", rTerrain.durationWorstTile.count()}, {"validationMs", rTerrain.durationValidation.count()}, {"productionOperations", LightingOperationJson(rTerrain.productionOperations)}, {"validationOperations", LightingOperationJson(rTerrain.validationOperations)}};
	rManifest["terrain"]["summaryArtifact"] = LightingWriteArtifact(rComparison.directory, "terrain-validation.bin", std::as_bytes(std::span(&rTerrain.validation, 1)));
	rManifest["terrain"]["tileArtifact"] = LightingWriteArtifact(rComparison.directory, "terrain-tiles.bin", std::as_bytes(rTerrain.tiles));
	// TEMP START bounded terrain evidence
	if (rTerrain.pEvidence != nullptr)
	{
		const shaders::LightingTerrainEvidence& rEvidence = *rTerrain.pEvidence;
		rManifest["terrain"]["evidence"] = {{"schema", 2}, {"artifact", LightingWriteArtifact(rComparison.directory, "terrain-evidence-v2.bin", std::as_bytes(std::span(&rEvidence, 1)))}, {"byteOrder", "little-endian"}, {"headerBytes", 152}, {"recordBytes", 512}, {"classCount", 18}, {"listCapacity", 136}, {"listCount", rEvidence.uiListCount}, {"overflow", rEvidence.uiOverflow}, {"targetWitnessMatched", rEvidence.puiClaims[16] == 2}, {"targetProductionMatched", rEvidence.puiClaims[17] == 2}};
		rManifest["terrain"]["evidence"]["classes"] = {"baseXYZ", "plusX-XYZ", "minusX-XYZ", "plusY-XYZ", "minusY-XYZ", "plusX-raw", "minusX-raw", "plusY-raw", "minusY-raw", "adjacent-raw", "adjacent-XYZ", "Global-raw", "original-XYZ", "original-own", "original-cutoff", "original-invalid", "target-witness", "target-production"};
		for (uint32_t uiClaim : rEvidence.puiClaims) rManifest["terrain"]["evidence"]["claims"].push_back(uiClaim);
		for (uint32_t uiCount : rEvidence.puiCounts) rManifest["terrain"]["evidence"]["counts"].push_back(uiCount);
		// TEMP START original operand acquisition
		bool bTargetIdentityMatched = true;
		for (int64_t iClass : {16, 17})
		{
			const shaders::LightingTerrainEvidenceRecord& rRecord = rEvidence.pClasses[iClass];
			bTargetIdentityMatched &= rEvidence.puiClaims[iClass] == 2 && rRecord.puiWords[0] == iClass - 16 && rRecord.puiWords[1] == 0 && rRecord.puiWords[2] == 24'356 && (static_cast<uint64_t>(rRecord.puiWords[4]) << 32 | rRecord.puiWords[3]) == 188'901'052'726'349'361ull;
		}
		rManifest["terrain"]["evidence"]["targetIdentityMatched"] = bTargetIdentityMatched;
		rManifest["terrain"]["evidence"]["targetAssociation"] = {{"framebuffer", mpState->iFramebuffer}, {"publication", rTerrain.uiPublication}, {"generation", rTerrain.uiResourceGeneration}, {"heldPublication", rComparison.uiHeldPublication}, {"recordedPublication", rFramebuffer.uiRecordedPublication}, {"request", rComparison.uiRequest}, {"requestedSerial", rTerrain.validation.uiRequestedSerial}, {"completedSerial", rTerrain.validation.uiCompletedSerial}, {"placement", 0}, {"workSlot", 0}, {"quadSlot", 0}, {"template", 188'901'052'726'349'361ull}, {"vertex", 24'356}};
		rManifest["terrain"]["evidence"]["targetWordSchema"] = {{"0:4", "role(witness=0,compute=1),placement,vertex,template low,high"}, {"5:11", "floatbits fetched localXY,rectXYWH,rotation"}, {"12:17", "floatbits fetched visibleArea,cos,sin"}, {"18:21", "floatbits half-extent products,centers"}, {"22:25", "floatbits localX*cos,localY*sin,localX*sin,localY*cos"}, {"26:29", "floatbits center-plus-localX partial sums,worldXY"}, {"30:37", "floatbits UV numerators,denominators,visibleUV,executed sampleUV"}, {"38:42", "floatbits pre-cutoff sampledZ,post-cutoff Z,own sampledZ,threshold,seaFloor"}, {"43:45", "cutoff,textureSlot,sample source(visible=0,scratch=1)"}, {"46:127", "zero"}};
		rManifest["terrain"]["evidence"]["fieldValidity"] = {{"classes0:15AndList", "wordSchema applies; mode0 words50:53 unused and56 not sampled; actual production beforeZ91/afterZ16, original beforeZ68/afterZ60"}, {"classes16:17", "targetWordSchema applies only for claim2; source-level intermediates require optimized SSA proof and paired control before numeric attribution"}, {"claims", "0 absent,1 incomplete,2 published; counts enumerate matching invocations, including witness original-claim losers; absent/incomplete is no sample"}};
		// TEMP END original operand acquisition
		rManifest["terrain"]["evidence"]["samplers"] = {{"compositeAndScratch", reinterpret_cast<uint64_t>(gpTextureManager->GetSampler(DescriptorFlags::kCombinedSamplers))}, {"own", reinterpret_cast<uint64_t>(gpTextureManager->GetSampler(DescriptorFlags::kSamplerElevation))}};
		rManifest["terrain"]["evidence"]["wordSchema"] = {{"0", "list flags: missing=1, candidate tail=2"}, {"1:4", "placement,vertex,vertexCount,remainder"}, {"5:6", "template low,high"}, {"7:11", "covered,written,invocations,cutoff,textureSlot"}, {"12:19", "floatbits localXY,xyz,own,threshold,seaFloor"}, {"20:25", "floatbits worldXY,visibleUV,pixelXY"}, {"26:38", "lowerXY,ownerOriginXY,originXY,shiftXY,visibleSizeXY,scratchSize,ownSizeXY"}, {"39:49", "floatbits rotation,quadRect,visibleArea,ownUV"}, {"50:57", "floatbits actual mode1 UV,beforeZ,afterZ,current sampleUV,beforeZ,afterZ"}, {"58:68", "original xyz/own floatbits,cutoff,written,UV/ownUV/beforeZ floatbits"}, {"69:76", "four base R16 bits then four other R16 bits; x changes fastest"}, {"77", "tap-valid masks: base bits0:3,other bits4:7"}, {"78:84", "failureTapXY,indexReferences,firstIndexOffset,mode,shiftIndex,otherIsGlobal"}, {"85:92", "floatbits actual production UV,ownUV,pixelXY,beforeZ,own"}, {"109:115", "four original-UV visible R16 taps,validMask,originalLowerXY"}, {"unused", "zero; signed coordinates use two's complement"}};
	}
	// TEMP END bounded terrain evidence
	rManifest["terrain"]["binarySchema"] = {{"summary", "Engine/Data/Shaders/Lighting/LightingTerrainCommon.h LightingTerrainValidation"}, {"summaryStride", sizeof(shaders::LightingTerrainValidation)}, {"tile", "LightingTerrainTileResult"}, {"tileStride", sizeof(shaders::LightingTerrainTileResult)}, {"byteOrder", "little-endian"}};
	const shaders::LightingTerrainValidation& rValidation = rTerrain.validation;
	rManifest["terrain"]["summary"] = {{"once", rValidation.uiOnce}, {"untouched", rValidation.uiUntouched}, {"duplicate", rValidation.uiDuplicate}, {"indices", rValidation.uiIndices}, {"invalidIndices", rValidation.uiInvalidIndices}, {"triangles", rValidation.uiTriangles}, {"nonfinite", rValidation.uiNonfinite}, {"overhang", rValidation.uiOverhang}, {"overhangBeyondFootprint", rValidation.uiOverhangBeyondFootprint}, {"seam", rValidation.uiSeam}, {"cutoffBelow", rValidation.uiCutoffBelow}, {"cutoffEqual", rValidation.uiCutoffEqual}, {"originalComparisons", rValidation.uiOriginalComparisons}, {"originalMissing", rValidation.uiOriginalMissing}, {"originalMismatches", rValidation.uiOriginalMismatches}, {"originalMaxBits", rValidation.uiOriginalMaxBits}, {"globalComparisons", rValidation.uiGlobalComparisons}, {"globalMismatches", rValidation.uiGlobalMismatches}, {"baseComparisons", rValidation.uiBaseComparisons}, {"baseMismatches", rValidation.uiBaseMismatches}, {"baseXYZMismatches", rValidation.uiBaseXYZMismatches}, {"tiles", rValidation.uiTiles}, {"emptyTiles", rValidation.uiEmptyTiles}, {"shifts", rValidation.uiShifts}, {"excludedTaps", rValidation.uiExcludedTaps}, {"adjacentRenders", rValidation.uiAdjacentRenders}, {"adjacentComparisons", rValidation.uiAdjacentComparisons}, {"adjacentMismatches", rValidation.uiAdjacentMismatches}, {"mappingMismatches", rValidation.uiMappingMismatches}};
	rManifest["terrain"]["firstFailure"] = {{"claim", rValidation.uiFirstClaim}, {"template", rValidation.uiFirstTemplate}, {"placement", rValidation.uiFirstPlacement}, {"vertex", rValidation.uiFirstVertex}, {"index", rValidation.uiFirstIndex}, {"local", {rValidation.f2FirstLocal.x, rValidation.f2FirstLocal.y}}, {"position", {rValidation.f3FirstPosition.x, rValidation.f3FirstPosition.y, rValidation.f3FirstPosition.z}}, {"own", rValidation.fFirstOwn}, {"pixel", {rValidation.f2FirstPixel.x, rValidation.f2FirstPixel.y}}, {"origin", {rValidation.iFirstOriginX, rValidation.iFirstOriginY}}, {"shift", rValidation.iFirstShift}, {"tap", {rValidation.iFirstTapX, rValidation.iFirstTapY}}, {"baseBits", rValidation.uiFirstBaseBits}, {"otherBits", rValidation.uiFirstOtherBits}, {"baseZ", rValidation.fFirstBaseZ}, {"otherZ", rValidation.fFirstOtherZ}, {"cutoff", rValidation.fFirstCutoff}};
	std::vector<LightingTerrainPlacementIdentity> placements(static_cast<size_t>(rFramebuffer.terrainCounts.iPlacements));
	mpState->terrain.WritePlacementIdentities(mpState->iFramebuffer, placements);
	for (const LightingTerrainPlacementIdentity& rPlacement : placements) rManifest["terrain"]["placements"].push_back({{"template", rPlacement.templateCrc}, {"placement", rPlacement.uiPlacement}, {"instance", rPlacement.uiInstance}, {"geometry", rPlacement.uiGeometry}, {"vertices", rPlacement.uiVertices}, {"triangles", rPlacement.uiTriangles}, {"xy", rPlacement.vkXY}, {"xyz", rPlacement.vkXYZ}, {"indices", rPlacement.vkIndices}, {"blas", reinterpret_cast<uint64_t>(rPlacement.vkBLAS)}, {"blasAddress", rPlacement.vkBLASAddress}, {"inclusiveTileBounds", {rPlacement.iMinTileX, rPlacement.iMinTileY, rPlacement.iMaxTileX, rPlacement.iMaxTileY}}});
	// TEMP START original operand acquisition
	for (const LightingTerrainPlacementIdentity& rPlacement : placements)
	{
		if (rTerrain.pEvidence == nullptr)
		{
			continue;
		}

		if (rPlacement.uiPlacement != 0)
		{
			continue;
		}

		if (rPlacement.templateCrc != 188'901'052'726'349'361ull)
		{
			continue;
		}

		rManifest["terrain"]["evidence"]["targetBindings"] = {{"provenance", "CPU binding metadata; operand values are shader records"}, {"xyAddress", rPlacement.vkXY}, {"xyStride", sizeof(XMFLOAT2)}, {"selectedXYAddress", rPlacement.vkXY + 24'356 * sizeof(XMFLOAT2)}, {"vertexBinding", 0}, {"vertexArenaBuffer", reinterpret_cast<uint64_t>(gpIslands->mIslandMeshArena.GetBuffer())}, {"vertexArenaAddress", gpIslands->mIslandMeshArena.GetDeviceAddress()}, {"vertexArenaOffset", rPlacement.vkXY - gpIslands->mIslandMeshArena.GetDeviceAddress()}, {"vertexCount", rPlacement.uiVertices}, {"witnessDraw", "vkCmdDraw(vertexCount,1,0,placement); POINT_LIST"}, {"computeInvocation", "push placement0; base+globalInvocationID.x=24356; visible initial pass or owning scratch tile"}, {"unobserved", "global uniform resource/range, work and quad resource addresses, image/view/mip resource identities and exact dispatch event require pipeline binding acquisition; not inferred from CPU values"}};
	}
	// TEMP END original operand acquisition
	if (mpState->completedDiagnostics.uiSceneDiagnosticPublication == rComparison.uiHeldPublication)
	{
		const shaders::LightingSceneDiagnosticResult& rScene = mpState->completedSceneDiagnostics;
		rManifest["sceneDiagnostics"] = {{"publication", rScene.uiPublication}, {"generation", rScene.uiGeneration}, {"checkedSources", rScene.uiCheckedSources}, {"badSources", rScene.uiBadSources}, {"checkedSamples", rScene.uiCheckedSamples}, {"badSamples", rScene.uiBadSamples}, {"artifact", LightingWriteArtifact(rComparison.directory, "scene-diagnostics.bin", std::as_bytes(std::span(&rScene, 1)))}, {"binarySchema", "Engine/Data/Shaders/Lighting/LightingSceneDiagnostics.h LightingSceneDiagnosticResult"}, {"stride", sizeof(rScene)}};
		for (const shaders::LightingSource& rSource : rScene.pSources) rManifest["sceneDiagnostics"]["sources"].push_back({{"center", {rSource.f3Center.x, rSource.f3Center.y, rSource.f3Center.z}}, {"range", rSource.fRange}, {"halfEdgeU", {rSource.f3HalfEdgeU.x, rSource.f3HalfEdgeU.y, rSource.f3HalfEdgeU.z}}, {"halfEdgeV", {rSource.f3HalfEdgeV.x, rSource.f3HalfEdgeV.y, rSource.f3HalfEdgeV.z}}, {"type", rSource.uiType}, {"power", {rSource.f3Power.x, rSource.f3Power.y, rSource.f3Power.z}}});
		for (const XMFLOAT2& rSample : rScene.pf2Samples) rManifest["sceneDiagnostics"]["samples"].push_back({rSample.x, rSample.y});
		for (const shaders::LightingSceneProbe& rProbe : rScene.pProbes) rManifest["sceneDiagnostics"]["probes"].push_back({{"hit", rProbe.uiHit}, {"instance", rProbe.hit.uiInstance}, {"primitive", rProbe.hit.uiPrimitive}, {"material", rProbe.hit.uiMaterial}, {"distance", rProbe.hit.fDistance}, {"position", {rProbe.hit.f3Position.x, rProbe.hit.f3Position.y, rProbe.hit.f3Position.z}}, {"geometricNormal", {rProbe.hit.f3GeometricNormal.x, rProbe.hit.f3GeometricNormal.y, rProbe.hit.f3GeometricNormal.z}}, {"shadingNormal", {rProbe.hit.f3ShadingNormal.x, rProbe.hit.f3ShadingNormal.y, rProbe.hit.f3ShadingNormal.z}}, {"albedo", {rProbe.hit.f3Albedo.x, rProbe.hit.f3Albedo.y, rProbe.hit.f3Albedo.z}}, {"radiance", {rProbe.f3Radiance.x, rProbe.f3Radiance.y, rProbe.f3Radiance.z}}, {"visible", rProbe.fVisible}});
	}
	else
	{
		rManifest["sceneDiagnostics"] = {{"status", "not-run"}};
	}
	rManifest["operations"] = {{"modelProduction", LightingOperationJson(rFramebuffer.modelOperations)}, {"sceneProduction", LightingOperationJson(rFramebuffer.sceneOperations)}, {"sceneDiagnostics", LightingOperationJson(rFramebuffer.diagnosticOperations)}, {"referenceDispatchesCompleted", rComparison.iCompletedTiles}, {"referenceDispatchesSubmitted", rComparison.iCompletedTiles}, {"referenceTimestampSlots", 2}, {"existingPreparation", "Main uniform copy, water displacement, smoke/wind emission, object shadows/blur and water foam; separate from lighting producer totals"}};
	rManifest["operations"]["receiver"] = LightingOperationJson(rComparison.receiverOperations);
	LightingTerrainBytes terrainBytes = mpState->terrain.GetResourceBytesDetail();
	LightingModelBytes modelBytes = mpState->models->GetResourceBytesDetail();
	uint64_t uiPayload = 0;
	uint64_t uiAllocated = 0;
	auto AddBytes = [&](std::string_view name, LightingAllocation bytes)
	{
		rManifest["allocations"][name] = {{"payload", bytes.uiPayload}, {"allocated", bytes.uiAllocated}};
		uiPayload += bytes.uiPayload;
		uiAllocated += bytes.uiAllocated;
	};
	for (const std::pair<std::string_view, LightingAllocation>& rAllocation : {std::pair<std::string_view, LightingAllocation>("terrainScratch", terrainBytes.scratch), {"terrainXYZ", terrainBytes.xyz}, {"terrainWork", terrainBytes.work}, {"terrainMaterials", terrainBytes.materials}, {"terrainBLAS", terrainBytes.blas}, {"terrainASScratch", terrainBytes.accelerationScratch}, {"terrainValidation", terrainBytes.validation}, {"modelMaterials", modelBytes.materials}, {"modelVertexMaterials", modelBytes.vertexMaterials}, {"modelXYZ", modelBytes.xyz}, {"modelBLAS", modelBytes.blas}, {"modelASScratch", modelBytes.accelerationScratch}}) AddBytes(rAllocation.first, rAllocation.second);
	LightingAllocation sceneBytes;
	LightingAllocation tlasBytes;
	LightingAllocation sceneScratch;
	LightingAllocation diagnosticBytes;
	auto Accumulate = [&](LightingAllocation& rBytes, const Buffer& rBuffer)
	{
		LightingAllocation added = GetBufferAllocation(rBuffer);
		rBytes.uiPayload += added.uiPayload;
		rBytes.uiAllocated += added.uiAllocated;
	};
	for (const std::unique_ptr<LightingFramebuffer>& rFrame : mpState->framebuffers)
	{
		for (const Buffer* pBuffer : {&rFrame->sources, &rFrame->samples, &rFrame->publication, &rFrame->instances, &rFrame->geometries, &rFrame->accelerationInstances, &rFrame->fixtureMaterials})
		{
			Accumulate(sceneBytes, *pBuffer);
		}
		Accumulate(tlasBytes, rFrame->scene.mStorage);
		Accumulate(sceneScratch, rFrame->scene.mScratch);
		Accumulate(diagnosticBytes, rFrame->diagnosticParameters);
		Accumulate(diagnosticBytes, rFrame->diagnosticResult);
		VmaAllocationInfo info {};
		vmaGetAllocationInfo(gpDeviceManager->mpAllocator, rFrame->diagnosticReadback->vmaAllocation, &info);
		diagnosticBytes.uiPayload += sizeof(shaders::LightingSceneDiagnosticResult);
		diagnosticBytes.uiAllocated += info.size;
	}
	AddBytes("sceneTables", sceneBytes);
	AddBytes("sceneTLAS", tlasBytes);
	AddBytes("sceneASScratch", sceneScratch);
	AddBytes("sceneDiagnostics", diagnosticBytes);
	LightingAllocation fixtureBytes;
	LightingAllocation fixtureBLAS;
	LightingAllocation fixtureScratch;
	for (const std::unique_ptr<State::FixtureSolid>& rSolid : mpState->fixtureSolids)
	{
		Accumulate(fixtureBytes, rSolid->vertices);
		Accumulate(fixtureBytes, rSolid->indices);
		Accumulate(fixtureBLAS, rSolid->acceleration.mStorage);
		Accumulate(fixtureScratch, rSolid->acceleration.mScratch);
	}
	AddBytes("fixtureGeometry", fixtureBytes);
	AddBytes("fixtureBLAS", fixtureBLAS);
	AddBytes("fixtureASScratch", fixtureScratch);
	for (const std::pair<std::string_view, const Texture*>& rTextureIdentity : {std::pair<std::string_view, const Texture*>("receiverPosition", &rComparison.receivers[0]), {"receiverShadingNormal", &rComparison.receivers[1]}, {"receiverGeometricNormal", &rComparison.receivers[2]}, {"receiverAlbedo", &rComparison.receivers[3]}, {"receiverDepth", &rComparison.depth}, {"referenceOutput", &rComparison.reference}, {"compositionFallback", &gpTextureManager->mTextureDescriptors.mLightingFallback}})
	{
		const std::string_view name = rTextureIdentity.first;
		const Texture* pTexture = rTextureIdentity.second;
		if (pTexture->mVmaAllocation == VK_NULL_HANDLE)
		{
			continue;
		}
		VmaAllocationInfo info {};
		vmaGetAllocationInfo(gpDeviceManager->mpAllocator, pTexture->mVmaAllocation, &info);
		AddBytes(name, {.uiPayload = static_cast<uint64_t>(common::SizeInBytes(pTexture->mInfo.vkFormat, pTexture->mInfo.vkExtent3D.width, pTexture->mInfo.vkExtent3D.height)), .uiAllocated = info.size});
		rManifest["imageResources"][name] = {{"image", reinterpret_cast<uint64_t>(pTexture->mVkImage)}, {"generation", pTexture->miGeneration}, {"format", pTexture->mInfo.vkFormat}, {"width", pTexture->mInfo.vkExtent3D.width}, {"height", pTexture->mInfo.vkExtent3D.height}};
	}
	LightingAllocation raster;
	auto AddPipeline = [&](const Pipeline& rPipeline)
	{
		LightingAllocation bytes = GetBufferAllocation(rPipeline.mModelMaterialsStorageBuffer);
		raster.uiPayload += bytes.uiPayload;
		raster.uiAllocated += bytes.uiAllocated;
		if (rPipeline.mIndirectVmaAllocation != VK_NULL_HANDLE)
		{
			VmaAllocationInfo info {};
			vmaGetAllocationInfo(gpDeviceManager->mpAllocator, rPipeline.mIndirectVmaAllocation, &info);
			raster.uiPayload += rPipeline.miIndirectSlotCount * sizeof(VkDrawIndexedIndirectCommand);
			raster.uiAllocated += info.size;
		}
	};
	AddPipeline(rComparison.water);
	if (rComparison.animationForward != nullptr)
	{
		for (const Pipeline& rPipeline : rComparison.animationForward->mpPipelines) AddPipeline(rPipeline);
	}
	for (const auto& [rKey, rModel] : rComparison.models)
	{
		std::ignore = rKey;
		for (const Pipeline& rPipeline : rModel->mpPipelines) AddPipeline(rPipeline);
	}
	AddBytes("receiverForwardMaterialsAndIndirect", raster);
	AddBytes("fixtureDraws", GetBufferAllocation(rComparison.solidDraws));
	for (int64_t i = 0; i < std::ssize(gpTextureManager->mTextureDescriptors.mLightingControls); ++i) AddBytes(std::format("compositionControl{}", i), GetBufferAllocation(gpTextureManager->mTextureDescriptors.mLightingControls.at(i)));
	rManifest["memory"] = {{"persistentPayload", uiPayload}, {"persistentAllocated", uiAllocated}, {"exportReadbackPayload", rComparison.exportReadback.uiPayload}, {"exportReadbackAllocated", rComparison.exportReadback.uiAllocated}, {"peakAllocated", uiAllocated + rComparison.exportReadback.uiAllocated}, {"querySlots", rComparison.vkQueryPool != VK_NULL_HANDLE ? 2 : 0}, {"driverCommandStorage", "not exposed; operation counts reported"}};
	mpState->completedDiagnostics.uiPersistentBytes = uiAllocated;
	mpState->completedDiagnostics.uiPeakBytes = std::max(mpState->completedDiagnostics.uiPeakBytes, uiAllocated + rComparison.exportReadback.uiAllocated);
	const VkPhysicalDeviceProperties& rVkPhysicalDeviceProperties = gpInstanceManager->mVkPhysicalDeviceProperties;
	rManifest["device"] = {{"name", rVkPhysicalDeviceProperties.deviceName}, {"vendor", rVkPhysicalDeviceProperties.vendorID}, {"device", rVkPhysicalDeviceProperties.deviceID}, {"driverVersion", rVkPhysicalDeviceProperties.driverVersion}, {"apiVersion", rVkPhysicalDeviceProperties.apiVersion}, {"rayQuery", gpDeviceManager->mEnabledVkPhysicalDeviceRayQueryFeaturesKHR.rayQuery}, {"accelerationStructure", gpDeviceManager->mEnabledVkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructure}, {"bufferDeviceAddress", gpDeviceManager->mEnabledVkPhysicalDeviceVulkan12Features.bufferDeviceAddress}, {"shaderInt64", gpDeviceManager->mEnabledVkPhysicalDeviceFeatures.shaderInt64}, {"scalarBlockLayout", gpDeviceManager->mEnabledVkPhysicalDeviceVulkan12Features.scalarBlockLayout}, {"vertexPipelineStoresAndAtomics", gpDeviceManager->mEnabledVkPhysicalDeviceFeatures.vertexPipelineStoresAndAtomics}};
	for (const char* pcExtension : gpDeviceManager->mEnabledExtensions) rManifest["device"]["extensions"].push_back(pcExtension);
	for (int64_t i = 0; i < TextureManager::kSamplerSlotCount; ++i)
	{
		const VkSamplerCreateInfo& rVkSamplerCreateInfo = gpTextureManager->mpSamplerInfosVkSamplerCreateInfo[i];
		rManifest["samplers"].push_back({{"slot", i}, {"handle", reinterpret_cast<uint64_t>(gpTextureManager->mpSamplersVkSampler[i])}, {"magFilter", rVkSamplerCreateInfo.magFilter}, {"minFilter", rVkSamplerCreateInfo.minFilter}, {"mipmapMode", rVkSamplerCreateInfo.mipmapMode}, {"addressModes", {rVkSamplerCreateInfo.addressModeU, rVkSamplerCreateInfo.addressModeV, rVkSamplerCreateInfo.addressModeW}}, {"mipBias", rVkSamplerCreateInfo.mipLodBias}, {"anisotropyEnabled", rVkSamplerCreateInfo.anisotropyEnable}, {"maxAnisotropy", rVkSamplerCreateInfo.maxAnisotropy}, {"minLod", rVkSamplerCreateInfo.minLod}, {"maxLod", rVkSamplerCreateInfo.maxLod}, {"borderColor", rVkSamplerCreateInfo.borderColor}, {"compareEnabled", rVkSamplerCreateInfo.compareEnable}, {"compareOp", rVkSamplerCreateInfo.compareOp}, {"unnormalizedCoordinates", rVkSamplerCreateInfo.unnormalizedCoordinates}});
	}
	static constexpr int64_t kiExecutableCapacity = 32'768;
	auto executableAllocation = common::gpThreadLocal->mWorkbuffer.PushBuffer<wchar_t*>(kiExecutableCapacity * static_cast<int64_t>(sizeof(wchar_t)));
	DWORD uiLength = GetModuleFileNameW(nullptr, executableAllocation.mpData, static_cast<DWORD>(kiExecutableCapacity));
	if (std::cmp_equal(uiLength, 0) || std::cmp_equal(uiLength, kiExecutableCapacity))
	{
		throw std::runtime_error("Lighting executable identity unavailable");
	}
	rManifest["build"]["executable"] = LightingFileIdentity(std::filesystem::path(executableAllocation.mpData), *this);
	rManifest["build"]["compiler"] = _MSC_FULL_VER;
	rManifest["build"]["compiled"] = __DATE__ " " __TIME__;
	rManifest["build"]["revisionEvidence"] = "Pair with the exact compile envelope and repository revision in the acceptance evidence";
	for (int64_t i = 0; i < data::kDataTypeCount; ++i)
	{
		std::filesystem::path pack = gpFileManager->mpPackChunks->GetDataFilePath(static_cast<data::DataTypes>(i), ".pack");
		if (std::filesystem::exists(pack))
		{
			rManifest["build"]["packs"].push_back(LightingFileIdentity(pack, *this));
		}
		std::filesystem::path packManifest = gpFileManager->mpPackChunks->GetDataFilePath(static_cast<data::DataTypes>(i), ".manifest");
		if (std::filesystem::exists(packManifest))
		{
			rManifest["build"]["packManifests"].push_back(LightingFileIdentity(packManifest, *this));
		}
	}
	const std::unordered_map<common::crc_t, EagerChunk>& rChunks = gpFileManager->mpPackChunks->GetEagerChunkMap();
	for (const auto& [uiCrc, rShader] : gpPipelineManager->mShaders)
	{
		std::ignore = rShader;
		const EagerChunk& rChunk = rChunks.at(uiCrc);
		rManifest["build"]["shaderChunks"].push_back({{"crc", uiCrc}, {"bytes", rChunk.iDataSize}, {"sha256", LightingHash(std::span(rChunk.pData, static_cast<size_t>(rChunk.iDataSize)))}});
	}
	if (rManifest.contains("png"))
	{
		rManifest["png"]["file"] = LightingFileIdentity(rComparison.directory / "finished.png", *this);
	}
}

} // namespace engine

#endif // defined(BT_CLIENT)
