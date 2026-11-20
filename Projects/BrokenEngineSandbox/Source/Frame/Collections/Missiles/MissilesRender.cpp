#if defined(BT_CLIENT)

#include "Missiles.h"

#include "Data/Scene.h"
#include "File/PackChunks.h"

#include "Profile/ProfileRows.h"

namespace game
{

constexpr common::crc_t kMissileModelCrc = data::kModelsaim9_missilescenegltfCrc;

constexpr int64_t kiDynamicBufferInitialCapacity = 512;

void MissilesInterpolate::GraphicsResources()
{
	engine::gpPipelineManager->mDynamicPipelines.CreateModelPipelines(kuiCrc, kpcName, kMissileModelCrc, kiDynamicBufferInitialCapacity);
}

void MissilesInterpolate::BeginRender(int64_t iCommandBuffer, const std::unordered_map<engine::GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<engine::GridCoord>& rActiveCoordinates)
{
	int64_t iTotalCapacity = engine::AccumulateRenderCapacity(rRenderInterpolates, rActiveCoordinates, [](const game::FrameInterpolate& rInterpolate) -> const MissilesInterpolate&
	{
		return *rInterpolate.pMissiles;
	});

	// Each cell's rows follow the previous cell's.
	int64_t iRendered = engine::gpPipelineManager->mDynamicPipelines.RenderModelPipelines(kuiCrc, kpcName, iCommandBuffer, iTotalCapacity, [&](shaders::ModelLayout* pLayouts)
	{
		int64_t iCount = 0;
		for (const engine::GridCoord& rCoordinate : rActiveCoordinates)
		{
			auto it = rRenderInterpolates.find(rCoordinate);
			if (it == rRenderInterpolates.end() || it->second.pMissiles->iCount == 0)
			{
				continue;
			}

			const MissilesInterpolate& rCurrent = *it->second.pMissiles;
			const engine::RenderBasis& rBasis = it->second.renderBasis;

			const std::unordered_map<common::crc_t, engine::EagerChunk>& rChunkMap = engine::gpFileManager->mpPackChunks->GetEagerChunkMap();
			const common::SceneHeader& rSceneHeader = rChunkMap.at(kMissileModelCrc).pHeader->sceneHeader;
			const engine::EagerChunk& rModelChunk = rChunkMap.at(rSceneHeader.modelCrc);
			XMMATRIX matAlignment = common::GetModelAlignment(rModelChunk.pHeader->modelHeader);
			engine::ModelShadowBounds shadowBounds = engine::ComputeModelShadowBounds(rModelChunk);

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

				float fDeathScale = 1.0f;
				if (rCurrent.pDestroyedTimes[i] > std::chrono::duration<float>::zero())
				{
					fDeathScale = std::pow(rCurrent.pDestroyedTimes[i] / kMissileDestroyTime, 0.5f);
				}

				XMMATRIX matScaling = XMMatrixScaling(fDeathScale, fDeathScale, fDeathScale);
				XMMATRIX matYaw = common::RotationMatrixFromDirection(rCurrent.pVecDirections[i], XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f));
				XMMATRIX matTranslation = XMMatrixTranslationFromVector(vecPosition);
				XMMATRIX matInstanceTransform = matScaling * matYaw * matTranslation;
				int64_t iPlane = engine::gpFlightPlanes->NearestPlaneIndex(f4Position.z);
				engine::FillModelLayout(shadowBounds, matAlignment, matInstanceTransform, vecPosition, iPlane, nullptr, {}, 0, {}, pLayouts[iCount++]);
			}
		}
		return iCount;
	});

	if constexpr (kbProfiling)
	{
		gpProfileManager->GetCpuCounter(game::kCpuCounterMissilesRendered).iCount = iRendered;
	}
}

void MissilesInterpolate::RequestTextures(const std::unordered_map<engine::GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<engine::GridCoord>& rActiveCoordinates)
{
	bool bPresent = std::ranges::any_of(rActiveCoordinates, [&](const engine::GridCoord& rCoordinate)
	{
		auto it = rRenderInterpolates.find(rCoordinate);
		return it != rRenderInterpolates.end() && it->second.pMissiles->iCount > 0;
	});
	if (bPresent)
	{
		engine::gpPipelineManager->mDynamicPipelines.RequestModelPipelineTextures(kuiCrc);
	}
}

} // namespace game

#endif // BT_CLIENT
