#if defined(BT_CLIENT)

#include "Players.h"

#include "Data/Scene.h"
#include "File/PackChunks.h"
#include "Graphics/Debug/DebugRender.h"
#include "Ui/WrapperBase.h"

#include "Frame/Collections/Spaceships/Spaceships.h"
#include "Profile/ProfileManager.h"
#include "Game.h"

namespace game
{

// Player model (also used by Players.cpp for animation lookup in PlayersInterpolate::Update)
#if 1
extern const common::crc_t kPlayerModel = data::kModelsspaceship2scenegltfCrc;
constexpr float kfModelScale = 0.3667f;
#endif

#if 0
extern const common::crc_t kPlayerModel = data::kModelsblack_dragon_with_idle_animationscenegltfCrc;
constexpr float kfModelScale = 2.0f;
#endif
#if 0
extern const common::crc_t kPlayerModel = data::kModelschernovan_nemesisscenegltfCrc;
constexpr float kfModelScale = 2.0f;
#endif
#if 0
extern const common::crc_t kPlayerModel = data::kModelsmirascenegltfCrc;
constexpr float kfModelScale = 0.0667f;
#endif
#if 0
extern const common::crc_t kPlayerModel = data::kModelsDamagedHelmetDamagedHelmetgltfCrc;
constexpr float kfModelScale = 13.333f;
#endif
#if 0
extern const common::crc_t kPlayerModel = data::kModelsSpaceshipscenegltfCrc;
constexpr float kfModelScale = 0.0667f;
#endif

constexpr float kfDeathShrinkPower = 2.0f;

void PlayersInterpolate::GraphicsResources()
{
	engine::Buffer* pStorageBuffers = engine::gpBufferManager->CreateDynamicBuffer(kCrc, engine::kBufferMain, kName, sizeof(shaders::ModelLayout));
	engine::gpPipelineManager->mDynamicPipelines.CreateModelPipeline(kCrc, kName, kPlayerModel, pStorageBuffers);
	engine::gpPipelineManager->mDynamicPipelines.CreateModelPipelineShadow(kCrc, kName, kPlayerModel, pStorageBuffers);
}

static int64_t siRendered = 0;
// Non-concurrency tripwire: per-frame Render calls must run sequentially across active coords — siRendered
// accumulates across them and offsets each call's slab writes. Parallelizing coord renders would race.
static std::atomic<bool> sbRenderActive = false;

// The guard clears sbRenderActive on scope exit, including exception unwinding after a failed capacity ASSERT.
struct [[nodiscard]] PlayersRenderActiveGuard
{
	PlayersRenderActiveGuard()
	{
		ASSERT(!sbRenderActive.exchange(true));
	}

	PlayersRenderActiveGuard(const PlayersRenderActiveGuard&) = delete;
	PlayersRenderActiveGuard& operator=(const PlayersRenderActiveGuard&) = delete;

