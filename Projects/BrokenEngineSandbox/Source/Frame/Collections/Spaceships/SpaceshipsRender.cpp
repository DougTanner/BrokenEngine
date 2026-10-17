#if defined(BT_CLIENT)

#include "Spaceships.h"

#include "Data/Scene.h"

#include "Profile/ProfileRows.h"

namespace game
{

// Spaceship model (also used by Spaceships.cpp for animation lookup)
#if 1
extern const common::crc_t kuiSpaceshipModel = data::kModelsSpaceshipscenegltfCrc;
constexpr float kfModelScale = 0.00175f;
#endif
#if 0
extern const common::crc_t kuiSpaceshipModel = data::kModelschernovan_nemesisscenegltfCrc;
constexpr float kfModelScale = 0.15f;
#endif

constexpr float kfRoll = 0.2f;

void SpaceshipsInterpolate::GraphicsResources()
{
	engine::Buffer* pStorageBuffers = engine::gpBufferManager->CreateDynamicBuffer(kuiCrc, engine::kBufferMain, kpcName, sizeof(shaders::ModelLayout));
	engine::gpPipelineManager->mDynamicPipelines.CreateModelPipelines(kuiCrc, kpcName, kuiSpaceshipModel, pStorageBuffers);
}

static int64_t siRendered = 0;
// Non-concurrency tripwire: per-frame Render calls must run sequentially across active coords — siRendered
// accumulates across them and offsets each call's slab writes. Parallelizing coord renders would race.
static std::atomic<bool> sbRenderActive = false;

// sbRenderActive must clear during exception unwinding so subsequent Render calls do not fail the concurrency ASSERT.
struct [[nodiscard]] SpaceshipsRenderActiveGuard
{
	SpaceshipsRenderActiveGuard()
	{
		ASSERT(!sbRenderActive.exchange(true));
	}

	SpaceshipsRenderActiveGuard(const SpaceshipsRenderActiveGuard&) = delete;
	SpaceshipsRenderActiveGuard& operator=(const SpaceshipsRenderActiveGuard&) = delete;

	~SpaceshipsRenderActiveGuard()
	{
		sbRenderActive.store(false);
	}
};

void SpaceshipsInterpolate::BeginRender([[maybe_unused]] int64_t iCommandBuffer, const std::unordered_map<engine::GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<engine::GridCoord>& rActiveCoordinates)
{
	siRendered = 0;

	int64_t iTotalCapacity = engine::AccumulateRenderCapacity(rRenderInterpolates, rActiveCoordinates, [](const game::FrameInterpolate& rInterpolate) -> const SpaceshipsInterpolate&
	{
		return *rInterpolate.pSpaceships;
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

void SpaceshipsInterpolate::Render(const FrameInterpolate& __restrict rFrameInterpolate, int64_t iCommandBuffer)
{
	engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerRenderSpaceships);

	const SpaceshipsInterpolate& rCurrent = *rFrameInterpolate.pSpaceships;
	const engine::RenderBasis& rBasis = rFrameInterpolate.renderBasis;

	if (rCurrent.iCount == 0)
	{
		return;
	}

	SpaceshipsRenderActiveGuard renderActiveGuard;

	static XMMATRIX smatPreRotate = XMMatrixRotationX(XM_PIDIV2) * XMMatrixRotationY(0.0f) * XMMatrixRotationZ(XM_PIDIV2);

	auto [pLayouts, iBufferCapacity] = engine::gpBufferManager->GetDynamicStorageBuffer<shaders::ModelLayout>(kuiCrc, engine::kBufferMain, iCommandBuffer);
	ASSERT(siRendered + rCurrent.iCount <= iBufferCapacity);

	const engine::AnimationData* pAnimationData = nullptr;
	if (engine::gAnimationDataMap.contains(kuiSpaceshipModel))
	{
		pAnimationData = &engine::gAnimationDataMap.at(kuiSpaceshipModel);
	}

	// Visibility culling runs on the main thread before worker dispatch.
	auto visibleIndicesAllocation = common::gpThreadLocal->mWorkbuffer.PushBuffer<int64_t*>(rCurrent.iCount * static_cast<int64_t>(sizeof(int64_t)));
	int64_t iVisibleCount = 0;

	for (int64_t i = 0; i < rCurrent.iCount; ++i)
	{
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

		visibleIndicesAllocation.mpData[iVisibleCount++] = i;
	}

	// Skinning output for all visible spaceships is allocated on the main thread before worker dispatch.
	engine::SkinningReservation skinningReservation {};
	if (pAnimationData != nullptr)
	{
		skinningReservation = pAnimationData->ReserveSkinning(iCommandBuffer, iVisibleCount);
	}

	int64_t iRenderedOffset = siRendered;

	// Per-range processing lambda — each visible index j writes to deterministic non-overlapping output slots
	auto ProcessRange = [&](int64_t iStart, int64_t iEnd)
	{
		for (int64_t j = iStart; j < iEnd; ++j)
		{
			int64_t i = visibleIndicesAllocation.mpData[j];

			float fSize = kfSpaceshipRadius * kfModelScale;
			if (rCurrent.pDestroyedTimes[i] > std::chrono::duration<float>::zero())
			{
				fSize *= std::pow(rCurrent.pDestroyedTimes[i] / kSpaceshipDestroyTime, 0.75f);
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

	gpProfileManager->GetCpuTimer(game::kCpuTimerRenderSpaceships).iThreads = common::gpMultithreading->WorkerCount() + 1;
	common::gpMultithreading->Dispatch(iVisibleCount, ProcessRange);

	siRendered += iVisibleCount;
}

void SpaceshipsInterpolate::EndRender([[maybe_unused]] int64_t iCommandBuffer)
{
	if constexpr (kbProfiling)
	{
		gpProfileManager->GetCpuCounter(game::kCpuCounterSpaceshipsRendered).iCount = siRendered;
	}
	engine::gpPipelineManager->mDynamicPipelines.WriteModelPipelineIndirectBuffers(kuiCrc, iCommandBuffer, siRendered);
}

} // namespace game

#endif // BT_CLIENT
