#if defined(BT_CLIENT)

#include "Omnis.h"

#include "Data/Scene.h"
#include "Frame/FlightPlanes.h"
#include "Graphics/Debug/DebugRender.h"

#include "Frame/Collections/Spaceships/Spaceships.h"
#include "Profile/ProfileRows.h"
#include "Game.h"

namespace game
{

// Omni model (also used by Omnis.cpp for animation lookup in OmnisInterpolate::Update)
#if 1
extern const common::crc_t kOmniModel = data::kModelsspaceship2scenegltfCrc;
constexpr float kfModelScale = 0.3667f;
#endif

#if 0
extern const common::crc_t kOmniModel = data::kModelsblack_dragon_with_idle_animationscenegltfCrc;
constexpr float kfModelScale = 2.0f;
#endif
#if 0
extern const common::crc_t kOmniModel = data::kModelschernovan_nemesisscenegltfCrc;
constexpr float kfModelScale = 2.0f;
#endif
#if 0
extern const common::crc_t kOmniModel = data::kModelsmirascenegltfCrc;
constexpr float kfModelScale = 0.0667f;
#endif
#if 0
extern const common::crc_t kOmniModel = data::kModelsDamagedHelmetDamagedHelmetgltfCrc;
constexpr float kfModelScale = 13.333f;
#endif
#if 0
extern const common::crc_t kOmniModel = data::kModelsSpaceshipscenegltfCrc;
constexpr float kfModelScale = 0.0667f;
#endif

constexpr float kfDeathShrinkPower = 2.0f;

void OmnisInterpolate::GraphicsResources()
{
	engine::Buffer* pStorageBuffers = engine::gpBufferManager->CreateDynamicBuffer(kCrc, engine::kBufferMain, kpcName, sizeof(shaders::ModelLayout));
	engine::gpPipelineManager->mDynamicPipelines.CreateModelPipelines(kCrc, kpcName, kOmniModel, pStorageBuffers);
}

static int64_t siRendered = 0;
// Non-concurrency tripwire: per-frame Render calls must run sequentially across active coords — siRendered
// accumulates across them and offsets each call's slab writes. Parallelizing coord renders would race.
static std::atomic<bool> sbRenderActive = false;

// The guard clears sbRenderActive on scope exit, including exception unwinding after a failed capacity ASSERT.
struct [[nodiscard]] OmnisRenderActiveGuard
{
	OmnisRenderActiveGuard()
	{
		ASSERT(!sbRenderActive.exchange(true));
	}

	OmnisRenderActiveGuard(const OmnisRenderActiveGuard&) = delete;
	OmnisRenderActiveGuard& operator=(const OmnisRenderActiveGuard&) = delete;