	~PlayersRenderActiveGuard()
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
		if (rSpaceships.pfDestroyedTimes[j] != -1.0f)
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
		// Yellow reticle at the lead-intercept point: where the player must aim so blasters land on the moving spaceship
		XMVECTOR vecLead = common::ComputeLeadPosition(vecPosition, vecClosestPosition, rSpaceshipsPostRender.pVecVelocities[iClosestSpaceship], kfPlayerBlastersSpeed);
		XMFLOAT3A f3Lead {};
		XMStoreFloat3A(&f3Lead, engine::Rebase(rBasis, vecLead));
		engine::DebugRender::Circle(f3Lead, kfPlayerRadius * 0.5f, {1.0f, 1.0f, 0.0f, 1.0f});

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

static std::pair<bool, XMVECTOR> FindFlagshipPosition(const PlayersInterpolate& __restrict rPlayers, const PlayersPostRender& __restrict rPostRender, int64_t i, int64_t iCount)
{
	for (int64_t j = 0; j < iCount; ++j)
	{
		if (j == i)
		{
			continue;
		}
		if (!(rPostRender.pFlags[j] & PlayerFlags::kIsFlagship))
		{
			continue;
		}
		return {true, rPlayers.pVecPositions[j]};
	}

	return {false, XMVectorZero()};
}

static void XM_CALLCONV RenderNavigation(const PlayersInterpolate& __restrict rPlayers, const PlayersPostRender& __restrict rPostRender, int64_t i, int64_t iCount, FXMVECTOR vecPosition, const engine::RenderBasis& rBasis)
{
	int64_t iNavigationDirection = GetNavigationDirection(rPostRender.pFlags[i]);
	bool bFlagshipFound = false;
	XMVECTOR vecFlagshipPosition = XMVectorZero();
	if (iNavigationDirection == 5)
	{
		auto [bFound, vecFoundPosition] = FindFlagshipPosition(rPlayers, rPostRender, i, iCount);
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
		XMStoreFloat3A(&f3NavigationEnd, engine::Rebase(rBasis, XMVectorSetZ(vecWaypoint, engine::gBaseHeight.mfCurrent)));
		engine::DebugRender::Line(f3NavigationStart, f3NavigationEnd, {0.0f, 1.0f, 0.0f, 1.0f});
		engine::DebugRender::Circle(f3NavigationEnd, kfPlayerRadius * 0.5f, {0.0f, 1.0f, 0.0f, 1.0f});
	}

	if (iNavigationDirection == 5)
	{
		if (bFlagshipFound)
		{
			XMFLOAT3A f3Destination {};
			XMStoreFloat3A(&f3Destination, engine::Rebase(rBasis, XMVectorSetZ(vecFlagshipPosition, engine::gBaseHeight.mfCurrent)));
			engine::DebugRender::Circle(f3Destination, kfPlayerRadius * 2.0f, {0.0f, 1.0f, 0.0f, 1.0f});
		}
	}
	else if (XMVectorGetW(rPostRender.pVecIslandDestinations[i]) > 0.0f)
	{
		XMFLOAT3A f3Destination {};
		XMStoreFloat3A(&f3Destination, engine::Rebase(rBasis, XMVectorSetZ(rPostRender.pVecIslandDestinations[i], engine::gBaseHeight.mfCurrent)));
		engine::DebugRender::Circle(f3Destination, kfPlayerRadius * 2.0f, {0.0f, 1.0f, 0.0f, 1.0f});
	}
}

void PlayersInterpolate::BeginRender([[maybe_unused]] int64_t iCommandBuffer, const std::unordered_map<engine::GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<engine::GridCoord>& rActiveCoordinates)
{
	siRendered = 0;

	int64_t iTotalCount = 0;
	for (const engine::GridCoord& rCoordinate : rActiveCoordinates)
	{
		auto it = rRenderInterpolates.find(rCoordinate);
		if (it != rRenderInterpolates.end())
		{
			// Players uses iCount not iCapacity for buffer sizing since count is always small
			const game::FrameInterpolate& rInterpolate = it->second;
			int64_t iCount = (rInterpolate.gameFlags & GameFlags::kMainMenu) ? 0 : rInterpolate.pPlayers->iCount;
			iTotalCount += iCount;
		}
	}

	if (iTotalCount == 0)
	{
		return;
	}

	int64_t iRequiredSize = iTotalCount * static_cast<int64_t>(sizeof(shaders::ModelLayout));
	engine::Buffer& rBuffer = engine::gpBufferManager->mDynamicStorageBuffers[engine::kBufferMain].at(kCrc).at(iCommandBuffer);
	if (rBuffer.mInfo.iDataSize < iRequiredSize)
	{
		engine::gpBufferManager->ResizeDynamicBuffer(kCrc, engine::kBufferMain, kName, iRequiredSize, iCommandBuffer);
		engine::gpPipelineManager->mDynamicPipelines.mModelPipelineMaps[engine::kDynamicModelPipelineModel].at(kCrc)->UpdateStorageBufferDescriptors(iCommandBuffer, 2, &rBuffer);
		engine::gpPipelineManager->mDynamicPipelines.mModelPipelineMaps[engine::kDynamicModelPipelineModelShadow].at(kCrc)->UpdateStorageBufferDescriptors(iCommandBuffer, 2, &rBuffer);
	}
}

void PlayersInterpolate::Render(const FrameInterpolate& __restrict rFrameInterpolate, int64_t iCommandBuffer)
{
	engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerRenderPlayer);

	const PlayersInterpolate& rCurrent = *rFrameInterpolate.pPlayers;
	const engine::RenderBasis& rBasis = rFrameInterpolate.renderBasis;

	int64_t iCount = (rFrameInterpolate.gameFlags & GameFlags::kMainMenu) ? 0 : rCurrent.iCount;

	if (iCount == 0)
	{
		return;
	}

	PlayersRenderActiveGuard renderActiveGuard;

	auto [pPlayerLayouts, iBufferCapacity] = engine::gpBufferManager->GetDynamicStorageBuffer<shaders::ModelLayout>(kCrc, engine::kBufferMain, iCommandBuffer);
	ASSERT(siRendered + rCurrent.iCount <= iBufferCapacity);

