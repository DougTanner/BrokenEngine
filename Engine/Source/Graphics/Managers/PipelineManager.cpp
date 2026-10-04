#if defined(BT_CLIENT)

#include "Graphics/Managers/PipelineManager.h"

#include "Data/Shader.h"
#include "Data/Texture.h"
#include "File/PackChunks.h"

namespace engine
{

PipelineManager::PipelineManager()
{
	ASSERT(gpPipelineManager == nullptr);

	gpPipelineManager = this;

	ScopedBootTimer scopedBootTimer(kBootTimerPipelineManager);

	const std::unordered_map<common::crc_t, EagerChunk>& rChunkMap = gpFileManager->mpPackChunks->GetEagerChunkMap();
	for (const auto& [rCrc, rChunk] : rChunkMap)
	{
		if (!(rChunk.pHeader->flags & common::ChunkFlags::kShader))
		{
			continue;
		}

		if constexpr (!kbDebugPrintf)
		{
			if (std::strcmp(rChunk.pHeader->pcPath, "Shaders\\Log.vert") == 0)
			{
				continue;
			}
		}

		const common::ShaderHeader& rShaderHeader = rChunk.pHeader->shaderHeader;

		// Trust boundary: the descriptor-binding / vertex-attribute counts come from on-disk pack bytes and drive
		// reinterpret_cast offsets + indexed walks aliasing the eager shader chunk. A negative or oversized count
		// would walk the alias pointers off the chunk, so reject against the structural maxima before the sizes are
		// computed. Boot-required shader; a throw propagates to MainThread's try/catch (HandleException — crash
		// report + exit) — boot hard-fail.
		if (rShaderHeader.iDescriptorSetLayoutBindings < 0
		 || rShaderHeader.iDescriptorSetLayoutBindings > common::ShaderHeader::kiMaxDescriptorSetLayoutBindings
		 || rShaderHeader.iVertexInputAttributeDescriptions < 0
		 || rShaderHeader.iVertexInputAttributeDescriptions > common::ShaderHeader::kiMaxVertexInputAttributeDescriptions)
		{
			char pcHex[20] {};
			LOG(kLoading, kError, "Corrupt shader chunk {}: implausible binding/attribute counts {} / {}", common::ToHex(std::span(pcHex), rCrc), rShaderHeader.iDescriptorSetLayoutBindings, rShaderHeader.iVertexInputAttributeDescriptions);
			throw std::ios_base::failure("PipelineManager shader");
		}

		int64_t iSetIndicesOffset = common::ShaderHeader::SetIndicesOffset(rShaderHeader.iDescriptorSetLayoutBindings);
		int64_t iAttributesOffset = common::ShaderHeader::AttributesOffset(rShaderHeader.iDescriptorSetLayoutBindings);
		int64_t iSpirvOffset = common::ShaderHeader::SpirvOffset(rShaderHeader.iDescriptorSetLayoutBindings, rShaderHeader.iVertexInputAttributeDescriptions);

		// Trust boundary (chunk bytes): the three aliased sections plus the SPIR-V tail (>= the 4-byte magic the
		// Shader ctor reads) must fit the chunk's actual bytes, else the alias walks / iSpirvSize run off the buffer.
		if (iSpirvOffset + static_cast<int64_t>(sizeof(uint32_t)) > rChunk.pHeader->iSize)
		{
			char pcHex[20] {};
			LOG(kLoading, kError, "Corrupt shader chunk {}: section extent exceeds chunk bytes", common::ToHex(std::span(pcHex), rCrc));
			throw std::ios_base::failure("PipelineManager shader");
		}

		ShaderInfo info
		{
			.pChunkHeader = rChunk.pHeader,
			.pVkDescriptorBindings = reinterpret_cast<const VkDescriptorSetLayoutBinding*>(rChunk.pData),
			.puiDescriptorSetIndices = reinterpret_cast<const uint32_t*>(rChunk.pData + iSetIndicesOffset),
			.pVkVertexAttributes = reinterpret_cast<const VkVertexInputAttributeDescription*>(rChunk.pData + iAttributesOffset),
			.iSpirvSize = rChunk.pHeader->iSize - iSpirvOffset,
		};
		auto [it, bInserted] = mShaders.try_emplace(rCrc, info, rChunk.pData + iSpirvOffset);
		ASSERT(bInserted);
	}

	// Generate BRDF LUT texture before creating model pipelines that reference it
	gpTextureManager->mTextureCache.GeneratePhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionLookupTable();

	// Clear stale pipeline pointers before pipelines are recreated
	gpTextureManager->mTextureDescriptors.ClearTextureBindings();

	mWorldLightingShadowPipelines.CreateLightingPipelines();
	mWorldLightingShadowPipelines.CreateLightingBlurPipelines();
	mWorldLightingShadowPipelines.CreatePipelineShadows();
	mWorldLightingShadowPipelines.CreateLightingShadowDependentPipelines();

	if constexpr (kbDebugPrintf)
	{
		mpPipelines[kPipelineLog].Create(
		{
			.name = "Log",
			.flags = {PipelineFlags::kRenderTarget, PipelineFlags::kPushConstants},
			.ppShaders = {&mShaders.at(data::kShadersLogvertCrc), &mShaders.at(data::kShadersClearfragCrc)},
			.pVertexBuffer = &gpBufferManager->mQuadsVertexBuffer,
			.vkTargetRenderPass = gpTextureManager->mRenderTargetTextures.mLogTexture.mVkRenderPass,
			.vkExtent3D = gpTextureManager->mRenderTargetTextures.mLogTexture.mInfo.vkExtent3D,
			.descriptorInfos =
			{
				{.flags = DescriptorFlags::kGlobalLayoutUniformBuffers},
				{.flags = DescriptorFlags::kMainLayoutUniformBuffers},
			},
		});
	}

	CreateTerrainDataPipelines();

	mpPipelines[kPipelineUiDepthPrepass].Create(
	{
		.name = "UiDepthPrepass",
		.flags = {PipelineFlags::kDepthTest, PipelineFlags::kDepthWrite, PipelineFlags::kNoColorWrite, PipelineFlags::kNoWireframe},
		.ppShaders = {&mShaders.at(data::kShadersUiUiDepthPrepassvertCrc), &mShaders.at(data::kShadersUiUiDepthPrepassfragCrc)},
		.pVertexBuffer = nullptr,
		.descriptorInfos =
		{
			{.flags = DescriptorFlags::kPerCommandBufferStorageBuffers, .pBuffers = gpBufferManager->mUiRectangleStorageBuffers.data()},
		},
	});

	if constexpr (kbDebugInput)
	{
		RenderTargetTextures& rTextures = gpTextureManager->mRenderTargetTextures;
		mpPipelines[kPipelineDebugTexture].Create(
		{
			.name = "DebugTexture",
			.flags = {PipelineFlags::kNoWireframe},
			.ppShaders = {&mShaders.at(data::kShadersQuadsQuadsFullscreenvertCrc), &mShaders.at(data::kShadersDebugTexturefragCrc)},
			.pVertexBuffer = &gpBufferManager->mQuadsVertexBuffer,
			.descriptorInfos =
			{
				{.flags = DescriptorFlags::kGlobalLayoutUniformBuffers},
				{.flags = DescriptorFlags::kCombinedSamplers, .iCount = shaders::kiMaxDebugTextures, .ppTextures = rTextures.mppDebugTextures},
				{.flags = DescriptorFlags::kCombinedSamplers, .iCount = 3, .ppTextures = rTextures.mppLightingDepositTextures},
				{.flags = DescriptorFlags::kCombinedSamplers, .iCount = shaders::kiMaxDebugTextures, .ppTextures = rTextures.mppDebugTexturesB},
				{.flags = DescriptorFlags::kCombinedSamplers, .iCount = shaders::kiMaxDebugTextures, .ppTextures = rTextures.mppDebugTexturesC},
			},
		});
	}

	CreateSmokeWindPipelines();

	CreateParticlePipelines();

	// HDR resolve: fullscreen quad sampling the F16 scene intermediate, tone-mapping + color-grading into the
	// swapchain (mVkRenderPass). kRenderTarget forces samples=1 to match the single-sample present pass.
	mpPipelines[kPipelineHdrResolve].Create(
	{
		.name = "HdrResolve",
		.flags = {PipelineFlags::kRenderTarget, PipelineFlags::kNoWireframe},
		.ppShaders = {&mShaders.at(data::kShadersQuadsQuadsFullscreenvertCrc), &mShaders.at(data::kShadersHdrResolvefragCrc)},
		.pVertexBuffer = &gpBufferManager->mQuadsVertexBuffer,
		.vkTargetRenderPass = gpSwapchainManager->mVkRenderPass,
		.vkExtent3D = gpSwapchainManager->mHdrTexture.mInfo.vkExtent3D,
		.descriptorInfos =
		{
			{.flags = DescriptorFlags::kGlobalLayoutUniformBuffers},
			{.flags = DescriptorFlags::kMainLayoutUniformBuffers},
			{.flags = DescriptorFlags::kCombinedSamplers, .pTexture = &gpSwapchainManager->mHdrTexture},
		},
	});

	CreateDebugRenderPipelines();

	game::FrameInterpolate::GraphicsResources();
}

PipelineManager::~PipelineManager()
{
	if (gpPipelineManager == this)
	{
		gpPipelineManager = nullptr;
	}
}

void PipelineManager::CreateTerrainDataPipelines()
{
	mpPipelines[kPipelineTerrainElevation].Create(
	{
		.name = "TerrainElevation",
		// kMax: see kPipelineShadowElevation — MAX-blend overlapping islands' heightmaps so the tallest
		// terrain wins per pixel. RTT clears to mfSeaFloorElevation, so single-island pixels are unchanged.
		.flags = {PipelineFlags::kRenderTarget, PipelineFlags::kPushConstants, PipelineFlags::kMax, PipelineFlags::kUpdateAfterBind},
		.ppShaders = {&mShaders.at(data::kShadersQuadsQuadsAxisAlignedVisibleAreavertCrc), &mShaders.at(data::kShadersTerrainTerrainElevationfragCrc)},
		.pVertexBuffer = &gpBufferManager->mQuadsVertexBuffer,
		.vkTargetRenderPass = gpTextureManager->mRenderTargetTextures.mTerrainElevationTexture.mVkRenderPass,
		.vkExtent3D = gpTextureManager->mRenderTargetTextures.mTerrainElevationTexture.mInfo.vkExtent3D,
		.descriptorInfos =
		{
			{.flags = DescriptorFlags::kGlobalLayoutUniformBuffers},
			{.flags = DescriptorFlags::kPerCommandBufferStorageBuffers, .pBuffers = gpIslands->mIslandsStorageBuffers.data()},
			// The unflagged default keeps this offscreen elevation prepass linear without player-facing anisotropy.
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kBindlessArrayConsumer}, .iCount = shaders::kiMaxIslands, .ppTextures = gpTextureManager->mRenderTargetTextures.mElevationTextures.data()}, // set=1 binding 2 (elevation, kPipelineTerrainElevation)
		},
	});
}

