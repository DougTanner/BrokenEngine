#if defined(BT_CLIENT)

#include "Fighters.h"

#include "File/PackChunks.h"

#include "Frame/Collections/Ships/ShipBehaviours.h"
#include "Frame/Collections/Ships/ShipHitboxes.h"
#include "Frame/Collections/Ships/ShipModels.h"
#include "Profile/ProfileRows.h"
#include "Game.h"

namespace game
{

constexpr float kfRoll = 0.2f;

// One model pipeline per ShipController, indexed by it.
constexpr int64_t kiPipelineCount = std::to_underlying(ShipController::kCount);
constexpr std::array<const char*, kiPipelineCount> kpcPipelineNames {"FightersFleet", "FightersRandom"};
constexpr std::array<common::crc_t, kiPipelineCount> kuiPipelineCrcs {common::CrcConsteval("FightersFleet"), common::CrcConsteval("FightersRandom")};
const std::array<common::crc_t, kiPipelineCount> kuiModels {kuiFighterFleetModel, kuiFighterRandomModel};

constexpr int64_t kiDynamicBufferInitialCapacity = 1'024;

void FightersInterpolate::GraphicsResources()
{
	for (int64_t i = 0; i < kiPipelineCount; ++i)
	{
		engine::gpPipelineManager->mDynamicPipelines.CreateModelPipelines(kuiPipelineCrcs[i], kpcPipelineNames[i], kuiModels[i], kiDynamicBufferInitialCapacity);
	}
}

void FightersInterpolate::RequestTextures(const std::unordered_map<engine::GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<engine::GridCoord>& rActiveCoordinates)
{
	std::array<bool, kiPipelineCount> bPresent {};
	for (const engine::GridCoord& rCoordinate : rActiveCoordinates)
	{
		auto it = rRenderInterpolates.find(rCoordinate);
		if (it == rRenderInterpolates.end())
		{
			continue;
		}

		// The interpolated rows align with the snapshot they were interpolated from, which holds each row's controller.
		const FightersPostRender& rPostRender = *gpGame->RenderFrame(rCoordinate).postRender.pFighters;
		for (int64_t i = 0; i < it->second.pFighters->iCount; ++i)
		{
			bPresent[std::to_underlying(rPostRender.pController[i])] = true;
		}
	}

	engine::gpPipelineManager->mDynamicPipelines.RequestModelPipelineTextures(kuiPipelineCrcs, bPresent);
}

// Draws the rows whose controller selects iPipeline from pLayouts on and returns how many it wrote. piVisibleIndices is
// scratch for every row.
static int64_t RenderPipeline(const FightersInterpolate& __restrict rCurrent, const FightersPostRender& __restrict rPostRender, const engine::RenderBasis& rBasis, int64_t iPipeline, int64_t* piVisibleIndices, shaders::ModelLayout* pLayouts, int64_t iCommandBuffer)
{
	const engine::AnimationData* pAnimationData = nullptr;
	if (engine::gAnimationDataMap.contains(kuiModels[iPipeline]))
	{
		pAnimationData = &engine::gAnimationDataMap.at(kuiModels[iPipeline]);
	}

	// Visibility culling and the controller partition run on the main thread before worker dispatch.
	float fRadius = gShipHitboxShapes[std::to_underlying(ShipType::kFighter)][iPipeline].fRadius;
	int64_t iVisibleCount = 0;

	for (int64_t i = 0; i < rCurrent.iCount; ++i)
	{
		if (std::to_underlying(rPostRender.pController[i]) != iPipeline)
		{
			continue;
		}

		if (rCurrent.pDestroyedTimes[i] == std::chrono::duration<float>::zero())
		{
			continue;
		}

		// Positions are local to the rendered cell; the visible area is in the camera cell's frame.
		XMFLOAT4A f4Position {};
		XMStoreFloat4A(&f4Position, engine::Rebase(rBasis, rCurrent.pVecPositions[i]));
		if (!engine::gpCamera->InVisibleArea(engine::gpCamera->mf4RenderVisibleArea, f4Position, fRadius, fRadius, fRadius, fRadius))
		{
			continue;
		}

		piVisibleIndices[iVisibleCount++] = i;
	}

	// Skinning output for all visible rows of this pipeline is allocated on the main thread before worker dispatch.
	engine::SkinningReservation skinningReservation {};
	if (pAnimationData != nullptr)
	{
		skinningReservation = pAnimationData->ReserveSkinning(iCommandBuffer, iVisibleCount);
	}

	const std::unordered_map<common::crc_t, engine::EagerChunk>& rChunkMap = engine::gpFileManager->mpPackChunks->GetEagerChunkMap();
	const common::SceneHeader& rSceneHeader = rChunkMap.at(kuiModels[iPipeline]).pHeader->sceneHeader;
	ASSERT(rSceneHeader.modelCrc == kShipHitboxTypes[std::to_underlying(ShipType::kFighter)].models[iPipeline]);
	const engine::EagerChunk& rModelChunk = rChunkMap.at(rSceneHeader.modelCrc);
	XMMATRIX matAlignment = common::GetModelAlignment(rModelChunk.pHeader->modelHeader);
	engine::ModelShadowBounds shadowBounds = engine::ComputeModelShadowBounds(rModelChunk);

	// Per-range processing lambda — each visible index j writes to deterministic non-overlapping output slots
	auto ProcessRange = [&](int64_t iStart, int64_t iEnd)
	{
		for (int64_t j = iStart; j < iEnd; ++j)
		{
			int64_t i = piVisibleIndices[j];

			float fDeathScale = 1.0f;
			if (rCurrent.pDestroyedTimes[i] > std::chrono::duration<float>::zero())
			{
				fDeathScale = std::pow(rCurrent.pDestroyedTimes[i] / kFighterDestroyTime, 0.75f);
			}

			XMVECTOR vecPosition = engine::Rebase(rBasis, rCurrent.pVecPositions[i]);

			XMMATRIX matScaling = XMMatrixScaling(fDeathScale, fDeathScale, fDeathScale);
			XMMATRIX matRoll = XMMatrixRotationX(-kfRoll * rCurrent.pfDeltaRotations[i]);
			// Negated so a positive climb tilt raises the +X nose.
			XMMATRIX matClimbTilt = XMMatrixRotationY(-rCurrent.pfClimbTilts[i]);
			XMMATRIX matYaw = common::RotationMatrixFromDirection(rCurrent.pVecDirections[i], XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f));
			XMMATRIX matTranslation = XMMatrixTranslationFromVector(vecPosition);
			XMMATRIX matInstanceTransform = matScaling * matRoll * matClimbTilt * matYaw * matTranslation;
			int64_t iPlane = engine::gpFlightPlanes->NearestPlaneIndex(XMVectorGetZ(vecPosition));
			engine::FillModelLayout(shadowBounds, matAlignment, matInstanceTransform, vecPosition, iPlane, pAnimationData, skinningReservation, j, rCurrent.pAnimationTimes[i], pLayouts[j]);
		}
	};

