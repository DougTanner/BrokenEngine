#if defined(BT_CLIENT)

#include "Battleships.h"

#include "Data/Scene.h"
#include "File/PackChunks.h"

#include "Frame/Collections/Ships/ShipHitboxes.h"
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

constexpr std::chrono::duration<float> kBattleshipWreckPowerLossTime = std::chrono::duration<float>(1.0f);
constexpr float kfBattleshipWreckGravity = 9.8f;
constexpr float kfBattleshipWreckWaterDeceleration = 30.0f;
constexpr float kfBattleshipWreckSinkSpeed = 0.5f;

// A dying Battleship's model, drawn from capture and kept after its row is removed; client-only, outside every Frame.
struct BattleshipWreck
{
	engine::GridCoord coordinate {};
	battleship_t id {};
	ShipController eController {};
	XMVECTOR vecPosition {};    // Cell-local, W = 1.
	XMVECTOR vecDirection {};
	XMVECTOR vecTilt {};        // Quaternion from +Z to the terrain normal at the center; identity while bSinks.
	float fStartZ = 0.0f;
	float fRestZ = 0.0f;        // Unused while bSinks.
	float fTopOffset = 0.0f;    // Hull top above vecPosition.
	float fFallSpeed = 0.0f;
	std::chrono::duration<float> fallTime {};
	std::chrono::duration<float> animationTime {};  // Frozen at capture.
	bool bSinks = false;
};

static std::vector<BattleshipWreck> sWrecks;
static int64_t siWrecksRendered = 0;

