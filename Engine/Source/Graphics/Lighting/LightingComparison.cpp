#if defined(BT_CLIENT)

#include "Data/Shader.h"
#include "File/PackChunks.h"
#include "Graphics/Objects/PipelineDescriptorWriter.h"
#include "LightingState.h"

namespace engine
{

static PipelineInfo LightingModelPipelineInfo(common::crc_t uiSceneCrc, common::crc_t uiBufferCrc, common::crc_t uiFragmentCrc)
{
	const common::SceneHeader& rScene = gpFileManager->mpPackChunks->GetEagerChunkMap().at(uiSceneCrc).pHeader->sceneHeader;
	return
	{
		.name = "LightingModel",
		.flags = {PipelineFlags::kIndirectHostVisible, PipelineFlags::kPushConstants, PipelineFlags::kDepthTest, PipelineFlags::kDepthWrite, PipelineFlags::kCullBack, PipelineFlags::kUpdateAfterBind, PipelineFlags::kMultiSet},
		.ppShaders = {&gpPipelineManager->mShaders.at(rScene.bHasAnimation ? data::kShadersModelModelSkinnedvertCrc : data::kShadersModelModelStaticvertCrc), &gpPipelineManager->mShaders.at(uiFragmentCrc)},
		.pVertexBuffer = &gpBufferManager->mModelMap.at(rScene.modelCrc),
		.descriptorInfos =
		{
			{.flags = DescriptorFlags::kGlobalLayoutUniformBuffers},
			{.flags = DescriptorFlags::kMainLayoutUniformBuffers},
			{.flags = DescriptorFlags::kPerCommandBufferStorageBuffers, .pBuffers = gpBufferManager->mpDynamicStorageBuffers[kBufferMain].at(uiBufferCrc).data()},
		},
	};
}

void Lighting::CreateComparisonResources()
{
	if (mpState->comparison != nullptr)
	{
		return;
	}
	mpState->comparison = std::make_unique<State::Comparison>();
	State::Comparison& rComparison = *mpState->comparison;
	if (!mpState->pendingResume.empty())
	{
		rComparison.manifest = std::move(mpState->pendingResume);
		rComparison.directory = std::move(mpState->pendingResumeDirectory);
		rComparison.uiResumePublication = 1;
		rComparison.uiSequence = rComparison.manifest.value("releaseSequence", static_cast<uint64_t>(0));
		rComparison.uiRequest = rComparison.manifest.value("request", static_cast<uint64_t>(0));
		rComparison.status = rComparison.manifest.value("status", std::string("complete"));
		rComparison.manifestPath = common::ToString((rComparison.directory / "manifest.json").native());
		rComparison.manifestHash = mpState->lastCaptureReport["capture"].value("manifestSha256", std::string());
	}
	PipelineInfo solid
	{
		.name = "LightingFixtureForward",
		.flags = {PipelineFlags::kDepthTest, PipelineFlags::kDepthWrite, PipelineFlags::kCullBack, PipelineFlags::kUpdateAfterBind},
		.ppShaders = {&gpPipelineManager->mShaders.at(data::kShadersLightingLightingFixturevertCrc), &gpPipelineManager->mShaders.at(data::kShadersLightingLightingFixturefragCrc)},
		.descriptorInfos =
		{
			{.flags = DescriptorFlags::kGlobalLayoutUniformBuffers, .iExplicitBinding = 0},
			{.flags = DescriptorFlags::kMainLayoutUniformBuffers, .iExplicitBinding = 1},
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerBorderWhite}, .iExplicitBinding = 2, .pTexture = &gpTextureManager->mRenderTargetTextures.mShadowBlurTexture},
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerBorderWhite}, .iExplicitBinding = 3, .pTexture = &gpTextureManager->mRenderTargetTextures.mObjectShadowsBlurTexture},
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerSmoke}, .iCount = shaders::kiMaxFlightPlanes, .iExplicitBinding = 4, .ppTextures = gpTextureManager->mRenderTargetTextures.mppSmokeTexturesOne},
			{.flags = DescriptorFlags::kStorageBuffer, .iExplicitBinding = shaders::kiLightingBindingInstances, .pBuffers = &mpState->framebuffers.front()->instances},
			{.flags = DescriptorFlags::kStorageBuffer, .iExplicitBinding = shaders::kiLightingBindingGeometry, .pBuffers = &mpState->framebuffers.front()->geometries},
			{.flags = DescriptorFlags::kStorageBuffer, .iExplicitBinding = shaders::kiLightingBindingMaterials, .pBuffers = &mpState->framebuffers.front()->fixtureMaterials},
		},
	};
	rComparison.solidForward.Create(solid);
	rComparison.solidDraws.mInfo = {.name = "LightingFixtureDraws", .flags = BufferFlags::kHostVisible, .iDataSize = std::ssize(mpState->framebuffers) * static_cast<int64_t>(sizeof(VkDrawIndirectCommand))};
	VmaAllocationInfo allocation;
	Buffer::CreateBuffer("LightingFixtureDraws", rComparison.solidDraws.mInfo.iDataSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, rComparison.solidDraws.mHostVisibleVkBuffer, rComparison.solidDraws.mHostVisibleVmaAllocation, &allocation);
	rComparison.solidDraws.mpMappedMemory = static_cast<char*>(allocation.pMappedData);
	std::memset(rComparison.solidDraws.mpMappedMemory, 0, static_cast<size_t>(rComparison.solidDraws.mInfo.iDataSize));
	if (mpState->fixture == nullptr)
	{
		mpState->fixture = std::make_unique<LightingFixture>(GridCoord {});
	}
	if (mpState->fixture->InitializeAnimation())
	{
		gpBufferManager->CreateDynamicBuffer(State::kFixtureAnimationBuffer, kBufferMain, "LightingFixtureAnimation", sizeof(shaders::ModelLayout), 1);
		rComparison.animationForward = std::make_unique<ModelPipeline>();
		rComparison.animationForward->Create(mpState->fixture->GetAnimation().sceneCrc, LightingModelPipelineInfo(mpState->fixture->GetAnimation().sceneCrc, State::kFixtureAnimationBuffer, data::kShadersLightingLightingNeutralModelfragCrc));
		for (int64_t i = 0; i < std::ssize(mpState->framebuffers); ++i) rComparison.animationForward->WriteIndirectBuffer(i, 0);
	}
}

