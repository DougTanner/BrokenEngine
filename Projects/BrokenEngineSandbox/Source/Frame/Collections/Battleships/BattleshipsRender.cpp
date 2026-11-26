#if defined(BT_CLIENT)

#include "Battleships.h"

#include "File/PackChunks.h"

#include "Frame/Collections/Ships/ShipBehaviours.h"
#include "Frame/Collections/Ships/ShipHitboxes.h"
#include "Frame/Collections/Ships/ShipModels.h"
#include "Frame/Collections/Wrecks/Wrecks.h"
#include "Profile/ProfileRows.h"
#include "Game.h"

namespace game
{

// One model pipeline per ShipController, indexed by it.
constexpr int64_t kiPipelineCount = std::to_underlying(ShipController::kCount);
constexpr std::array<const char*, kiPipelineCount> kpcPipelineNames {"BattleshipsFleet", "BattleshipsRandom"};
constexpr std::array<common::crc_t, kiPipelineCount> kuiPipelineCrcs {common::CrcConsteval("BattleshipsFleet"), common::CrcConsteval("BattleshipsRandom")};
const std::array<common::crc_t, kiPipelineCount> kuiModels {kuiBattleshipFleetModel, kuiBattleshipRandomModel};

constexpr int64_t kiDynamicBufferInitialCapacity = 256;

void BattleshipsInterpolate::GraphicsResources()
{
	for (int64_t i = 0; i < kiPipelineCount; ++i)
	{
		engine::gpPipelineManager->mDynamicPipelines.CreateModelPipelines(kuiPipelineCrcs[i], kpcPipelineNames[i], kuiModels[i], kiDynamicBufferInitialCapacity);
	}
}

void BattleshipsInterpolate::RequestTextures(const std::unordered_map<engine::GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<engine::GridCoord>& rActiveCoordinates)
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
		const BattleshipsPostRender& rPostRender = *gpGame->RenderFrame(rCoordinate).postRender.pBattleships;
		for (int64_t i = 0; i < it->second.pBattleships->iCount; ++i)
		{
			bPresent[std::to_underlying(rPostRender.pController[i])] = true;
		}
	}

	engine::gpPipelineManager->mDynamicPipelines.RequestModelPipelineTextures(kuiPipelineCrcs, bPresent);
}

// Writes one model instance; vecPosition is local to rBasis's cell, and vecTilt applies after the yaw.
static void XM_CALLCONV WriteModelLayout(const engine::RenderBasis& rBasis, const engine::ModelShadowBounds& rShadowBounds, FXMMATRIX matAlignment, FXMVECTOR vecPosition, FXMVECTOR vecDirection, FXMVECTOR vecTilt, const engine::AnimationData* pAnimationData, const engine::SkinningReservation& rReservation, int64_t iInstance, std::chrono::duration<float> animationTime, shaders::ModelLayout& rModelLayout)
{
	XMVECTOR vecRenderPosition = engine::Rebase(rBasis, vecPosition);

	XMMATRIX matYaw = common::RotationMatrixFromDirection(vecDirection, XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f));
	XMMATRIX matTilt = XMMatrixRotationQuaternion(vecTilt);
	XMMATRIX matTranslation = XMMatrixTranslationFromVector(vecRenderPosition);
	XMMATRIX matInstanceTransform = matYaw * matTilt * matTranslation;
	engine::FillModelLayout(rShadowBounds, matAlignment, matInstanceTransform, vecRenderPosition, pAnimationData, rReservation, iInstance, animationTime, rModelLayout);
}

