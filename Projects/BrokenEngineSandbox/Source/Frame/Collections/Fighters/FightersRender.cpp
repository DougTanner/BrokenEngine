#if defined(BT_CLIENT)

#include "Fighters.h"

#include "Data/Scene.h"

#include "Profile/ProfileRows.h"
#include "Game.h"

namespace game
{

// Placeholder models, one per ShipController (also used by Fighters.cpp for animation lookup).
extern const common::crc_t kFighterFleetModel = data::kModelsspaceship2scenegltfCrc;
constexpr float kfFighterFleetModelScale = 0.3667f;
extern const common::crc_t kFighterRandomModel = data::kModelsSpaceshipscenegltfCrc;
constexpr float kfFighterRandomModelScale = 0.00175f;

constexpr float kfRoll = 0.2f;

// One model pipeline per ShipController, indexed by it.
constexpr int64_t kiPipelineCount = std::to_underlying(ShipController::kCount);
constexpr std::array<const char*, kiPipelineCount> kpcPipelineNames {"FightersFleet", "FightersRandom"};
constexpr std::array<common::crc_t, kiPipelineCount> kuiPipelineCrcs {common::CrcConsteval("FightersFleet"), common::CrcConsteval("FightersRandom")};
constexpr std::array<float, kiPipelineCount> kfModelScales {kfFighterFleetModelScale, kfFighterRandomModelScale};
const std::array<common::crc_t, kiPipelineCount> kuiModels {kFighterFleetModel, kFighterRandomModel};

void FightersInterpolate::GraphicsResources()
{
	for (int64_t i = 0; i < kiPipelineCount; ++i)
	{
		engine::Buffer* pStorageBuffers = engine::gpBufferManager->CreateDynamicBuffer(kuiPipelineCrcs[i], engine::kBufferMain, kpcPipelineNames[i], sizeof(shaders::ModelLayout));
		engine::gpPipelineManager->mDynamicPipelines.CreateModelPipelines(kuiPipelineCrcs[i], kpcPipelineNames[i], kuiModels[i], pStorageBuffers);
	}
}

static std::array<int64_t, kiPipelineCount> siRendered {};
// Non-concurrency tripwire: per-frame Render calls must run sequentially across active coords — siRendered
// accumulates across them and offsets each call's slab writes. Parallelizing coord renders would race.
static std::atomic<bool> sbRenderActive = false;

// sbRenderActive must clear during exception unwinding so subsequent Render calls do not fail the concurrency ASSERT.
struct [[nodiscard]] FightersRenderActiveGuard
{
	FightersRenderActiveGuard()
	{
		ASSERT(!sbRenderActive.exchange(true));
	}

	FightersRenderActiveGuard(const FightersRenderActiveGuard&) = delete;
	FightersRenderActiveGuard& operator=(const FightersRenderActiveGuard&) = delete;