void Lighting::DestroyComparisonResources()
{
	// TEMP START original native request capture
	mpState->nativeCapture.Abandon();
	// TEMP END original native request capture
	if (mpState->comparison == nullptr)
	{
		return;
	}
	State::Comparison& rComparison = *mpState->comparison;
	if (!std::cmp_equal(rComparison.uiHeldPublication, 0))
	{
		rComparison.flags.Set(State::CaptureFlags::kReferenceValid, false);
		rComparison.status = "aborted";
		rComparison.error = "Graphics recreation abandoned the drained capture";
		rComparison.manifest["exportComplete"] = false;
		rComparison.manifest["invalidationSequence"] = ++rComparison.uiSequence;
		ReleasePublication(mpState->iFramebuffer, rComparison.uiHeldPublication);
		rComparison.uiHeldPublication = 0;
		WriteProfile(mpState->lastCaptureReport);
	}
	for (int64_t i = 0; i < std::ssize(gpTextureManager->mTextureDescriptors.mGlobalDescriptorSets); ++i) gpTextureManager->mTextureDescriptors.SetLightingReference(i, nullptr, false);
	if (!std::cmp_equal(rComparison.uiResumePublication, 0))
	{
		mpState->pendingResume = std::move(rComparison.manifest);
		mpState->pendingResumeDirectory = std::move(rComparison.directory);
	}
	if (rComparison.vkFramebuffer != VK_NULL_HANDLE)
	{
		vkDestroyFramebuffer(gpDeviceManager->mVkDevice, rComparison.vkFramebuffer, nullptr);
	}
	if (rComparison.vkRenderPass != VK_NULL_HANDLE)
	{
		vkDestroyRenderPass(gpDeviceManager->mVkDevice, rComparison.vkRenderPass, nullptr);
	}
	if (rComparison.vkFence != VK_NULL_HANDLE)
	{
		vkDestroyFence(gpDeviceManager->mVkDevice, rComparison.vkFence, nullptr);
	}
	if (rComparison.vkQueryPool != VK_NULL_HANDLE)
	{
		vkDestroyQueryPool(gpDeviceManager->mVkDevice, rComparison.vkQueryPool, nullptr);
	}
	if (rComparison.vkCommandPool != VK_NULL_HANDLE)
	{
		vkDestroyCommandPool(gpDeviceManager->mVkDevice, rComparison.vkCommandPool, nullptr);
	}
	mpState->comparison.reset();
}