	for (int64_t i = 0; i < iCount; ++i)
	{
		float fSize = kfPlayerRadius * kfModelScale;
		if (rCurrent.pfDestroyedTimes[i] > 0.0f)
		{
			fSize *= std::pow(rCurrent.pfDestroyedTimes[i] / kfDestroyTime, kfDeathShrinkPower);
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

		shaders::ModelLayout& rPlayerLayout = pPlayerLayouts[siRendered];
		XMStoreFloat4(&rPlayerLayout.f4Position, vecPosition);

		XMStoreFloat3x4(reinterpret_cast<XMFLOAT3X4*>(&rPlayerLayout.f3x4Transform[0]), matTransform);
		XMStoreFloat3x4(reinterpret_cast<XMFLOAT3X4*>(&rPlayerLayout.f3x4TransformNormal[0]), XMMatrixTranspose(XMMatrixInverse(nullptr, matTransform)));
		rPlayerLayout.f4ColorAdd = {0.0f, 0.0f, 0.0f, 0.0f};
		rPlayerLayout.uiMeshDataBase = 0;

		if (engine::gAnimationDataMap.contains(kPlayerModel))
		{
			const engine::AnimationData& rAnimationData = engine::gAnimationDataMap.at(kPlayerModel);
			const engine::EagerChunk& rChunk = engine::gpFileManager->mpPackChunks->GetEagerChunkMap().at(kPlayerModel);
			int64_t iMaterialCount = rChunk.pHeader->sceneHeader.uiMaterialCount;

			int64_t iMeshDataBase = engine::gpBufferManager->AllocateMeshData(iCommandBuffer, iMaterialCount);
			rPlayerLayout.uiMeshDataBase = static_cast<uint32_t>(iMeshDataBase);

			common::MeshData* pMeshData = reinterpret_cast<common::MeshData*>(engine::gpBufferManager->mMeshDataStorageBuffers.at(iCommandBuffer).mpMappedMemory) + iMeshDataBase;

			int64_t iSkinnedMaterialCount = rAnimationData.SkinnedMaterialCount(iMaterialCount);

			int64_t iJointMatrixOffset = engine::gpBufferManager->AllocateJointMatrices(iCommandBuffer, iSkinnedMaterialCount * rAnimationData.mHeader.skeleton.uiSkinJointCount);

			common::JointMatrix* pJointMatrices = reinterpret_cast<common::JointMatrix*>(engine::gpBufferManager->mJointMatrixStorageBuffers.at(iCommandBuffer).mpMappedMemory);

			rAnimationData.EvaluateAnimation(0, rCurrent.pfAnimationTimes[i], std::span(pMeshData, static_cast<size_t>(iMaterialCount)), pJointMatrices, iJointMatrixOffset);
		}

		++siRendered;
	}
}

void PlayersInterpolate::EndRender([[maybe_unused]] int64_t iCommandBuffer)
{
	engine::gpPipelineManager->mDynamicPipelines.mModelPipelineMaps[engine::kDynamicModelPipelineModel].at(kCrc)->WriteIndirectBuffer(iCommandBuffer, siRendered);
	engine::gpPipelineManager->mDynamicPipelines.mModelPipelineMaps[engine::kDynamicModelPipelineModelShadow].at(kCrc)->WriteIndirectBuffer(iCommandBuffer, siRendered);
}

void PlayersInterpolate::DebugRender(const FrameInterpolate& __restrict rFrameInterpolate, engine::GridCoord coordinate)
{
	if constexpr (!kbDebugRender)
	{
		return;
	}

	// Entity positions MUST be read from rFrameInterpolate (the fully-interpolated frame),
	// never from PostRender. PostRender positions lag behind the rendered frame.
	// Only flags, metadata, and static world positions (nav waypoints, island destinations) come from PostRender.
	const PlayersInterpolate& rPlayers = *rFrameInterpolate.pPlayers;
	const SpaceshipsInterpolate& rSpaceships = *rFrameInterpolate.pSpaceships;
	const PlayersPostRender& rPostRender = *gpGame->RenderFrame(coordinate).postRender.pPlayers;
	const SpaceshipsPostRender& rSpaceshipsPostRender = *gpGame->RenderFrame(coordinate).postRender.pSpaceships;

	int64_t iCount = (rFrameInterpolate.gameFlags & GameFlags::kMainMenu) ? 0 : rPlayers.iCount;

	for (int64_t i = 0; i < iCount; ++i)
	{
		XMVECTOR vecPosition = rPlayers.pVecPositions[i];
		RenderCombatAim(rSpaceships, rSpaceshipsPostRender, vecPosition, rPostRender.pVecWantedDirections[i], rFrameInterpolate.renderBasis);
		RenderNavigation(rPlayers, rPostRender, i, iCount, vecPosition, rFrameInterpolate.renderBasis);
	}
}

} // namespace game

#endif // BT_CLIENT