	gpProfileManager->GetCpuTimer(game::kCpuTimerRenderFighters).iThreads = common::gpMultithreading->WorkerCount() + 1;
	common::gpMultithreading->Dispatch(iVisibleCount, ProcessRange);

	return iVisibleCount;
}

void FightersInterpolate::BeginRender(int64_t iCommandBuffer, const std::unordered_map<engine::GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<engine::GridCoord>& rActiveCoordinates)
{
	engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerRenderFighters);

	int64_t iTotalCapacity = engine::AccumulateRenderCapacity(rRenderInterpolates, rActiveCoordinates, [](const game::FrameInterpolate& rInterpolate) -> const FightersInterpolate&
	{
		return *rInterpolate.pFighters;
	});

	// Any row can use either pipeline, so each reserves the whole capacity. Each cell's rows follow the previous cell's.
	int64_t iRendered = 0;
	for (int64_t i = 0; i < kiPipelineCount; ++i)
	{
		iRendered += engine::gpPipelineManager->mDynamicPipelines.RenderModelPipelines(kuiPipelineCrcs[i], kpcPipelineNames[i], iCommandBuffer, iTotalCapacity, [&](shaders::ModelLayout* pLayouts)
		{
			int64_t iCount = 0;
			for (const engine::GridCoord& rCoordinate : rActiveCoordinates)
			{
				auto it = rRenderInterpolates.find(rCoordinate);
				if (it == rRenderInterpolates.end() || it->second.pFighters->iCount == 0)
				{
					continue;
				}

				const FightersInterpolate& rCurrent = *it->second.pFighters;
				// The interpolated rows align with the snapshot they were interpolated from, which holds each row's controller.
				const Frame& rRenderFrame = gpGame->RenderFrame(rCoordinate);
				auto visibleIndicesAllocation = common::gpThreadLocal->mWorkbuffer.PushBuffer<int64_t*>(rCurrent.iCount * static_cast<int64_t>(sizeof(int64_t)));
				iCount += RenderPipeline(rCurrent, *rRenderFrame.postRender.pFighters, it->second.renderBasis, i, visibleIndicesAllocation.mpData, pLayouts + iCount, iCommandBuffer);
			}
			return iCount;
		});
	}

	if constexpr (kbProfiling)
	{
		gpProfileManager->GetCpuCounter(game::kCpuCounterFightersRendered).iCount = iRendered;
	}
}

void FightersInterpolate::Render(const FrameInterpolate& __restrict rFrameInterpolate, [[maybe_unused]] int64_t iCommandBuffer)
{
	engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerRenderFighters);

	if constexpr (kbDebugRender)
	{
		const FightersInterpolate& rCurrent = *rFrameInterpolate.pFighters;
		const engine::RenderBasis& rBasis = rFrameInterpolate.renderBasis;
		const Frame& rRenderFrame = gpGame->RenderFrame(rBasis.coordinate);
		const FightersPostRender& rPostRender = *rRenderFrame.postRender.pFighters;

		// Positions come from rFrameInterpolate (the fully-interpolated frame); PostRender supplies only flags, metadata,
		// and static world positions.
		for (int64_t i = 0; i < rCurrent.iCount; ++i)
		{
			DebugRenderShipHitbox(rBasis, ShipType::kFighter, rPostRender.pController[i], rCurrent.pVecPositions[i], rCurrent.pVecDirections[i]);
			if (rPostRender.pController[i] == ShipController::kFleet)
			{
				DebugRenderNavigation(rFrameInterpolate, rRenderFrame, rCurrent.pVecPositions[i], rPostRender.pVecDebugNavigationWaypoints[i], rPostRender.pVecIslandDestinations[i], rPostRender.pFlags[i], rPostRender.pFlagshipGlobalIds[i], kfFighterRadius);
			}
		}
	}
}

} // namespace game

#endif // BT_CLIENT