	~OmnisRenderActiveGuard()
	{
		sbRenderActive.store(false);
	}
};

// Every position here is local to the rendered cell, so each one converts into the camera cell's frame where it is
// handed to the debug renderer; the distances and directions are computed in the local frame, which is the same.
static void XM_CALLCONV RenderCombatAim(const SpaceshipsInterpolate& __restrict rSpaceships, const SpaceshipsPostRender& __restrict rSpaceshipsPostRender, FXMVECTOR vecPosition, FXMVECTOR vecWantedDirection, const engine::RenderBasis& rBasis)
{
	float fClosestDistanceSquared = std::numeric_limits<float>::max();
	XMVECTOR vecClosestPosition = XMVectorZero();
	int64_t iClosestSpaceship = -1;

	for (int64_t j = 0; j < rSpaceships.iCount; ++j)
	{
		if (rSpaceships.pDestroyedTimes[j] != std::chrono::duration<float>(-1.0f))
		{
			continue;
		}

		float fDistanceSquared = XMVectorGetX(XMVector3LengthSq(XMVectorSubtract(vecPosition, rSpaceships.pVecPositions[j])));
		if (fDistanceSquared < fClosestDistanceSquared)
		{
			fClosestDistanceSquared = fDistanceSquared;
			vecClosestPosition = rSpaceships.pVecPositions[j];
			iClosestSpaceship = j;
		}
	}

	if (iClosestSpaceship >= 0)
	{
		// Yellow reticle at the lead-intercept point: where the omni must aim so blasters land on the moving spaceship
		XMVECTOR vecLead = common::ComputeLeadPosition(vecPosition, vecClosestPosition, rSpaceshipsPostRender.pVecVelocities[iClosestSpaceship], kfOmniBlastersSpeed);
		XMFLOAT3A f3Lead {};
		XMStoreFloat3A(&f3Lead, engine::Rebase(rBasis, vecLead));
		engine::DebugRender::Circle(f3Lead, kfOmniRadius * 0.5f, {1.0f, 1.0f, 0.0f, 1.0f});

		// The red line uses PostRender's wanted-aim direction; the yellow reticle recomputes the nearest alive spaceship's intercept from interpolated positions.
		float fDistanceToLead = common::Distance(vecPosition, vecLead);
		XMVECTOR vecLineEnd = XMVectorAdd(vecPosition, XMVectorScale(vecWantedDirection, fDistanceToLead));
		XMFLOAT3A f3Start {};
		XMFLOAT3A f3End {};
		XMStoreFloat3A(&f3Start, engine::Rebase(rBasis, vecPosition));
		XMStoreFloat3A(&f3End, engine::Rebase(rBasis, vecLineEnd));
		engine::DebugRender::Line(f3Start, f3End, {1.0f, 0.0f, 0.0f, 1.0f});
	}
}

static std::pair<bool, XMVECTOR> FindFlagshipPosition(const OmnisInterpolate& __restrict rOmnis, const OmnisPostRender& __restrict rPostRender, int64_t i, int64_t iCount)
{
	for (int64_t j = 0; j < iCount; ++j)
	{
		if (j == i)
		{
			continue;
		}
		if (!(rPostRender.pFlags[j] & OmniFlags::kIsFlagship))
		{
			continue;
		}
		return {true, rOmnis.pVecPositions[j]};
	}

	return {false, XMVectorZero()};
}

static void XM_CALLCONV RenderNavigation(const OmnisInterpolate& __restrict rOmnis, const OmnisPostRender& __restrict rPostRender, int64_t i, int64_t iCount, FXMVECTOR vecPosition, const engine::RenderBasis& rBasis)
{
	int64_t iNavigationDirection = GetNavigationDirection(rPostRender.pFlags[i]);
	bool bFlagshipFound = false;
	XMVECTOR vecFlagshipPosition = XMVectorZero();
	if (iNavigationDirection == 5)
	{
		auto [bFound, vecFoundPosition] = FindFlagshipPosition(rOmnis, rPostRender, i, iCount);
		bFlagshipFound = bFound;
		vecFlagshipPosition = vecFoundPosition;
	}

	if (XMVectorGetW(rPostRender.pVecDebugNavigationWaypoints[i]) > 0.0f && (iNavigationDirection != 5 || bFlagshipFound))
	{
		// In mode 5 (flagship follow), use the flagship's interpolated position as the waypoint
		XMVECTOR vecWaypoint = (iNavigationDirection == 5) ? vecFlagshipPosition : rPostRender.pVecDebugNavigationWaypoints[i];
		XMFLOAT3A f3NavigationStart {};
		XMFLOAT3A f3NavigationEnd {};
		XMStoreFloat3A(&f3NavigationStart, engine::Rebase(rBasis, vecPosition));
		XMStoreFloat3A(&f3NavigationEnd, engine::Rebase(rBasis, XMVectorSetZ(vecWaypoint, engine::gpFlightPlanes->Lowest().fHeightMeters)));
		engine::DebugRender::Line(f3NavigationStart, f3NavigationEnd, {0.0f, 1.0f, 0.0f, 1.0f});
		engine::DebugRender::Circle(f3NavigationEnd, kfOmniRadius * 0.5f, {0.0f, 1.0f, 0.0f, 1.0f});
	}

	if (iNavigationDirection == 5)
	{
		if (bFlagshipFound)
		{
			XMFLOAT3A f3Destination {};
			XMStoreFloat3A(&f3Destination, engine::Rebase(rBasis, XMVectorSetZ(vecFlagshipPosition, engine::gpFlightPlanes->Lowest().fHeightMeters)));
			engine::DebugRender::Circle(f3Destination, kfOmniRadius * 2.0f, {0.0f, 1.0f, 0.0f, 1.0f});
		}
	}
	else if (XMVectorGetW(rPostRender.pVecIslandDestinations[i]) > 0.0f)
	{
		XMFLOAT3A f3Destination {};
		XMStoreFloat3A(&f3Destination, engine::Rebase(rBasis, XMVectorSetZ(rPostRender.pVecIslandDestinations[i], engine::gpFlightPlanes->Lowest().fHeightMeters)));
		engine::DebugRender::Circle(f3Destination, kfOmniRadius * 2.0f, {0.0f, 1.0f, 0.0f, 1.0f});
	}
}

void OmnisInterpolate::BeginRender([[maybe_unused]] int64_t iCommandBuffer, const std::unordered_map<engine::GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<engine::GridCoord>& rActiveCoordinates)
{
	siRendered = 0;

	int64_t iTotalCapacity = engine::AccumulateRenderCapacity(rRenderInterpolates, rActiveCoordinates, [](const game::FrameInterpolate& rInterpolate) -> const OmnisInterpolate&
	{
		return *rInterpolate.pOmnis;
	});

	if (iTotalCapacity == 0)
	{
		return;
	}

	if (engine::Buffer* pBuffer = engine::gpBufferManager->ResizeDynamicBufferIfNeeded(kCrc, engine::kBufferMain, kpcName, sizeof(shaders::ModelLayout), iTotalCapacity, iCommandBuffer); pBuffer != nullptr)
	{
		engine::gpPipelineManager->mDynamicPipelines.UpdateModelPipelineDescriptors(kCrc, iCommandBuffer, 2, pBuffer);
	}
}

void OmnisInterpolate::Render(const FrameInterpolate& __restrict rFrameInterpolate, int64_t iCommandBuffer)
{
	engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerRenderOmnis);

