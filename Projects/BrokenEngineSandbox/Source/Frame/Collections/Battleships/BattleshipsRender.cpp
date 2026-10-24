#if defined(BT_CLIENT)

#include "Battleships.h"

#include "Data/Scene.h"

#include "Profile/ProfileRows.h"
#include "Game.h"

namespace game
{

// One model per ShipController, also used by Battleships.cpp for animation lookup. A scale of 2 / L, where L is the
// model's longest horizontal extent in exported model space, makes the hull span the collision diameter. The
// pre-rotation stands the Y-up model upright with its nose along +X.
extern const common::crc_t kBattleshipFleetModel = data::kModelsBattleshipJesterz86scenegltfCrc;
constexpr float kfBattleshipFleetModelScale = 1.000f; // L = 2.000
constexpr XMFLOAT3 kf3BattleshipFleetModelOffset {0.0f, 0.0f, 0.0f};
const XMMATRIX kmatBattleshipFleetModelPreRotate = XMMatrixRotationX(XM_PIDIV2) * XMMatrixRotationY(0.0f) * XMMatrixRotationZ(XM_PIDIV2);
extern const common::crc_t kBattleshipRandomModel = data::kModelsSpaceBattleshipAquamarinescenegltfCrc;
// L = 0.9691 is the hull alone: the model's transparent AO_shadow_catcher quad is larger and sets its exported bounds.
constexpr float kfBattleshipRandomModelScale = 2.064f;
// The exported origin is the center of those bounds, which puts the hull center 0.0994 ahead of it along model Z (the
// nose axis); the offset moves it back onto the ship position.
constexpr XMFLOAT3 kf3BattleshipRandomModelOffset {0.0f, 0.0f, -0.0994f};
const XMMATRIX kmatBattleshipRandomModelPreRotate = XMMatrixRotationX(XM_PIDIV2) * XMMatrixRotationY(0.0f) * XMMatrixRotationZ(XM_PIDIV2);

constexpr float kfRoll = 0.2f;

// One model pipeline per ShipController, indexed by it.
constexpr int64_t kiPipelineCount = std::to_underlying(ShipController::kCount);
constexpr std::array<const char*, kiPipelineCount> kpcPipelineNames {"BattleshipsFleet", "BattleshipsRandom"};
constexpr std::array<common::crc_t, kiPipelineCount> kuiPipelineCrcs {common::CrcConsteval("BattleshipsFleet"), common::CrcConsteval("BattleshipsRandom")};
constexpr std::array<float, kiPipelineCount> kfModelScales {kfBattleshipFleetModelScale, kfBattleshipRandomModelScale};
constexpr std::array<XMFLOAT3, kiPipelineCount> kf3ModelOffsets {kf3BattleshipFleetModelOffset, kf3BattleshipRandomModelOffset};
const std::array<XMMATRIX, kiPipelineCount> kmatModelPreRotates {kmatBattleshipFleetModelPreRotate, kmatBattleshipRandomModelPreRotate};
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

void BattleshipsInterpolate::BeginRender([[maybe_unused]] int64_t iCommandBuffer, const std::unordered_map<engine::GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<engine::GridCoord>& rActiveCoordinates)
{
	siRendered = {};

	int64_t iTotalCapacity = engine::AccumulateRenderCapacity(rRenderInterpolates, rActiveCoordinates, [](const game::FrameInterpolate& rInterpolate) -> const BattleshipsInterpolate&
	{
		return *rInterpolate.pBattleships;
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

// Draws the rows whose controller selects iPipeline into that pipeline's buffer. piVisibleIndices is scratch for every row.
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
		if (!engine::gpCamera->InVisibleArea(engine::gpCamera->mf4RenderVisibleArea, f4Position))
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

	int64_t iRenderedOffset = siRendered[iPipeline];
	float fModelScale = kfModelScales[iPipeline];
	XMMATRIX matPreMove = XMMatrixTranslationFromVector(XMLoadFloat3(&kf3ModelOffsets[iPipeline]));
	XMMATRIX matPreRotate = kmatModelPreRotates[iPipeline];

	// Per-range processing lambda — each visible index j writes to deterministic non-overlapping output slots
	auto ProcessRange = [&](int64_t iStart, int64_t iEnd)
	{
		for (int64_t j = iStart; j < iEnd; ++j)
		{
			int64_t i = piVisibleIndices[j];

			float fSize = kfBattleshipRadius * fModelScale;
			if (rCurrent.pDestroyedTimes[i] > std::chrono::duration<float>::zero())
			{
				fSize *= std::pow(rCurrent.pDestroyedTimes[i] / kBattleshipDestroyTime, 0.75f);
			}

			XMVECTOR vecPosition = engine::Rebase(rBasis, rCurrent.pVecPositions[i]);
			XMFLOAT4A f4Position {};
			XMStoreFloat4A(&f4Position, vecPosition);

			XMMATRIX matScaling = XMMatrixScaling(fSize, fSize, fSize);
			XMMATRIX matRoll = XMMatrixRotationX(-kfRoll * rCurrent.pfDeltaRotations[i]);
			XMMATRIX matYaw = common::RotationMatrixFromDirection(rCurrent.pVecDirections[i], XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f));
			XMMATRIX matTranslation = XMMatrixTranslationFromVector(vecPosition);
			XMMATRIX matTransform = matPreMove * matScaling * matPreRotate * matRoll * matYaw * matTranslation;

			shaders::ModelLayout& rModelLayout = pLayouts[iRenderedOffset + j];
			rModelLayout.f4Position = f4Position;
			XMStoreFloat3x4(reinterpret_cast<XMFLOAT3X4*>(&rModelLayout.f3x4Transform[0]), matTransform);
			XMStoreFloat3x4(reinterpret_cast<XMFLOAT3X4*>(&rModelLayout.f3x4TransformNormal[0]), XMMatrixTranspose(XMMatrixInverse(nullptr, matTransform)));

			rModelLayout.f4ColorAdd = {0.0f, 0.0f, 0.0f, 0.0f};
			rModelLayout.uiMeshDataBase = 0;

			if (pAnimationData != nullptr)
			{
				rModelLayout.uiMeshDataBase = static_cast<uint32_t>(pAnimationData->EvaluateInstance(skinningReservation, j, rCurrent.pAnimationTimes[i].count()));
			}
		}
	};

	gpProfileManager->GetCpuTimer(game::kCpuTimerRenderBattleships).iThreads = common::gpMultithreading->WorkerCount() + 1;
	common::gpMultithreading->Dispatch(iVisibleCount, ProcessRange);

	siRendered[iPipeline] += iVisibleCount;
}

void BattleshipsInterpolate::Render(const FrameInterpolate& __restrict rFrameInterpolate, int64_t iCommandBuffer)
{
	engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerRenderBattleships);

	const BattleshipsInterpolate& rCurrent = *rFrameInterpolate.pBattleships;
	const engine::RenderBasis& rBasis = rFrameInterpolate.renderBasis;

	if (rCurrent.iCount == 0)
	{
		return;
	}

	BattleshipsRenderActiveGuard renderActiveGuard;

	// The interpolated rows align with the snapshot they were interpolated from, which holds each row's controller.
	const BattleshipsPostRender& rPostRender = *gpGame->RenderFrame(rBasis.coordinate).postRender.pBattleships;

	auto visibleIndicesAllocation = common::gpThreadLocal->mWorkbuffer.PushBuffer<int64_t*>(rCurrent.iCount * static_cast<int64_t>(sizeof(int64_t)));

	for (int64_t i = 0; i < kiPipelineCount; ++i)
	{
		RenderPipeline(rCurrent, rPostRender, rBasis, i, visibleIndicesAllocation.mpData, iCommandBuffer);
	}
}

void BattleshipsInterpolate::EndRender([[maybe_unused]] int64_t iCommandBuffer)
{
	if constexpr (kbProfiling)
	{
		gpProfileManager->GetCpuCounter(game::kCpuCounterBattleshipsRendered).iCount = siRendered[0] + siRendered[1];
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
		if (rPostRender.pController[i] == ShipController::kFleet)
		{
			DebugRenderNavigation(rFrameInterpolate, rRenderFrame, rBattleships.pVecPositions[i], rPostRender.pVecDebugNavigationWaypoints[i], rPostRender.pVecIslandDestinations[i], rPostRender.pFlags[i], rPostRender.pFlagshipGlobalIds[i], kfBattleshipRadius);
		}
	}
}

} // namespace game

#endif // BT_CLIENT
