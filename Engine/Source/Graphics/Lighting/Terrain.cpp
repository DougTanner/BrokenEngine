#if defined(BT_CLIENT)

#include "Terrain.h"

#include "../../../Data/Shaders/Lighting/LightingTerrainCommon.h"
#include "Data/Shader.h"
#include "Frame/IslandTerrain.h"
#include "Frame/IslandTerrainResidency.h"
#include "File/PackChunks.h"
#include "Graphics/Objects/PipelineDescriptorWriter.h"
#include "Graphics/Islands.h"
#include "LightingAccelerationStructure.h"

namespace engine
{

struct LightingTerrain::State
{
	struct Placement
	{
		uint32_t uiPlacement = 0;
		uint32_t uiVertexCount = 0;
		uint32_t uiPrimitiveCount = 0;
		VkDeviceAddress vkXY = 0;
		VkDeviceAddress vkIndices = 0;
		VkDeviceSize vkXYZOffset = 0;
		int32_t iMinTileX = 0;
		int32_t iMinTileY = 0;
		int32_t iMaxTileX = 0;
		int32_t iMaxTileY = 0;
		int32_t iFootprintMinTileX = 0;
		int32_t iFootprintMinTileY = 0;
		int32_t iFootprintMaxTileX = 0;
		int32_t iFootprintMaxTileY = 0;
		common::crc_t templateCrc = 0;
		VkDeviceSize vkXYOffset = 0;
		LightingAccelerationStructure acceleration;
	};

	struct Framebuffer
	{
		Texture scratch;
		Buffer vertices;
		Buffer work;
		Buffer materials;
		Buffer writes;
		Buffer originals;
		Buffer baseTile;
		Buffer baseZ;
		Buffer summary;
		Buffer tileResults;
		// TEMP START bounded terrain evidence
		Buffer evidence;
		std::unique_ptr<StagingBuffer> pEvidenceReadback;
		// TEMP END bounded terrain evidence
		std::unique_ptr<StagingBuffer> readback;
		std::unique_ptr<StagingBuffer> tileReadback;
		int64_t iTileReadbackBytes = 0;
		Texture witness;
		std::vector<Placement> placements;
		LightingTerrainCounts counts;
		LightingTerrainCompletion pending;
		LightingTerrainCompletion completed;
		uint64_t uiValidationSerial = 0;
		uint64_t uiRecordedSerial = 0;
		uint64_t uiRecordedPublication = 0;
		uint64_t uiResourceGeneration = 0;
		VkQueryPool vkQueryPool = VK_NULL_HANDLE;
		uint32_t uiQueryCount = 0;
		int64_t iElevationPlacements = 0;
		// TEMP START matched memory cost
		std::array<uint8_t, 32> matchedCommandDigest {};
		// TEMP END matched memory cost
		uint32_t uiFirstInstance = 0;
		uint32_t uiFirstGeometry = 0;

		~Framebuffer()
		{
			if (vkQueryPool != VK_NULL_HANDLE)
			{
				vkDestroyQueryPool(gpDeviceManager->mVkDevice, vkQueryPool, nullptr);
			}
		}
	};

	std::vector<std::unique_ptr<Framebuffer>> framebuffers;
	Pipeline rasterPipeline;
	Pipeline vertexPipeline;
	Pipeline validatePipeline;
	Pipeline witnessPipeline;
	VkDeviceSize vkMaxAllocationSize = 0;
	uint64_t uiResourceGeneration = 0;

	static VkAccelerationStructureGeometryKHR GetGeometry(const Framebuffer& rFramebuffer, const Placement& rPlacement);
};

static bool TerrainValidationBuffer(Buffer& rBuffer, std::string_view name, int64_t iBytes)
{
	if (iBytes <= rBuffer.mInfo.iDataSize)
	{
		return false;
	}
	rBuffer.Destroy();
	rBuffer.mInfo = {.name = name, .flags = {BufferFlags::kStorage, BufferFlags::kDeviceLocal, BufferFlags::kDeviceAddress}, .iDataSize = iBytes, .eMemoryPlacement = BufferMemoryPlacement::kOrdinaryRam};
	Buffer::CreateBuffer(name, iBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, rBuffer.mDeviceLocalVkBuffer, rBuffer.mDeviceLocalVmaAllocation, nullptr, rBuffer.mInfo.eMemoryPlacement);
	return true;
}

LightingTerrain::LightingTerrain()
: mpState(std::make_unique<State>())
{
}

LightingTerrain::~LightingTerrain() = default;

void LightingTerrain::CreateResources()
{
	VkPhysicalDeviceMaintenance3Properties vkMaintenanceProperties {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES, };
	VkPhysicalDeviceProperties2 vkProperties {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &vkMaintenanceProperties, };
	vkGetPhysicalDeviceProperties2(gpInstanceManager->mVkPhysicalDevice, &vkProperties);
	mpState->vkMaxAllocationSize = vkMaintenanceProperties.maxMemoryAllocationSize;
	if (gpInstanceManager->mVkQueueFamilyProperties.at(gpInstanceManager->miGraphicsQueueFamilyIndex).timestampValidBits == 0)
	{
		throw std::runtime_error("Lighting terrain requires graphics queue timestamps");
	}
	VkFormatProperties vkFormatProperties {};
	vkGetPhysicalDeviceFormatProperties(gpInstanceManager->mVkPhysicalDevice, shaders::kVkFormatElevation, &vkFormatProperties);
	VkFormatFeatureFlags vkRequired = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;
	if ((vkFormatProperties.optimalTilingFeatures & vkRequired) != vkRequired)
	{
		throw std::runtime_error("Lighting terrain R16 raster sampling/blending unsupported");
	}
	mpState->framebuffers.reserve(static_cast<size_t>(std::ssize(gpSwapchainManager->mFramebuffers)));
	for (int64_t i = 0; i < std::ssize(gpSwapchainManager->mFramebuffers); ++i)
	{
		auto pFramebuffer = std::make_unique<State::Framebuffer>();
		pFramebuffer->scratch.Create(
		{
			.textureFlags = TextureFlags::kRenderPass,
			.name = "LightingTerrainScratch",
			.vkFormat = shaders::kVkFormatElevation,
			.vkExtent3D = {.width = shaders::kiLightingTerrainScratch, .height = shaders::kiLightingTerrainScratch, .depth = 1, },
			.vkImageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
			.vkImageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
			.vkRenderPassAttachmentLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.vkRenderPassFinalImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			.vkRenderPassClearColorValue = {gpIslandTerrain->mfSeaFloorElevation, 0.0f, 0.0f, 1.0f, },
			.vkRenderPassDestinationStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			.vkRenderPassDestinationAccessMask = VK_ACCESS_SHADER_READ_BIT,
		});
		pFramebuffer->work.Create(
		{
			.name = "LightingTerrainWork",
			.flags = {BufferFlags::kStorage, BufferFlags::kHostVisible, },
			.iDataSize = sizeof(shaders::LightingTerrainWork),
		});
		pFramebuffer->witness.Create(
		{
			.textureFlags = TextureFlags::kRenderPass,
			.name = "LightingTerrainOriginalWitness",
			.vkFormat = shaders::kVkFormatElevation,
			.vkExtent3D = {.width = 1, .height = 1, .depth = 1},
			.vkImageUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			.vkImageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
			.vkRenderPassAttachmentLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.vkRenderPassFinalImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.eTextureLayout = TextureLayout::kUndefined,
			.vkRenderPassDestinationStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			.vkRenderPassDestinationAccessMask = VK_ACCESS_SHADER_READ_BIT,
		});
		mpState->framebuffers.push_back(std::move(pFramebuffer));
	}
}

void LightingTerrain::DestroyResources()
{
	DestroyPipelines();
	mpState->framebuffers.clear();
}

void LightingTerrain::CreatePipelines()
{
	DestroyPipelines();
	State::Framebuffer& rFramebuffer = *mpState->framebuffers.front();
	mpState->rasterPipeline.Create(
	{
		.name = "LightingTerrainTile",
		.flags = {PipelineFlags::kRenderTarget, PipelineFlags::kPushConstants, PipelineFlags::kMax, PipelineFlags::kUpdateAfterBind, PipelineFlags::kNoWireframe, },
		.ppShaders = {&gpPipelineManager->mShaders.at(data::kShadersLightingLightingTerrainTilevertCrc), &gpPipelineManager->mShaders.at(data::kShadersTerrainTerrainElevationfragCrc), },
		.pVertexBuffer = &gpBufferManager->mQuadsVertexBuffer,
		.vkTargetRenderPass = rFramebuffer.scratch.mVkRenderPass,
		.vkExtent3D = rFramebuffer.scratch.mInfo.vkExtent3D,
		.descriptorInfos =
		{
			{.flags = DescriptorFlags::kGlobalLayoutUniformBuffers, },
			{.flags = DescriptorFlags::kPerCommandBufferStorageBuffers, .pBuffers = gpIslands->mIslandsStorageBuffers.data(), },
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kBindlessArrayConsumer, }, .iCount = shaders::kiMaxIslands, .ppTextures = gpTextureManager->mRenderTargetTextures.mElevationTextures.data(), },
			{.flags = DescriptorFlags::kCombinedSamplers, .pTexture = &gpTextureManager->mRenderTargetTextures.mTerrainElevationTexture, },
		},
	});
	mpState->vertexPipeline.Create(
	{
		.name = "LightingTerrainVertices",
		.flags = {PipelineFlags::kCompute, PipelineFlags::kPushConstants, PipelineFlags::kUpdateAfterBind, },
		.ppShaders = {&gpPipelineManager->mShaders.at(data::kShadersLightingLightingTerraincompCrc), },
		.descriptorInfos =
		{
			{.flags = DescriptorFlags::kGlobalLayoutUniformBuffers, },
			{.flags = DescriptorFlags::kPerCommandBufferStorageBuffers, .pBuffers = gpIslands->mIslandsStorageBuffers.data(), },
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kBindlessArrayConsumer, DescriptorFlags::kSamplerElevation, }, .iCount = shaders::kiMaxIslands, .ppTextures = gpTextureManager->mRenderTargetTextures.mElevationTextures.data(), },
			{.flags = DescriptorFlags::kCombinedSamplers, .pTexture = &gpTextureManager->mRenderTargetTextures.mTerrainElevationTexture, },
			{.flags = DescriptorFlags::kStorageBuffer, .iExplicitBinding = 5, .pBuffers = &rFramebuffer.work, },
		},
	});
	PipelineInfo validateInfo = mpState->vertexPipeline.mInfo;
	validateInfo.name = "LightingTerrainValidate";
	validateInfo.ppShaders[0] = &gpPipelineManager->mShaders.at(data::kShadersLightingLightingTerrainValidatecompCrc);
	validateInfo.iPushConstantBytes = sizeof(shaders::LightingTerrainValidatePush);
	mpState->validatePipeline.Create(validateInfo);
	mpState->witnessPipeline.Create(
	{
		.name = "LightingTerrainOriginalWitness",
		.flags = {PipelineFlags::kRenderTarget, PipelineFlags::kNoColorWrite, PipelineFlags::kUpdateAfterBind, PipelineFlags::kPointList},
		.ppShaders = {&gpPipelineManager->mShaders.at(data::kShadersLightingLightingTerrainWitnessvertCrc), &gpPipelineManager->mShaders.at(data::kShadersLightingLightingTerrainWitnessfragCrc)},
		.vkTargetRenderPass = rFramebuffer.witness.mVkRenderPass,
		.vkExtent3D = rFramebuffer.witness.mInfo.vkExtent3D,
		.descriptorInfos =
		{
			{.flags = DescriptorFlags::kGlobalLayoutUniformBuffers, .iExplicitBinding = shaders::kiGlobalBindingGlobalUniform},
			{.flags = DescriptorFlags::kMainLayoutUniformBuffers, .iExplicitBinding = shaders::kiGlobalBindingMainUniform},
			{.flags = DescriptorFlags::kCombinedSamplers, .iExplicitBinding = 4, .pTexture = &gpTextureManager->mRenderTargetTextures.mTerrainElevationTexture},
			{.flags = DescriptorFlags::kStorageBuffer, .iExplicitBinding = 5, .pBuffers = &rFramebuffer.work},
			{.flags = DescriptorFlags::kPerCommandBufferStorageBuffers, .iExplicitBinding = 17, .pBuffers = gpIslands->mIslandsStorageBuffers.data()},
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kBindlessArrayConsumer, DescriptorFlags::kSamplerElevation}, .iCount = shaders::kiMaxIslands, .iExplicitBinding = 19, .ppTextures = gpTextureManager->mRenderTargetTextures.mElevationTextures.data()},
		},
	});
}