// Drops the wrecks whose cell left view and the sunk ones, captures each visible dying row, then advances every wreck by
// fSeconds.
static void UpdateWrecks(const std::unordered_map<engine::GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<engine::GridCoord>& rActiveCoordinates, float fSeconds)
{
	// The non-map camera's visible area, the client subscription's test; map view leaves it unchanged.
	auto OverlapsWorldVisibleArea = [](engine::GridCoord coordinate)
	{
		return common::RectangleOverlapsHex(engine::gpCamera->mf4WorldVisibleArea, engine::CellCenterOffset(engine::gpCamera->mBasisCoordinate, coordinate), engine::kCellDirections);
	};

	std::erase_if(sWrecks, [&](const BattleshipWreck& rWreck)
	{
		bool bVisible = std::ranges::contains(rActiveCoordinates, rWreck.coordinate) && rRenderInterpolates.contains(rWreck.coordinate) && OverlapsWorldVisibleArea(rWreck.coordinate);
		return !bVisible || (rWreck.bSinks && XMVectorGetZ(rWreck.vecPosition) + rWreck.fTopOffset < 0.0f);
	});

	for (const engine::GridCoord& rCoordinate : rActiveCoordinates)
	{
		auto it = rRenderInterpolates.find(rCoordinate);
		if (it == rRenderInterpolates.end())
		{
			continue;
		}

		if (!OverlapsWorldVisibleArea(rCoordinate))
		{
			continue;
		}

		const BattleshipsInterpolate& rBattleships = *it->second.pBattleships;
		// The interpolated rows align with the snapshot they were interpolated from, which holds each row's Id and controller.
		const BattleshipsPostRender& rPostRender = *gpGame->RenderFrame(rCoordinate).postRender.pBattleships;
		const engine::CellStaticData& rStaticData = gpGame->mCells.at(rCoordinate).staticData;
		engine::CellElevationSampler sampler = engine::gpIslandTerrain->MakeCellElevationSampler(rStaticData);

		for (int64_t i = 0; i < rBattleships.iCount; ++i)
		{
			if (rBattleships.pDestroyedTimes[i].count() < 0.0f)
			{
				continue;
			}

			battleship_t id = rPostRender.pIds[i];
			bool bCaptured = std::ranges::any_of(sWrecks, [&](const BattleshipWreck& rWreck)
			{
				return rWreck.coordinate == rCoordinate && rWreck.id == id;
			});
			if (bCaptured)
			{
				continue;
			}

			XMVECTOR vecPosition = rBattleships.pVecPositions[i];
			engine::CollisionHitbox hitbox {};
			BuildShipHitbox(ShipType::kBattleship, rPostRender.pController[i], vecPosition, rBattleships.pVecDirections[i], hitbox);
			ASSERT(hitbox.eHitboxLevel >= common::ModelHitboxLevel::kBox);

			// The box rests on the highest terrain or seafloor under its center and four corners.
			XMVECTOR vecCenter = XMVectorSetZ(vecPosition, 0.0f);
			float fHighestElevation = sampler.Sample(vecCenter);
			for (const XMFLOAT2& rf2Corner : hitbox.pf2BoxVertices)
			{
				fHighestElevation = std::max(fHighestElevation, sampler.Sample(XMVectorSet(rf2Corner.x, rf2Corner.y, 0.0f, 1.0f)));
			}

			float fStartZ = XMVectorGetZ(vecPosition);
			BattleshipWreck wreck
			{
				.coordinate = rCoordinate,
				.id = id,
				.eController = rPostRender.pController[i],
				.vecPosition = vecPosition,
				.vecDirection = rBattleships.pVecDirections[i],
				.vecTilt = XMQuaternionIdentity(),
				.fStartZ = fStartZ,
				.fTopOffset = hitbox.fMaximumZ - fStartZ,
				.animationTime = rBattleships.pAnimationTimes[i],
				.bSinks = fHighestElevation <= sampler.fSeaFloor,
			};
			if (!wreck.bSinks)
			{
				wreck.fRestZ = std::min(fStartZ, fHighestElevation + (fStartZ - hitbox.fMinimumZ));
				wreck.vecTilt = common::QuaternionFromDirection(engine::gpIslandTerrain->CellNormal(rStaticData, vecCenter), XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f));
			}

			// Heap: the wreck list grows past its largest size so far
			ScopedSuppressAllocationTracking suppress;
			sWrecks.push_back(wreck);
		}
	}

	for (BattleshipWreck& rWreck : sWrecks)
	{
		// Gravity ramps up as the ship loses power, the water brakes the fall to a slow sink, and the terrain stops it.
		rWreck.fallTime += std::chrono::duration<float>(fSeconds);
		float fGravity = kfBattleshipWreckGravity * std::min(rWreck.fallTime / kBattleshipWreckPowerLossTime, 1.0f);
		float fZ = XMVectorGetZ(rWreck.vecPosition);
		if (fZ > 0.0f)
		{
			rWreck.fFallSpeed += fGravity * fSeconds;
		}
		else
		{
			rWreck.fFallSpeed = std::max(rWreck.fFallSpeed - kfBattleshipWreckWaterDeceleration * fSeconds, kfBattleshipWreckSinkSpeed);
		}
		fZ -= rWreck.fFallSpeed * fSeconds;
		if (!rWreck.bSinks && fZ <= rWreck.fRestZ)
		{
			fZ = rWreck.fRestZ;
			rWreck.fFallSpeed = 0.0f;
		}
		rWreck.vecPosition = XMVectorSetZ(rWreck.vecPosition, fZ);
	}
}

void BattleshipsInterpolate::ClearWrecks()
{
	sWrecks.clear();
}

