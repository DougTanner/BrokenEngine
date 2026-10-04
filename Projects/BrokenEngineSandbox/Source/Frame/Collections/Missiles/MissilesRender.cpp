#if defined(BT_CLIENT)

#include "Missiles.h"

#include "Data/Scene.h"

#include "Profile/ProfileManager.h"

namespace game
{

constexpr float kfMissileScale = 0.5f;
constexpr float kfMissileWidth = 2.0f;

constexpr common::crc_t kMissileModelCrc = data::kModelsaim9_missilescenegltfCrc;

static int64_t siRendered = 0;
// Non-concurrency tripwire: per-frame Render calls must run sequentially across active coords — siRendered
// accumulates across them and offsets each call's slab writes. Parallelizing coord renders would race.
static std::atomic<bool> sbRenderActive = false;

// sbRenderActive must clear during exception unwinding so subsequent Render calls do not fail the concurrency ASSERT.
struct MissilesRenderActiveGuard
{
	MissilesRenderActiveGuard()
	{
		ASSERT(!sbRenderActive.exchange(true));
	}

	~MissilesRenderActiveGuard()
	{
		sbRenderActive.store(false);
	}
};

void MissilesInterpolate::GraphicsResources()
{
	engine::Buffer* pStorageBuffers = engine::gpBufferManager->CreateDynamicBuffer(kuiCrc, engine::kBufferMain, kpcName, sizeof(shaders::ModelLayout));
	engine::gpPipelineManager->mDynamicPipelines.CreateModelPipeline(kuiCrc, kpcName, kMissileModelCrc, pStorageBuffers);
	engine::gpPipelineManager->mDynamicPipelines.CreateModelPipelineShadow(kuiCrc, kpcName, kMissileModelCrc, pStorageBuffers);
}

void MissilesInterpolate::BeginRender([[maybe_unused]] int64_t iCommandBuffer, const std::unordered_map<engine::GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<engine::GridCoord>& rActiveCoordinates)
{
	siRendered = 0;

	int64_t iTotalCapacity = 0;
	for (const engine::GridCoord& rCoordinate : rActiveCoordinates)
	{
		auto it = rRenderInterpolates.find(rCoordinate);
		if (it != rRenderInterpolates.end())
		{
			iTotalCapacity += it->second.pMissiles->iCapacity;
		}
	}

	if (iTotalCapacity == 0)
	{
		return;
	}

	if (engine::Buffer* pBuffer = engine::gpBufferManager->ResizeDynamicBufferIfNeeded(kuiCrc, engine::kBufferMain, kpcName, sizeof(shaders::ModelLayout), iTotalCapacity, iCommandBuffer); pBuffer != nullptr)
	{
		engine::gpPipelineManager->mDynamicPipelines.mModelPipelineMaps[engine::kDynamicModelPipelineModel].at(kuiCrc)->UpdateStorageBufferDescriptors(iCommandBuffer, 2, pBuffer);
		engine::gpPipelineManager->mDynamicPipelines.mModelPipelineMaps[engine::kDynamicModelPipelineModelShadow].at(kuiCrc)->UpdateStorageBufferDescriptors(iCommandBuffer, 2, pBuffer);
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

	static XMMATRIX sMatPreMove = XMMatrixTranslation(0.0f, 0.0f, 0.0f);
	static XMMATRIX sMatPreRotate = XMMatrixRotationX(XM_PIDIV2) * XMMatrixRotationZ(XM_PIDIV2);

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
		if (rCurrent.pfDestroyedTimes[i] == 0.0f)
		{
			continue;
		}

		float fScale = kfMissileScale;
		if (rCurrent.pfDestroyedTimes[i] > 0.0f)
		{
			fScale *= std::pow(rCurrent.pfDestroyedTimes[i] / kMissileDestroyTime.count(), 0.5f);
		}

		XMMATRIX matScaling = XMMatrixScaling(kfMissileWidth * fScale, fScale, fScale);
		XMMATRIX matYaw = common::RotationMatrixFromDirection(rCurrent.pVecDirections[i], XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f));
		XMMATRIX matTranslation = XMMatrixTranslationFromVector(vecPosition);
		XMMATRIX matTransform = sMatPreMove * matScaling * sMatPreRotate * matYaw * matTranslation;

		shaders::ModelLayout& rModelLayout = pLayouts[siRendered++];
		rModelLayout.f4Position = f4Position;
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
	engine::gpPipelineManager->mDynamicPipelines.mModelPipelineMaps[engine::kDynamicModelPipelineModel].at(kuiCrc)->WriteIndirectBuffer(iCommandBuffer, siRendered);
	engine::gpPipelineManager->mDynamicPipelines.mModelPipelineMaps[engine::kDynamicModelPipelineModelShadow].at(kuiCrc)->WriteIndirectBuffer(iCommandBuffer, siRendered);
}

} // namespace game

#endif // BT_CLIENT
