#if defined(BT_CLIENT)

#include "LightingModels.h"

#include "../../../Data/Shaders/Lighting/LightingModelsCommon.h"
#include "File/PackChunks.h"
#include "LightingAccelerationStructure.h"
#include "LightingState.h"

namespace engine
{

struct LightingModels::State
{
	struct Primitive
	{
		uint32_t uiMaterial = 0;
		uint32_t uiFirstIndex = 0;
		uint32_t uiIndexCount = 0;
	};

	struct Asset
	{
		enum class Flags : uint8_t { kAnimated = 1, kBuilt = 2 };
		common::crc_t bufferCrc = 0;
		uint32_t uiVertexCount = 0;
		uint32_t uiMaterialCount = 0;
		common::Flags<Flags> flags;
		Buffer materials;
		Buffer vertexMaterials;
		std::vector<Primitive> primitives;
		LightingAccelerationStructure acceleration;
	};

	struct Deformed
	{
		Buffer vertices;
		LightingAccelerationStructure acceleration;
	};

	std::unordered_map<common::crc_t, Asset> assets;
	using InstanceKey = std::tuple<common::crc_t, common::crc_t, int64_t>;
	struct InstanceKeyHash
	{
		std::size_t operator()(const InstanceKey& rKey) const
		{
			return std::hash<common::crc_t> {}(std::get<0>(rKey)) ^ (std::hash<common::crc_t> {}(std::get<1>(rKey)) << 1) ^ (std::hash<int64_t> {}(std::get<2>(rKey)) << 2);
		}
	};
	using Framebuffer = std::unordered_map<InstanceKey, Deformed, InstanceKeyHash>;
	std::vector<std::unique_ptr<Framebuffer>> framebuffers;
	VkPipeline vkPipeline = VK_NULL_HANDLE;
	VkPipelineLayout vkPipelineLayout = VK_NULL_HANDLE;

	Asset& GetAsset(common::crc_t sceneCrc);
	void GetBuildGeometry(const Asset& rAsset, const Deformed* pDeformed, std::span<VkAccelerationStructureGeometryKHR> geometries, std::span<VkAccelerationStructureBuildRangeInfoKHR> ranges) const;
	void CreateAcceleration(Asset& rAsset, Deformed* pDeformed);
};

LightingModels::LightingModels()
: mpState(std::make_unique<State>())
{
}

LightingModels::~LightingModels()
{
	DestroyPipeline();
}

void LightingModels::DestroyPipeline()
{
	if (mpState->vkPipeline != VK_NULL_HANDLE)
	{
		vkDestroyPipeline(gpDeviceManager->mVkDevice, mpState->vkPipeline, nullptr);
		mpState->vkPipeline = VK_NULL_HANDLE;
	}
	if (mpState->vkPipelineLayout != VK_NULL_HANDLE)
	{
		vkDestroyPipelineLayout(gpDeviceManager->mVkDevice, mpState->vkPipelineLayout, nullptr);
		mpState->vkPipelineLayout = VK_NULL_HANDLE;
	}
}

void LightingModels::Create(const Shader& rDeformationShader)
{
	int64_t iFirstFramebuffer = std::ssize(mpState->framebuffers);
	mpState->framebuffers.resize(static_cast<size_t>(std::ssize(gpSwapchainManager->mFramebuffers)));
	for (int64_t i = iFirstFramebuffer; i < std::ssize(mpState->framebuffers); ++i)
	{
		mpState->framebuffers.at(i) = std::make_unique<State::Framebuffer>();
	}
	VkPushConstantRange vkPushConstantRange
	{
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
		.size = sizeof(shaders::LightingModelDeform),
	};
	VkPipelineLayoutCreateInfo vkLayoutInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &vkPushConstantRange,
	};
	CHECK_VK(vkCreatePipelineLayout(gpDeviceManager->mVkDevice, &vkLayoutInfo, nullptr, &mpState->vkPipelineLayout));
	VkComputePipelineCreateInfo vkPipelineInfo
	{
		.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		.stage =
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_COMPUTE_BIT,
			.module = rDeformationShader.mVkShaderModule,
			.pName = "main",
		},
		.layout = mpState->vkPipelineLayout,
	};
	CHECK_VK(vkCreateComputePipelines(gpDeviceManager->mVkDevice, VK_NULL_HANDLE, 1, &vkPipelineInfo, nullptr, &mpState->vkPipeline));
}