void PipelineManager::CreateSmokeWindPipelines()
{
	struct SmokeClearPipelineDescription
	{
		Pipelines ePipeline;
		std::string_view name;
		Texture* pTargetTexture = nullptr;
	};
	SmokeClearPipelineDescription pSmokeClearPipelineDescriptions[]
	{
		{.ePipeline = kPipelineSmokeClearA, .name = "SmokeClearA", .pTargetTexture = &gpTextureManager->mRenderTargetTextures.mSmokeTextureOne},
		{.ePipeline = kPipelineSmokeClearB, .name = "SmokeClearB", .pTargetTexture = &gpTextureManager->mRenderTargetTextures.mSmokeTextureTwo},
	};
	for (const SmokeClearPipelineDescription& rDescription : pSmokeClearPipelineDescriptions)
	{
		mpPipelines[rDescription.ePipeline].Create(
		{
			.name = rDescription.name,
			.flags = {PipelineFlags::kRenderTarget, PipelineFlags::kPushConstants, PipelineFlags::kIndirectHostVisible},
			.ppShaders = {&mShaders.at(data::kShadersQuadsQuadsFullscreenvertCrc), &mShaders.at(data::kShadersClearfragCrc)},
			.pVertexBuffer = &gpBufferManager->mQuadsVertexBuffer,
			.vkTargetRenderPass = rDescription.pTargetTexture->mVkRenderPass,
			.vkExtent3D = rDescription.pTargetTexture->mInfo.vkExtent3D,
			.descriptorInfos =
			{
				{.flags = DescriptorFlags::kGlobalLayoutUniformBuffers},
			},
		});
	}

	gpBufferManager->CreateSmokeHierarchicalBuffers();

	struct SmokeOccupancyPipelineDescription
	{
		Pipelines ePipeline;
		std::string_view name;
		common::crc_t shaderCrc;
		VkBuffer* pVkSourceOccupancyBuffer = nullptr;
		VkBuffer* pVkActiveTileBuffer = nullptr;
		VkBuffer* pVkDestinationOccupancyBuffer = nullptr;
	};
	SmokeOccupancyPipelineDescription pSmokeOccupancyPipelineDescriptions[]
	{
		{.ePipeline = kPipelineSmokeOccupancyDilate, .name = "SmokeOccupancyDilate", .shaderCrc = data::kShadersSmokeSmokeOccupancyDilatecompCrc, .pVkSourceOccupancyBuffer = &gpBufferManager->mSmokeOccupancyVkBuffers[0], .pVkActiveTileBuffer = &gpBufferManager->mSmokeActiveTileVkBuffer, .pVkDestinationOccupancyBuffer = &gpBufferManager->mSmokeOccupancyVkBuffers[1]},
		{.ePipeline = kPipelineSmokeOccupancyDilateRemap, .name = "SmokeOccupancyDilateRemap", .shaderCrc = data::kShadersSmokeSmokeOccupancyDilateRemapcompCrc, .pVkSourceOccupancyBuffer = &gpBufferManager->mSmokeOccupancyVkBuffers[1], .pVkActiveTileBuffer = &gpBufferManager->mSmokeActiveTileVkBuffer, .pVkDestinationOccupancyBuffer = &gpBufferManager->mSmokeOccupancyVkBuffers[0]},
	};
	for (const SmokeOccupancyPipelineDescription& rDescription : pSmokeOccupancyPipelineDescriptions)
	{
		mpPipelines[rDescription.ePipeline].Create(
		{
			.name = rDescription.name,
			.flags = {PipelineFlags::kCompute},
			.ppShaders = {&mShaders.at(rDescription.shaderCrc)},
			.descriptorInfos =
			{
				{.flags = DescriptorFlags::kGlobalLayoutUniformBuffers},
				{.flags = DescriptorFlags::kStorageBuffer, .pVkBuffers = rDescription.pVkSourceOccupancyBuffer},
				{.flags = DescriptorFlags::kStorageBuffer, .pVkBuffers = rDescription.pVkActiveTileBuffer},
				{.flags = DescriptorFlags::kStorageBuffer, .pVkBuffers = rDescription.pVkDestinationOccupancyBuffer},
			},
		});
	}

	mpPipelines[kPipelineSmokeSpreadComputeB].Create(
	{
		.name = "SmokeSpreadComputeB",
		.flags = {PipelineFlags::kCompute, PipelineFlags::kUpdateAfterBind},
		.ppShaders = {&mShaders.at(data::kShadersSmokeSmokeSpreadTwocompCrc)},
		.descriptorInfos =
		{
			{.flags = DescriptorFlags::kGlobalLayoutUniformBuffers},
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerSmoke}, .pTexture = &gpTextureManager->mRenderTargetTextures.mSmokeTextureOne},
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerMirroredRepeatLinear}, .textureCrc = data::kTexturesSmokeBC4tex_glass_0001_MKjpgCrc},
			{.flags = DescriptorFlags::kCombinedSamplers, .pTexture = &gpTextureManager->mRenderTargetTextures.mTerrainElevationTexture},
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerWindClamp}, .pTexture = &gpTextureManager->mRenderTargetTextures.mWindTextureOne},
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerWindClamp}, .pTexture = &gpTextureManager->mRenderTargetTextures.mWindTextureTwo},
			{.flags = DescriptorFlags::kStorageImages, .pTexture = &gpTextureManager->mRenderTargetTextures.mSmokeTextureTwo},
			{.flags = DescriptorFlags::kStorageBuffer, .pVkBuffers = &gpBufferManager->mSmokeActiveTileVkBuffer},
			{.flags = DescriptorFlags::kStorageBuffer, .pVkBuffers = &gpBufferManager->mSmokeOccupancyVkBuffers[1]},
		},
	});

	mpPipelines[kPipelineSmokeSpreadComputeA].Create(
	{
		.name = "SmokeSpreadComputeA",
		.flags = {PipelineFlags::kCompute, PipelineFlags::kUpdateAfterBind},
		.ppShaders = {&mShaders.at(data::kShadersSmokeSmokeSpreadOnecompCrc)},
		.descriptorInfos =
		{
			{.flags = DescriptorFlags::kGlobalLayoutUniformBuffers},
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerSmoke}, .pTexture = &gpTextureManager->mRenderTargetTextures.mSmokeTextureTwo},
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerMirroredRepeatLinear}, .textureCrc = data::kTexturesSmokeBC4tex_swirl_0002_MKjpgCrc},
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerWindClamp}, .pTexture = &gpTextureManager->mRenderTargetTextures.mWindTextureOne},
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerWindClamp}, .pTexture = &gpTextureManager->mRenderTargetTextures.mWindTextureTwo},
			{.flags = DescriptorFlags::kStorageImages, .pTexture = &gpTextureManager->mRenderTargetTextures.mSmokeTextureOne},
			{.flags = DescriptorFlags::kStorageBuffer, .pVkBuffers = &gpBufferManager->mSmokeActiveTileVkBuffer},
			{.flags = DescriptorFlags::kStorageBuffer, .pVkBuffers = &gpBufferManager->mSmokeOccupancyVkBuffers[0]},
		},
	});

	struct WindOccupancyPipelineDescription
	{
		Pipelines ePipeline;
		std::string_view name;
		VkBuffer* pVkSourceOccupancyBuffer = nullptr;
		VkBuffer* pVkActiveTileBuffer = nullptr;
	};
	WindOccupancyPipelineDescription pWindOccupancyPipelineDescriptions[]
	{
		{.ePipeline = kPipelineWindOccupancyDilateA, .name = "WindOccupancyDilateA", .pVkSourceOccupancyBuffer = &gpBufferManager->mWindOccupancyVkBuffers[1], .pVkActiveTileBuffer = &gpBufferManager->mWindActiveTileVkBuffers[0]},
		{.ePipeline = kPipelineWindOccupancyDilateB, .name = "WindOccupancyDilateB", .pVkSourceOccupancyBuffer = &gpBufferManager->mWindOccupancyVkBuffers[0], .pVkActiveTileBuffer = &gpBufferManager->mWindActiveTileVkBuffers[1]},
	};
	for (const WindOccupancyPipelineDescription& rDescription : pWindOccupancyPipelineDescriptions)
	{
		mpPipelines[rDescription.ePipeline].Create(
		{
			.name = rDescription.name,
			.flags = {PipelineFlags::kCompute},
			.ppShaders = {&mShaders.at(data::kShadersWindWindOccupancyDilatecompCrc)},
			.descriptorInfos =
			{
				{.flags = DescriptorFlags::kGlobalLayoutUniformBuffers},
				{.flags = DescriptorFlags::kStorageBuffer, .pVkBuffers = rDescription.pVkSourceOccupancyBuffer},
				{.flags = DescriptorFlags::kStorageBuffer, .pVkBuffers = rDescription.pVkActiveTileBuffer},
			},
		});
	}

	struct WindSpreadPipelineDescription
	{
		Pipelines ePipeline;
		std::string_view name;
		common::crc_t shaderCrc;
		Texture* pSourceTexture = nullptr;
		Texture* pDestinationTexture = nullptr;
		VkBuffer* pVkActiveTileBuffer = nullptr;
		VkBuffer* pVkOccupancyBuffer = nullptr;
	};
	WindSpreadPipelineDescription pWindSpreadPipelineDescriptions[]
	{
		{.ePipeline = kPipelineWindSpreadComputeA, .name = "WindSpreadComputeA", .shaderCrc = data::kShadersWindWindSpreadOnecompCrc, .pSourceTexture = &gpTextureManager->mRenderTargetTextures.mWindTextureTwo, .pDestinationTexture = &gpTextureManager->mRenderTargetTextures.mWindTextureOne, .pVkActiveTileBuffer = &gpBufferManager->mWindActiveTileVkBuffers[0], .pVkOccupancyBuffer = &gpBufferManager->mWindOccupancyVkBuffers[0]},
		{.ePipeline = kPipelineWindSpreadComputeB, .name = "WindSpreadComputeB", .shaderCrc = data::kShadersWindWindSpreadTwocompCrc, .pSourceTexture = &gpTextureManager->mRenderTargetTextures.mWindTextureOne, .pDestinationTexture = &gpTextureManager->mRenderTargetTextures.mWindTextureTwo, .pVkActiveTileBuffer = &gpBufferManager->mWindActiveTileVkBuffers[1], .pVkOccupancyBuffer = &gpBufferManager->mWindOccupancyVkBuffers[1]},
	};
	for (const WindSpreadPipelineDescription& rDescription : pWindSpreadPipelineDescriptions)
	{
		mpPipelines[rDescription.ePipeline].Create(
		{
			.name = rDescription.name,
			.flags = {PipelineFlags::kCompute, PipelineFlags::kUpdateAfterBind},
			.ppShaders = {&mShaders.at(rDescription.shaderCrc)},
			.descriptorInfos =
			{
				{.flags = DescriptorFlags::kGlobalLayoutUniformBuffers},
				{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerWindClamp}, .pTexture = rDescription.pSourceTexture},
				{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerMirroredRepeatLinear}, .textureCrc = data::kTexturesSmokeBC4tex_swirl_0002_MKjpgCrc},
				{.flags = DescriptorFlags::kStorageImages, .pTexture = rDescription.pDestinationTexture},
				{.flags = DescriptorFlags::kStorageBuffer, .pVkBuffers = rDescription.pVkActiveTileBuffer},
				{.flags = DescriptorFlags::kStorageBuffer, .pVkBuffers = rDescription.pVkOccupancyBuffer},
			},
		});
	}
}

