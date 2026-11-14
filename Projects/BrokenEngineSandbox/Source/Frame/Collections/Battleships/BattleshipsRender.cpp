#if defined(BT_CLIENT)

#include "Battleships.h"

#include "Data/Scene.h"
#include "File/PackChunks.h"

#include "Frame/Collections/Ships/ShipHitboxes.h"
#include "Frame/Collections/Wrecks/Wrecks.h"
#include "Profile/ProfileRows.h"
#include "Game.h"

namespace game
{

// One model per ShipController, also used for animation lookup.
extern const common::crc_t kBattleshipFleetModel = data::kModelsBattleshipJesterz86scenegltfCrc;
extern const common::crc_t kBattleshipRandomModel = data::kModelsSpaceBattleshipAquamarinescenegltfCrc;

// One model pipeline per ShipController, indexed by it.
constexpr int64_t kiPipelineCount = std::to_underlying(ShipController::kCount);
constexpr std::array<const char*, kiPipelineCount> kpcPipelineNames {"BattleshipsFleet", "BattleshipsRandom"};
constexpr std::array<common::crc_t, kiPipelineCount> kuiPipelineCrcs {common::CrcConsteval("BattleshipsFleet"), common::CrcConsteval("BattleshipsRandom")};
const std::array<common::crc_t, kiPipelineCount> kuiModels {kBattleshipFleetModel, kBattleshipRandomModel};

constexpr int64_t kiDynamicBufferInitialCapacity = 256;

void BattleshipsInterpolate::GraphicsResources()
{
	for (int64_t i = 0; i < kiPipelineCount; ++i)
	{
		engine::Buffer* pStorageBuffers = engine::gpBufferManager->CreateDynamicBuffer(kuiPipelineCrcs[i], engine::kBufferMain, kpcPipelineNames[i], sizeof(shaders::ModelLayout), kiDynamicBufferInitialCapacity);
		engine::gpPipelineManager->mDynamicPipelines.CreateModelPipelines(kuiPipelineCrcs[i], kpcPipelineNames[i], kuiModels[i], pStorageBuffers);
	}
}

static std::array<int64_t, kiPipelineCount> siRendered {};
// Non-concurrency tripwire: per-frame Render calls must run sequentially across active coords — siRendered
// accumulates across them and offsets each call's slab writes. Parallelizing coord renders would race.
static std::atomic<bool> sbRenderActive = false;

// sbRenderActive must clear during exception unwinding so subsequent Render calls do not fail the concurrency ASSERT.
struct [[nodiscard]] BattleshipsRenderActiveGuard
{
	BattleshipsRenderActiveGuard()
	{
		ASSERT(!sbRenderActive.exchange(true));
	}

	BattleshipsRenderActiveGuard(const BattleshipsRenderActiveGuard&) = delete;
	BattleshipsRenderActiveGuard& operator=(const BattleshipsRenderActiveGuard&) = delete;