	const OmnisInterpolate& rCurrent = *rFrameInterpolate.pOmnis;
	const engine::RenderBasis& rBasis = rFrameInterpolate.renderBasis;

	int64_t iCount = rCurrent.iCount;

	if (iCount == 0)
	{
		return;
	}

	OmnisRenderActiveGuard renderActiveGuard;

	auto [pOmniLayouts, iBufferCapacity] = engine::gpBufferManager->GetDynamicStorageBuffer<shaders::ModelLayout>(kCrc, engine::kBufferMain, iCommandBuffer);
	ASSERT(siRendered + rCurrent.iCount <= iBufferCapacity);

	const engine::AnimationData* pAnimationData = nullptr;
	engine::SkinningReservation skinningReservation {};
	if (engine::gAnimationDataMap.contains(kOmniModel))
	{
		pAnimationData = &engine::gAnimationDataMap.at(kOmniModel);
		skinningReservation = pAnimationData->ReserveSkinning(iCommandBuffer, iCount);
	}

	for (int64_t i = 0; i < iCount; ++i)
	{
		float fSize = kfOmniRadius * kfModelScale;
		if (rCurrent.pDestroyedTimes[i] > std::chrono::duration<float>::zero())
		{
			fSize *= std::pow(rCurrent.pDestroyedTimes[i] / kDestroyTime, kfDeathShrinkPower);
		}

		// The position is local to the rendered cell; one conversion into the camera cell's frame feeds both the
		// model translation and the layout position.
		XMVECTOR vecPosition = engine::Rebase(rBasis, rCurrent.pVecPositions[i]);

		auto matScaling = XMMatrixScaling(fSize, fSize, fSize);
		auto matTranslation = XMMatrixTranslationFromVector(vecPosition);
		auto matRotationX = XMMatrixRotationX(XM_PIDIV2);
		auto matRotationY = XMMatrixRotationY(0.0f);
		auto matRotationZ = common::RotationMatrixFromDirection(rCurrent.pVecDirections[i], XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f));
		auto matRotationAccelerationX = XMMatrixRotationY(rCurrent.pfRotationAccelerationXs[i]);
		auto matRotationAccelerationY = XMMatrixRotationX(rCurrent.pfRotationAccelerationYs[i]);
		auto matTransform = XMMatrixMultiply(matRotationX, XMMatrixMultiply(matRotationY, XMMatrixMultiply(matRotationZ, XMMatrixMultiply(matRotationAccelerationX, XMMatrixMultiply(matRotationAccelerationY, XMMatrixMultiply(matScaling, matTranslation))))));