void LightingTerrain::DestroyPipelines()
{
	mpState->rasterPipeline.Destroy();
	mpState->vertexPipeline.Destroy();
	mpState->validatePipeline.Destroy();
	mpState->witnessPipeline.Destroy();
}

static int32_t TerrainTileBound(double fPixel, bool bUpper)
{
	double fOutward = std::nextafter(fPixel, bUpper ? INFINITY : -INFINITY) + (bUpper ? 1.0 : -1.0);
	double fTile = std::floor(std::floor(fOutward - 0.5) / shaders::kiLightingTerrainTile);
	if (!std::isfinite(fTile) || fTile < (std::numeric_limits<int32_t>::min() + 512.0) / shaders::kiLightingTerrainTile || fTile > (std::numeric_limits<int32_t>::max() - 512.0) / shaders::kiLightingTerrainTile)
	{
		throw std::runtime_error("Lighting terrain tile coordinate exceeds signed lattice range");
	}
	return static_cast<int32_t>(fTile);
}

VkAccelerationStructureGeometryKHR LightingTerrain::State::GetGeometry(const Framebuffer& rFramebuffer, const Placement& rPlacement)
{
	return
	{
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
		.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
		.geometry = {.triangles =
		{
			.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
			.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
			.vertexData = {.deviceAddress = rFramebuffer.vertices.GetDeviceAddress() + rPlacement.vkXYZOffset, },
			.vertexStride = 3 * sizeof(float),
			.maxVertex = rPlacement.uiVertexCount - 1,
			.indexType = VK_INDEX_TYPE_UINT32,
			.indexData = {.deviceAddress = rPlacement.vkIndices, },
		}},
		.flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
	};
}