void Lighting::ApplyComparisonSettings()
{
	const LightingComparisonSettings& requested = mRequestedComparisonSettings;
	if (requested == mpState->settings)
	{
		return;
	}
	// TEMP START original native request capture
	mpState->nativeCapture.Abandon();
	// TEMP END original native request capture
	// Heap: fixture blobs and comparison pipelines persist across ordinary render publications.
	ScopedSuppressAllocationTracking suppress;
	if (gpCommandBufferManager == nullptr)
	{
		return;
	}
	gpCommandBufferManager->mSubmitGlobal.Wait();
	gpCommandBufferManager->mSubmitMain.Wait();
	gpSwapchainManager->mPresent.Wait();
	gpTextureUploadManager->WaitIdle();
	CHECK_VK(vkDeviceWaitIdle(gpDeviceManager->mVkDevice));
	if (requested.eMode != LightingComparisonMode::kDisabled && mpState->comparison->animationForward == nullptr)
	{
		throw std::runtime_error("Lighting fixture requires the packed Animation.001 scene");
	}
	if (requested.eMode != LightingComparisonMode::kDisabled && mpState->settings.eMode == LightingComparisonMode::kDisabled)
	{
		mpState->fixture = std::make_unique<LightingFixture>(gpCamera->mBasisCoordinate);
		if (!mpState->fixture->InitializeAnimation())
		{
			throw std::runtime_error("Lighting fixture animation initialization failed");
		}
	}
	mpState->settings = requested;
	mpState->eScenario = requested.eScenario;
	mpState->uiValidationRequest = requested.uiValidationRequest;
	// TEMP START original native request capture
	State::NativeCapture& rNative = mpState->nativeCapture;
	if (requested.eMode != LightingComparisonMode::kDisabled && requested.uiValidationRequest != 0 && requested.uiValidationRequest != mpState->uiServicedValidation && gpInstanceManager->mpRenderDocApi != nullptr)
	{
		if (rNative.flags & State::NativeCapture::Flags::kCleanupFailed)
		{
			throw std::runtime_error("Owned RenderDoc capture cleanup failed; further acquisition blocked");
		}
		rNative = {};
		rNative.uiRequest = requested.uiValidationRequest;
		rNative.pApi = gpInstanceManager->mpRenderDocApi;
		rNative.iBeforeActive = rNative.pApi->IsFrameCapturing();
		if (requested.uiCaptureRequest != 0 && requested.uiCaptureRequest != mpState->uiServicedCapture)
		{
			rNative.pcStatus = "reference-overlap";
		}
		else if (mpState->comparison->uiResumePublication != 0)
		{
			rNative.pcStatus = "resume-pending";
		}
		else if (rNative.iBeforeActive != 0)
		{
			rNative.pcStatus = "foreign-capture";
		}
		else
		{
			rNative.pDevice = RENDERDOC_DEVICEPOINTER_FROM_VKINSTANCE(gpInstanceManager->mVkInstance);
			rNative.pWindow = gpGraphics->mHwnd;
			rNative.iBaselineCount = rNative.pApi->GetNumCaptures();
			rNative.pApi->StartFrameCapture(rNative.pDevice, rNative.pWindow);
			rNative.iStartActive = rNative.pApi->IsFrameCapturing();
			rNative.flags.Set(State::NativeCapture::Flags::kActive, rNative.iStartActive != 0);
			rNative.pcStatus = rNative.iStartActive != 0 ? "active" : "start-failed";
		}
	}
	// TEMP END original native request capture
}