LightingModels::State::Asset& LightingModels::State::GetAsset(common::crc_t sceneCrc)
{
	auto [it, bInserted] = assets.try_emplace(sceneCrc);
	Asset& rAsset = it->second;
	if (!bInserted)
	{
		return rAsset;
	}
	const std::unordered_map<common::crc_t, EagerChunk>& rChunks = gpFileManager->mpPackChunks->GetEagerChunkMap();
	const EagerChunk& rSceneChunk = rChunks.at(sceneCrc);
	const common::SceneHeader& rScene = rSceneChunk.pHeader->sceneHeader;
	const EagerChunk& rModelChunk = rChunks.at(rScene.modelCrc);
	const common::ModelHeader& rModel = rModelChunk.pHeader->modelHeader;
	int64_t iMaterialOffset = common::SceneHeader::MaterialDataOffset(rScene.uiTextureCount, rScene.uiMaterialCount);
	if (!(rSceneChunk.pHeader->flags & common::ChunkFlags::kScene))
	{
		throw std::ios_base::failure("LightingModels scene");
	}
	if (std::cmp_greater(rScene.uiTextureCount, common::SceneHeader::kiMaxTextures))
	{
		throw std::ios_base::failure("LightingModels scene");
	}
	if (std::cmp_equal(rScene.uiMaterialCount, 0))
	{
		throw std::ios_base::failure("LightingModels scene");
	}
	if (std::cmp_greater(rScene.uiMaterialCount, common::SceneHeader::kiMaxMaterials))
	{
		throw std::ios_base::failure("LightingModels scene");
	}
	if (iMaterialOffset + rScene.uiMaterialCount * static_cast<int64_t>(sizeof(common::MaterialShaderData)) > rSceneChunk.iDataSize)
	{
		throw std::ios_base::failure("LightingModels scene");
	}
	if (std::cmp_not_equal(rModel.iStride, sizeof(common::ModelVertex)))
	{
		throw std::ios_base::failure("LightingModels scene");
	}
	if (!std::in_range<uint32_t>(rModel.iVertexCount))
	{
		throw std::ios_base::failure("LightingModels scene");
	}
	rAsset.bufferCrc = rScene.modelCrc;
	rAsset.uiVertexCount = static_cast<uint32_t>(rModel.iVertexCount);
	rAsset.uiMaterialCount = rScene.uiMaterialCount;
	if (rScene.bHasAnimation)
	{
		rAsset.flags.Set(Asset::Flags::kAnimated);
	}
	const uint32_t* puiStarts = reinterpret_cast<const uint32_t*>(rSceneChunk.pData + common::SceneHeader::IndexStartsOffset(rScene.uiTextureCount));
	const common::MaterialShaderData* pMaterials = reinterpret_cast<const common::MaterialShaderData*>(rSceneChunk.pData + iMaterialOffset);
	const common::crc_t* pTextureCrcs = reinterpret_cast<const common::crc_t*>(rSceneChunk.pData);
	rAsset.materials.Create(
	{
		.name = "LightingModelMaterials",
		.flags = {BufferFlags::kStorage, BufferFlags::kHostVisible, BufferFlags::kDeviceAddress},
		.iDataSize = rScene.uiMaterialCount * static_cast<int64_t>(sizeof(shaders::LightingModelMaterial)),
	});
	shaders::LightingModelMaterial* pOutput = reinterpret_cast<shaders::LightingModelMaterial*>(rAsset.materials.mpMappedMemory);
	uint32_t* puiVertexMaterials = nullptr;
	if (rAsset.flags & Asset::Flags::kAnimated && std::cmp_greater(rAsset.uiVertexCount, 0))
	{
		rAsset.vertexMaterials.Create(
		{
			.name = "LightingModelVertexMaterials",
			.flags = {BufferFlags::kStorage, BufferFlags::kHostVisible, BufferFlags::kDeviceAddress},
			.iDataSize = rAsset.uiVertexCount * static_cast<int64_t>(sizeof(uint32_t)),
		});
		puiVertexMaterials = reinterpret_cast<uint32_t*>(rAsset.vertexMaterials.mpMappedMemory);
		std::ignore = std::fill_n(puiVertexMaterials, rAsset.uiVertexCount, std::numeric_limits<uint32_t>::max());
	}
	rAsset.primitives.reserve(rScene.uiMaterialCount);
	for (int64_t i = 0; std::cmp_less(i, rScene.uiMaterialCount); ++i)
	{
		uint32_t uiMaterial = static_cast<uint32_t>(i);
		const common::MaterialShaderData& rMaterial = pMaterials[i];
		pOutput[i] = {};
		pOutput[i].f4BaseColorFactor = rMaterial.f4BaseColorFactor;
		pOutput[i].fMetallicFactor = rMaterial.fMetallicFactor;
		pOutput[i].iColorTextureSet = rMaterial.iColorTextureSet;
		pOutput[i].iPhysicalTextureSet = rMaterial.iPhysicalDescriptorTextureSet;
		pOutput[i].iNormalTextureSet = rMaterial.iNormalTextureSet;
		if (rMaterial.iNormalTextureSet >= 0)
		{
			if (rMaterial.uiNormalTextureIndex >= rScene.uiTextureCount)
			{
				throw std::ios_base::failure("LightingModels normal texture");
			}
			pOutput[i].uiNormalTexture = static_cast<uint32_t>(gpTextureManager->mTextureDescriptors.CrcToIndex(pTextureCrcs[rMaterial.uiNormalTextureIndex]));
		}
		if (rMaterial.iColorTextureSet >= 0)
		{
			if (rMaterial.uiColorTextureIndex >= rScene.uiTextureCount)
			{
				throw std::ios_base::failure("LightingModels color texture");
			}
			pOutput[i].uiColorTexture = static_cast<uint32_t>(gpTextureManager->mTextureDescriptors.CrcToIndex(pTextureCrcs[rMaterial.uiColorTextureIndex]));
		}
		if (rMaterial.iPhysicalDescriptorTextureSet >= 0)
		{
			if (rMaterial.uiPhysicalDescriptorTextureIndex >= rScene.uiTextureCount)
			{
				throw std::ios_base::failure("LightingModels physical texture");
			}
			pOutput[i].uiPhysicalTexture = static_cast<uint32_t>(gpTextureManager->mTextureDescriptors.CrcToIndex(pTextureCrcs[rMaterial.uiPhysicalDescriptorTextureIndex]));
		}
		uint32_t uiEnd = std::cmp_less(i + 1, rScene.uiMaterialCount) ? puiStarts[i + 1] : static_cast<uint32_t>(rModel.iIndexCount);
		if (puiStarts[i] > uiEnd)
		{
			throw std::ios_base::failure("LightingModels primitive range");
		}
		if (std::cmp_greater(uiEnd, rModel.iIndexCount))
		{
			throw std::ios_base::failure("LightingModels primitive range");
		}
		if ((uiEnd - puiStarts[i]) % 3 != 0)
		{
			throw std::ios_base::failure("LightingModels primitive range");
		}
		if (rMaterial.fAlphaMask >= 2.0f)
		{
			continue;
		}
		if (puiStarts[i] == uiEnd)
		{
			continue;
		}
		for (int64_t j = puiStarts[i]; j < uiEnd; ++j)
		{
			uint32_t uiIndex = common::ModelHeader::UsesU16Indices(rModel.iVertexCount) ? reinterpret_cast<const uint16_t*>(rModelChunk.pData)[j] : reinterpret_cast<const uint32_t*>(rModelChunk.pData)[j];
			if (uiIndex >= rAsset.uiVertexCount)
			{
				throw std::ios_base::failure("LightingModels vertex index");
			}
			if (puiVertexMaterials != nullptr)
			{
				// Exporters append each primitive's vertices; mesh/material transforms never share a vertex slot.
				if (puiVertexMaterials[uiIndex] != std::numeric_limits<uint32_t>::max() && puiVertexMaterials[uiIndex] != uiMaterial)
				{
					throw std::ios_base::failure("LightingModels shared material vertex");
				}
				puiVertexMaterials[uiIndex] = uiMaterial;
			}
		}
		rAsset.primitives.push_back(Primitive { .uiMaterial = uiMaterial, .uiFirstIndex = puiStarts[i], .uiIndexCount = uiEnd - puiStarts[i], });
	}
	gpFileManager->mpPackChunks->mLoader.RequestChunkLoad(std::span(pTextureCrcs, rScene.uiTextureCount), LoadPriority::kNormal);
	if (!(rAsset.flags & Asset::Flags::kAnimated) && !rAsset.primitives.empty())
	{
		CreateAcceleration(rAsset, nullptr);
	}
	return rAsset;
}