LightingTerrainCounts LightingTerrain::Prepare(int64_t iFramebuffer, uint64_t uiValidationSerial, uint64_t uiPublication)
{
	// Heap: framebuffer-owned geometry grows after its fence completes; scalar work ranges survive command recording.
	ScopedSuppressAllocationTracking suppress;
	State::Framebuffer& rFramebuffer = *mpState->framebuffers.at(iFramebuffer);
	rFramebuffer.counts = {};
	rFramebuffer.uiValidationSerial = uiValidationSerial;
	rFramebuffer.pending = {};
	rFramebuffer.pending.uiPublication = uiPublication;
	rFramebuffer.pending.basis = gpCamera->mBasisCoordinate;
	const shaders::GlobalLayout& rGlobal = *reinterpret_cast<const shaders::GlobalLayout*>(gpBufferManager->mGlobalLayoutUniformBuffers.at(iFramebuffer).mpMappedMemory);
	const shaders::AxisAlignedQuadLayout* pQuads = reinterpret_cast<const shaders::AxisAlignedQuadLayout*>(gpIslands->mIslandsStorageBuffers.at(iFramebuffer).mpMappedMemory);
	const VkDrawIndexedIndirectCommand* pDraws = gpIslands->mppIslandsIndirectMappedVkDrawIndexedIndirectCommands.at(iFramebuffer);
	const VkExtent3D& rExtent = gpTextureManager->mRenderTargetTextures.mTerrainElevationTexture.mInfo.vkExtent3D;
	static_cast<XMFLOAT4&>(rFramebuffer.pending.f4VisibleArea) = rGlobal.f4VisibleArea;
	rFramebuffer.pending.vkVisibleExtent = rExtent;
	int64_t iPlacementCount = gpIslands->mLastWrittenCounts.at(iFramebuffer);
	rFramebuffer.iElevationPlacements = iPlacementCount;
	int64_t iWorkBytes = iPlacementCount * static_cast<int64_t>(sizeof(shaders::LightingTerrainWork));
	if (iWorkBytes > rFramebuffer.work.mInfo.iDataSize)
	{
		rFramebuffer.work.Create({.name = "LightingTerrainWork", .flags = {BufferFlags::kStorage, BufferFlags::kHostVisible, }, .iDataSize = iWorkBytes, });
		rFramebuffer.uiResourceGeneration = ++mpState->uiResourceGeneration;
	}
	for (const Pipeline* pPipeline : {&mpState->vertexPipeline, &mpState->validatePipeline})
	{
		PipelineDescriptorWriter::UpdateImageDescriptorInSet(*pPipeline, iFramebuffer, 4, gpTextureManager->GetSampler(DescriptorFlags::kCombinedSamplers), rFramebuffer.scratch.mVkImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	}
	for (const Pipeline* pPipeline : {&mpState->vertexPipeline, &mpState->validatePipeline, &mpState->witnessPipeline})
	{
		PipelineDescriptorWriter::UpdateStorageBuffer(*pPipeline, iFramebuffer, 5, &rFramebuffer.work);
	}
	// TEMP START descriptor owner evidence
	static uint32_t suiOwnerEvidence = 0;
	if (uiValidationSerial != 0 && suiOwnerEvidence < 24)
	{
		++suiOwnerEvidence;
		LOG(kTemp, kInfo, "DESCRIPTOR_OWNER terrain framebuffer={} serial={} vertexSet={} validateSet={} witnessSet={} scratch4={} sampler={} work5={}", iFramebuffer, uiValidationSerial, reinterpret_cast<uint64_t>(mpState->vertexPipeline.mVkDescriptorSets.at(iFramebuffer)), reinterpret_cast<uint64_t>(mpState->validatePipeline.mVkDescriptorSets.at(iFramebuffer)), reinterpret_cast<uint64_t>(mpState->witnessPipeline.mVkDescriptorSets.at(iFramebuffer)), reinterpret_cast<uint64_t>(rFramebuffer.scratch.mVkImageView), reinterpret_cast<uint64_t>(gpTextureManager->GetSampler(DescriptorFlags::kCombinedSamplers)), reinterpret_cast<uint64_t>(rFramebuffer.work.GetBuffer()));
	}
	// TEMP END descriptor owner evidence
	const VkPhysicalDeviceLimits& rLimits = gpInstanceManager->mVkPhysicalDeviceProperties.limits;
	if (std::cmp_less(rLimits.maxComputeWorkGroupSize[0], shaders::kiLightingTerrainGroup) || std::cmp_less(rLimits.maxComputeWorkGroupInvocations, shaders::kiLightingTerrainGroup))
	{
		throw std::runtime_error("Lighting terrain compute group unsupported");
	}
	int64_t iRun = 0;
	for (int64_t i = 0; i < gpIslands->miTemplateCount; ++i)
	{
		const IslandTemplate& rTemplate = gpIslandTerrain->mIslands.at(gpIslandTerrain->mIslandCrcsSorted.at(i));
		int64_t iFirst = pDraws[i].firstInstance;
		int64_t iEnd = i + 1 < gpIslands->miTemplateCount ? pDraws[i + 1].firstInstance : iPlacementCount;
		if (rTemplate.eMeshResidency != IslandMeshResidency::kResident)
		{
			continue;
		}
		if (std::cmp_equal(pDraws[i].indexCount, 0))
		{
			continue;
		}
		if (!std::in_range<uint32_t>(rTemplate.iMeshVertexCount) || !std::in_range<uint32_t>(rTemplate.iMeshIndexCount / 3) ||
		    std::cmp_greater(rTemplate.iMeshIndexCount / 3, gpInstanceManager->mVkPhysicalDeviceAccelerationStructurePropertiesKHR.maxPrimitiveCount))
		{
			throw std::runtime_error("Lighting terrain acceleration geometry exceeds device limits");
		}
		if (!std::in_range<uint32_t>(rTemplate.iMeshIndexCount))
		{
			throw std::runtime_error("Lighting terrain index count exceeds validation range");
		}
		for (int64_t j = iFirst; j < iEnd; ++j, ++iRun)
		{
			if (iRun == std::ssize(rFramebuffer.placements))
			{
				rFramebuffer.placements.emplace_back();
			}
			State::Placement& rPlacement = rFramebuffer.placements.at(iRun);
			uint32_t uiVertices = static_cast<uint32_t>(rTemplate.iMeshVertexCount);
			uint32_t uiPrimitives = static_cast<uint32_t>(rTemplate.iMeshIndexCount / 3);
			if (rPlacement.uiVertexCount != uiVertices || rPlacement.uiPrimitiveCount != uiPrimitives)
			{
				rPlacement.acceleration.Destroy();
			}
			rPlacement.uiPlacement = static_cast<uint32_t>(j);
			rPlacement.uiVertexCount = uiVertices;
			rPlacement.uiPrimitiveCount = uiPrimitives;
			rPlacement.vkXY = gpIslands->mIslandMeshArena.GetDeviceAddress() + rTemplate.iMeshVertexOffset;
			rPlacement.vkXYOffset = rTemplate.iMeshVertexOffset;
			rPlacement.templateCrc = gpIslandTerrain->mIslandCrcsSorted.at(i);
			rPlacement.vkIndices = gpIslands->mIslandMeshArena.GetDeviceAddress() + rTemplate.iMeshIndexOffset;
			rPlacement.vkXYZOffset = rFramebuffer.counts.iVertices * 3 * sizeof(float);
			const shaders::AxisAlignedQuadLayout& rQuad = pQuads[j];
			float fCos = std::cos(rQuad.fRotation);
			float fSin = std::sin(rQuad.fRotation);
			double fMinX = INFINITY;
			double fMinY = INFINITY;
			double fMaxX = -INFINITY;
			double fMaxY = -INFINITY;
			for (float fX : {rTemplate.fMeshMinX, rTemplate.fMeshMaxX, })
			{
				for (float fY : {rTemplate.fMeshMinY, rTemplate.fMeshMaxY, })
				{
					float fWorldX = rQuad.f4VertexRect.x + 0.5f * rQuad.f4VertexRect.z + fX * fCos - fY * fSin;
					float fWorldY = rQuad.f4VertexRect.y + 0.5f * rQuad.f4VertexRect.w + fX * fSin + fY * fCos;
					double fPixelX = (static_cast<double>(fWorldX) - rGlobal.f4VisibleArea.x) / (rGlobal.f4VisibleArea.z - rGlobal.f4VisibleArea.x) * rExtent.width;
					double fPixelY = (static_cast<double>(fWorldY) - rGlobal.f4VisibleArea.y) / (rGlobal.f4VisibleArea.w - rGlobal.f4VisibleArea.y) * rExtent.height;
					fMinX = std::min(fMinX, fPixelX);
					fMaxX = std::max(fMaxX, fPixelX);
					fMinY = std::min(fMinY, fPixelY);
					fMaxY = std::max(fMaxY, fPixelY);
				}
			}
			rPlacement.iMinTileX = TerrainTileBound(fMinX, false);
			rPlacement.iMinTileY = TerrainTileBound(fMinY, false);
			rPlacement.iMaxTileX = TerrainTileBound(fMaxX, true);
			rPlacement.iMaxTileY = TerrainTileBound(fMaxY, true);
			double fFootMinX = INFINITY;
			double fFootMinY = INFINITY;
			double fFootMaxX = -INFINITY;
			double fFootMaxY = -INFINITY;
			for (float fX : {-0.5f * std::abs(rQuad.f4VertexRect.z), 0.5f * std::abs(rQuad.f4VertexRect.z)})
			{
				for (float fY : {-0.5f * std::abs(rQuad.f4VertexRect.w), 0.5f * std::abs(rQuad.f4VertexRect.w)})
				{
					float fWorldX = rQuad.f4VertexRect.x + 0.5f * rQuad.f4VertexRect.z + fX * fCos - fY * fSin;
					float fWorldY = rQuad.f4VertexRect.y + 0.5f * rQuad.f4VertexRect.w + fX * fSin + fY * fCos;
					double fPixelX = (static_cast<double>(fWorldX) - rGlobal.f4VisibleArea.x) / (rGlobal.f4VisibleArea.z - rGlobal.f4VisibleArea.x) * rExtent.width;
					double fPixelY = (static_cast<double>(fWorldY) - rGlobal.f4VisibleArea.y) / (rGlobal.f4VisibleArea.w - rGlobal.f4VisibleArea.y) * rExtent.height;
					fFootMinX = std::min(fFootMinX, fPixelX);
					fFootMaxX = std::max(fFootMaxX, fPixelX);
					fFootMinY = std::min(fFootMinY, fPixelY);
					fFootMaxY = std::max(fFootMaxY, fPixelY);
				}
			}
			rPlacement.iFootprintMinTileX = TerrainTileBound(fFootMinX, false);
			rPlacement.iFootprintMinTileY = TerrainTileBound(fFootMinY, false);
			rPlacement.iFootprintMaxTileX = TerrainTileBound(fFootMaxX, true);
			rPlacement.iFootprintMaxTileY = TerrainTileBound(fFootMaxY, true);
			int64_t iTiles = (static_cast<int64_t>(rPlacement.iMaxTileX) - rPlacement.iMinTileX + 1) * (static_cast<int64_t>(rPlacement.iMaxTileY) - rPlacement.iMinTileY + 1);
			int64_t iGroups = (static_cast<int64_t>(uiVertices) + shaders::kiLightingTerrainGroup - 1) / shaders::kiLightingTerrainGroup;
			if (iTiles > (std::numeric_limits<int64_t>::max() - rFramebuffer.counts.iWorkgroups) / iGroups - 1)
			{
				throw std::runtime_error("Lighting terrain workload exceeds counter range");
			}
			rFramebuffer.counts.iVertices += uiVertices;
			rFramebuffer.counts.iTriangles += uiPrimitives;
			rFramebuffer.counts.iTiles += iTiles;
			rFramebuffer.counts.iWorkgroups += (iTiles + 1) * iGroups;
		}
	}
	rFramebuffer.placements.resize(static_cast<size_t>(iRun));
	rFramebuffer.counts.iPlacements = iRun;
	if (iRun == 0)
	{
		rFramebuffer.pending.counts = rFramebuffer.counts;
		rFramebuffer.pending.validation.uiRequestedSerial = uiValidationSerial;
		rFramebuffer.pending.uiResourceGeneration = rFramebuffer.uiResourceGeneration;
		return rFramebuffer.counts;
	}
	int64_t iVertexBytes = rFramebuffer.counts.iVertices * 3 * static_cast<int64_t>(sizeof(float));
	if (std::cmp_greater(iVertexBytes, mpState->vkMaxAllocationSize))
	{
		throw std::runtime_error("Lighting terrain XYZ exceeds allocation limit");
	}
	if (iVertexBytes > rFramebuffer.vertices.mInfo.iDataSize)
	{
		rFramebuffer.vertices.Create({.name = "LightingTerrainXYZ", .flags = {BufferFlags::kStorage, BufferFlags::kDeviceLocal, BufferFlags::kAccelerationInput, }, .iDataSize = iVertexBytes, });
		rFramebuffer.uiResourceGeneration = ++mpState->uiResourceGeneration;
	}
	int64_t iMaterialBytes = iRun * static_cast<int64_t>(sizeof(shaders::LightingTerrainMaterial));
	if (iMaterialBytes > rFramebuffer.materials.mInfo.iDataSize)
	{
		rFramebuffer.materials.Create({.name = "LightingTerrainMaterials", .flags = {BufferFlags::kStorage, BufferFlags::kHostVisible, BufferFlags::kDeviceAddress, }, .iDataSize = iMaterialBytes, });
	}
	shaders::LightingTerrainMaterial* pMaterials = reinterpret_cast<shaders::LightingTerrainMaterial*>(rFramebuffer.materials.mpMappedMemory);
	for (int64_t i = 0; i < iRun; ++i)
	{
		State::Placement& rPlacement = rFramebuffer.placements.at(i);
		const shaders::AxisAlignedQuadLayout& rQuad = pQuads[rPlacement.uiPlacement];
		pMaterials[i] = {.uiPlacement = rPlacement.uiPlacement, .uiTextureSlot = rQuad.uiTextureSlot, .f2Footprint = {XMFLOAT2(rQuad.f4VertexRect.z, rQuad.f4VertexRect.w), }, .fRotation = rQuad.fRotation, };
		if (rPlacement.acceleration.mVkAccelerationStructureKHR == VK_NULL_HANDLE)
		{
			VkAccelerationStructureGeometryKHR vkGeometry = State::GetGeometry(rFramebuffer, rPlacement);
			rPlacement.acceleration.Create("LightingTerrainBLAS", VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, std::span(&vkGeometry, 1), std::span(&rPlacement.uiPrimitiveCount, 1), VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
		}
	}
	rFramebuffer.pending.counts = rFramebuffer.counts;
	if (uiValidationSerial != 0)
	{
		if (std::cmp_greater(rFramebuffer.counts.iVertices, std::numeric_limits<uint32_t>::max() / 32) || std::cmp_greater(rFramebuffer.counts.iTriangles, std::numeric_limits<uint32_t>::max() / 3))
		{
			throw std::runtime_error("Lighting terrain validation counter range exceeded");
		}
		bool bGrowth = TerrainValidationBuffer(rFramebuffer.writes, "LightingTerrainWriteCounts", rFramebuffer.counts.iVertices * sizeof(uint32_t));
		bGrowth |= TerrainValidationBuffer(rFramebuffer.originals, "LightingTerrainOriginalXYZ", rFramebuffer.counts.iVertices * sizeof(shaders::LightingTerrainOriginal));
		// TEMP START bounded terrain evidence
		bGrowth |= TerrainValidationBuffer(rFramebuffer.baseZ, "LightingTerrainBaseFilteredZ", rFramebuffer.counts.iVertices * sizeof(shaders::LightingTerrainBaseSample));
		bGrowth |= TerrainValidationBuffer(rFramebuffer.evidence, "LightingTerrainEvidence", sizeof(shaders::LightingTerrainEvidence));
		if (rFramebuffer.pEvidenceReadback == nullptr)
		{
			rFramebuffer.pEvidenceReadback = std::make_unique<StagingBuffer>("LightingTerrainEvidenceReadback", sizeof(shaders::LightingTerrainEvidence), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
		}
		// TEMP END bounded terrain evidence
		bGrowth |= TerrainValidationBuffer(rFramebuffer.baseTile, "LightingTerrainSavedR16", shaders::kiLightingTerrainScratch * shaders::kiLightingTerrainScratch * 2);
		bGrowth |= TerrainValidationBuffer(rFramebuffer.summary, "LightingTerrainValidationSummary", sizeof(shaders::LightingTerrainValidation));
		bGrowth |= TerrainValidationBuffer(rFramebuffer.tileResults, "LightingTerrainValidationTiles", rFramebuffer.counts.iTiles * sizeof(shaders::LightingTerrainTileResult));
		if (bGrowth)
		{
			rFramebuffer.uiResourceGeneration = ++mpState->uiResourceGeneration;
		}
		if (rFramebuffer.readback == nullptr)
		{
			rFramebuffer.readback = std::make_unique<StagingBuffer>("LightingTerrainSummaryReadback", sizeof(shaders::LightingTerrainValidation), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
		}
		int64_t iTileBytes = rFramebuffer.counts.iTiles * sizeof(shaders::LightingTerrainTileResult);
		if (rFramebuffer.iTileReadbackBytes < iTileBytes)
		{
			rFramebuffer.tileReadback = std::make_unique<StagingBuffer>("LightingTerrainTileReadback", iTileBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
			rFramebuffer.iTileReadbackBytes = iTileBytes;
		}
	}
	uint64_t uiQueries = 6 + 4 * static_cast<uint64_t>(rFramebuffer.counts.iTiles);
	if (!std::in_range<uint32_t>(uiQueries))
	{
		throw std::runtime_error("Lighting terrain timestamp count overflow");
	}
	if (uiQueries > rFramebuffer.uiQueryCount)
	{
		if (rFramebuffer.vkQueryPool != VK_NULL_HANDLE)
		{
			vkDestroyQueryPool(gpDeviceManager->mVkDevice, rFramebuffer.vkQueryPool, nullptr);
		}
		VkQueryPoolCreateInfo info {.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, .queryType = VK_QUERY_TYPE_TIMESTAMP, .queryCount = static_cast<uint32_t>(uiQueries)};
		CHECK_VK(vkCreateQueryPool(gpDeviceManager->mVkDevice, &info, nullptr, &rFramebuffer.vkQueryPool));
		rFramebuffer.uiQueryCount = static_cast<uint32_t>(uiQueries);
	}
	rFramebuffer.pending.uiResourceGeneration = rFramebuffer.uiResourceGeneration;
	return rFramebuffer.counts;
}

void LightingTerrain::Write(int64_t iFramebuffer, uint32_t uiFirstInstance, uint32_t uiFirstGeometry, std::span<shaders::LightingInstance> instances, std::span<shaders::LightingGeometry> geometries, std::span<VkAccelerationStructureInstanceKHR> accelerationInstances, VkDeviceAddress vkGeometryAddress)
{
	State::Framebuffer& rFramebuffer = *mpState->framebuffers.at(iFramebuffer);
	rFramebuffer.uiFirstInstance = uiFirstInstance;
	rFramebuffer.uiFirstGeometry = uiFirstGeometry;
	for (int64_t i = 0; i < std::ssize(rFramebuffer.placements); ++i)
	{
		const State::Placement& rPlacement = rFramebuffer.placements.at(i);
		uint32_t uiInstance = uiFirstInstance + static_cast<uint32_t>(i);
		uint32_t uiGeometry = uiFirstGeometry + static_cast<uint32_t>(i);
		shaders::LightingTerrainWork work {.uiXYAddress = rPlacement.vkXY, .uiXYZAddress = rFramebuffer.vertices.GetDeviceAddress() + rPlacement.vkXYZOffset, .uiVertexCount = rPlacement.uiVertexCount};
		work.uiIndexAddress = rPlacement.vkIndices;
		work.uiTemplate = rPlacement.templateCrc;
		work.uiSerial = rFramebuffer.uiValidationSerial;
		work.uiPrimitiveCount = rPlacement.uiPrimitiveCount;
		work.uiValidation = rFramebuffer.uiValidationSerial != 0 ? 1 : 0;
		work.uiHitGeometryAddress = vkGeometryAddress + uiGeometry * sizeof(shaders::LightingGeometry);
		work.iFootprintMinTileX = rPlacement.iFootprintMinTileX;
		work.iFootprintMinTileY = rPlacement.iFootprintMinTileY;
		work.iFootprintMaxTileX = rPlacement.iFootprintMaxTileX;
		work.iFootprintMaxTileY = rPlacement.iFootprintMaxTileY;
		work.uiBuildPrimitiveCount = rPlacement.uiPrimitiveCount;
		work.uiBuildVertexCount = rPlacement.uiVertexCount;
		if (work.uiValidation != 0)
		{
			uint64_t uiFirstVertex = rPlacement.vkXYZOffset / (3 * sizeof(float));
			work.uiWritesAddress = rFramebuffer.writes.GetDeviceAddress() + uiFirstVertex * sizeof(uint32_t);
			work.uiOriginalAddress = rFramebuffer.originals.GetDeviceAddress() + uiFirstVertex * sizeof(shaders::LightingTerrainOriginal);
			work.uiSummaryAddress = rFramebuffer.summary.GetDeviceAddress();
			work.uiBaseTileAddress = rFramebuffer.baseTile.GetDeviceAddress();
			// TEMP START bounded terrain evidence
			work.uiBaseZAddress = rFramebuffer.baseZ.GetDeviceAddress() + uiFirstVertex * sizeof(shaders::LightingTerrainBaseSample);
			work.uiEvidenceAddress = rFramebuffer.evidence.GetDeviceAddress();
			// TEMP END bounded terrain evidence
			work.uiTileResultsAddress = rFramebuffer.tileResults.GetDeviceAddress();
		}
		reinterpret_cast<shaders::LightingTerrainWork*>(rFramebuffer.work.mpMappedMemory)[rPlacement.uiPlacement] = work;
		shaders::LightingInstance& rInstance = instances[uiInstance];
		rInstance = {};
		rInstance.pf4ObjectToWorld[0] = rInstance.pf4NormalToWorld[0] = {1.0f, 0.0f, 0.0f, 0.0f, };
		rInstance.pf4ObjectToWorld[1] = rInstance.pf4NormalToWorld[1] = {0.0f, 1.0f, 0.0f, 0.0f, };
		rInstance.pf4ObjectToWorld[2] = rInstance.pf4NormalToWorld[2] = {0.0f, 0.0f, 1.0f, 0.0f, };
		rInstance.uiFirstGeometry = uiGeometry;
		rInstance.uiGeometryCount = 1;
		rInstance.uiIdentity = uiInstance;
		geometries[uiGeometry] =
		{
			.uiPositionAddress = rFramebuffer.vertices.GetDeviceAddress() + rPlacement.vkXYZOffset,
			.uiAttributeAddress = rPlacement.vkXY,
			.uiIndexAddress = rPlacement.vkIndices,
			.uiMaterialAddress = rFramebuffer.materials.GetDeviceAddress(),
			.uiPositionStride = 3 * sizeof(float),
			.uiAttributeStride = 2 * sizeof(float),
			.uiIndexBytes = sizeof(uint32_t),
			.uiPrimitiveCount = rPlacement.uiPrimitiveCount,
			.uiMaterial = static_cast<uint32_t>(i),
			.uiKind = shaders::kiLightingGeometryTerrain,
			.uiPayload = static_cast<uint32_t>(i),
		};
		VkAccelerationStructureInstanceKHR& rAccelerationInstance = accelerationInstances[uiInstance];
		rAccelerationInstance = {};
		rAccelerationInstance.transform.matrix[0][0] = 1.0f;
		rAccelerationInstance.transform.matrix[1][1] = 1.0f;
		rAccelerationInstance.transform.matrix[2][2] = 1.0f;
		rAccelerationInstance.instanceCustomIndex = uiInstance;
		rAccelerationInstance.mask = 0xff;
		rAccelerationInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		rAccelerationInstance.accelerationStructureReference = rPlacement.acceleration.GetDeviceAddress();
	}
}

void LightingTerrain::RecordPreparation(VkCommandBuffer vkCommandBuffer, int64_t iFramebuffer)
{
	State::Framebuffer& rFramebuffer = *mpState->framebuffers.at(iFramebuffer);
	if (rFramebuffer.placements.empty())
	{
		rFramebuffer.uiRecordedSerial = 0;
		rFramebuffer.uiRecordedPublication = rFramebuffer.pending.uiPublication;
		return;
	}
	// TEMP START matched memory cost
	std::optional<common::Sha256Hasher> commandHasher;
	if (rFramebuffer.uiValidationSerial != 0) commandHasher.emplace();
	auto HashCommand = [&]<typename T>(const T& rValue)
	{
		if (commandHasher.has_value()) commandHasher->Update(std::as_writable_bytes(std::span(const_cast<T*>(&rValue), 1)));
	};
	// TEMP END matched memory cost
	bool bValidation = rFramebuffer.uiValidationSerial != 0;
	LightingTerrainOperations& rProduction = rFramebuffer.pending.productionOperations;
	LightingTerrainOperations& rValidation = rFramebuffer.pending.validationOperations;
	auto Barrier = [&](VkPipelineStageFlags source, VkPipelineStageFlags destination, VkAccessFlags sourceAccess, VkAccessFlags destinationAccess, LightingTerrainOperations& rOperations)
	{
		VkMemoryBarrier barrier {.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER, .srcAccessMask = sourceAccess, .dstAccessMask = destinationAccess};
		vkCmdPipelineBarrier(vkCommandBuffer, source, destination, 0, 1, &barrier, 0, nullptr, 0, nullptr);
		++rOperations.iBarriers;
	};
	auto Timestamp = [&](uint32_t uiQuery, LightingTerrainOperations& rOperations)
	{
		vkCmdWriteTimestamp(vkCommandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, rFramebuffer.vkQueryPool, uiQuery);
		++rOperations.iTimestamps;
	};
	uint32_t uiQueries = 6 + 4 * static_cast<uint32_t>(rFramebuffer.counts.iTiles);
	vkCmdResetQueryPool(vkCommandBuffer, rFramebuffer.vkQueryPool, 0, uiQueries);
	++rProduction.iQueryResets;
	Timestamp(0, rProduction);
	Barrier(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, rProduction);
	LightingAccelerationStructure::RecordInputBarrier(vkCommandBuffer);
	++rProduction.iBarriers;
	int64_t iMaxGroups = gpInstanceManager->mVkPhysicalDeviceProperties.limits.maxComputeWorkGroupCount[0];
	auto Dispatch = [&](const State::Placement& rPlacement, int32_t iOriginX, int32_t iOriginY)
	{
		for (int64_t iBase = 0; std::cmp_less(iBase, rPlacement.uiVertexCount);)
		{
			int64_t iCount = std::min<int64_t>(rPlacement.uiVertexCount - iBase, iMaxGroups * shaders::kiLightingTerrainGroup);
			XMFLOAT4 f4Push {std::bit_cast<float>(iOriginX), std::bit_cast<float>(iOriginY), std::bit_cast<float>(rPlacement.uiPlacement), std::bit_cast<float>(static_cast<uint32_t>(iBase))};
			// TEMP START matched memory cost
			HashCommand(f4Push.x); HashCommand(f4Push.y); HashCommand(f4Push.z); HashCommand(f4Push.w); HashCommand(iCount);
			// TEMP END matched memory cost
			mpState->vertexPipeline.RecordCompute(iFramebuffer, vkCommandBuffer, (iCount + shaders::kiLightingTerrainGroup - 1) / shaders::kiLightingTerrainGroup, 1, 1, f4Push);
			++rProduction.iDispatches;
			iBase += iCount;
		}
	};
	auto Validate = [&](const State::Placement& rPlacement, uint32_t uiMode, uint32_t uiTile, uint32_t uiShift, int32_t iOriginX, int32_t iOriginY, int64_t iCount)
	{
		Pipeline& rPipeline = mpState->validatePipeline;
		vkCmdBindPipeline(vkCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, rPipeline.mVkPipeline);
		BindComputeDescriptorSets(vkCommandBuffer, rPipeline.mVkPipelineLayout, rPipeline.mExternalVkDescriptorSetLayout, iFramebuffer, rPipeline.mbPerCommandBuffer ? iFramebuffer : 0, rPipeline.mVkDescriptorSets);
		for (int64_t iBase = 0; iBase < iCount;)
		{
			int64_t iRun = std::min(iCount - iBase, iMaxGroups * shaders::kiLightingTerrainGroup);
			shaders::LightingTerrainValidatePush push {.iOriginX = iOriginX, .iOriginY = iOriginY, .uiPlacement = rPlacement.uiPlacement, .uiBase = static_cast<uint32_t>(iBase), .uiMode = uiMode, .uiShift = uiShift, .uiTile = uiTile};
			// TEMP START matched memory cost
			HashCommand(push.iOriginX); HashCommand(push.iOriginY); HashCommand(push.uiPlacement); HashCommand(push.uiBase);
			HashCommand(push.uiMode); HashCommand(push.uiShift); HashCommand(push.uiTile); HashCommand(iRun);
			// TEMP END matched memory cost
			vkCmdPushConstants(vkCommandBuffer, rPipeline.mVkPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), &push);
			vkCmdDispatch(vkCommandBuffer, static_cast<uint32_t>((iRun + shaders::kiLightingTerrainGroup - 1) / shaders::kiLightingTerrainGroup), 1, 1);
			++rValidation.iDispatches;
			iBase += iRun;
		}
	};
	auto DrawTile = [&](int32_t iOriginX, int32_t iOriginY, LightingTerrainOperations& rOperations)
	{
		Barrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, rOperations);
		// TEMP START matched memory cost
		HashCommand(iOriginX); HashCommand(iOriginY); HashCommand(rFramebuffer.iElevationPlacements);
		// TEMP END matched memory cost
		rFramebuffer.scratch.RecordBeginRenderPass(vkCommandBuffer);
		++rOperations.iRenderPasses;
		mpState->rasterPipeline.RecordDraw(iFramebuffer, vkCommandBuffer, rFramebuffer.iElevationPlacements, 0, {std::bit_cast<float>(iOriginX), std::bit_cast<float>(iOriginY), 0.0f, 0.0f});
		++rOperations.iDraws;
		vkCmdEndRenderPass(vkCommandBuffer);
	};
	if (bValidation)
	{
		Timestamp(2, rValidation);
		// TEMP START bounded terrain evidence
		vkCmdFillBuffer(vkCommandBuffer, rFramebuffer.evidence.GetBuffer(), 0, VK_WHOLE_SIZE, 0);
		++rValidation.iFills;
		// TEMP END bounded terrain evidence
		for (Buffer* pBuffer : {&rFramebuffer.writes, &rFramebuffer.originals, &rFramebuffer.tileResults})
		{
			vkCmdFillBuffer(vkCommandBuffer, pBuffer->GetBuffer(), 0, VK_WHOLE_SIZE, 0);
			++rValidation.iFills;
		}
		static constexpr VkDeviceSize kVkSummaryOffset = offsetof(shaders::LightingTerrainValidation, uiCompletedSerial);
		vkCmdFillBuffer(vkCommandBuffer, rFramebuffer.summary.GetBuffer(), kVkSummaryOffset, sizeof(shaders::LightingTerrainValidation) - kVkSummaryOffset, 0);
		++rValidation.iFills;
		vkCmdUpdateBuffer(vkCommandBuffer, rFramebuffer.summary.GetBuffer(), 0, sizeof(uint64_t), &rFramebuffer.uiValidationSerial);
		++rValidation.iUpdates;
		Barrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, rValidation);
		rFramebuffer.witness.RecordBeginRenderPass(vkCommandBuffer);
		++rValidation.iRenderPasses;
		mpState->witnessPipeline.RecordBindPipelineAndDescriptors(iFramebuffer, vkCommandBuffer);
		for (const State::Placement& rPlacement : rFramebuffer.placements)
		{
			VkBuffer buffer = gpIslands->mIslandMeshArena.GetBuffer();
			vkCmdBindVertexBuffers(vkCommandBuffer, 0, 1, &buffer, &rPlacement.vkXYOffset);
			vkCmdDraw(vkCommandBuffer, rPlacement.uiVertexCount, 1, 0, rPlacement.uiPlacement);
			++rValidation.iDraws;
		}
		vkCmdEndRenderPass(vkCommandBuffer);
		Barrier(VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, rValidation);
		Timestamp(3, rValidation);
	}
	for (const State::Placement& rPlacement : rFramebuffer.placements) Dispatch(rPlacement, std::numeric_limits<int32_t>::max(), std::numeric_limits<int32_t>::max());
	uint32_t uiTile = 0;
	for (const State::Placement& rPlacement : rFramebuffer.placements)
	{
		for (int64_t iY = rPlacement.iMinTileY; iY <= rPlacement.iMaxTileY; ++iY)
		{
			for (int64_t iX = rPlacement.iMinTileX; iX <= rPlacement.iMaxTileX; ++iX, ++uiTile)
			{
				int32_t iOriginX = static_cast<int32_t>(iX * shaders::kiLightingTerrainTile);
				int32_t iOriginY = static_cast<int32_t>(iY * shaders::kiLightingTerrainTile);
				uint32_t uiQuery = 6 + 4 * uiTile;
				Timestamp(uiQuery, rProduction);
				DrawTile(iOriginX, iOriginY, rProduction);
				Dispatch(rPlacement, iOriginX, iOriginY);
				Timestamp(uiQuery + 1, rProduction);
				if (bValidation)
				{
					Timestamp(uiQuery + 2, rValidation);
					VkImageMemoryBarrier image {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, .srcAccessMask = VK_ACCESS_SHADER_READ_BIT, .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT, .oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .image = rFramebuffer.scratch.mVkImage, .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1}};
					VkMemoryBarrier reuse {.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER, .srcAccessMask = VK_ACCESS_SHADER_READ_BIT, .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT};
					vkCmdPipelineBarrier(vkCommandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &reuse, 0, nullptr, 1, &image);
					++rValidation.iBarriers;
					VkBufferImageCopy copy {.imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .layerCount = 1}, .imageExtent = rFramebuffer.scratch.mInfo.vkExtent3D};
					vkCmdCopyImageToBuffer(vkCommandBuffer, rFramebuffer.scratch.mVkImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, rFramebuffer.baseTile.GetBuffer(), 1, &copy);
					++rValidation.iCopies;
					image.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
					image.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
					image.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
					image.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
					VkMemoryBarrier memory {.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER, .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT, .dstAccessMask = VK_ACCESS_SHADER_READ_BIT};
					vkCmdPipelineBarrier(vkCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memory, 0, nullptr, 1, &image);
					++rValidation.iBarriers;
					Barrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, rValidation);
					Validate(rPlacement, 1, uiTile, 0, iOriginX, iOriginY, rPlacement.uiVertexCount);
	for (uint32_t uiMode : {2ui32, 4ui32})
					{
						static constexpr int32_t kpiShifts[4][2] {{2, 0}, {-2, 0}, {0, 2}, {0, -2}};
						for (uint32_t uiShift = 0; uiShift < 4; ++uiShift)
						{
							int32_t iScale = uiMode == 2 ? 1 : shaders::kiLightingTerrainTile / 2;
							DrawTile(iOriginX + iScale * kpiShifts[uiShift][0], iOriginY + iScale * kpiShifts[uiShift][1], rValidation);
							Barrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, rValidation);
							Validate(rPlacement, uiMode, uiTile, uiShift, iOriginX, iOriginY, rPlacement.uiVertexCount);
						}
					}
					Timestamp(uiQuery + 3, rValidation);
				}
			}
		}
	}
	LightingAccelerationStructure::RecordInputBarrier(vkCommandBuffer);
	++rProduction.iBarriers;
	for (State::Placement& rPlacement : rFramebuffer.placements)
	{
		VkAccelerationStructureGeometryKHR geometry = State::GetGeometry(rFramebuffer, rPlacement);
		VkAccelerationStructureBuildRangeInfoKHR range {.primitiveCount = rPlacement.uiPrimitiveCount};
		rPlacement.acceleration.RecordBuild(vkCommandBuffer, std::span(&geometry, 1), std::span(&range, 1));
		++rProduction.iBuilds;
	}
	LightingAccelerationStructure::RecordBuildBarrier(vkCommandBuffer);
	++rProduction.iBarriers;
	if (bValidation)
	{
		Timestamp(4, rValidation);
		Barrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, rValidation);
		for (const State::Placement& rPlacement : rFramebuffer.placements) Validate(rPlacement, 0, 0, 0, 0, 0, std::max<int64_t>(rPlacement.uiVertexCount, 3i64 * rPlacement.uiPrimitiveCount));
		Barrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, rValidation);
		// TEMP START bounded terrain evidence
		for (const State::Placement& rPlacement : rFramebuffer.placements) Validate(rPlacement, 6, 0, 0, 0, 0, 3i64 * rPlacement.uiPrimitiveCount);
		Barrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, rValidation);
		// TEMP END bounded terrain evidence
		Validate(rFramebuffer.placements.front(), 3, uiTile, 0, 0, 0, std::max<int64_t>(1, uiTile));
		Barrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, rValidation);
		VkBufferCopy summaryCopy {.size = sizeof(shaders::LightingTerrainValidation)};
		vkCmdCopyBuffer(vkCommandBuffer, rFramebuffer.summary.GetBuffer(), rFramebuffer.readback->vkStagingBuffer, 1, &summaryCopy);
		VkBufferCopy tileCopy {.size = uiTile * sizeof(shaders::LightingTerrainTileResult)};
		vkCmdCopyBuffer(vkCommandBuffer, rFramebuffer.tileResults.GetBuffer(), rFramebuffer.tileReadback->vkStagingBuffer, 1, &tileCopy);
		rValidation.iCopies += 2;
		// TEMP START bounded terrain evidence
		VkBufferCopy vkEvidenceCopy {.size = sizeof(shaders::LightingTerrainEvidence)};
		vkCmdCopyBuffer(vkCommandBuffer, rFramebuffer.evidence.GetBuffer(), rFramebuffer.pEvidenceReadback->vkStagingBuffer, 1, &vkEvidenceCopy);
		++rValidation.iCopies;
		// TEMP END bounded terrain evidence
		Barrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, rValidation);
		Timestamp(5, rValidation);
	}
	Timestamp(1, rProduction);
	// TEMP START matched memory cost
	if (commandHasher.has_value() && !commandHasher->Finish(rFramebuffer.matchedCommandDigest)) throw std::runtime_error("Matched command SHA256 failed");
	// TEMP END matched memory cost
	rFramebuffer.uiRecordedSerial = rFramebuffer.uiValidationSerial;
	rFramebuffer.uiRecordedPublication = rFramebuffer.pending.uiPublication;
}
// TEMP START matched memory cost
void LightingTerrain::RecordMatchedCostReceipt(int64_t iFramebuffer, const Buffer& rGeometry, nlohmann::json& rReceipt) const
{
	const State::Framebuffer& rFramebuffer = *mpState->framebuffers.at(iFramebuffer);
	const Buffer& rQuads = gpIslands->mIslandsStorageBuffers.at(iFramebuffer);
	const shaders::AxisAlignedQuadLayout* pQuads = reinterpret_cast<const shaders::AxisAlignedQuadLayout*>(rQuads.mpMappedMemory);
	const VkDrawIndexedIndirectCommand* pDraws = gpIslands->mppIslandsIndirectMappedVkDrawIndexedIndirectCommands.at(iFramebuffer);
	const shaders::GlobalLayout& rGlobal = *reinterpret_cast<const shaders::GlobalLayout*>(gpBufferManager->mGlobalLayoutUniformBuffers.at(iFramebuffer).mpMappedMemory);
	const shaders::MainLayout& rMain = *reinterpret_cast<const shaders::MainLayout*>(gpBufferManager->mMainLayoutUniformBuffers.at(iFramebuffer).mpMappedMemory);
	common::Sha256Hasher hasher;
	auto Hash = [&]<typename T>(const T& rValue)
	{
		hasher.Update(std::as_writable_bytes(std::span(const_cast<T*>(&rValue), 1)));
	};
	auto Hash4 = [&](const XMFLOAT4& rValue)
	{
		Hash(rValue.x); Hash(rValue.y); Hash(rValue.z); Hash(rValue.w);
	};
	rReceipt["schema"] = "TEMP matched-memory-cost-v1; little-endian raw fields in source order";
	rReceipt["provenance"] = "Immutable pack/manifest plus validated upload ranges and CPU publication owners; not an independent GPU-byte or native descriptor hash";
	rReceipt["activeQuads"] = rFramebuffer.iElevationPlacements;
	Hash(rFramebuffer.iElevationPlacements);
	bool bTailZero = true;
	for (int64_t i = 0; i < kiMaxActivePlacements; ++i)
	{
		const shaders::AxisAlignedQuadLayout& rQuad = pQuads[i];
		if (i < rFramebuffer.iElevationPlacements)
		{
			Hash4(rQuad.f4VertexRect); Hash4(rQuad.f4TextureRect);
			Hash(rQuad.fRotation); Hash(rQuad.uiTextureSlot); Hash(rQuad.f4Parameters.x);
		}
		else
		{
			bTailZero &= rQuad.f4VertexRect.z == 0.0f && rQuad.f4VertexRect.w == 0.0f;
		}
	}
	rReceipt["unusedQuadTail"] = {{"first", rFramebuffer.iElevationPlacements}, {"end", kiMaxActivePlacements}, {"zeroWidthAndHeight", bTailZero}, {"scope", "fixed-arena visible elevation prepass; scratch draws only activeQuads; no padding requirement"}};
	Hash4(rGlobal.f4VisibleArea);
	Hash(rGlobal.fSeaFloorElevation); Hash(rGlobal.fSeaFloorElevationInverse);
	Hash(rGlobal.fUnderwaterMaskThreshold); Hash(rGlobal.fWaterUnderseaCompressionInverse);
	Hash(rMain.f4x4ViewProjection);
	auto Image = [&](std::string_view name, const Texture& rTexture)
	{
		const TextureInfo& rInfo = rTexture.mInfo;
		Hash(rInfo.vkFormat); Hash(rInfo.vkExtent3D.width); Hash(rInfo.vkExtent3D.height); Hash(rInfo.vkExtent3D.depth);
		Hash(rInfo.iMipLevels); Hash(rInfo.iArrayLayers); Hash(rInfo.vkImageAspectFlags); Hash(rInfo.vkSampleCountFlagBits);
		rReceipt["images"][name] = {{"image", reinterpret_cast<uint64_t>(rTexture.mVkImage)}, {"view", reinterpret_cast<uint64_t>(rTexture.mVkImageView)}, {"generation", rTexture.miGeneration}, {"format", rInfo.vkFormat}, {"extent", {rInfo.vkExtent3D.width, rInfo.vkExtent3D.height, rInfo.vkExtent3D.depth}}, {"viewRange", {rInfo.vkImageAspectFlags, 0, rInfo.iMipLevels, 0, rInfo.iArrayLayers}}, {"real", rTexture.mVkImage != VK_NULL_HANDLE}};
	};
	Image("visibleElevation", gpTextureManager->mRenderTargetTextures.mTerrainElevationTexture);
	Image("scratch", rFramebuffer.scratch);
	for (float fClear : rFramebuffer.scratch.mInfo.vkRenderPassClearColorValue.float32) Hash(fClear);
	Hash(rFramebuffer.scratch.mInfo.vkRenderPassAttachmentLoadOp);
	Hash(rFramebuffer.scratch.mInfo.vkRenderPassFinalImageLayout);
	Hash(gpIslands->miTemplateCount);
	for (int64_t i = 0; i < gpIslands->miTemplateCount; ++i)
	{
		common::crc_t crc = gpIslandTerrain->mIslandCrcsSorted.at(i);
		const IslandTemplate& rTemplate = gpIslandTerrain->mIslands.at(crc);
		const VkDrawIndexedIndirectCommand& rDraw = pDraws[i];
		Hash(crc); Hash(rDraw.indexCount); Hash(rDraw.instanceCount); Hash(rDraw.firstIndex); Hash(rDraw.vertexOffset); Hash(rDraw.firstInstance);
		int64_t iEnd = i + 1 < gpIslands->miTemplateCount ? pDraws[i + 1].firstInstance : rFramebuffer.iElevationPlacements;
		rReceipt["runs"].push_back({{"template", crc}, {"first", rDraw.firstInstance}, {"end", iEnd}, {"indices", rDraw.indexCount}, {"meshVisible", rDraw.instanceCount}, {"firstIndex", rDraw.firstIndex}, {"vertexOffset", rDraw.vertexOffset}});
		if (iEnd == rDraw.firstInstance) continue;
		const LazyChunk& rChunk = gpFileManager->mpPackChunks->mLazyChunkMap.at(crc);
		int64_t iHeightBytes = rTemplate.iHeightmapWidth * rTemplate.iHeightmapHeight * sizeof(uint16_t);
		int64_t iXYBytes = rTemplate.iMeshVertexCount * sizeof(XMFLOAT2);
		int64_t iIndexBytes = rTemplate.iMeshIndexCount * sizeof(uint32_t);
		Hash(rTemplate.iTextureSlot); Hash(rTemplate.iHeightmapWidth); Hash(rTemplate.iHeightmapHeight);
		Hash(rTemplate.iMeshVertexCount); Hash(rTemplate.iMeshIndexCount);
		Hash(rChunk.location.contentCrc); Hash(rChunk.header.iSize);
		Hash(rChunk.location.uiOffset); Hash(rChunk.location.uiSize);
		Hash(rTemplate.iMeshVertexOffset); Hash(rTemplate.iMeshIndexOffset);
		Image(std::format("heightSlot{}", rTemplate.iTextureSlot), *gpTextureManager->mRenderTargetTextures.mElevationTextures.at(rTemplate.iTextureSlot));
		bool bSourceRanges = iHeightBytes <= rChunk.header.iSize && iXYBytes <= rChunk.header.iSize - iHeightBytes && iIndexBytes <= rChunk.header.iSize - iHeightBytes - iXYBytes;
		bool bArenaRanges = rTemplate.iMeshVertexOffset >= 0 && iXYBytes <= gpIslands->mIslandMeshArena.mInfo.iDataSize - rTemplate.iMeshVertexOffset && rTemplate.iMeshIndexOffset >= 0 && iIndexBytes <= gpIslands->mIslandMeshArena.mInfo.iDataSize - rTemplate.iMeshIndexOffset;
		rReceipt["contributors"].push_back({{"template", crc}, {"slot", rTemplate.iTextureSlot}, {"gpuResident", rTemplate.bGpuResident}, {"meshResidency", static_cast<int32_t>(rTemplate.eMeshResidency)}, {"sourceContentCrc", rChunk.location.contentCrc}, {"packOffset", rChunk.location.uiOffset}, {"packBytes", rChunk.location.uiSize}, {"heightPointerMatchesPayload", reinterpret_cast<const std::byte*>(rTemplate.puiHeightmapHalf) == rChunk.pData}, {"payloadBytes", rChunk.header.iSize}, {"heightRange", {0, iHeightBytes}}, {"xyRange", {iHeightBytes, iXYBytes}}, {"indexRange", {iHeightBytes + iXYBytes, iIndexBytes}}, {"sourceRangesValid", bSourceRanges}, {"arenaRangesValid", bArenaRanges}, {"xyStride", sizeof(XMFLOAT2)}, {"indexStride", sizeof(uint32_t)}});
	}
	const shaders::LightingTerrainWork* pWork = reinterpret_cast<const shaders::LightingTerrainWork*>(rFramebuffer.work.mpMappedMemory);
	auto Address = [&](uint64_t uiAddress, const Buffer& rOwner, uint64_t uiBytes)
	{
		uint64_t uiBase = rOwner.GetDeviceAddress();
		uint64_t uiOffset = uiAddress - uiBase;
		bool bValid = uiAddress >= uiBase && uiOffset <= static_cast<uint64_t>(rOwner.mInfo.iDataSize) && uiBytes <= static_cast<uint64_t>(rOwner.mInfo.iDataSize) - uiOffset;
		Hash(uiOffset); Hash(uiBytes);
		return nlohmann::json{{"owner", rOwner.mInfo.name}, {"buffer", reinterpret_cast<uint64_t>(rOwner.GetBuffer())}, {"offset", uiOffset}, {"bytes", uiBytes}, {"valid", bValid}};
	};
	for (const State::Placement& rPlacement : rFramebuffer.placements)
	{
		const shaders::LightingTerrainWork& rWork = pWork[rPlacement.uiPlacement];
		Hash(rPlacement.uiPlacement); Hash(rWork.uiTemplate); Hash(rWork.uiVertexCount); Hash(rWork.uiPrimitiveCount); Hash(rWork.uiValidation);
		Hash(rWork.iFootprintMinTileX); Hash(rWork.iFootprintMinTileY); Hash(rWork.iFootprintMaxTileX); Hash(rWork.iFootprintMaxTileY);
		Hash(rWork.uiBuildPrimitiveCount); Hash(rWork.uiBuildVertexCount);
		Hash(rPlacement.iMinTileX); Hash(rPlacement.iMinTileY); Hash(rPlacement.iMaxTileX); Hash(rPlacement.iMaxTileY);
		nlohmann::json work = {{"placement", rPlacement.uiPlacement}, {"template", rWork.uiTemplate}, {"serial", rWork.uiSerial}, {"tileBounds", {rPlacement.iMinTileX, rPlacement.iMinTileY, rPlacement.iMaxTileX, rPlacement.iMaxTileY}}};
		work["xy"] = Address(rWork.uiXYAddress, gpIslands->mIslandMeshArena, rWork.uiVertexCount * sizeof(XMFLOAT2));
		work["indices"] = Address(rWork.uiIndexAddress, gpIslands->mIslandMeshArena, rWork.uiPrimitiveCount * 3ull * sizeof(uint32_t));
		work["xyz"] = Address(rWork.uiXYZAddress, rFramebuffer.vertices, rWork.uiVertexCount * 3ull * sizeof(float));
		work["hitGeometry"] = Address(rWork.uiHitGeometryAddress, rGeometry, sizeof(shaders::LightingGeometry));
		if (rWork.uiValidation != 0)
		{
			work["writes"] = Address(rWork.uiWritesAddress, rFramebuffer.writes, rWork.uiVertexCount * sizeof(uint32_t));
			work["originals"] = Address(rWork.uiOriginalAddress, rFramebuffer.originals, rWork.uiVertexCount * sizeof(shaders::LightingTerrainOriginal));
			work["summary"] = Address(rWork.uiSummaryAddress, rFramebuffer.summary, sizeof(shaders::LightingTerrainValidation));
			work["baseTile"] = Address(rWork.uiBaseTileAddress, rFramebuffer.baseTile, rFramebuffer.baseTile.mInfo.iDataSize);
			work["baseZ"] = Address(rWork.uiBaseZAddress, rFramebuffer.baseZ, rWork.uiVertexCount * sizeof(shaders::LightingTerrainBaseSample));
			work["tiles"] = Address(rWork.uiTileResultsAddress, rFramebuffer.tileResults, rFramebuffer.counts.iTiles * sizeof(shaders::LightingTerrainTileResult));
			work["evidence"] = Address(rWork.uiEvidenceAddress, rFramebuffer.evidence, sizeof(shaders::LightingTerrainEvidence));
		}
		rReceipt["work"].push_back(std::move(work));
	}
	Hash(gpInstanceManager->mVkPhysicalDeviceProperties.limits.maxComputeWorkGroupCount[0]);
	Hash(shaders::kiLightingTerrainGroup); Hash(shaders::kiLightingTerrainTile); Hash(shaders::kiLightingTerrainHalo);
	const TextureDescriptors& rDescriptors = gpTextureManager->mTextureDescriptors;
	rReceipt["pending"] = {{"adoptions", gpTextureUploadManager->miPendingAdoptions.load()}, {"restoration", gpIslandTerrainResidency->AnyRestorationPending()}, {"eviction", gpIslandTerrainResidency->AnyEvictionPending()}, {"descriptorPublication", rDescriptors.HasUnappliedPublications(iFramebuffer)}};
	for (int64_t iSlot = 0; iSlot < shaders::kiMaxIslands; ++iSlot)
	{
		auto it = rDescriptors.mIslandSlots.find(iSlot);
		if (it == rDescriptors.mIslandSlots.end()) continue;
		const IslandTemplate& rTemplate = gpIslandTerrain->mIslands.at(it->second.islandCrc);
		const Texture& rHeight = *gpTextureManager->mRenderTargetTextures.mElevationTextures[iSlot];
		rReceipt["retainedSlots"].push_back({{"slot", iSlot}, {"template", it->second.islandCrc}, {"resident", rTemplate.bGpuResident}, {"meshResidency", static_cast<int32_t>(rTemplate.eMeshResidency)}, {"image", reinterpret_cast<uint64_t>(rHeight.mVkImage)}, {"view", reinterpret_cast<uint64_t>(rHeight.mVkImageView)}, {"generation", rHeight.miGeneration}, {"ownedHeight", &rHeight == &rTemplate.elevationTexture && it->second.pElevationTexture == &rHeight}, {"real", rHeight.mVkImage != VK_NULL_HANDLE}});
	}
	for (DescriptorFlags flag : {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerElevation})
	{
		VkSampler vkSampler = gpTextureManager->GetSampler(flag);
		for (int64_t i = 0; i < TextureManager::kSamplerSlotCount; ++i)
		{
			if (gpTextureManager->mpSamplersVkSampler[i] != vkSampler) continue;
			const VkSamplerCreateInfo& rInfo = gpTextureManager->mpSamplerInfosVkSamplerCreateInfo[i];
			Hash(rInfo.magFilter); Hash(rInfo.minFilter); Hash(rInfo.mipmapMode);
			Hash(rInfo.addressModeU); Hash(rInfo.addressModeV); Hash(rInfo.addressModeW);
			Hash(rInfo.mipLodBias); Hash(rInfo.anisotropyEnable); Hash(rInfo.maxAnisotropy);
			Hash(rInfo.compareEnable); Hash(rInfo.compareOp); Hash(rInfo.minLod); Hash(rInfo.maxLod);
			Hash(rInfo.borderColor); Hash(rInfo.unnormalizedCoordinates);
		}
	}
	for (const Pipeline* pPipeline : {&gpPipelineManager->mpPipelines[kPipelineTerrainElevation], &mpState->rasterPipeline, &mpState->vertexPipeline, &mpState->validatePipeline, &mpState->witnessPipeline})
	{
		Hash(pPipeline->mInfo.flags.meFlags); Hash(pPipeline->mInfo.iPushConstantBytes);
		Hash(pPipeline->mInfo.vkExtent3D.width); Hash(pPipeline->mInfo.vkExtent3D.height);
		nlohmann::json pipeline = {{"name", pPipeline->mInfo.name}, {"set1", reinterpret_cast<uint64_t>(pPipeline->mVkDescriptorSets.at(iFramebuffer))}, {"pipeline", reinterpret_cast<uint64_t>(pPipeline->mVkPipeline)}, {"flags", std::to_underlying(pPipeline->mInfo.flags.meFlags)}, {"pushBytes", pPipeline->mInfo.iPushConstantBytes}};
		for (const DescriptorInfo& rInfo : pPipeline->mInfo.descriptorInfos)
		{
			int64_t iBinding = rInfo.iExplicitBinding;
			if (iBinding < 0) iBinding = &rInfo - pPipeline->mInfo.descriptorInfos.data();
			if (!PipelineDescriptorWriter::BindingExistsInShaderLayout(*pPipeline, iBinding)) continue;
			nlohmann::json binding = {{"binding", iBinding}, {"setIndex", Pipeline::ResolveBindingSetIndex(pPipeline->mInfo, iBinding)}};
			const Buffer* pBuffer = nullptr;
			if (rInfo.flags & DescriptorFlags::kGlobalLayoutUniformBuffers) pBuffer = &gpBufferManager->mGlobalLayoutUniformBuffers.at(iFramebuffer);
			else if (rInfo.flags & DescriptorFlags::kMainLayoutUniformBuffers) pBuffer = &gpBufferManager->mMainLayoutUniformBuffers.at(iFramebuffer);
			else if (rInfo.flags & DescriptorFlags::kPerCommandBufferStorageBuffers) pBuffer = &rInfo.pBuffers[iFramebuffer];
			else if (rInfo.flags & DescriptorFlags::kStorageBuffer) pBuffer = &rFramebuffer.work;
			if (pBuffer != nullptr) binding["buffer"] = {{"handle", reinterpret_cast<uint64_t>(pBuffer->GetBuffer())}, {"offset", 0}, {"range", "VK_WHOLE_SIZE"}, {"bytes", pBuffer->mInfo.iDataSize}};
			if (iBinding == 4 && pPipeline != &mpState->rasterPipeline)
			{
				const Texture& rImage = pPipeline == &mpState->witnessPipeline ? gpTextureManager->mRenderTargetTextures.mTerrainElevationTexture : rFramebuffer.scratch;
				binding["image"] = {{"view", reinterpret_cast<uint64_t>(rImage.mVkImageView)}, {"generation", rImage.miGeneration}, {"sampler", reinterpret_cast<uint64_t>(gpTextureManager->GetSampler(DescriptorFlags::kCombinedSamplers))}};
			}
			pipeline["bindings"].push_back(std::move(binding));
		}
		for (const auto& [crc, bindings] : rDescriptors.mTextureBindings)
		{
			for (const TextureDescriptors::TextureBinding& rBinding : bindings)
			{
				if (rBinding.pPipeline != pPipeline || rBinding.pTexture == nullptr) continue;
				pipeline["publishedImages"].push_back({{"key", crc}, {"binding", rBinding.iBinding}, {"element", rBinding.iArrayIndex}, {"view", reinterpret_cast<uint64_t>(rBinding.pTexture->mVkImageView)}, {"writtenGeneration", rBinding.iTextureGeneration}, {"liveGeneration", rBinding.pTexture->miGeneration}, {"placeholder", rBinding.bTextureUsesPlaceholder}, {"sampler", reinterpret_cast<uint64_t>(gpTextureManager->GetSampler(rBinding.samplerFlags))}});
			}
		}
		rReceipt["pipelines"].push_back(std::move(pipeline));
	}
	std::array<VmaBudget, VK_MAX_MEMORY_HEAPS> budgets {};
	vmaGetHeapBudgets(gpDeviceManager->mpAllocator, budgets.data());
	for (uint32_t i = 0; i < gpInstanceManager->mVkPhysicalDeviceMemoryProperties.memoryHeapCount; ++i)
	{
		const VmaBudget& rBudget = budgets[i];
		rReceipt["heapBudgets"].push_back({{"heap", i}, {"usage", rBudget.usage}, {"budget", rBudget.budget}, {"allocationBytes", rBudget.statistics.allocationBytes}, {"blockBytes", rBudget.statistics.blockBytes}});
	}
	std::array<uint8_t, 32> uiDigest {};
	if (!hasher.Finish(uiDigest)) throw std::runtime_error("Matched memory cost SHA256 failed");
	std::string digest;
	for (uint8_t uiByte : uiDigest) digest += std::format("{:02x}", uiByte);
	rReceipt["semanticSha256"] = digest;
	std::string commands;
	for (uint8_t uiByte : rFramebuffer.matchedCommandDigest) commands += std::format("{:02x}", uiByte);
	rReceipt["recordedCommandSha256"] = commands;
}
// TEMP END matched memory cost


