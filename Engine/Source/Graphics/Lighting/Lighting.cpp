#if defined(BT_CLIENT)

#include "Data/Shader.h"
#include "Graphics/Managers/CommandBufferRecordMain.h"
#include "Graphics/Objects/PipelineDescriptorWriter.h"
#include "LightingState.h"

#include "Profile/ProfileRows.h"

namespace engine
{

static VkAccelerationStructureGeometryKHR LightingSceneGeometry(const LightingFramebuffer& rFramebuffer)
{
	return
	{
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
		.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
		.geometry =
		{
			.instances =
			{
				.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
				.data = {.deviceAddress = rFramebuffer.accelerationInstances.GetDeviceAddress()},
			},
		},
	};
}

static bool LightingReserve(Buffer& rBuffer, std::string_view name, int64_t iBytes, BufferFlags_t flags)
{
	if (rBuffer.mInfo.iDataSize >= iBytes)
	{
		return false;
	}
	rBuffer.Create({.name = name, .flags = flags, .iDataSize = iBytes});
	return true;
}

static VkAccelerationStructureGeometryKHR LightingSolidGeometry(const Buffer& rVertices, const Buffer& rIndices)
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
				.vertexData = {.deviceAddress = rVertices.GetDeviceAddress()},
				.vertexStride = sizeof(common::ModelVertex),
				.maxVertex = LightingFixture::kiVerticesPerSolid - 1,
				.indexType = VK_INDEX_TYPE_UINT32,
				.indexData = {.deviceAddress = rIndices.GetDeviceAddress()},
			},
		},
		.flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
	};
}

