#if defined(BT_CLIENT)

#include "Missiles.h"

#include "Data/Scene.h"
#include "File/PackChunks.h"

#include "Profile/ProfileRows.h"

namespace game
{

constexpr common::crc_t kMissileModelCrc = data::kModelsaim9_missilescenegltfCrc;

constexpr int64_t kiDynamicBufferInitialCapacity = 512;

static int64_t siRendered = 0;
// Non-concurrency tripwire: per-frame Render calls must run sequentially across active coords — siRendered
// accumulates across them and offsets each call's slab writes. Parallelizing coord renders would race.
static std::atomic<bool> sbRenderActive = false;

// sbRenderActive must clear during exception unwinding so subsequent Render calls do not fail the concurrency ASSERT.
struct [[nodiscard]] MissilesRenderActiveGuard
{
	MissilesRenderActiveGuard()
	{
		ASSERT(!sbRenderActive.exchange(true));
	}

	MissilesRenderActiveGuard(const MissilesRenderActiveGuard&) = delete;
	MissilesRenderActiveGuard& operator=(const MissilesRenderActiveGuard&) = delete;

	~MissilesRenderActiveGuard()
	{
		sbRenderActive.store(false);
	}
};

void MissilesInterpolate::GraphicsResources()
{
	engine::Buffer* pStorageBuffers = engine::gpBufferManager->CreateDynamicBuffer(kuiCrc, engine::kBufferMain, kpcName, sizeof(shaders::ModelLayout), kiDynamicBufferInitialCapacity);
	engine::gpPipelineManager->mDynamicPipelines.CreateModelPipelines(kuiCrc, kpcName, kMissileModelCrc, pStorageBuffers);
}

void MissilesInterpolate::BeginRender([[maybe_unused]] int64_t iCommandBuffer, const std::unordered_map<engine::GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<engine::GridCoord>& rActiveCoordinates)
{
	siRendered = 0;

	int64_t iTotalCapacity = engine::AccumulateRenderCapacity(rRenderInterpolates, rActiveCoordinates, [](const game::FrameInterpolate& rInterpolate) -> const MissilesInterpolate&
	{
		return *rInterpolate.pMissiles;
	});

	if (iTotalCapacity == 0)
	{
		return;
	}

	if (engine::Buffer* pBuffer = engine::gpBufferManager->ResizeDynamicBufferIfNeeded(kuiCrc, engine::kBufferMain, kpcName, sizeof(shaders::ModelLayout), iTotalCapacity, iCommandBuffer); pBuffer != nullptr)
	{
		engine::gpPipelineManager->mDynamicPipelines.UpdateModelPipelineDescriptors(kuiCrc, iCommandBuffer, 2, pBuffer);
	}
}

void MissilesInterpolate::Render(const FrameInterpolate& __restrict rFrameInterpolate, int64_t iCommandBuffer)
{
	const MissilesInterpolate& rCurrent = *rFrameInterpolate.pMissiles;
	const engine::RenderBasis& rBasis = rFrameInterpolate.renderBasis;

	if (rCurrent.iCount == 0)
	{
		return;
	}

	MissilesRenderActiveGuard renderActiveGuard;

	const std::unordered_map<common::crc_t, engine::EagerChunk>& rChunkMap = engine::gpFileManager->mpPackChunks->GetEagerChunkMap();
	const common::SceneHeader& rSceneHeader = rChunkMap.at(kMissileModelCrc).pHeader->sceneHeader;
	engine::ModelAlignment alignment = engine::GetModelAlignment(rChunkMap.at(rSceneHeader.modelCrc).pHeader->modelHeader);

	auto [pLayouts, iBufferCapacity] = engine::gpBufferManager->GetDynamicStorageBuffer<shaders::ModelLayout>(kuiCrc, engine::kBufferMain, iCommandBuffer);
	ASSERT(siRendered + rCurrent.iCount <= iBufferCapacity);

	for (int64_t i = 0; i < rCurrent.iCount; ++i)
	{
		// The position is local to the rendered cell; converting it once here feeds the cull, the layout position,
		// and the model translation, all of which are in the camera cell's frame.
		XMVECTOR vecPosition = engine::Rebase(rBasis, rCurrent.pVecPositions[i]);
		XMFLOAT4A f4Position {};
		XMStoreFloat4A(&f4Position, vecPosition);
		if (!engine::gpCamera->InVisibleArea(engine::gpCamera->mf4RenderVisibleArea, f4Position))
		{
			continue;
		}

		// Sentinel value: 0.0f means explosion finished, skip rendering
		if (rCurrent.pDestroyedTimes[i] == std::chrono::duration<float>::zero())
		{
			continue;
		}

		XMFLOAT3A f3Scale = alignment.f3Scale;
		if (rCurrent.pDestroyedTimes[i] > std::chrono::duration<float>::zero())
		{
			float fDeathScale = std::pow(rCurrent.pDestroyedTimes[i] / kMissileDestroyTime, 0.5f);
			f3Scale.x *= fDeathScale;
			f3Scale.y *= fDeathScale;
			f3Scale.z *= fDeathScale;
		}

		XMMATRIX matScaling = XMMatrixScaling(f3Scale.x, f3Scale.y, f3Scale.z);
		XMMATRIX matYaw = common::RotationMatrixFromDirection(rCurrent.pVecDirections[i], XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f));
		XMMATRIX matTranslation = XMMatrixTranslationFromVector(vecPosition);
		XMMATRIX matTransform = alignment.matPreMove * matScaling * alignment.matPreRotate * matYaw * matTranslation;

		shaders::ModelLayout& rModelLayout = pLayouts[siRendered++];
		rModelLayout.f4PositionLayer = f4Position;
		rModelLayout.f4PositionLayer.w = static_cast<float>(engine::gpFlightPlanes->NearestPlaneIndex(f4Position.z));
		XMStoreFloat3x4(reinterpret_cast<XMFLOAT3X4*>(&rModelLayout.f3x4Transform[0]), matTransform);
		XMStoreFloat3x4(reinterpret_cast<XMFLOAT3X4*>(&rModelLayout.f3x4TransformNormal[0]), XMMatrixTranspose(XMMatrixInverse(nullptr, matTransform)));
		rModelLayout.f4ColorAdd = {0.0f, 0.0f, 0.0f, 0.0f};
		rModelLayout.uiMeshDataBase = 0;
	}
}

void MissilesInterpolate::EndRender([[maybe_unused]] int64_t iCommandBuffer)
{
	if constexpr (kbProfiling)
	{
		gpProfileManager->GetCpuCounter(game::kCpuCounterMissilesRendered).iCount = siRendered;
	}
	engine::gpPipelineManager->mDynamicPipelines.WriteModelPipelineIndirectBuffers(kuiCrc, iCommandBuffer, siRendered);
}

} // namespace game

#endif // BT_CLIENT