void LightingModels::State::GetBuildGeometry(const Asset& rAsset, const Deformed* pDeformed, std::span<VkAccelerationStructureGeometryKHR> geometries, std::span<VkAccelerationStructureBuildRangeInfoKHR> ranges) const
{
	const Buffer& rBuffer = gpBufferManager->mModelMap.at(rAsset.bufferCrc);
	int64_t iIndexBytes = rBuffer.mInfo.vkIndexType == VK_INDEX_TYPE_UINT16 ? 2 : 4;
	VkDeviceAddress vkAddress = rBuffer.GetDeviceAddress();
	VkDeviceAddress vkVertices = vkAddress + common::ModelHeader::VerticesOffset(rBuffer.mInfo.iCount, iIndexBytes);
	for (int64_t i = 0; i < std::ssize(rAsset.primitives); ++i)
	{
		const Primitive& rPrimitive = rAsset.primitives.at(static_cast<size_t>(i));
		geometries[i] =
		{
			.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
			.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
			.geometry =
			{
				.triangles =
				{
					.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
					.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
					.vertexData = {.deviceAddress = pDeformed == nullptr ? vkVertices : pDeformed->vertices.GetDeviceAddress()},
					.vertexStride = pDeformed == nullptr ? static_cast<VkDeviceSize>(rBuffer.mInfo.iVertexStride) : sizeof(shaders::LightingModelVertex),
					.maxVertex = rAsset.uiVertexCount - 1,
					.indexType = rBuffer.mInfo.vkIndexType,
					.indexData = {.deviceAddress = vkAddress},
				},
			},
			.flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
		};
		ranges[i] = {.primitiveCount = rPrimitive.uiIndexCount / 3, .primitiveOffset = rPrimitive.uiFirstIndex * static_cast<uint32_t>(iIndexBytes)};
	}
}