	~BattleshipsRenderActiveGuard()
	{
		sbRenderActive.store(false);
	}
};

static int64_t siWrecksRendered = 0;

void BattleshipsInterpolate::BeginRender([[maybe_unused]] int64_t iCommandBuffer, const std::unordered_map<engine::GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<engine::GridCoord>& rActiveCoordinates)
{
	siRendered = {};
	siWrecksRendered = 0;

	int64_t iTotalCapacity = engine::AccumulateRenderCapacity(rRenderInterpolates, rActiveCoordinates, [](const game::FrameInterpolate& rInterpolate) -> const BattleshipsInterpolate&
	{
		return *rInterpolate.pBattleships;
	}) + engine::AccumulateRenderCapacity(rRenderInterpolates, rActiveCoordinates, [](const game::FrameInterpolate& rInterpolate) -> const WrecksInterpolate&
	{
		return *rInterpolate.pWrecks;
	});

	if (iTotalCapacity == 0)
	{
		return;
	}

	// Any row can use either pipeline, so each reserves the whole capacity.
	for (int64_t i = 0; i < kiPipelineCount; ++i)
	{
		if (engine::Buffer* pBuffer = engine::gpBufferManager->ResizeDynamicBufferIfNeeded(kuiPipelineCrcs[i], engine::kBufferMain, kpcPipelineNames[i], sizeof(shaders::ModelLayout), iTotalCapacity, iCommandBuffer); pBuffer != nullptr)
		{
			engine::gpPipelineManager->mDynamicPipelines.UpdateModelPipelineDescriptors(kuiPipelineCrcs[i], iCommandBuffer, 2, pBuffer);
		}
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

	for (int64_t i = 0; i < kiPipelineCount; ++i)
	{
		if (bPresent[i])
		{
			engine::gpPipelineManager->mDynamicPipelines.RequestModelPipelineTextures(kuiPipelineCrcs[i]);
		}
	}
}

// Writes one model instance; vecPosition is local to rBasis's cell, and vecTilt applies after the yaw.
static void XM_CALLCONV WriteModelLayout(const engine::RenderBasis& rBasis, const engine::ModelShadowBounds& rShadowBounds, FXMMATRIX matAlignment, FXMVECTOR vecPosition, FXMVECTOR vecDirection, FXMVECTOR vecTilt, const engine::AnimationData* pAnimationData, const engine::SkinningReservation& rReservation, int64_t iInstance, std::chrono::duration<float> animationTime, shaders::ModelLayout& rModelLayout)
{
	XMVECTOR vecRenderPosition = engine::Rebase(rBasis, vecPosition);
	XMFLOAT4A f4Position {};
	XMStoreFloat4A(&f4Position, vecRenderPosition);

	XMMATRIX matYaw = common::RotationMatrixFromDirection(vecDirection, XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f));
	XMMATRIX matTilt = XMMatrixRotationQuaternion(vecTilt);
	XMMATRIX matTranslation = XMMatrixTranslationFromVector(vecRenderPosition);
	XMMATRIX matInstanceTransform = matYaw * matTilt * matTranslation;
	XMMATRIX matTransform = matAlignment * matInstanceTransform;

	rModelLayout.f4PositionLayer = f4Position;
	rModelLayout.f2ShadowWorldXRange = {engine::ComputeModelShadowRange(rShadowBounds, matInstanceTransform, vecRenderPosition)};
	rModelLayout.f4PositionLayer.w = static_cast<float>(engine::gpFlightPlanes->NearestPlaneIndex(f4Position.z));
	XMStoreFloat3x4(reinterpret_cast<XMFLOAT3X4*>(&rModelLayout.f3x4Transform[0]), matTransform);
	XMStoreFloat3x4(reinterpret_cast<XMFLOAT3X4*>(&rModelLayout.f3x4TransformNormal[0]), XMMatrixTranspose(XMMatrixInverse(nullptr, matTransform)));

	rModelLayout.f4ColorAdd = {0.0f, 0.0f, 0.0f, 0.0f};
	rModelLayout.uiMeshDataBase = 0;

	if (pAnimationData != nullptr)
	{
		rModelLayout.uiMeshDataBase = static_cast<uint32_t>(pAnimationData->EvaluateInstance(rReservation, iInstance, animationTime.count()));
	}
}

// Draws the rows and wrecks whose controller selects iPipeline into that pipeline's buffer. piVisibleIndices is scratch for
// every row and wreck.
static void RenderPipeline(const BattleshipsInterpolate& __restrict rCurrent, const BattleshipsPostRender& __restrict rPostRender, const WrecksInterpolate& __restrict rWrecks, const WrecksPostRender& __restrict rWrecksPostRender, const engine::RenderBasis& rBasis, int64_t iPipeline, int64_t* piVisibleIndices, int64_t iCommandBuffer)
{
	auto [pLayouts, iBufferCapacity] = engine::gpBufferManager->GetDynamicStorageBuffer<shaders::ModelLayout>(kuiPipelineCrcs[iPipeline], engine::kBufferMain, iCommandBuffer);
	ASSERT(siRendered[iPipeline] + rCurrent.iCount <= iBufferCapacity);

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

	int64_t iRenderedOffset = siRendered[iPipeline];
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
			WriteModelLayout(rBasis, shadowBounds, matAlignment, rCurrent.pVecPositions[i], rCurrent.pVecDirections[i], XMQuaternionIdentity(), pAnimationData, skinningReservation, j, rCurrent.pAnimationTimes[i], pLayouts[iRenderedOffset + j]);
		}
	};

	gpProfileManager->GetCpuTimer(game::kCpuTimerRenderBattleships).iThreads = common::gpMultithreading->WorkerCount() + 1;
	common::gpMultithreading->Dispatch(iVisibleCount, ProcessRange);

	ASSERT(iRenderedOffset + iVisibleCount + iWreckCount <= iBufferCapacity);
	for (int64_t k = 0; k < iWreckCount; ++k)
	{
		int64_t iWreck = piVisibleIndices[iVisibleCount + k];
		int64_t iInstance = iVisibleCount + k;
		WriteModelLayout(rBasis, shadowBounds, matAlignment, rWrecks.pVecPositions[iWreck], rWrecks.pVecDirections[iWreck], WreckTilt(rWrecks, rWrecksPostRender, iWreck), pAnimationData, skinningReservation, iInstance, rWrecks.pAnimationTimes[iWreck], pLayouts[iRenderedOffset + iInstance]);
	}

	siRendered[iPipeline] += iVisibleCount + iWreckCount;
	siWrecksRendered += iWreckCount;
}