LightingTerrainBytes LightingTerrain::GetResourceBytesDetail() const
{
	LightingTerrainBytes bytes;
	auto AddAllocation = [&](LightingAllocation& rTotal, VmaAllocation allocation, uint64_t uiPayload)
	{
		if (allocation == VK_NULL_HANDLE)
		{
			return;
		}
		VmaAllocationInfo info {};
		vmaGetAllocationInfo(gpDeviceManager->mpAllocator, allocation, &info);
		rTotal.uiPayload += uiPayload;
		rTotal.uiAllocated += info.size;
	};
	auto AddBuffer = [&](LightingAllocation& rTotal, const Buffer& rBuffer)
	{
		AddAllocation(rTotal, rBuffer.mHostVisibleVmaAllocation, rBuffer.mInfo.iDataSize);
		AddAllocation(rTotal, rBuffer.mDeviceLocalVmaAllocation, rBuffer.mInfo.iDataSize);
	};
	for (const std::unique_ptr<State::Framebuffer>& rpFramebuffer : mpState->framebuffers)
	{
		const State::Framebuffer& rFramebuffer = *rpFramebuffer;
		AddAllocation(bytes.scratch, rFramebuffer.scratch.mVmaAllocation, shaders::kiLightingTerrainScratch * shaders::kiLightingTerrainScratch * 2);
		AddBuffer(bytes.xyz, rFramebuffer.vertices);
		AddBuffer(bytes.work, rFramebuffer.work);
		AddBuffer(bytes.materials, rFramebuffer.materials);
		for (const State::Placement& rPlacement : rFramebuffer.placements)
		{
			AddBuffer(bytes.blas, rPlacement.acceleration.mStorage);
			AddBuffer(bytes.accelerationScratch, rPlacement.acceleration.mScratch);
		}
		for (const Buffer* pBuffer : {&rFramebuffer.writes, &rFramebuffer.originals, &rFramebuffer.baseZ, &rFramebuffer.baseTile, &rFramebuffer.summary, &rFramebuffer.tileResults}) AddBuffer(bytes.validation, *pBuffer);
		AddAllocation(bytes.validation, rFramebuffer.witness.mVmaAllocation, 2);
		if (rFramebuffer.readback != nullptr)
		{
			AddAllocation(bytes.validation, rFramebuffer.readback->vmaAllocation, sizeof(shaders::LightingTerrainValidation));
		}
		if (rFramebuffer.tileReadback != nullptr)
		{
			AddAllocation(bytes.validation, rFramebuffer.tileReadback->vmaAllocation, rFramebuffer.iTileReadbackBytes);
		}
		// TEMP START bounded terrain evidence
		AddBuffer(bytes.validation, rFramebuffer.evidence);
		if (rFramebuffer.pEvidenceReadback != nullptr)
		{
			AddAllocation(bytes.validation, rFramebuffer.pEvidenceReadback->vmaAllocation, sizeof(shaders::LightingTerrainEvidence));
		}
		// TEMP END bounded terrain evidence
	}
	return bytes;
}