void LightingModels::State::CreateAcceleration(Asset& rAsset, Deformed* pDeformed)
{
	VkAccelerationStructureGeometryKHR pVkGeometries[common::SceneHeader::kiMaxMaterials] {};
	VkAccelerationStructureBuildRangeInfoKHR pVkRanges[common::SceneHeader::kiMaxMaterials] {};
	uint32_t puiCounts[common::SceneHeader::kiMaxMaterials] {};
	int64_t iCount = std::ssize(rAsset.primitives);
	GetBuildGeometry(rAsset, pDeformed, std::span(pVkGeometries, static_cast<size_t>(iCount)), std::span(pVkRanges, static_cast<size_t>(iCount)));
	for (int64_t i = 0; i < iCount; ++i) puiCounts[i] = pVkRanges[i].primitiveCount;
	LightingAccelerationStructure& rAcceleration = pDeformed == nullptr ? rAsset.acceleration : pDeformed->acceleration;
	rAcceleration.Create("LightingModelBLAS", VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, std::span(pVkGeometries, static_cast<size_t>(iCount)), std::span(puiCounts, static_cast<size_t>(iCount)), VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
}

LightingModelCounts LightingModels::Prepare(int64_t iFramebuffer, std::span<const LightingModelRange> ranges)
{
	// Heap: resident asset/instance resources grow only at their first publication and persist through framebuffer/capture users.
	ScopedSuppressAllocationTracking suppress;
	LightingModelCounts counts;
	for (const LightingModelRange& rRange : ranges)
	{
		State::Asset& rAsset = mpState->GetAsset(rRange.modelCrc);
		if (rAsset.primitives.empty())
		{
			continue;
		}
		counts.uiInstances += static_cast<uint32_t>(rRange.iInstanceCount);
		// Visible ranges start at instance 0; a trace-only range is the off-screen capacity tail.
		if (rRange.iFirstInstance > 0)
		{
			counts.uiTraceOnlyInstances += static_cast<uint32_t>(rRange.iInstanceCount);
		}
		counts.uiGeometries += static_cast<uint32_t>(rRange.iInstanceCount * std::ssize(rAsset.primitives));
		std::span previousRanges = ranges.first(static_cast<size_t>(&rRange - ranges.data()));
		if (std::ranges::none_of(previousRanges, [&](const LightingModelRange& rPrevious) { return rPrevious.modelCrc == rRange.modelCrc; }))
		{
			counts.uiMaterials += rAsset.uiMaterialCount;
		}
		for (const State::Primitive& rPrimitive : rAsset.primitives) counts.uiTriangles += static_cast<uint32_t>(rRange.iInstanceCount * (rPrimitive.uiIndexCount / 3));
		if (!(rAsset.flags & State::Asset::Flags::kAnimated))
		{
			continue;
		}
		for (int64_t i = rRange.iFirstInstance; i < rRange.iFirstInstance + rRange.iInstanceCount; ++i)
		{
			auto [it, bInserted] = mpState->framebuffers.at(iFramebuffer)->try_emplace(State::InstanceKey(rRange.modelCrc, rRange.instanceBufferCrc, i));
			if (!bInserted)
			{
				continue;
			}
			State::Deformed& rDeformed = it->second;
			rDeformed.vertices.Create(
			{
				.name = "LightingModelDeformed",
				.flags = {BufferFlags::kStorage, BufferFlags::kDeviceLocal, BufferFlags::kDeviceAddress, BufferFlags::kAccelerationInput},
				.iDataSize = static_cast<int64_t>(rAsset.uiVertexCount) * sizeof(shaders::LightingModelVertex),
			});
			mpState->CreateAcceleration(rAsset, &rDeformed);
		}
	}
	return counts;
}

void LightingModels::Write(int64_t iFramebuffer, std::span<const LightingModelRange> ranges, uint32_t uiFirstInstance, uint32_t uiFirstGeometry, std::span<shaders::LightingInstance> instances, std::span<shaders::LightingGeometry> geometries, std::span<VkAccelerationStructureInstanceKHR> accelerationInstances)
{
	uint32_t uiInstance = uiFirstInstance;
	uint32_t uiGeometry = uiFirstGeometry;
	for (const LightingModelRange& rRange : ranges)
	{
		const State::Asset& rAsset = mpState->assets.at(rRange.modelCrc);
		if (rAsset.primitives.empty())
		{
			continue;
		}
		const Buffer& rPacked = gpBufferManager->mModelMap.at(rAsset.bufferCrc);
		const auto* pModels = gpBufferManager->GetDynamicStorageBuffer<shaders::ModelLayout>(rRange.instanceBufferCrc, kBufferMain, iFramebuffer).pData;
		int64_t iIndexBytes = rPacked.mInfo.vkIndexType == VK_INDEX_TYPE_UINT16 ? 2 : 4;
		VkDeviceAddress vkPacked = rPacked.GetDeviceAddress();
		VkDeviceAddress vkAttributes = vkPacked + common::ModelHeader::VerticesOffset(rPacked.mInfo.iCount, iIndexBytes);
		for (int64_t i = rRange.iFirstInstance; i < rRange.iFirstInstance + rRange.iInstanceCount; ++i, ++uiInstance)
		{
			const shaders::ModelLayout& rModel = pModels[i];
			const State::Deformed* pDeformed = rAsset.flags & State::Asset::Flags::kAnimated ? &mpState->framebuffers.at(iFramebuffer)->at(State::InstanceKey(rRange.modelCrc, rRange.instanceBufferCrc, i)) : nullptr;
			shaders::LightingInstance& rInstance = instances[uiInstance];
			rInstance = {};
			std::ignore = std::copy(std::begin(rModel.f3x4Transform), std::end(rModel.f3x4Transform), rInstance.pf4ObjectToWorld);
			std::ignore = std::copy(std::begin(rModel.f3x4TransformNormal), std::end(rModel.f3x4TransformNormal), rInstance.pf4NormalToWorld);
			rInstance.uiFirstGeometry = uiGeometry;
			rInstance.uiGeometryCount = static_cast<uint32_t>(std::ssize(rAsset.primitives));
			rInstance.uiIdentity = uiInstance;
			VkAccelerationStructureInstanceKHR& rVkAccelerationInstance = accelerationInstances[uiInstance];
			rVkAccelerationInstance = {};
			std::ignore = std::memcpy(rVkAccelerationInstance.transform.matrix, rModel.f3x4Transform, sizeof(rVkAccelerationInstance.transform));
			rVkAccelerationInstance.instanceCustomIndex = uiInstance;
			rVkAccelerationInstance.mask = 0xff;
			rVkAccelerationInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
			rVkAccelerationInstance.accelerationStructureReference = pDeformed == nullptr ? rAsset.acceleration.GetDeviceAddress() : pDeformed->acceleration.GetDeviceAddress();
			for (int64_t j = 0; j < std::ssize(rAsset.primitives); ++j, ++uiGeometry)
			{
				const State::Primitive& rPrimitive = rAsset.primitives.at(static_cast<size_t>(j));
				geometries[uiGeometry] =
				{
					.uiPositionAddress = pDeformed == nullptr ? vkAttributes : pDeformed->vertices.GetDeviceAddress(),
					.uiAttributeAddress = vkAttributes,
					.uiIndexAddress = vkPacked,
					.uiMaterialAddress = rAsset.materials.GetDeviceAddress(),
					.uiPositionStride = pDeformed == nullptr ? static_cast<uint32_t>(rPacked.mInfo.iVertexStride) : static_cast<uint32_t>(sizeof(shaders::LightingModelVertex)),
					.uiAttributeStride = static_cast<uint32_t>(rPacked.mInfo.iVertexStride),
					.uiIndexBytes = static_cast<uint32_t>(iIndexBytes),
					.uiFirstIndex = rPrimitive.uiFirstIndex,
					.uiPrimitiveCount = rPrimitive.uiIndexCount / 3,
					.uiMaterial = rPrimitive.uiMaterial,
					.uiKind = shaders::kiLightingGeometryModel,
				};
			}
		}
	}
}

void LightingModels::RecordPreparation(VkCommandBuffer vkCommandBuffer, int64_t iFramebuffer, std::span<const LightingModelRange> ranges)
{
	LightingAccelerationStructure::RecordInputBarrier(vkCommandBuffer);
	vkCmdBindPipeline(vkCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, mpState->vkPipeline);
	for (const LightingModelRange& rRange : ranges)
	{
		State::Asset& rAsset = mpState->assets.at(rRange.modelCrc);
		if (!(rAsset.flags & State::Asset::Flags::kAnimated))
		{
			continue;
		}
		if (rAsset.primitives.empty())
		{
			continue;
		}
		const Buffer& rPacked = gpBufferManager->mModelMap.at(rAsset.bufferCrc);
		int64_t iIndexBytes = rPacked.mInfo.vkIndexType == VK_INDEX_TYPE_UINT16 ? 2 : 4;
		for (int64_t i = rRange.iFirstInstance; i < rRange.iFirstInstance + rRange.iInstanceCount; ++i)
		{
			State::Deformed& rDeformed = mpState->framebuffers.at(iFramebuffer)->at(State::InstanceKey(rRange.modelCrc, rRange.instanceBufferCrc, i));
			shaders::LightingModelDeform deform
			{
				.uiVertexAddress = rPacked.GetDeviceAddress() + common::ModelHeader::VerticesOffset(rPacked.mInfo.iCount, iIndexBytes),
				.uiInstanceAddress = gpBufferManager->mpDynamicStorageBuffers[kBufferMain].at(rRange.instanceBufferCrc).at(iFramebuffer).GetDeviceAddress(),
				.uiMeshAddress = gpBufferManager->mMeshDataStorageBuffers.at(iFramebuffer).GetDeviceAddress(),
				.uiJointAddress = gpBufferManager->mJointMatrixStorageBuffers.at(iFramebuffer).GetDeviceAddress(),
				.uiOutputAddress = rDeformed.vertices.GetDeviceAddress(),
				.uiVertexMaterialAddress = rAsset.vertexMaterials.GetDeviceAddress(),
				.uiInstance = static_cast<uint32_t>(i),
				.uiVertexCount = rAsset.uiVertexCount,
			};
			vkCmdPushConstants(vkCommandBuffer, mpState->vkPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(deform), &deform);
			vkCmdDispatch(vkCommandBuffer, common::RoundUp<uint32_t, 64>(deform.uiVertexCount) / 64, 1, 1);
		}
	}
	LightingAccelerationStructure::RecordInputBarrier(vkCommandBuffer);
	for (const LightingModelRange& rRange : ranges)
	{
		State::Asset& rAsset = mpState->assets.at(rRange.modelCrc);
		if (rAsset.primitives.empty())
		{
			continue;
		}
		if (!(rAsset.flags & State::Asset::Flags::kAnimated) && rAsset.flags & State::Asset::Flags::kBuilt)
		{
			continue;
		}
		VkAccelerationStructureGeometryKHR pVkGeometries[common::SceneHeader::kiMaxMaterials] {};
		VkAccelerationStructureBuildRangeInfoKHR pVkRanges[common::SceneHeader::kiMaxMaterials] {};
		int64_t iCount = std::ssize(rAsset.primitives);
		int64_t iBuildCount = rAsset.flags & State::Asset::Flags::kAnimated ? rRange.iInstanceCount : 1;
		for (int64_t i = 0; i < iBuildCount; ++i)
		{
			State::Deformed* pDeformed = rAsset.flags & State::Asset::Flags::kAnimated ? &mpState->framebuffers.at(iFramebuffer)->at(State::InstanceKey(rRange.modelCrc, rRange.instanceBufferCrc, rRange.iFirstInstance + i)) : nullptr;
			mpState->GetBuildGeometry(rAsset, pDeformed, std::span(pVkGeometries, static_cast<size_t>(iCount)), std::span(pVkRanges, static_cast<size_t>(iCount)));
			LightingAccelerationStructure& rAcceleration = pDeformed == nullptr ? rAsset.acceleration : pDeformed->acceleration;
			rAcceleration.RecordBuild(vkCommandBuffer, std::span(pVkGeometries, static_cast<size_t>(iCount)), std::span(pVkRanges, static_cast<size_t>(iCount)));
		}
		rAsset.flags.Set(State::Asset::Flags::kBuilt);
	}
	LightingAccelerationStructure::RecordBuildBarrier(vkCommandBuffer);
}

} // namespace engine

#endif // defined(BT_CLIENT)