	~FightersRenderActiveGuard()
	{
		sbRenderActive.store(false);
	}
};

void FightersInterpolate::BeginRender([[maybe_unused]] int64_t iCommandBuffer, const std::unordered_map<engine::GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<engine::GridCoord>& rActiveCoordinates)
{
	siRendered = {};

	int64_t iTotalCapacity = engine::AccumulateRenderCapacity(rRenderInterpolates, rActiveCoordinates, [](const game::FrameInterpolate& rInterpolate) -> const FightersInterpolate&
	{
		return *rInterpolate.pFighters;
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
static void RenderPipeline(const FightersInterpolate& __restrict rCurrent, const FightersPostRender& __restrict rPostRender, const engine::RenderBasis& rBasis, int64_t iPipeline, int64_t* piVisibleIndices, int64_t iCommandBuffer)
{
	static XMMATRIX smatPreRotate = XMMatrixRotationX(XM_PIDIV2) * XMMatrixRotationY(0.0f) * XMMatrixRotationZ(XM_PIDIV2);

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

	// Per-range processing lambda — each visible index j writes to deterministic non-overlapping output slots
	auto ProcessRange = [&](int64_t iStart, int64_t iEnd)
	{
		for (int64_t j = iStart; j < iEnd; ++j)
		{
			int64_t i = piVisibleIndices[j];

			float fSize = kfFighterRadius * fModelScale;
			if (rCurrent.pDestroyedTimes[i] > std::chrono::duration<float>::zero())
			{
				fSize *= std::pow(rCurrent.pDestroyedTimes[i] / kFighterDestroyTime, 0.75f);
			}

			XMVECTOR vecPosition = engine::Rebase(rBasis, rCurrent.pVecPositions[i]);
			XMFLOAT4A f4Position {};
			XMStoreFloat4A(&f4Position, vecPosition);

			XMMATRIX matScaling = XMMatrixScaling(fSize, fSize, fSize);
			XMMATRIX matRoll = XMMatrixRotationX(-kfRoll * rCurrent.pfDeltaRotations[i]);
			XMMATRIX matYaw = common::RotationMatrixFromDirection(rCurrent.pVecDirections[i], XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f));
			XMMATRIX matTranslation = XMMatrixTranslationFromVector(vecPosition);
			XMMATRIX matTransform = matScaling * smatPreRotate * matRoll * matYaw * matTranslation;

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

	gpProfileManager->GetCpuTimer(game::kCpuTimerRenderFighters).iThreads = common::gpMultithreading->WorkerCount() + 1;
	common::gpMultithreading->Dispatch(iVisibleCount, ProcessRange);

	siRendered[iPipeline] += iVisibleCount;
}

void FightersInterpolate::Render(const FrameInterpolate& __restrict rFrameInterpolate, int64_t iCommandBuffer)
{
	engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerRenderFighters);

	const FightersInterpolate& rCurrent = *rFrameInterpolate.pFighters;
	const engine::RenderBasis& rBasis = rFrameInterpolate.renderBasis;

	if (rCurrent.iCount == 0)
	{
		return;
	}

	FightersRenderActiveGuard renderActiveGuard;

	// The interpolated rows align with the snapshot they were interpolated from, which holds each row's controller.
	const FightersPostRender& rPostRender = *gpGame->RenderFrame(rBasis.coordinate).postRender.pFighters;

	auto visibleIndicesAllocation = common::gpThreadLocal->mWorkbuffer.PushBuffer<int64_t*>(rCurrent.iCount * static_cast<int64_t>(sizeof(int64_t)));

	for (int64_t i = 0; i < kiPipelineCount; ++i)
	{
		RenderPipeline(rCurrent, rPostRender, rBasis, i, visibleIndicesAllocation.mpData, iCommandBuffer);
	}
}

void FightersInterpolate::EndRender([[maybe_unused]] int64_t iCommandBuffer)
{
	if constexpr (kbProfiling)
	{
		gpProfileManager->GetCpuCounter(game::kCpuCounterFightersRendered).iCount = siRendered[0] + siRendered[1];
	}
	for (int64_t i = 0; i < kiPipelineCount; ++i)
	{
		engine::gpPipelineManager->mDynamicPipelines.WriteModelPipelineIndirectBuffers(kuiPipelineCrcs[i], iCommandBuffer, siRendered[i]);
	}
}

void FightersInterpolate::DebugRender(const FrameInterpolate& __restrict rFrameInterpolate, engine::GridCoord coordinate)
{
	if constexpr (!kbDebugRender)
	{
		return;
	}

	// Positions come from rFrameInterpolate (the fully-interpolated frame); PostRender supplies only flags, metadata, and
	// static world positions.
	const FightersInterpolate& rFighters = *rFrameInterpolate.pFighters;
	const Frame& rRenderFrame = gpGame->RenderFrame(coordinate);
	const FightersPostRender& rPostRender = *rRenderFrame.postRender.pFighters;

	for (int64_t i = 0; i < rFighters.iCount; ++i)
	{
		if (rPostRender.pController[i] == ShipController::kFleet)
		{
			DebugRenderNavigation(rFrameInterpolate, rRenderFrame, rFighters.pVecPositions[i], rPostRender.pVecDebugNavigationWaypoints[i], rPostRender.pVecIslandDestinations[i], rPostRender.pFlags[i], rPostRender.pFlagshipGlobalIds[i], kfFighterRadius);
		}
	}
}

} // namespace game

#endif // BT_CLIENT
