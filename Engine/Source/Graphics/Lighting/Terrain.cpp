#if defined(BT_CLIENT)

#include "Terrain.h"

#include "../../../Data/Shaders/Lighting/LightingTerrainCommon.h"
#include "Data/Shader.h"
#include "Frame/IslandTerrain.h"
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
		LightingAccelerationStructure acceleration;
	};

	struct Framebuffer
	{
		Buffer vertices;
		Buffer work;
		Buffer materials;
		std::vector<Placement> placements;
		LightingTerrainCounts counts;
	};

	std::vector<std::unique_ptr<Framebuffer>> framebuffers;
	Pipeline vertexPipeline;
	VkDeviceSize vkMaxAllocationSize = 0;

	static VkAccelerationStructureGeometryKHR GetGeometry(const Framebuffer& rFramebuffer, const Placement& rPlacement);
};

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
	mpState->framebuffers.reserve(static_cast<size_t>(std::ssize(gpSwapchainManager->mFramebuffers)));
	for (int64_t i = 0; i < std::ssize(gpSwapchainManager->mFramebuffers); ++i)
	{
		auto pFramebuffer = std::make_unique<State::Framebuffer>();
		pFramebuffer->work.Create(
		{
			.name = "LightingTerrainWork",
			.flags = {BufferFlags::kStorage, BufferFlags::kHostVisible, },
			.iDataSize = sizeof(shaders::LightingTerrainWork),
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
}

void LightingTerrain::DestroyPipelines()
{
	mpState->vertexPipeline.Destroy();
}

VkAccelerationStructureGeometryKHR LightingTerrain::State::GetGeometry(const Framebuffer& rFramebuffer, const Placement& rPlacement)
{
	return
	{
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
		.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
		.geometry =
		{
			.triangles =
			{
				.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
				.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
				.vertexData = {.deviceAddress = rFramebuffer.vertices.GetDeviceAddress() + rPlacement.vkXYZOffset, },
				.vertexStride = 3 * sizeof(float),
				.maxVertex = rPlacement.uiVertexCount - 1,
				.indexType = VK_INDEX_TYPE_UINT32,
				.indexData = {.deviceAddress = rPlacement.vkIndices, },
			},
		},
		.flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
	};
}

LightingTerrainCounts LightingTerrain::Prepare(int64_t iFramebuffer)
{
	// Heap: framebuffer-owned geometry grows after its fence completes; scalar work ranges survive command recording.
	ScopedSuppressAllocationTracking suppress;
	State::Framebuffer& rFramebuffer = *mpState->framebuffers.at(iFramebuffer);
	rFramebuffer.counts = {};
	const shaders::AxisAlignedQuadLayout* pQuads = reinterpret_cast<const shaders::AxisAlignedQuadLayout*>(gpIslands->mIslandsStorageBuffers.at(iFramebuffer).mpMappedMemory);
	const VkDrawIndexedIndirectCommand* pVkDraws = gpIslands->mppIslandsIndirectMappedVkDrawIndexedIndirectCommands.at(iFramebuffer);
	int64_t iPlacementCount = gpIslands->mLastWrittenCounts.at(iFramebuffer);
	int64_t iWorkBytes = iPlacementCount * static_cast<int64_t>(sizeof(shaders::LightingTerrainWork));
	if (iWorkBytes > rFramebuffer.work.mInfo.iDataSize)
	{
		rFramebuffer.work.Create({.name = "LightingTerrainWork", .flags = {BufferFlags::kStorage, BufferFlags::kHostVisible, }, .iDataSize = iWorkBytes, });
	}
	PipelineDescriptorWriter::UpdateStorageBuffer(mpState->vertexPipeline, iFramebuffer, 5, &rFramebuffer.work);
	const VkPhysicalDeviceLimits& rVkLimits = gpInstanceManager->mVkPhysicalDeviceProperties.limits;
	if (std::cmp_less(rVkLimits.maxComputeWorkGroupSize[0], shaders::kiLightingTerrainGroup) || std::cmp_less(rVkLimits.maxComputeWorkGroupInvocations, shaders::kiLightingTerrainGroup))
	{
		throw std::runtime_error("Lighting terrain compute group unsupported");
	}
	int64_t iRun = 0;
	for (int64_t i = 0; i < gpIslands->miTemplateCount; ++i)
	{
		const IslandTemplate& rTemplate = gpIslandTerrain->mIslands.at(gpIslandTerrain->mIslandCrcsSorted.at(i));
		int64_t iFirst = pVkDraws[i].firstInstance;
		int64_t iEnd = i + 1 < gpIslands->miTemplateCount ? pVkDraws[i + 1].firstInstance : iPlacementCount;
		if (rTemplate.eMeshResidency != IslandMeshResidency::kResident)
		{
			continue;
		}
		if (std::cmp_equal(pVkDraws[i].indexCount, 0))
		{
			continue;
		}
		if (!std::in_range<uint32_t>(rTemplate.iMeshVertexCount) || !std::in_range<uint32_t>(rTemplate.iMeshIndexCount / 3))
		{
			throw std::runtime_error("Lighting terrain acceleration geometry exceeds device limits");
		}
		if (std::cmp_greater(rTemplate.iMeshIndexCount / 3, gpInstanceManager->mVkPhysicalDeviceAccelerationStructurePropertiesKHR.maxPrimitiveCount))
		{
			throw std::runtime_error("Lighting terrain acceleration geometry exceeds device limits");
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
			rPlacement.vkIndices = gpIslands->mIslandMeshArena.GetDeviceAddress() + rTemplate.iMeshIndexOffset;
			rPlacement.vkXYZOffset = rFramebuffer.counts.iVertices * 3 * sizeof(float);
			rFramebuffer.counts.iVertices += uiVertices;
			rFramebuffer.counts.iTriangles += uiPrimitives;
		}
	}
	rFramebuffer.placements.resize(static_cast<size_t>(iRun));
	rFramebuffer.counts.iPlacements = iRun;
	if (iRun == 0)
	{
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
	return rFramebuffer.counts;
}

void LightingTerrain::Write(int64_t iFramebuffer, uint32_t uiFirstInstance, uint32_t uiFirstGeometry, std::span<shaders::LightingInstance> instances, std::span<shaders::LightingGeometry> geometries, std::span<VkAccelerationStructureInstanceKHR> accelerationInstances)
{
	State::Framebuffer& rFramebuffer = *mpState->framebuffers.at(iFramebuffer);
	for (int64_t i = 0; i < std::ssize(rFramebuffer.placements); ++i)
	{
		const State::Placement& rPlacement = rFramebuffer.placements.at(i);
		uint32_t uiInstance = uiFirstInstance + static_cast<uint32_t>(i);
		uint32_t uiGeometry = uiFirstGeometry + static_cast<uint32_t>(i);
		shaders::LightingTerrainWork work {.uiXYAddress = rPlacement.vkXY, .uiXYZAddress = rFramebuffer.vertices.GetDeviceAddress() + rPlacement.vkXYZOffset, .uiVertexCount = rPlacement.uiVertexCount};
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
		VkAccelerationStructureInstanceKHR& rVkAccelerationInstance = accelerationInstances[uiInstance];
		rVkAccelerationInstance = {};
		rVkAccelerationInstance.transform.matrix[0][0] = 1.0f;
		rVkAccelerationInstance.transform.matrix[1][1] = 1.0f;
		rVkAccelerationInstance.transform.matrix[2][2] = 1.0f;
		rVkAccelerationInstance.instanceCustomIndex = uiInstance;
		rVkAccelerationInstance.mask = 0xff;
		rVkAccelerationInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		rVkAccelerationInstance.accelerationStructureReference = rPlacement.acceleration.GetDeviceAddress();
	}
}

void LightingTerrain::RecordPreparation(VkCommandBuffer vkCommandBuffer, int64_t iFramebuffer)
{
	State::Framebuffer& rFramebuffer = *mpState->framebuffers.at(iFramebuffer);
	if (rFramebuffer.placements.empty())
	{
		return;
	}
	VkMemoryBarrier vkBarrier {.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER, .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, .dstAccessMask = VK_ACCESS_SHADER_READ_BIT};
	vkCmdPipelineBarrier(vkCommandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, 0, 1, &vkBarrier, 0, nullptr, 0, nullptr);
	LightingAccelerationStructure::RecordInputBarrier(vkCommandBuffer);
	int64_t iMaxGroups = gpInstanceManager->mVkPhysicalDeviceProperties.limits.maxComputeWorkGroupCount[0];
	auto Dispatch = [&](const State::Placement& rPlacement)
	{
		for (int64_t i = 0; std::cmp_less(i, rPlacement.uiVertexCount);)
		{
			int64_t iCount = std::min<int64_t>(rPlacement.uiVertexCount - i, iMaxGroups * shaders::kiLightingTerrainGroup);
			XMFLOAT4 f4Push {0.0f, 0.0f, std::bit_cast<float>(rPlacement.uiPlacement), std::bit_cast<float>(static_cast<uint32_t>(i))};
			mpState->vertexPipeline.RecordCompute(iFramebuffer, vkCommandBuffer, (iCount + shaders::kiLightingTerrainGroup - 1) / shaders::kiLightingTerrainGroup, 1, 1, f4Push);
			i += iCount;
		}
	};
	for (const State::Placement& rPlacement : rFramebuffer.placements) Dispatch(rPlacement);
	LightingAccelerationStructure::RecordInputBarrier(vkCommandBuffer);
	for (State::Placement& rPlacement : rFramebuffer.placements)
	{
		VkAccelerationStructureGeometryKHR vkGeometry = State::GetGeometry(rFramebuffer, rPlacement);
		VkAccelerationStructureBuildRangeInfoKHR vkRange {.primitiveCount = rPlacement.uiPrimitiveCount};
		rPlacement.acceleration.RecordBuild(vkCommandBuffer, std::span(&vkGeometry, 1), std::span(&vkRange, 1));
	}
	LightingAccelerationStructure::RecordBuildBarrier(vkCommandBuffer);
}

} // namespace engine

#endif // defined(BT_CLIENT)