// Draws the rows and wrecks whose controller selects iPipeline from pLayouts on, adds the wrecks to riWrecks, and returns
// how many it wrote. piVisibleIndices is scratch for every row and wreck.
static int64_t RenderPipeline(const BattleshipsInterpolate& __restrict rCurrent, const BattleshipsPostRender& __restrict rPostRender, const WrecksInterpolate& __restrict rWrecks, const WrecksPostRender& __restrict rWrecksPostRender, const engine::RenderBasis& rBasis, int64_t iPipeline, int64_t* piVisibleIndices, shaders::ModelLayout* pLayouts, int64_t& riWrecks, int64_t iCommandBuffer)
{
	const engine::AnimationData* pAnimationData = nullptr;
	if (engine::gAnimationDataMap.contains(kuiModels[iPipeline]))
	{
		pAnimationData = &engine::gAnimationDataMap.at(kuiModels[iPipeline]);
	}

	// Visibility culling and the controller partition run on the main thread before worker dispatch.
	float fRadius = gShipHitboxShapes[std::to_underlying(ShipType::kBattleship)][iPipeline].fRadius;
	int64_t iVisibleCount = 0;

	for (int64_t i = 0; i < rCurrent.iCount; ++i)
	{
		if (std::to_underlying(rPostRender.pController[i]) != iPipeline)
		{
			continue;
		}

		// A dying row draws as a wreck.
		if (rCurrent.pDestroyedTimes[i].count() >= 0.0f)
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

	// Visible wrecks follow the rows: the n-th takes visible slot and skinning instance iVisibleCount + n, and its slot
	// stores the wreck's row index.
	int64_t iWreckCount = 0;
	for (int64_t k = 0; k < rWrecks.iCount; ++k)
	{
		if (std::to_underlying(rWrecksPostRender.pController[k]) != iPipeline)
		{
			continue;
		}

		if (!engine::gpCamera->InVisibleArea(engine::gpCamera->mf4RenderVisibleArea, engine::Rebase(rBasis, rWrecks.pVecPositions[k]), fRadius, fRadius, fRadius, fRadius))
		{
			continue;
		}

		piVisibleIndices[iVisibleCount + iWreckCount++] = k;
	}

	// Skinning output for all visible rows and wrecks of this pipeline is allocated on the main thread before worker dispatch.
	engine::SkinningReservation skinningReservation {};
	if (pAnimationData != nullptr)
	{
		skinningReservation = pAnimationData->ReserveSkinning(iCommandBuffer, iVisibleCount + iWreckCount);
	}

	const std::unordered_map<common::crc_t, engine::EagerChunk>& rChunkMap = engine::gpFileManager->mpPackChunks->GetEagerChunkMap();
	const common::SceneHeader& rSceneHeader = rChunkMap.at(kuiModels[iPipeline]).pHeader->sceneHeader;
	ASSERT(rSceneHeader.modelCrc == kShipHitboxTypes[std::to_underlying(ShipType::kBattleship)].models[iPipeline]);
	const engine::EagerChunk& rModelChunk = rChunkMap.at(rSceneHeader.modelCrc);
	XMMATRIX matAlignment = common::GetModelAlignment(rModelChunk.pHeader->modelHeader);
	engine::ModelShadowBounds shadowBounds = engine::ComputeModelShadowBounds(rModelChunk);

	// Per-range processing lambda — each visible index j writes to deterministic non-overlapping output slots
	auto ProcessRange = [&](int64_t iStart, int64_t iEnd)
	{
		for (int64_t j = iStart; j < iEnd; ++j)
		{
			int64_t i = piVisibleIndices[j];
			WriteModelLayout(rBasis, shadowBounds, matAlignment, rCurrent.pVecPositions[i], rCurrent.pVecDirections[i], XMQuaternionIdentity(), pAnimationData, skinningReservation, j, rCurrent.pAnimationTimes[i], pLayouts[j]);
		}
	};

	gpProfileManager->GetCpuTimer(game::kCpuTimerRenderBattleships).iThreads = common::gpMultithreading->WorkerCount() + 1;
	common::gpMultithreading->Dispatch(iVisibleCount, ProcessRange);

	for (int64_t k = 0; k < iWreckCount; ++k)
	{
		int64_t iWreck = piVisibleIndices[iVisibleCount + k];
		int64_t iInstance = iVisibleCount + k;
		WriteModelLayout(rBasis, shadowBounds, matAlignment, rWrecks.pVecPositions[iWreck], rWrecks.pVecDirections[iWreck], WreckTilt(rWrecks, rWrecksPostRender, iWreck), pAnimationData, skinningReservation, iInstance, rWrecks.pAnimationTimes[iWreck], pLayouts[iInstance]);
	}

	riWrecks += iWreckCount;
	return iVisibleCount + iWreckCount;
}

void BattleshipsInterpolate::BeginRender(int64_t iCommandBuffer, const std::unordered_map<engine::GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<engine::GridCoord>& rActiveCoordinates)
{
	engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerRenderBattleships);

	int64_t iTotalCapacity = engine::AccumulateRenderCapacity(rRenderInterpolates, rActiveCoordinates, [](const game::FrameInterpolate& rInterpolate) -> const BattleshipsInterpolate&
	{
		return *rInterpolate.pBattleships;
	}) + engine::AccumulateRenderCapacity(rRenderInterpolates, rActiveCoordinates, [](const game::FrameInterpolate& rInterpolate) -> const WrecksInterpolate&
	{
		return *rInterpolate.pWrecks;
	});

	// Any row can use either pipeline, so each reserves the whole capacity. Each cell's rows follow the previous cell's.
	int64_t iRendered = 0;
	int64_t iWrecks = 0;
	for (int64_t i = 0; i < kiPipelineCount; ++i)
	{
		iRendered += engine::gpPipelineManager->mDynamicPipelines.RenderModelPipelines(kuiPipelineCrcs[i], kpcPipelineNames[i], iCommandBuffer, iTotalCapacity, [&](shaders::ModelLayout* pLayouts)
		{
			int64_t iCount = 0;
			for (const engine::GridCoord& rCoordinate : rActiveCoordinates)
			{
				auto it = rRenderInterpolates.find(rCoordinate);
				if (it == rRenderInterpolates.end() || (it->second.pBattleships->iCount == 0 && it->second.pWrecks->iCount == 0))
				{
					continue;
				}

				const BattleshipsInterpolate& rCurrent = *it->second.pBattleships;
				const WrecksInterpolate& rWrecks = *it->second.pWrecks;
				// The interpolated rows align with the snapshot they were interpolated from, which holds each row's controller.
				const Frame& rRenderFrame = gpGame->RenderFrame(rCoordinate);
				auto visibleIndicesAllocation = common::gpThreadLocal->mWorkbuffer.PushBuffer<int64_t*>((rCurrent.iCount + rWrecks.iCount) * static_cast<int64_t>(sizeof(int64_t)));
				iCount += RenderPipeline(rCurrent, *rRenderFrame.postRender.pBattleships, rWrecks, *rRenderFrame.postRender.pWrecks, it->second.renderBasis, i, visibleIndicesAllocation.mpData, pLayouts + iCount, iWrecks, iCommandBuffer);
			}
			return iCount;
		});
	}

	if constexpr (kbProfiling)
	{
		// iRendered also counts the wrecks, which the indirect draw needs.
		gpProfileManager->GetCpuCounter(game::kCpuCounterBattleshipsRendered).iCount = iRendered - iWrecks;
		gpProfileManager->GetCpuCounter(game::kCpuCounterBattleshipWrecks).iCount = iWrecks;
	}
}