void BattleshipsInterpolate::Render(const FrameInterpolate& __restrict rFrameInterpolate, int64_t iCommandBuffer)
{
	engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerRenderBattleships);

	const BattleshipsInterpolate& rCurrent = *rFrameInterpolate.pBattleships;
	const WrecksInterpolate& rWrecks = *rFrameInterpolate.pWrecks;
	const engine::RenderBasis& rBasis = rFrameInterpolate.renderBasis;

	if (rCurrent.iCount == 0 && rWrecks.iCount == 0)
	{
		return;
	}

	BattleshipsRenderActiveGuard renderActiveGuard;

	// The interpolated rows align with the snapshot they were interpolated from, which holds each row's controller.
	const Frame& rRenderFrame = gpGame->RenderFrame(rBasis.coordinate);
	const BattleshipsPostRender& rPostRender = *rRenderFrame.postRender.pBattleships;
	const WrecksPostRender& rWrecksPostRender = *rRenderFrame.postRender.pWrecks;

	auto visibleIndicesAllocation = common::gpThreadLocal->mWorkbuffer.PushBuffer<int64_t*>((rCurrent.iCount + rWrecks.iCount) * static_cast<int64_t>(sizeof(int64_t)));

	for (int64_t i = 0; i < kiPipelineCount; ++i)
	{
		RenderPipeline(rCurrent, rPostRender, rWrecks, rWrecksPostRender, rBasis, i, visibleIndicesAllocation.mpData, iCommandBuffer);
	}
}

void BattleshipsInterpolate::EndRender([[maybe_unused]] int64_t iCommandBuffer)
{
	if constexpr (kbProfiling)
	{
		// siRendered also counts the wrecks, which the indirect draw needs.
		gpProfileManager->GetCpuCounter(game::kCpuCounterBattleshipsRendered).iCount = siRendered[0] + siRendered[1] - siWrecksRendered;
		gpProfileManager->GetCpuCounter(game::kCpuCounterBattleshipWrecks).iCount = siWrecksRendered;
	}
	for (int64_t i = 0; i < kiPipelineCount; ++i)
	{
		engine::gpPipelineManager->mDynamicPipelines.WriteModelPipelineIndirectBuffers(kuiPipelineCrcs[i], iCommandBuffer, siRendered[i]);
	}
}

void BattleshipsInterpolate::DebugRender(const FrameInterpolate& __restrict rFrameInterpolate, engine::GridCoord coordinate)
{
	if constexpr (!kbDebugRender)
	{
		return;
	}

	// Positions come from rFrameInterpolate (the fully-interpolated frame); PostRender supplies only flags, metadata, and
	// static world positions.
	const BattleshipsInterpolate& rBattleships = *rFrameInterpolate.pBattleships;
	const Frame& rRenderFrame = gpGame->RenderFrame(coordinate);
	const BattleshipsPostRender& rPostRender = *rRenderFrame.postRender.pBattleships;

	for (int64_t i = 0; i < rBattleships.iCount; ++i)
	{
		DebugRenderShipHitbox(rFrameInterpolate.renderBasis, ShipType::kBattleship, rPostRender.pController[i], rBattleships.pVecPositions[i], rBattleships.pVecDirections[i]);
		if (rPostRender.pController[i] == ShipController::kFleet)
		{
			DebugRenderNavigation(rFrameInterpolate, rRenderFrame, rBattleships.pVecPositions[i], rPostRender.pVecDebugNavigationWaypoints[i], rPostRender.pVecIslandDestinations[i], rPostRender.pFlags[i], rPostRender.pFlagshipGlobalIds[i], kfBattleshipRadius);
		}
	}
}

} // namespace game

#endif // BT_CLIENT