void Lighting::CreateFixtureResources()
{
	if (!mpState->fixtureSolids.empty())
	{
		return;
	}
	gpBufferManager->CreateDynamicBuffer(State::kFixtureAnimationBuffer, kBufferMain, "LightingFixtureAnimation", sizeof(shaders::ModelLayout), 1);
	mpState->fixtureSolids.reserve(LightingFixture::kiSolidCount);
	for (int64_t i = 0; i < LightingFixture::kiSolidCount; ++i)
	{
		auto pSolid = std::make_unique<State::FixtureSolid>();
		LightingReserve(pSolid->vertices, "LightingFixtureVertices", LightingFixture::kiVerticesPerSolid * sizeof(common::ModelVertex), {BufferFlags::kStorage, BufferFlags::kHostVisible, BufferFlags::kAccelerationInput});
		LightingReserve(pSolid->indices, "LightingFixtureIndices", LightingFixture::kiIndicesPerSolid * sizeof(uint32_t), {BufferFlags::kStorage, BufferFlags::kHostVisible, BufferFlags::kAccelerationInput});
		LightingFixture::WriteSolidGeometry(mpState->fixture->GetSolid(i, 0, mpState->eScenario), std::span<common::ModelVertex>(reinterpret_cast<common::ModelVertex*>(pSolid->vertices.mpMappedMemory), LightingFixture::kiVerticesPerSolid), std::span<uint32_t>(reinterpret_cast<uint32_t*>(pSolid->indices.mpMappedMemory), LightingFixture::kiIndicesPerSolid));
		VkAccelerationStructureGeometryKHR vkGeometry = LightingSolidGeometry(pSolid->vertices, pSolid->indices);
		uint32_t uiTriangles = LightingFixture::kiIndicesPerSolid / 3;
		pSolid->acceleration.Create("LightingFixtureBLAS", VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, std::span(&vkGeometry, 1), std::span(&uiTriangles, 1), VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
		mpState->fixtureSolids.push_back(std::move(pSolid));
	}
}

void Lighting::PublishFixtureAnimation()
{
	const LightingFixtureAnimation& rAnimation = mpState->fixture->GetAnimation();
	const AnimationData& rData = gAnimationDataMap.at(rAnimation.sceneCrc);
	SkinningReservation reservation = rData.ReserveSkinning(mpState->iFramebuffer, 1);
	mpState->fixture->WriteAnimation(mpState->settings.iEvent, std::span<common::MeshData>(reservation.pMeshData + reservation.iMeshDataBase, static_cast<size_t>(reservation.iMaterialCount)), reservation.pJointMatrices, reservation.iJointBase);
	XMVECTOR vecPosition = Rebase(MakeRenderBasis(mpState->fixture->GetAnchor(), gpCamera->mBasisCoordinate), XMVectorSetW(XMLoadFloat3A(&rAnimation.f3Center), 1.0f));
	XMMATRIX matTransform = XMMatrixScaling(rAnimation.fScale, rAnimation.fScale, rAnimation.fScale) * XMMatrixTranslationFromVector(vecPosition);
	shaders::ModelLayout& rModel = gpBufferManager->GetDynamicStorageBuffer<shaders::ModelLayout>(State::kFixtureAnimationBuffer, kBufferMain, mpState->iFramebuffer).pData[0];
	rModel = {};
	XMStoreFloat4(&rModel.f4Position, vecPosition);
	XMStoreFloat3x4(reinterpret_cast<XMFLOAT3X4*>(rModel.f3x4Transform), matTransform);
	XMStoreFloat3x4(reinterpret_cast<XMFLOAT3X4*>(rModel.f3x4TransformNormal), XMMatrixTranspose(XMMatrixInverse(nullptr, matTransform)));
	rModel.uiMeshDataBase = static_cast<uint32_t>(reservation.iMeshDataBase);
	AddModels(rAnimation.sceneCrc, State::kFixtureAnimationBuffer, 0, 1);
}

void Lighting::WriteFixtureSolids(std::span<shaders::LightingInstance> instances, std::span<shaders::LightingGeometry> geometries, std::span<VkAccelerationStructureInstanceKHR> accelerationInstances)
{
	LightingFramebuffer& rFramebuffer = *mpState->framebuffers.at(mpState->iFramebuffer);
	for (int64_t i = 0; i < LightingFixture::kiSolidCount; ++i)
	{
		const State::FixtureSolid& rResource = *mpState->fixtureSolids.at(i);
		LightingFixtureSolid solid = mpState->fixture->GetSolid(i, mpState->settings.iEvent, mpState->eScenario);
		XMFLOAT3A f3Center;
		XMStoreFloat3A(&f3Center, Rebase(MakeRenderBasis(mpState->fixture->GetAnchor(), gpCamera->mBasisCoordinate), XMVectorSetW(XMLoadFloat3A(&solid.f3Center), 1.0f)));
		uint32_t uiInstance = rFramebuffer.uiFixtureFirstInstance + static_cast<uint32_t>(i);
		uint32_t uiGeometry = rFramebuffer.uiFixtureFirstGeometry + static_cast<uint32_t>(i);
		shaders::LightingInstance& rInstance = instances[uiInstance];
		rInstance = {};
		rInstance.pf4ObjectToWorld[0] = {solid.f3AxisX.x, solid.f3AxisY.x, solid.f3AxisZ.x, f3Center.x};
		rInstance.pf4ObjectToWorld[1] = {solid.f3AxisX.y, solid.f3AxisY.y, solid.f3AxisZ.y, f3Center.y};
		rInstance.pf4ObjectToWorld[2] = {solid.f3AxisX.z, solid.f3AxisY.z, solid.f3AxisZ.z, f3Center.z};
		for (int64_t j = 0; j < 3; ++j)
		{
			rInstance.pf4NormalToWorld[j] = rInstance.pf4ObjectToWorld[j];
			rInstance.pf4NormalToWorld[j].w = 0.0f;
		}
		rInstance.uiFirstGeometry = uiGeometry;
		rInstance.uiGeometryCount = 1;
		rInstance.uiIdentity = uiInstance;
		geometries[uiGeometry] = {.uiPositionAddress = rResource.vertices.GetDeviceAddress(), .uiAttributeAddress = rResource.vertices.GetDeviceAddress(), .uiIndexAddress = rResource.indices.GetDeviceAddress(), .uiPositionStride = sizeof(common::ModelVertex), .uiAttributeStride = sizeof(common::ModelVertex), .uiIndexBytes = 4, .uiPrimitiveCount = LightingFixture::kiIndicesPerSolid / 3, .uiMaterial = solid.uiMaterial, .uiKind = shaders::kiLightingGeometryFixture};
		VkAccelerationStructureInstanceKHR& rVkAccelerationInstance = accelerationInstances[uiInstance];
		rVkAccelerationInstance = {};
		std::ignore = std::memcpy(rVkAccelerationInstance.transform.matrix, rInstance.pf4ObjectToWorld, sizeof(rVkAccelerationInstance.transform));
		rVkAccelerationInstance.instanceCustomIndex = uiInstance;
		rVkAccelerationInstance.mask = 0xff;
		rVkAccelerationInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		rVkAccelerationInstance.accelerationStructureReference = rResource.acceleration.GetDeviceAddress();
	}
}

Lighting::Lighting()
{
	mpState = std::make_unique<State>();
	CreateResources();
}

void Lighting::CreateResources()
{
	mpState->framebuffers.resize(static_cast<size_t>(std::ssize(gpSwapchainManager->mFramebuffers)));
	for (std::unique_ptr<LightingFramebuffer>& rpFramebuffer : mpState->framebuffers)
	{
		rpFramebuffer = std::make_unique<LightingFramebuffer>();
		LightingFramebuffer& rFramebuffer = *rpFramebuffer;
		rFramebuffer.uiGeneration = ++mpState->uiResourceGeneration;
		rFramebuffer.sources.Create(
		{
			.name = "LightingSources",
			.flags = {BufferFlags::kStorage, BufferFlags::kHostVisible},
			.iDataSize = shaders::kiLightingMaxSources * static_cast<int64_t>(sizeof(shaders::LightingSource)),
		});
		rFramebuffer.publication.Create(
		{
			.name = "LightingPublication",
			.flags = {BufferFlags::kStorage, BufferFlags::kHostVisible},
			.iDataSize = sizeof(shaders::LightingPublication),
		});
		*reinterpret_cast<shaders::LightingPublication*>(rFramebuffer.publication.mpMappedMemory) = {};
		LightingReserve(rFramebuffer.instances, "LightingInstances", sizeof(shaders::LightingInstance), {BufferFlags::kStorage, BufferFlags::kHostVisible, BufferFlags::kDeviceAddress});
		LightingReserve(rFramebuffer.geometries, "LightingGeometries", sizeof(shaders::LightingGeometry), {BufferFlags::kStorage, BufferFlags::kHostVisible, BufferFlags::kDeviceAddress});
		LightingReserve(rFramebuffer.accelerationInstances, "LightingAccelerationInstances", sizeof(VkAccelerationStructureInstanceKHR), {BufferFlags::kStorage, BufferFlags::kHostVisible, BufferFlags::kAccelerationInput});
		LightingReserve(rFramebuffer.samples, "LightingSamples", shaders::kiLightingMaxSamples * sizeof(XMFLOAT2), {BufferFlags::kStorage, BufferFlags::kHostVisible});
		LightingReserve(rFramebuffer.fixtureMaterials, "LightingFixtureMaterials", 2 * sizeof(XMFLOAT4), {BufferFlags::kStorage, BufferFlags::kHostVisible});
		for (int64_t i = 0; i < 2; ++i)
		{
			XMFLOAT3A f3Color = LightingFixture::GetAlbedo(i);
			reinterpret_cast<XMFLOAT4*>(rFramebuffer.fixtureMaterials.mpMappedMemory)[i] = {f3Color.x, f3Color.y, f3Color.z, 0.0f};
		}
		VkAccelerationStructureGeometryKHR vkGeometry = LightingSceneGeometry(rFramebuffer);
		uint32_t uiCount = 0;
		rFramebuffer.scene.Create("LightingScene", VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR, std::span(&vkGeometry, 1), std::span(&uiCount, 1), VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
		rFramebuffer.modelRanges.reserve(256);
	}
	mpState->terrain.CreateResources();
	mpState->terrain.CreatePipelines();
	mpState->models = std::make_unique<LightingModels>();
	mpState->models->Create(gpPipelineManager->mShaders.at(data::kShadersLightingLightingModelscompCrc));
	CreateComparisonResources();
}

Lighting::~Lighting() = default;

void Lighting::DestroyResources()
{
	DestroyComparisonResources();
	mpState->terrain.DestroyResources();
	mpState->models.reset();
	mpState->fixtureSolids.clear();
	mpState->framebuffers.clear();
}

void Lighting::CreateTerrainPipelines()
{
	mpState->terrain.CreatePipelines();
	mpState->models->Create(gpPipelineManager->mShaders.at(data::kShadersLightingLightingModelscompCrc));
	CreateComparisonResources();
}

void Lighting::DestroyTerrainPipelines()
{
	DestroyComparisonResources();
	mpState->terrain.DestroyPipelines();
	mpState->models->DestroyPipeline();
}

void Lighting::BeginRender(int64_t iFramebuffer)
{
	mpState->iFramebuffer = iFramebuffer;
	LightingFramebuffer& rFramebuffer = *mpState->framebuffers.at(iFramebuffer);
	// Runs after this framebuffer's fence wait, so its previously recorded publication has completed on the GPU.
	if (rFramebuffer.uiRecordedPublication != 0 && rFramebuffer.uiRecordedPublication == rFramebuffer.uiPendingPublication)
	{
		LightingDiagnostics& rDiagnostics = mpState->diagnostics;
		const LightingDiagnostics& rPending = rFramebuffer.pendingDiagnostics;
		rDiagnostics.iSources = rPending.iSources;
		rDiagnostics.iInstances = rPending.iInstances;
		rDiagnostics.iTraceOnlyInstances = rPending.iTraceOnlyInstances;
		rDiagnostics.iTriangles = rPending.iTriangles;
	}
	rFramebuffer.modelRanges.clear();
	*reinterpret_cast<shaders::LightingPublication*>(rFramebuffer.publication.mpMappedMemory) = {};
}

void Lighting::AddModels(common::crc_t modelCrc, common::crc_t instanceBufferCrc, int64_t iFirstInstance, int64_t iInstanceCount)
{
	if (iInstanceCount == 0)
	{
		return;
	}
	LightingFramebuffer& rFramebuffer = *mpState->framebuffers.at(mpState->iFramebuffer);
	// Heap: resident model range capacity grows only when publication exceeds its previous maximum.
	ScopedSuppressAllocationTracking scopedSuppressAllocationTracking;
	rFramebuffer.modelRanges.push_back(
	{
		.modelCrc = modelCrc,
		.instanceBufferCrc = instanceBufferCrc,
		.iFirstInstance = iFirstInstance,
		.iInstanceCount = iInstanceCount,
	});
}

void Lighting::EndRender()
{
	// Heap: only the acquired framebuffer grows after its fence, before any addresses are published.
	ScopedSuppressAllocationTracking suppress;
	LightingFramebuffer& rFramebuffer = *mpState->framebuffers.at(mpState->iFramebuffer);
	rFramebuffer.modelCounts = {};
	rFramebuffer.terrainCounts = {};
	bool bActive = mpState->settings.eMode != LightingComparisonMode::kDisabled;
	if (bActive)
	{
		if (mpState->fixture != nullptr)
		{
			CreateFixtureResources();
			PublishFixtureAnimation();
		}
		rFramebuffer.modelCounts = mpState->models->Prepare(mpState->iFramebuffer, rFramebuffer.modelRanges);
		rFramebuffer.terrainCounts = mpState->terrain.Prepare(mpState->iFramebuffer);
	}
	int64_t iSolidCount = bActive && mpState->fixture != nullptr ? LightingFixture::kiSolidCount : 0;
	rFramebuffer.uiFixtureFirstInstance = rFramebuffer.modelCounts.uiInstances + static_cast<uint32_t>(rFramebuffer.terrainCounts.iPlacements);
	rFramebuffer.uiFixtureFirstGeometry = rFramebuffer.modelCounts.uiGeometries + static_cast<uint32_t>(rFramebuffer.terrainCounts.iPlacements);
	int64_t iInstances = rFramebuffer.uiFixtureFirstInstance + iSolidCount;
	int64_t iGeometries = rFramebuffer.uiFixtureFirstGeometry + iSolidCount;
	if (iInstances > 0xffffff)
	{
		throw std::runtime_error("Lighting scene exceeds acceleration structure limits");
	}
	if (std::cmp_greater(iInstances, gpInstanceManager->mVkPhysicalDeviceAccelerationStructurePropertiesKHR.maxInstanceCount))
	{
		throw std::runtime_error("Lighting scene exceeds acceleration structure limits");
	}
	if (!std::in_range<uint32_t>(iGeometries))
	{
		throw std::runtime_error("Lighting scene exceeds acceleration structure limits");
	}
	bool bGrowth = LightingReserve(rFramebuffer.instances, "LightingInstances", std::max<int64_t>(1, iInstances) * sizeof(shaders::LightingInstance), {BufferFlags::kStorage, BufferFlags::kHostVisible, BufferFlags::kDeviceAddress});
	bGrowth |= LightingReserve(rFramebuffer.geometries, "LightingGeometries", std::max<int64_t>(1, iGeometries) * sizeof(shaders::LightingGeometry), {BufferFlags::kStorage, BufferFlags::kHostVisible, BufferFlags::kDeviceAddress});
	bGrowth |= LightingReserve(rFramebuffer.accelerationInstances, "LightingAccelerationInstances", std::max<int64_t>(1, iInstances) * sizeof(VkAccelerationStructureInstanceKHR), {BufferFlags::kStorage, BufferFlags::kHostVisible, BufferFlags::kAccelerationInput});
	if (std::cmp_not_equal(rFramebuffer.uiInstanceCount, iInstances))
	{
		VkAccelerationStructureGeometryKHR vkGeometry = LightingSceneGeometry(rFramebuffer);
		uint32_t uiCount = static_cast<uint32_t>(iInstances);
		rFramebuffer.scene.Create("LightingScene", VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR, std::span(&vkGeometry, 1), std::span(&uiCount, 1), VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
		bGrowth = true;
	}
	if (bGrowth)
	{
		rFramebuffer.uiGeneration = ++mpState->uiResourceGeneration;
	}
	rFramebuffer.uiInstanceCount = static_cast<uint32_t>(iInstances);
	rFramebuffer.uiGeometryCount = static_cast<uint32_t>(iGeometries);
	std::span instances(reinterpret_cast<shaders::LightingInstance*>(rFramebuffer.instances.mpMappedMemory), static_cast<size_t>(iInstances));
	std::span geometries(reinterpret_cast<shaders::LightingGeometry*>(rFramebuffer.geometries.mpMappedMemory), static_cast<size_t>(iGeometries));
	std::span accelerationInstances(reinterpret_cast<VkAccelerationStructureInstanceKHR*>(rFramebuffer.accelerationInstances.mpMappedMemory), static_cast<size_t>(iInstances));
	if (bActive)
	{
		mpState->models->Write(mpState->iFramebuffer, rFramebuffer.modelRanges, 0, 0, instances, geometries, accelerationInstances);
		mpState->terrain.Write(mpState->iFramebuffer, rFramebuffer.modelCounts.uiInstances, rFramebuffer.modelCounts.uiGeometries, instances, geometries, accelerationInstances);
		if (iSolidCount > 0)
		{
			rFramebuffer.uiFixtureAnimationInstance = rFramebuffer.modelCounts.uiInstances - 1;
			instances[rFramebuffer.uiFixtureAnimationInstance].uiPadding = 1;
			WriteFixtureSolids(instances, geometries, accelerationInstances);
		}
	}
	uint32_t uiSourceCount = 0;
	if (bActive && mpState->fixture != nullptr)
	{
		uiSourceCount = mpState->eScenario == LightingFixtureScenario::kDensity ? static_cast<uint32_t>(mpState->settings.iSourceCount) : 1;
		mpState->fixture->WriteSources(std::span<shaders::LightingSource>(reinterpret_cast<shaders::LightingSource*>(rFramebuffer.sources.mpMappedMemory), uiSourceCount), mpState->settings.iEvent, gpCamera->mBasisCoordinate, mpState->eScenario);
		mpState->fixture->WriteSamples(std::span<XMFLOAT2>(reinterpret_cast<XMFLOAT2*>(rFramebuffer.samples.mpMappedMemory), static_cast<size_t>(mpState->settings.iAreaSamples)));
	}
	int64_t iTriangles = rFramebuffer.modelCounts.uiTriangles + rFramebuffer.terrainCounts.iTriangles + iSolidCount * (LightingFixture::kiIndicesPerSolid / 3);
	int64_t iMaterials = rFramebuffer.modelCounts.uiMaterials + rFramebuffer.terrainCounts.iPlacements + (iSolidCount > 0 ? 2 : 0);
	if (!std::in_range<uint32_t>(iTriangles) || !std::in_range<uint32_t>(iMaterials))
	{
		throw std::runtime_error("Lighting publication count overflow");
	}
	*reinterpret_cast<shaders::LightingPublication*>(rFramebuffer.publication.mpMappedMemory) = {.uiSourceCount = uiSourceCount, .uiInstanceCount = static_cast<uint32_t>(iInstances), .uiTriangleCount = static_cast<uint32_t>(iTriangles), .uiMaterialCount = static_cast<uint32_t>(iMaterials)};
	rFramebuffer.uiPendingPublication = ++mpState->uiPublication;
	rFramebuffer.pendingDiagnostics = {};
	rFramebuffer.pendingDiagnostics.iSources = uiSourceCount;
	rFramebuffer.pendingDiagnostics.iInstances = iInstances;
	rFramebuffer.pendingDiagnostics.iTraceOnlyInstances = rFramebuffer.modelCounts.uiTraceOnlyInstances;
	rFramebuffer.pendingDiagnostics.iTriangles = iTriangles;
	PrepareComparisonPipelines();
}

void Lighting::RecordPreparation(VkCommandBuffer vkCommandBuffer)
{
	int64_t iFramebuffer = mpState->iFramebuffer;
	gpProfileManager->ResetQueryPools(iFramebuffer, vkCommandBuffer, kGpuTimerMain, kGpuTimerUiRender);
	gpProfileManager->GpuStart(iFramebuffer, vkCommandBuffer, kGpuTimerMain);
	gpProfileManager->GpuStart(iFramebuffer, vkCommandBuffer, kGpuTimerMainUniformCopy);
	gpBufferManager->mMainLayoutUniformBuffers.at(iFramebuffer).RecordCopy(vkCommandBuffer);
	gpProfileManager->GpuStop(iFramebuffer, vkCommandBuffer, kGpuTimerMainUniformCopy);
	CommandBufferRecordMain::RecordWaterDisplacement(vkCommandBuffer, iFramebuffer);
	LightingFramebuffer& rFramebuffer = *mpState->framebuffers.at(iFramebuffer);
	if (mpState->settings.eMode != LightingComparisonMode::kDisabled)
	{
		mpState->models->RecordPreparation(vkCommandBuffer, iFramebuffer, rFramebuffer.modelRanges);
		gpProfileManager->GpuStart(iFramebuffer, vkCommandBuffer, kGpuTimerLightingTerrain);
		mpState->terrain.RecordPreparation(vkCommandBuffer, iFramebuffer);
		gpProfileManager->GpuStop(iFramebuffer, vkCommandBuffer, kGpuTimerLightingTerrain);
		for (const std::unique_ptr<State::FixtureSolid>& rpSolid : mpState->fixtureSolids)
		{
			if (rpSolid->bBuilt)
			{
				continue;
			}
			VkAccelerationStructureGeometryKHR vkGeometry = LightingSolidGeometry(rpSolid->vertices, rpSolid->indices);
			VkAccelerationStructureBuildRangeInfoKHR vkRange {.primitiveCount = LightingFixture::kiIndicesPerSolid / 3};
			rpSolid->acceleration.RecordBuild(vkCommandBuffer, std::span(&vkGeometry, 1), std::span(&vkRange, 1));
			rpSolid->bBuilt = true;
		}
		LightingAccelerationStructure::RecordBuildBarrier(vkCommandBuffer);
	}
	if (mpState->settings.eMode != LightingComparisonMode::kDisabled || rFramebuffer.uiBuiltSceneGeneration != rFramebuffer.uiGeneration)
	{
		LightingAccelerationStructure::RecordInputBarrier(vkCommandBuffer);
		VkAccelerationStructureGeometryKHR vkGeometry = LightingSceneGeometry(rFramebuffer);
		VkAccelerationStructureBuildRangeInfoKHR vkRange {.primitiveCount = rFramebuffer.uiInstanceCount};
		rFramebuffer.scene.RecordBuild(vkCommandBuffer, std::span(&vkGeometry, 1), std::span(&vkRange, 1));
		LightingAccelerationStructure::RecordQueryBarrier(vkCommandBuffer);
		rFramebuffer.uiBuiltSceneGeneration = rFramebuffer.uiGeneration;
	}
	CommandBufferRecordMain::RecordEnvironment(vkCommandBuffer, iFramebuffer);
	RecordCapture(vkCommandBuffer);
	rFramebuffer.uiRecordedPublication = rFramebuffer.uiPendingPublication;
}

void Lighting::BindQueryDescriptors(const Pipeline& rPipeline, int64_t iFramebuffer) const
{
	const LightingFramebuffer& rFramebuffer = *mpState->framebuffers.at(iFramebuffer);
	for (const std::pair<int64_t, const Buffer*>& rBinding : {std::pair<int64_t, const Buffer*>(shaders::kiLightingBindingSources, &rFramebuffer.sources), std::pair<int64_t, const Buffer*>(shaders::kiLightingBindingPublication, &rFramebuffer.publication), std::pair<int64_t, const Buffer*>(shaders::kiLightingBindingInstances, &rFramebuffer.instances), std::pair<int64_t, const Buffer*>(shaders::kiLightingBindingGeometry, &rFramebuffer.geometries), std::pair<int64_t, const Buffer*>(shaders::kiLightingBindingMaterials, &rFramebuffer.fixtureMaterials), std::pair<int64_t, const Buffer*>(shaders::kiLightingBindingSamples, &rFramebuffer.samples)})
	{
		if (PipelineDescriptorWriter::BindingExistsInShaderLayout(rPipeline, rBinding.first))
		{
			PipelineDescriptorWriter::UpdateStorageBuffer(rPipeline, iFramebuffer, rBinding.first, rBinding.second);
		}
	}
	if (PipelineDescriptorWriter::BindingExistsInShaderLayout(rPipeline, shaders::kiLightingBindingScene))
	{
		PipelineDescriptorWriter::UpdateAccelerationStructure(rPipeline, iFramebuffer, shaders::kiLightingBindingScene, rFramebuffer.scene.mVkAccelerationStructureKHR);
	}
}

void Lighting::AddQueryDescriptorInfos(PipelineInfo& rInfo, int64_t iFramebuffer) const
{
	const LightingFramebuffer& rFramebuffer = *mpState->framebuffers.at(iFramebuffer);
	rInfo.vkExternalDescriptorSetLayout = gpTextureManager->mTextureDescriptors.mGlobalVkDescriptorSetLayout;
	rInfo.descriptorInfos.reserve(static_cast<size_t>(std::ssize(rInfo.descriptorInfos) + 18));
	rInfo.descriptorInfos.push_back({.flags = DescriptorFlags::kGlobalLayoutUniformBuffers, .iExplicitBinding = shaders::kiGlobalBindingGlobalUniform});
	for (const std::pair<int64_t, Texture**>& rBinding : {std::pair<int64_t, Texture**>(5i64, gpTextureManager->mRenderTargetTextures.mColorTextures.data()), std::pair<int64_t, Texture**>(6i64, gpTextureManager->mRenderTargetTextures.mNormalsTextures.data()), std::pair<int64_t, Texture**>(7i64, gpTextureManager->mRenderTargetTextures.mMasksTextures.data())})
	{
		rInfo.descriptorInfos.push_back({.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerClamp, DescriptorFlags::kBindlessArrayConsumer}, .iCount = shaders::kiMaxIslands, .iExplicitBinding = rBinding.first, .ppTextures = rBinding.second});
	}
	for (const std::pair<int64_t, common::crc_t>& rBinding : {std::pair<int64_t, common::crc_t>(8i64, data::kTexturesTerrainBC7Rock0jpgCrc), std::pair<int64_t, common::crc_t>(9i64, data::kTexturesTerrainBC7SandpngCrc), std::pair<int64_t, common::crc_t>(10i64, data::kTexturesTerrainBC5RockNormal1jpgCrc), std::pair<int64_t, common::crc_t>(11i64, data::kTexturesTerrainBC5RockNormal2jpgCrc), std::pair<int64_t, common::crc_t>(14i64, data::kTexturesTerrainBC5RockNormal4jpgCrc), std::pair<int64_t, common::crc_t>(15i64, data::kTexturesTerrainBC5SandNormal0jpgCrc), std::pair<int64_t, common::crc_t>(16i64, data::kTexturesTerrainBC5SandNormal1pngCrc), std::pair<int64_t, common::crc_t>(17i64, data::kTexturesTerrainBC5SandNormal2pngCrc)})
	{
		rInfo.descriptorInfos.push_back({.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerRepeat}, .iExplicitBinding = rBinding.first, .textureCrc = rBinding.second});
	}
	for (const std::pair<int64_t, const Buffer*>& rBinding : {std::pair<int64_t, const Buffer*>(shaders::kiLightingBindingSources, &rFramebuffer.sources), std::pair<int64_t, const Buffer*>(shaders::kiLightingBindingPublication, &rFramebuffer.publication), std::pair<int64_t, const Buffer*>(shaders::kiLightingBindingInstances, &rFramebuffer.instances), std::pair<int64_t, const Buffer*>(shaders::kiLightingBindingGeometry, &rFramebuffer.geometries), std::pair<int64_t, const Buffer*>(shaders::kiLightingBindingMaterials, &rFramebuffer.fixtureMaterials), std::pair<int64_t, const Buffer*>(shaders::kiLightingBindingSamples, &rFramebuffer.samples)})
	{
		rInfo.descriptorInfos.push_back({.flags = DescriptorFlags::kStorageBuffer, .iExplicitBinding = rBinding.first, .pBuffers = const_cast<Buffer*>(rBinding.second)});
	}
}

void Lighting::SetComparison(const LightingComparisonSettings& rSettings)
{
	mRequestedComparisonSettings = rSettings;
}

bool Lighting::GetComparisonCamera(LightingCameraPose& rPose) const
{
	if (mpState->settings.eMode == LightingComparisonMode::kDisabled)
	{
		return false;
	}
	rPose = mpState->settings.eCamera == LightingComparisonCamera::kEvent ? mpState->fixture->GetEvent(mpState->settings.iEvent).camera : mpState->fixture->GetCamera(mpState->settings.eCamera);
	return true;
}

const LightingDiagnostics& Lighting::GetDiagnostics() const
{
	return mpState->diagnostics;
}

} // namespace engine

#endif // defined(BT_CLIENT)