void PipelineManager::CreateParticlePipelines()
{
	struct ParticlePipelineDescription
	{
		Pipelines eUpdatePipeline;
		Pipelines eRenderPipeline;
		Pipelines eSpawnPipeline;
		std::string_view updateName;
		std::string_view renderName;
		std::string_view spawnName;
		Buffer* pStorageBuffer = nullptr;
		Buffer* pSpawnStorageBuffers = nullptr;
		common::crc_t renderVertexShaderCrc;
	};
	ParticlePipelineDescription pParticlePipelineDescriptions[]
	{
		{.eUpdatePipeline = kPipelineLongParticlesUpdate, .eRenderPipeline = kPipelineLongParticlesRender, .eSpawnPipeline = kPipelineLongParticlesSpawn, .updateName = "LongParticlesUpdate", .renderName = "LongParticlesRender", .spawnName = "LongParticlesSpawn", .pStorageBuffer = &gpBufferManager->mLongParticlesStorageBuffer, .pSpawnStorageBuffers = gpBufferManager->mLongParticlesSpawnStorageBuffers.data(), .renderVertexShaderCrc = data::kShadersParticlesLongParticlesRendervertCrc},
		{.eUpdatePipeline = kPipelineSquareParticlesUpdate, .eRenderPipeline = kPipelineSquareParticlesRender, .eSpawnPipeline = kPipelineSquareParticlesSpawn, .updateName = "SquareParticlesUpdate", .renderName = "SquareParticlesRender", .spawnName = "SquareParticlesSpawn", .pStorageBuffer = &gpBufferManager->mSquareParticlesStorageBuffer, .pSpawnStorageBuffers = gpBufferManager->mSquareParticlesSpawnStorageBuffers.data(), .renderVertexShaderCrc = data::kShadersParticlesSquareParticlesRendervertCrc},
	};
	for (const ParticlePipelineDescription& rDescription : pParticlePipelineDescriptions)
	{
		mpPipelines[rDescription.eUpdatePipeline].Create(
		{
			.name = rDescription.updateName,
			.flags = {PipelineFlags::kCompute, PipelineFlags::kIndirectDeviceLocal},
			.ppShaders = {&mShaders.at(data::kShadersParticlesParticlesUpdatecompCrc)},
			.descriptorInfos =
			{
				{.flags = DescriptorFlags::kGlobalLayoutUniformBuffers},
				{.flags = DescriptorFlags::kStorageBuffer, .pBuffers = rDescription.pStorageBuffer},
				{.flags = DescriptorFlags::kCombinedSamplers, .pTexture = &gpTextureManager->mRenderTargetTextures.mTerrainElevationTexture},
			},
		});

		mpPipelines[rDescription.eRenderPipeline].Create(
		{
			.name = rDescription.renderName,
			.flags = {PipelineFlags::kIndirectDeviceLocal, PipelineFlags::kDepthTest, PipelineFlags::kAdd, PipelineFlags::kUpdateAfterBind},
			.ppShaders = {&mShaders.at(rDescription.renderVertexShaderCrc), &mShaders.at(data::kShadersParticlesParticlesRenderfragCrc)},
			.pVertexBuffer = &gpBufferManager->mQuadsVertexBuffer,
			.descriptorInfos =
			{
				{.flags = DescriptorFlags::kGlobalLayoutUniformBuffers},
				{.flags = DescriptorFlags::kMainLayoutUniformBuffers},
				{.flags = DescriptorFlags::kStorageBuffer, .pBuffers = rDescription.pStorageBuffer},
				{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerSmoke}, .pTexture = &gpTextureManager->mRenderTargetTextures.mSmokeTextureOne},
				{.flags = DescriptorFlags::kSamplerClamp},
				{.flags = DescriptorFlags::kTextures},
			},
		});

		mpPipelines[rDescription.eSpawnPipeline].Create(
		{
			.name = rDescription.spawnName,
			.flags = {PipelineFlags::kCompute},
			.ppShaders = {&mShaders.at(data::kShadersParticlesParticlesSpawncompCrc)},
			.descriptorInfos =
			{
				{.flags = DescriptorFlags::kGlobalLayoutUniformBuffers},
				{.flags = DescriptorFlags::kPerCommandBufferStorageBuffers, .pBuffers = rDescription.pSpawnStorageBuffers},
				{.flags = DescriptorFlags::kStorageBuffer, .pBuffers = rDescription.pStorageBuffer},
				{.flags = DescriptorFlags::kStorageBuffer, .pVkBuffers = &mpPipelines[rDescription.eUpdatePipeline].mIndirectVkBuffer},
				{.flags = DescriptorFlags::kStorageBuffer, .pVkBuffers = &mpPipelines[rDescription.eRenderPipeline].mIndirectVkBuffer},
			},
		});
	}
}