void BattleshipsInterpolate::Render(const FrameInterpolate& __restrict rFrameInterpolate, [[maybe_unused]] int64_t iCommandBuffer)
{
	engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerRenderBattleships);

	if constexpr (kbDebugRender)
	{
		const BattleshipsInterpolate& rCurrent = *rFrameInterpolate.pBattleships;
		const engine::RenderBasis& rBasis = rFrameInterpolate.renderBasis;
		const Frame& rRenderFrame = gpGame->RenderFrame(rBasis.coordinate);
		const BattleshipsPostRender& rPostRender = *rRenderFrame.postRender.pBattleships;

		// Positions come from rFrameInterpolate (the fully-interpolated frame); PostRender supplies only flags, metadata,
		// and static world positions.
		for (int64_t i = 0; i < rCurrent.iCount; ++i)
		{
			DebugRenderShipHitbox(rBasis, ShipType::kBattleship, rPostRender.pController[i], rCurrent.pVecPositions[i], rCurrent.pVecDirections[i]);
			if (rPostRender.pController[i] == ShipController::kFleet)
			{
				DebugRenderNavigation(rFrameInterpolate, rRenderFrame, rCurrent.pVecPositions[i], rPostRender.pVecDebugNavigationWaypoints[i], rPostRender.pVecIslandDestinations[i], rPostRender.pFlags[i], rPostRender.pFlagshipGlobalIds[i], kfBattleshipRadius);
			}
		}
	}
}

} // namespace game

#endif // BT_CLIENT