		shaders::ModelLayout& rOmniLayout = pOmniLayouts[siRendered];
		XMStoreFloat4(&rOmniLayout.f4Position, vecPosition);

		XMStoreFloat3x4(reinterpret_cast<XMFLOAT3X4*>(&rOmniLayout.f3x4Transform[0]), matTransform);
		XMStoreFloat3x4(reinterpret_cast<XMFLOAT3X4*>(&rOmniLayout.f3x4TransformNormal[0]), XMMatrixTranspose(XMMatrixInverse(nullptr, matTransform)));
		rOmniLayout.f4ColorAdd = {0.0f, 0.0f, 0.0f, 0.0f};
		rOmniLayout.uiMeshDataBase = 0;

		if (pAnimationData != nullptr)
		{
			rOmniLayout.uiMeshDataBase = static_cast<uint32_t>(pAnimationData->EvaluateInstance(skinningReservation, i, rCurrent.pAnimationTimes[i].count()));
		}

		++siRendered;
	}
}

void OmnisInterpolate::EndRender([[maybe_unused]] int64_t iCommandBuffer)
{
	engine::gpPipelineManager->mDynamicPipelines.WriteModelPipelineIndirectBuffers(kCrc, iCommandBuffer, siRendered);
}

void OmnisInterpolate::DebugRender(const FrameInterpolate& __restrict rFrameInterpolate, engine::GridCoord coordinate)
{
	if constexpr (!kbDebugRender)
	{
		return;
	}

	// Entity positions MUST be read from rFrameInterpolate (the fully-interpolated frame),
	// never from PostRender. PostRender positions lag behind the rendered frame.
	// Only flags, metadata, and static world positions (nav waypoints, island destinations) come from PostRender.
	const OmnisInterpolate& rOmnis = *rFrameInterpolate.pOmnis;
	const SpaceshipsInterpolate& rSpaceships = *rFrameInterpolate.pSpaceships;
	const OmnisPostRender& rPostRender = *gpGame->RenderFrame(coordinate).postRender.pOmnis;
	const SpaceshipsPostRender& rSpaceshipsPostRender = *gpGame->RenderFrame(coordinate).postRender.pSpaceships;

	int64_t iCount = rOmnis.iCount;

	for (int64_t i = 0; i < iCount; ++i)
	{
		XMVECTOR vecPosition = rOmnis.pVecPositions[i];
		RenderCombatAim(rSpaceships, rSpaceshipsPostRender, vecPosition, rPostRender.pVecWantedDirections[i], rFrameInterpolate.renderBasis);
		RenderNavigation(rOmnis, rPostRender, i, iCount, vecPosition, rFrameInterpolate.renderBasis);
	}
}

} // namespace game

#endif // BT_CLIENT