void PipelineManager::CreateDebugRenderPipelines()
{
	if constexpr (!kbDebugRender)
	{
		return;
	}

	struct DebugRenderPipelineEntry
	{
		Pipelines ePipeline;
		std::string_view name;
		common::crc_t crc;
		Buffer* pVertexBuffer = nullptr;
		common::crc_t vertexShaderCrc;
	};

	DebugRenderPipelineEntry pEntries[]
	{
		{.ePipeline = kPipelineDebugBox, .name = "DebugBox", .crc = common::CrcConsteval("DebugBox"), .pVertexBuffer = &gpBufferManager->mDebugBoxVertexBuffer, .vertexShaderCrc = data::kShadersDebugDebugRendervertCrc},
		{.ePipeline = kPipelineDebugSphere, .name = "DebugSphere", .crc = common::CrcConsteval("DebugSphere"), .pVertexBuffer = &gpBufferManager->mDebugSphereVertexBuffer, .vertexShaderCrc = data::kShadersDebugDebugRendervertCrc},
		{.ePipeline = kPipelineDebugCircle, .name = "DebugCircle", .crc = common::CrcConsteval("DebugCircle"), .pVertexBuffer = &gpBufferManager->mDebugCircleVertexBuffer, .vertexShaderCrc = data::kShadersDebugDebugRenderBillboardvertCrc},
		{.ePipeline = kPipelineDebugLine, .name = "DebugLine", .crc = common::CrcConsteval("DebugLine"), .pVertexBuffer = &gpBufferManager->mDebugLineVertexBuffer, .vertexShaderCrc = data::kShadersDebugDebugRendervertCrc},
	};

	for (const DebugRenderPipelineEntry& rEntry : pEntries)
	{
		gpBufferManager->CreateDynamicBuffer(rEntry.crc, kBufferMain, rEntry.name, sizeof(shaders::DebugRenderLayout));

		mpPipelines[rEntry.ePipeline].Create(
		{
			.name = rEntry.name,
			.flags = {PipelineFlags::kIndirectHostVisible, PipelineFlags::kLineList, PipelineFlags::kAlphaBlend, PipelineFlags::kUpdateAfterBind},
			.ppShaders = {&mShaders.at(rEntry.vertexShaderCrc), &mShaders.at(data::kShadersDebugDebugRenderfragCrc)},
			.pVertexBuffer = rEntry.pVertexBuffer,
			.descriptorInfos =
			{
				{.flags = DescriptorFlags::kGlobalLayoutUniformBuffers},
				{.flags = DescriptorFlags::kMainLayoutUniformBuffers},
				{.flags = DescriptorFlags::kPerCommandBufferStorageBuffers, .pBuffers = gpBufferManager->mDynamicStorageBuffers[kBufferMain].at(rEntry.crc).data()},
			},
		});
	}
}

} // namespace engine

#endif // defined(BT_CLIENT)