static void LightingReceiverInfo(PipelineInfo& rInfo, VkRenderPass vkPass)
{
	rInfo.name = "LightingReceiver";
	rInfo.flags.Set(PipelineFlags::kRenderTarget);
	rInfo.flags.Set(PipelineFlags::kAlphaBlend, false);
	rInfo.flags.Set(PipelineFlags::kSampleShading, false);
	rInfo.vkTargetRenderPass = vkPass;
	rInfo.vkExtent3D = {1'920, 1'080, 1};
	rInfo.iColorAttachmentCount = 4;
}

void Lighting::PrepareComparisonPipelines()
{
	State::Comparison& rComparison = *mpState->comparison;
	int64_t iFramebuffer = mpState->iFramebuffer;
	LightingFramebuffer& rFrame = *mpState->framebuffers.at(iFramebuffer);
	bool bActive = mpState->settings.eMode != LightingComparisonMode::kDisabled;
	VkDrawIndirectCommand& rVkDrawIndirectCommand = reinterpret_cast<VkDrawIndirectCommand*>(rComparison.solidDraws.mpMappedMemory)[iFramebuffer];
	rVkDrawIndirectCommand = {.vertexCount = 36, .instanceCount = bActive ? static_cast<uint32_t>(LightingFixture::kiSolidCount) : 0, .firstVertex = 0, .firstInstance = rFrame.uiFixtureFirstInstance};
	BindQueryDescriptors(rComparison.solidForward, iFramebuffer);
	if (rComparison.animationForward != nullptr)
	{
		rComparison.animationForward->WriteIndirectBuffer(iFramebuffer, bActive ? 1 : 0);
		rComparison.animationForward->UpdateStorageBufferDescriptors(iFramebuffer, shaders::kiModelBindingMeshData, &gpBufferManager->mMeshDataStorageBuffers.at(iFramebuffer));
		rComparison.animationForward->UpdateStorageBufferDescriptors(iFramebuffer, shaders::kiModelBindingJointMatrix, &gpBufferManager->mJointMatrixStorageBuffers.at(iFramebuffer));
	}
	if (!bActive || std::cmp_equal(mpState->settings.uiCaptureRequest, 0) || mpState->settings.uiCaptureRequest == mpState->uiServicedCapture)
	{
		return;
	}
	if (std::cmp_equal(gpInstanceManager->mVkQueueFamilyProperties.at(gpInstanceManager->miGraphicsQueueFamilyIndex).timestampValidBits, 0))
	{
		rComparison.status = "failed";
		rComparison.error = "Lighting reference requires graphics queue GPU timestamp support";
		mpState->uiServicedCapture = mpState->settings.uiCaptureRequest;
		return;
	}
	gpCommandBufferManager->mSubmitGlobal.Wait();
	if (rComparison.vkRenderPass == VK_NULL_HANDLE)
	{
		for (Texture& rTexture : rComparison.receivers) rTexture.Create({.name = "LightingReceiver", .vkFormat = VK_FORMAT_R32G32B32A32_SFLOAT, .vkExtent3D = State::Comparison::kExtent, .vkImageUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, .vkImageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT, .eTextureLayout = TextureLayout::kColorAttachment});
		rComparison.depth.Create({.name = "LightingReceiverDepth", .vkFormat = gpInstanceManager->mDepthVkFormat, .vkExtent3D = State::Comparison::kExtent, .vkImageUsageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, .vkImageAspectFlags = gpSwapchainManager->mDepthTexture.mInfo.vkImageAspectFlags, .eTextureLayout = TextureLayout::kUndefined});
		rComparison.reference.Create({.name = "LightingReference", .vkFormat = VK_FORMAT_R32G32B32A32_SFLOAT, .vkExtent3D = State::Comparison::kExtent, .vkImageUsageFlags = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, .vkImageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT, .eTextureLayout = TextureLayout::kGeneral});
		std::array<VkAttachmentDescription, 5> attachments {};
		std::array<VkAttachmentReference, 4> colors {};
		std::array<VkImageView, 5> views {};
		for (int64_t i = 0; i < 4; ++i)
		{
			attachments[i] = {.format = VK_FORMAT_R32G32B32A32_SFLOAT, .samples = VK_SAMPLE_COUNT_1_BIT, .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, .storeOp = VK_ATTACHMENT_STORE_OP_STORE, .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE, .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE, .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED, .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
			colors[i] = {.attachment = static_cast<uint32_t>(i), .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
			views[i] = rComparison.receivers[i].mVkImageView;
		}
		attachments[4] = {.format = gpInstanceManager->mDepthVkFormat, .samples = VK_SAMPLE_COUNT_1_BIT, .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE, .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE, .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE, .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED, .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
		views[4] = rComparison.depth.mVkImageView;
		VkAttachmentReference vkAttachmentReference {.attachment = 4, .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
		VkSubpassDescription vkSubpassDescription {.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS, .colorAttachmentCount = 4, .pColorAttachments = colors.data(), .pDepthStencilAttachment = &vkAttachmentReference};
		VkSubpassDependency pVkSubpassDependencies[]
		{
			{.srcSubpass = VK_SUBPASS_EXTERNAL, .dstSubpass = 0, .srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT},
			{.srcSubpass = 0, .dstSubpass = VK_SUBPASS_EXTERNAL, .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, .dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, .dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT},
		};
		VkRenderPassCreateInfo vkRenderPassCreateInfo {.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, .attachmentCount = 5, .pAttachments = attachments.data(), .subpassCount = 1, .pSubpasses = &vkSubpassDescription, .dependencyCount = 2, .pDependencies = pVkSubpassDependencies};
		CHECK_VK(vkCreateRenderPass(gpDeviceManager->mVkDevice, &vkRenderPassCreateInfo, nullptr, &rComparison.vkRenderPass));
		VkFramebufferCreateInfo vkFramebufferCreateInfo {.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, .renderPass = rComparison.vkRenderPass, .attachmentCount = 5, .pAttachments = views.data(), .width = 1'920, .height = 1'080, .layers = 1};
		CHECK_VK(vkCreateFramebuffer(gpDeviceManager->mVkDevice, &vkFramebufferCreateInfo, nullptr, &rComparison.vkFramebuffer));
		VkCommandPoolCreateInfo vkCommandPoolCreateInfo {.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, .queueFamilyIndex = static_cast<uint32_t>(gpInstanceManager->miGraphicsQueueFamilyIndex)};
		CHECK_VK(vkCreateCommandPool(gpDeviceManager->mVkDevice, &vkCommandPoolCreateInfo, nullptr, &rComparison.vkCommandPool));
		VkCommandBufferAllocateInfo vkCommandBufferAllocateInfo {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = rComparison.vkCommandPool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 1};
		CHECK_VK(vkAllocateCommandBuffers(gpDeviceManager->mVkDevice, &vkCommandBufferAllocateInfo, &rComparison.vkCommandBuffer));
		VkFenceCreateInfo vkFenceCreateInfo {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
		if (rComparison.vkFence == VK_NULL_HANDLE)
		{
			CHECK_VK(vkCreateFence(gpDeviceManager->mVkDevice, &vkFenceCreateInfo, nullptr, &rComparison.vkFence));
		}
		VkQueryPoolCreateInfo vkQueryPoolCreateInfo {.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, .queryType = VK_QUERY_TYPE_TIMESTAMP, .queryCount = 2};
		CHECK_VK(vkCreateQueryPool(gpDeviceManager->mVkDevice, &vkQueryPoolCreateInfo, nullptr, &rComparison.vkQueryPool));
		PipelineInfo terrain = gpPipelineManager->mpPipelines[kPipelineTerrain].mInfo;
		terrain.ppShaders[1] = &gpPipelineManager->mShaders.at(data::kShadersLightingLightingTerrainReceiverfragCrc);
		LightingReceiverInfo(terrain, rComparison.vkRenderPass);
		rComparison.terrain.Create(terrain);
		PipelineInfo water = gpPipelineManager->mpPipelines[kPipelineWater].mInfo;
		water.ppShaders[1] = &gpPipelineManager->mShaders.at(data::kShadersLightingLightingWaterReceiverfragCrc);
		LightingReceiverInfo(water, rComparison.vkRenderPass);
		rComparison.water.Create(water);
		PipelineInfo solid = rComparison.solidForward.mInfo;
		solid.ppShaders[1] = &gpPipelineManager->mShaders.at(data::kShadersLightingLightingFixtureReceiverfragCrc);
		LightingReceiverInfo(solid, rComparison.vkRenderPass);
		rComparison.solidReceiver.Create(solid);
	}
	rComparison.models.clear();
	for (const LightingModelRange& rRange : rFrame.modelRanges)
	{
		std::tuple<common::crc_t, common::crc_t, int64_t> key(rRange.modelCrc, rRange.instanceBufferCrc, rRange.iFirstInstance);
		if (!rComparison.models.contains(key))
		{
			auto model = std::make_unique<ModelPipeline>();
			PipelineInfo pipelineInfo = LightingModelPipelineInfo(rRange.modelCrc, rRange.instanceBufferCrc, rRange.instanceBufferCrc == State::kFixtureAnimationBuffer ? data::kShadersLightingLightingNeutralModelReceiverfragCrc : data::kShadersLightingLightingModelReceiverfragCrc);
			LightingReceiverInfo(pipelineInfo, rComparison.vkRenderPass);
			model->Create(rRange.modelCrc, pipelineInfo);
			rComparison.models.emplace(key, std::move(model));
		}
		ModelPipeline& rModel = *rComparison.models.at(key);
		rModel.UpdateStorageBufferDescriptors(iFramebuffer, 2, &gpBufferManager->mpDynamicStorageBuffers[kBufferMain].at(rRange.instanceBufferCrc).at(iFramebuffer));
		if (PipelineDescriptorWriter::BindingExistsInShaderLayout(rModel.mpPipelines.front(), shaders::kiModelBindingMeshData))
		{
			rModel.UpdateStorageBufferDescriptors(iFramebuffer, shaders::kiModelBindingMeshData, &gpBufferManager->mMeshDataStorageBuffers.at(iFramebuffer));
		}
		if (PipelineDescriptorWriter::BindingExistsInShaderLayout(rModel.mpPipelines.front(), shaders::kiModelBindingJointMatrix))
		{
			rModel.UpdateStorageBufferDescriptors(iFramebuffer, shaders::kiModelBindingJointMatrix, &gpBufferManager->mJointMatrixStorageBuffers.at(iFramebuffer));
		}
	}
	PipelineInfo compute {.name = "LightingReference", .flags = {PipelineFlags::kCompute, PipelineFlags::kPushConstants}, .ppShaders = {&gpPipelineManager->mShaders.at(data::kShadersLightingLightingReferencecompCrc)}};
	AddQueryDescriptorInfos(compute, iFramebuffer);
	compute.descriptorInfos.push_back({.flags = DescriptorFlags::kStorageImages, .iExplicitBinding = 2, .pTexture = &rComparison.reference});
	for (int64_t i = 0; i < 4; ++i) compute.descriptorInfos.push_back({.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerClamp}, .iExplicitBinding = std::array<int64_t, 4> {18, 19, 27, 28}[i], .pTexture = &rComparison.receivers[i]});
	rComparison.compute.Create(compute);
	BindQueryDescriptors(rComparison.compute, iFramebuffer);
	BindQueryDescriptors(rComparison.solidReceiver, iFramebuffer);
}

void Lighting::RecordFixtureDraws(VkCommandBuffer vkCommandBuffer, int64_t iFramebuffer)
{
	State::Comparison& rComparison = *mpState->comparison;
	rComparison.solidForward.RecordBindPipelineAndDescriptors(iFramebuffer, vkCommandBuffer);
	vkCmdDrawIndirect(vkCommandBuffer, rComparison.solidDraws.GetBuffer(), static_cast<VkDeviceSize>(iFramebuffer) * sizeof(VkDrawIndirectCommand), 1, sizeof(VkDrawIndirectCommand));
	if (rComparison.animationForward != nullptr)
	{
		rComparison.animationForward->RecordDrawIndirect(iFramebuffer, vkCommandBuffer, {}, ModelDrawPass::kOpaque);
	}
}

void Lighting::RecordReceivers(VkCommandBuffer vkCommandBuffer)
{
	State::Comparison& rComparison = *mpState->comparison;
	int64_t iFramebuffer = mpState->iFramebuffer;
	std::array<VkClearValue, 5> clears {};
	clears[4].depthStencil.depth = kfMaxDepth;
	VkRenderPassBeginInfo vkRenderPassBeginInfo {.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, .renderPass = rComparison.vkRenderPass, .framebuffer = rComparison.vkFramebuffer, .renderArea = {.extent = {1'920, 1'080}}, .clearValueCount = 5, .pClearValues = clears.data()};
	vkCmdBeginRenderPass(vkCommandBuffer, &vkRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
	rComparison.receiverOperations = {};
	++rComparison.receiverOperations.iRenderPasses;
	for (const LightingModelRange& rRange : mpState->framebuffers.at(iFramebuffer)->modelRanges)
	{
		ModelPipeline& rModel = *rComparison.models.at(std::tuple(rRange.modelCrc, rRange.instanceBufferCrc, rRange.iFirstInstance));
		rModel.WriteIndirectBuffer(iFramebuffer, rRange.iInstanceCount);
		for (Pipeline& rPipeline : rModel.mpPipelines) rPipeline.mpIndirectVkDrawIndexedIndirectCommand[iFramebuffer].firstInstance = static_cast<uint32_t>(rRange.iFirstInstance);
		rModel.RecordDrawIndirect(iFramebuffer, vkCommandBuffer, {}, ModelDrawPass::kOpaque);
		for (bool bTransparent : rModel.mpbTransparentMaterials)
		{
			if (!bTransparent)
			{
				++rComparison.receiverOperations.iDraws;
			}
		}
	}
	rComparison.terrain.RecordBindPipelineAndDescriptors(iFramebuffer, vkCommandBuffer);
	vkCmdBindIndexBuffer(vkCommandBuffer, gpIslands->mIslandMeshArena.mDeviceLocalVkBuffer, 0, VK_INDEX_TYPE_UINT32);
	VkDeviceSize vkOffset = 0;
	vkCmdBindVertexBuffers(vkCommandBuffer, 0, 1, &gpIslands->mIslandMeshArena.mDeviceLocalVkBuffer, &vkOffset);
	for (int64_t i = 0; i < gpIslands->miTemplateCount; ++i)
	{
		vkCmdDrawIndexedIndirect(vkCommandBuffer, gpIslands->mIslandsIndirectVkBuffers.at(iFramebuffer), static_cast<VkDeviceSize>(i) * sizeof(VkDrawIndexedIndirectCommand), 1, sizeof(VkDrawIndexedIndirectCommand));
		++rComparison.receiverOperations.iDraws;
	}
	std::memcpy(&rComparison.water.mpIndirectVkDrawIndexedIndirectCommand[iFramebuffer], &gpPipelineManager->mpPipelines[kPipelineWater].mpIndirectVkDrawIndexedIndirectCommand[iFramebuffer], sizeof(VkDrawIndexedIndirectCommand));
	rComparison.water.RecordDrawIndirect(iFramebuffer, vkCommandBuffer);
	++rComparison.receiverOperations.iDraws;
	rComparison.solidReceiver.RecordBindPipelineAndDescriptors(iFramebuffer, vkCommandBuffer);
	vkCmdDraw(vkCommandBuffer, 36, LightingFixture::kiSolidCount, 0, mpState->framebuffers.at(iFramebuffer)->uiFixtureFirstInstance);
	++rComparison.receiverOperations.iDraws;
	vkCmdEndRenderPass(vkCommandBuffer);
}

} // namespace engine

#endif // defined(BT_CLIENT)