uint64_t LightingTerrain::GetResourceBytes() const
{
	LightingTerrainBytes bytes = GetResourceBytesDetail();
	return bytes.scratch.uiAllocated + bytes.xyz.uiAllocated + bytes.work.uiAllocated + bytes.materials.uiAllocated + bytes.blas.uiAllocated + bytes.accelerationScratch.uiAllocated + bytes.validation.uiAllocated;
}

void LightingTerrain::WritePlacementIdentities(int64_t iFramebuffer, std::span<LightingTerrainPlacementIdentity> identities) const
{
	const State::Framebuffer& rFramebuffer = *mpState->framebuffers.at(iFramebuffer);
	for (int64_t i = 0; i < std::ssize(rFramebuffer.placements); ++i)
	{
		const State::Placement& rPlacement = rFramebuffer.placements.at(i);
		identities[i] = {.templateCrc = rPlacement.templateCrc, .uiPlacement = rPlacement.uiPlacement, .uiInstance = rFramebuffer.uiFirstInstance + static_cast<uint32_t>(i), .uiGeometry = rFramebuffer.uiFirstGeometry + static_cast<uint32_t>(i), .uiVertices = rPlacement.uiVertexCount, .uiTriangles = rPlacement.uiPrimitiveCount, .vkXY = rPlacement.vkXY, .vkXYZ = rFramebuffer.vertices.GetDeviceAddress() + rPlacement.vkXYZOffset, .vkIndices = rPlacement.vkIndices, .vkBLAS = rPlacement.acceleration.mVkAccelerationStructureKHR, .vkBLASAddress = rPlacement.acceleration.GetDeviceAddress(), .iMinTileX = rPlacement.iMinTileX, .iMinTileY = rPlacement.iMinTileY, .iMaxTileX = rPlacement.iMaxTileX, .iMaxTileY = rPlacement.iMaxTileY};
	}
}