void BattleshipsInterpolate::BeginRender([[maybe_unused]] int64_t iCommandBuffer, const std::unordered_map<engine::GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<engine::GridCoord>& rActiveCoordinates)
{
	siRendered = {};
	siWrecksRendered = 0;

	// The sim-scaled render delta keeps running while paused; only the render clock stops.
	float fSeconds = (gpGame->mGameFlags & engine::GameFlags::kPaused) ? 0.0f : static_cast<float>(gpGame->mfLastRenderFrameSeconds);
	UpdateWrecks(rRenderInterpolates, rActiveCoordinates, fSeconds);

	int64_t iTotalCapacity = engine::AccumulateRenderCapacity(rRenderInterpolates, rActiveCoordinates, [](const game::FrameInterpolate& rInterpolate) -> const BattleshipsInterpolate&
	{
		return *rInterpolate.pBattleships;
	}) + std::ssize(sWrecks);

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
static void RenderPipeline(const BattleshipsInterpolate& __restrict rCurrent, const BattleshipsPostRender& __restrict rPostRender, const engine::RenderBasis& rBasis, int64_t iPipeline, int64_t* piVisibleIndices, int64_t iCommandBuffer)
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
	// stores the wreck's index into sWrecks.
	int64_t iWreckCount = 0;
	for (int64_t k = 0; k < std::ssize(sWrecks); ++k)
	{
		const BattleshipWreck& rWreck = sWrecks.at(k);
		if (rWreck.coordinate != rBasis.coordinate)
		{
			continue;
		}

		if (std::to_underlying(rWreck.eController) != iPipeline)
		{
			continue;
		}

		if (!engine::gpCamera->InVisibleArea(engine::gpCamera->mf4RenderVisibleArea, engine::Rebase(rBasis, rWreck.vecPosition), fRadius, fRadius, fRadius, fRadius))
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
	engine::ModelShadowBounds shadowBounds = engine::ComputeModelShadowBounds(*reinterpret_cast<const common::ModelOutline*>(rModelChunk.pData + common::ModelHeader::OutlineOffset(rModelChunk.pHeader->iSize)));

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
		const BattleshipWreck& rWreck = sWrecks.at(piVisibleIndices[iVisibleCount + k]);

		// The tilt toward the terrain normal grows with the fall and is complete at rest.
		XMVECTOR vecTilt = XMQuaternionIdentity();
		if (!rWreck.bSinks)
		{
			float fTiltPercent = 1.0f;
			if (rWreck.fStartZ != rWreck.fRestZ)
			{
				fTiltPercent = std::clamp((rWreck.fStartZ - XMVectorGetZ(rWreck.vecPosition)) / (rWreck.fStartZ - rWreck.fRestZ), 0.0f, 1.0f);
			}
			vecTilt = XMQuaternionSlerp(XMQuaternionIdentity(), rWreck.vecTilt, fTiltPercent);
		}

		int64_t iInstance = iVisibleCount + k;
		WriteModelLayout(rBasis, shadowBounds, matAlignment, rWreck.vecPosition, rWreck.vecDirection, vecTilt, pAnimationData, skinningReservation, iInstance, rWreck.animationTime, pLayouts[iRenderedOffset + iInstance]);
	}

	siRendered[iPipeline] += iVisibleCount + iWreckCount;
	siWrecksRendered += iWreckCount;
}

void BattleshipsInterpolate::Render(const FrameInterpolate& __restrict rFrameInterpolate, int64_t iCommandBuffer)
{
	engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerRenderBattleships);

	const BattleshipsInterpolate& rCurrent = *rFrameInterpolate.pBattleships;
	const engine::RenderBasis& rBasis = rFrameInterpolate.renderBasis;

	bool bHasWrecks = std::ranges::any_of(sWrecks, [&](const BattleshipWreck& rWreck)
	{
		return rWreck.coordinate == rBasis.coordinate;
	});
	if (rCurrent.iCount == 0 && !bHasWrecks)
	{
		return;
	}

	BattleshipsRenderActiveGuard renderActiveGuard;

	// The interpolated rows align with the snapshot they were interpolated from, which holds each row's controller.
	const BattleshipsPostRender& rPostRender = *gpGame->RenderFrame(rBasis.coordinate).postRender.pBattleships;

	auto visibleIndicesAllocation = common::gpThreadLocal->mWorkbuffer.PushBuffer<int64_t*>((rCurrent.iCount + std::ssize(sWrecks)) * static_cast<int64_t>(sizeof(int64_t)));

	for (int64_t i = 0; i < kiPipelineCount; ++i)
	{
		RenderPipeline(rCurrent, rPostRender, rBasis, i, visibleIndicesAllocation.mpData, iCommandBuffer);
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