const LightingTerrainCompletion& LightingTerrain::Complete(int64_t iFramebuffer)
{
	State::Framebuffer& rFramebuffer = *mpState->framebuffers.at(iFramebuffer);
	if (rFramebuffer.uiRecordedPublication == 0 || rFramebuffer.uiRecordedPublication != rFramebuffer.pending.uiPublication)
	{
		return rFramebuffer.completed;
	}
	rFramebuffer.completed = rFramebuffer.pending;
	LightingTerrainCompletion& rCompleted = rFramebuffer.completed;
	if (rFramebuffer.placements.empty())
	{
		return rCompleted;
	}
	if (rFramebuffer.uiRecordedSerial != 0)
	{
		std::ignore = std::memcpy(&rCompleted.validation, rFramebuffer.readback->vmaAllocationInfo.pMappedData, sizeof(rCompleted.validation));
		// TEMP START terrain validation serial evidence
		LOG(kTemp, kInfo, "TERRAIN_VALIDATION_SERIAL framebuffer={} recordedPublication={} pendingPublication={} recordedSerial={} requestedSerial={} completedSerial={} placements={} vertices={} tiles={} workgroups={}", iFramebuffer, rFramebuffer.uiRecordedPublication, rFramebuffer.pending.uiPublication, rFramebuffer.uiRecordedSerial, rCompleted.validation.uiRequestedSerial, rCompleted.validation.uiCompletedSerial, rFramebuffer.counts.iPlacements, rFramebuffer.counts.iVertices, rFramebuffer.counts.iTiles, rFramebuffer.counts.iWorkgroups);
		// TEMP END terrain validation serial evidence
		if (rCompleted.validation.uiRequestedSerial != rFramebuffer.uiRecordedSerial || rCompleted.validation.uiCompletedSerial != rFramebuffer.uiRecordedSerial)
		{
			throw std::runtime_error("Lighting terrain validation serial mismatch");
		}
		rCompleted.tiles = std::span<const shaders::LightingTerrainTileResult>(static_cast<const shaders::LightingTerrainTileResult*>(rFramebuffer.tileReadback->vmaAllocationInfo.pMappedData), static_cast<size_t>(rFramebuffer.counts.iTiles));
		// TEMP START bounded terrain evidence
		rCompleted.pEvidence = static_cast<const shaders::LightingTerrainEvidence*>(rFramebuffer.pEvidenceReadback->vmaAllocationInfo.pMappedData);
		// TEMP END bounded terrain evidence
	}
	uint32_t uiValidBits = gpInstanceManager->mVkQueueFamilyProperties.at(gpInstanceManager->miGraphicsQueueFamilyIndex).timestampValidBits;
	uint64_t uiMask = uiValidBits == 64 ? std::numeric_limits<uint64_t>::max() : ((1ui64 << uiValidBits) - 1);
	auto Duration = [&](uint32_t uiQuery)
	{
		uint64_t timestamps[2] {};
		CHECK_VK(vkGetQueryPoolResults(gpDeviceManager->mVkDevice, rFramebuffer.vkQueryPool, uiQuery, 2, sizeof(timestamps), timestamps, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT));
		double fMilliseconds = static_cast<double>((timestamps[1] - timestamps[0]) & uiMask) * gpInstanceManager->mVkPhysicalDeviceProperties.limits.timestampPeriod / 1'000'000.0;
		return std::chrono::duration<double, std::milli>(fMilliseconds);
	};
	std::chrono::duration<double, std::milli> total = Duration(0);
	if (rFramebuffer.uiRecordedSerial != 0)
	{
		rCompleted.durationValidation = Duration(2) + Duration(4);
	}
	for (int64_t i = 0; i < rFramebuffer.counts.iTiles; ++i)
	{
		uint32_t uiQuery = 6 + 4 * static_cast<uint32_t>(i);
		rCompleted.durationWorstTile = std::max(rCompleted.durationWorstTile, Duration(uiQuery));
		if (rFramebuffer.uiRecordedSerial != 0)
		{
			rCompleted.durationValidation += Duration(uiQuery + 2);
		}
	}
	rCompleted.durationPreparation = total - rCompleted.durationValidation;
	return rCompleted;
}
} // namespace engine

#endif // defined(BT_CLIENT)
