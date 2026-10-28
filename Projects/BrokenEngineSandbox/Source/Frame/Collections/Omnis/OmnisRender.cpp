#if defined(BT_CLIENT)

#include "Omnis.h"

#include "Data/Scene.h"
#include "File/PackChunks.h"
#include "Graphics/Debug/DebugRender.h"

#include "Frame/Collections/Ships/ShipQueries.h"
#include "Profile/ProfileRows.h"
#include "Game.h"

namespace game
{

// One model per ShipController, also used for animation lookup.
extern const common::crc_t kOmniFleetModel = data::kModelsspaceship2scenegltfCrc;
extern const common::crc_t kOmniRandomModel = data::kModelsSpaceFighterscenegltfCrc;

constexpr float kfDeathShrinkPower = 2.0f;

// One model pipeline per ShipController, indexed by it.
constexpr int64_t kiPipelineCount = std::to_underlying(ShipController::kCount);
constexpr std::array<const char*, kiPipelineCount> kpcPipelineNames {"OmnisFleet", "OmnisRandom"};
constexpr std::array<common::crc_t, kiPipelineCount> kuiPipelineCrcs {common::CrcConsteval("OmnisFleet"), common::CrcConsteval("OmnisRandom")};
const std::array<common::crc_t, kiPipelineCount> kuiModels {kOmniFleetModel, kOmniRandomModel};

constexpr int64_t kiDynamicBufferInitialCapacity = 256;

void OmnisInterpolate::GraphicsResources()
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
static void XM_CALLCONV RenderCombatAim(const FrameInterpolate& rFrameInterpolate, const FramePostRender& rRenderPostRender, FXMVECTOR vecPosition, FXMVECTOR vecWantedDirection, engine::AlignmentIdentifier alignment)
{
	const engine::RenderBasis& rBasis = rFrameInterpolate.renderBasis;
	float fClosestDistanceSquared = std::numeric_limits<float>::max();
	XMVECTOR vecClosestPosition = XMVectorZero();
	XMVECTOR vecClosestVelocity = XMVectorZero();
	bool bFound = false;

	ForEachEnemyShip(rFrameInterpolate, rRenderPostRender, alignment, [&]([[maybe_unused]] ShipLocator locator, FXMVECTOR vecEnemyPosition, FXMVECTOR vecEnemyVelocity)
	{
		float fDistanceSquared = XMVectorGetX(XMVector3LengthSq(XMVectorSubtract(vecPosition, vecEnemyPosition)));
		if (fDistanceSquared < fClosestDistanceSquared)
		{
			fClosestDistanceSquared = fDistanceSquared;
			vecClosestPosition = vecEnemyPosition;
			vecClosestVelocity = vecEnemyVelocity;
			bFound = true;
		}
	});

	if (bFound)
	{
		// Yellow reticle at the lead-intercept point: where the omni must aim so blasters land on the moving enemy ship
		XMVECTOR vecLead = common::ComputeLeadPosition(vecPosition, vecClosestPosition, vecClosestVelocity, kfOmniBlastersSpeed);
		XMFLOAT3A f3Lead {};
		XMStoreFloat3A(&f3Lead, engine::Rebase(rBasis, vecLead));
		engine::DebugRender::Circle(f3Lead, kfOmniRadius * 0.5f, {1.0f, 1.0f, 0.0f, 1.0f});

		// The red line uses PostRender's wanted-aim direction; the yellow reticle recomputes the nearest alive enemy ship's intercept from interpolated positions.
		float fDistanceToLead = common::Distance(vecPosition, vecLead);
		XMVECTOR vecLineEnd = XMVectorAdd(vecPosition, XMVectorScale(vecWantedDirection, fDistanceToLead));
		XMFLOAT3A f3Start {};
		XMFLOAT3A f3End {};
		XMStoreFloat3A(&f3Start, engine::Rebase(rBasis, vecPosition));
		XMStoreFloat3A(&f3End, engine::Rebase(rBasis, vecLineEnd));
		engine::DebugRender::Line(f3Start, f3End, {1.0f, 0.0f, 0.0f, 1.0f});
	}
}

void OmnisInterpolate::BeginRender([[maybe_unused]] int64_t iCommandBuffer, const std::unordered_map<engine::GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<engine::GridCoord>& rActiveCoordinates)
{
	siRendered = {};

	int64_t iTotalCapacity = engine::AccumulateRenderCapacity(rRenderInterpolates, rActiveCoordinates, [](const game::FrameInterpolate& rInterpolate) -> const OmnisInterpolate&
	{
		return *rInterpolate.pOmnis;
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

// Draws the rows whose controller selects iPipeline into that pipeline's buffer.
static void RenderPipeline(const OmnisInterpolate& __restrict rCurrent, const OmnisPostRender& __restrict rPostRender, const engine::RenderBasis& rBasis, int64_t iPipeline, int64_t iCommandBuffer)
{
	int64_t iCount = 0;
	for (int64_t i = 0; i < rCurrent.iCount; ++i)
	{
		iCount += std::to_underlying(rPostRender.pController[i]) == iPipeline ? 1 : 0;
	}

	auto [pOmniLayouts, iBufferCapacity] = engine::gpBufferManager->GetDynamicStorageBuffer<shaders::ModelLayout>(kuiPipelineCrcs[iPipeline], engine::kBufferMain, iCommandBuffer);
	ASSERT(siRendered[iPipeline] + iCount <= iBufferCapacity);

	const engine::AnimationData* pAnimationData = nullptr;
	engine::SkinningReservation skinningReservation {};
	if (engine::gAnimationDataMap.contains(kuiModels[iPipeline]))
	{
		pAnimationData = &engine::gAnimationDataMap.at(kuiModels[iPipeline]);
		skinningReservation = pAnimationData->ReserveSkinning(iCommandBuffer, iCount);
	}

	const std::unordered_map<common::crc_t, engine::EagerChunk>& rChunkMap = engine::gpFileManager->mpPackChunks->GetEagerChunkMap();
	const common::SceneHeader& rSceneHeader = rChunkMap.at(kuiModels[iPipeline]).pHeader->sceneHeader;
	const common::ModelHeader& rModelHeader = rChunkMap.at(rSceneHeader.modelCrc).pHeader->modelHeader;
	XMFLOAT3A f3ModelScale {rModelHeader.f3Scale.x, rModelHeader.f3Scale.y, rModelHeader.f3Scale.z};
	XMMATRIX matPreMove = XMMatrixTranslationFromVector(XMLoadFloat3(&rModelHeader.f3Offset));
	XMMATRIX matPreRotate = XMMatrixRotationX(rModelHeader.f3RotationRadians.x) * XMMatrixRotationY(rModelHeader.f3RotationRadians.y);
	if (rModelHeader.f3RotationRadians.z != 0.0f)
	{
		matPreRotate *= XMMatrixRotationZ(rModelHeader.f3RotationRadians.z);
	}

	int64_t iInstance = 0;
	for (int64_t i = 0; i < rCurrent.iCount; ++i)
	{
		if (std::to_underlying(rPostRender.pController[i]) != iPipeline)
		{
			continue;
		}

		XMFLOAT3A f3Size {kfOmniRadius * f3ModelScale.x, kfOmniRadius * f3ModelScale.y, kfOmniRadius * f3ModelScale.z};
		if (rCurrent.pDestroyedTimes[i] > std::chrono::duration<float>::zero())
		{
			float fDeathScale = std::pow(rCurrent.pDestroyedTimes[i] / kDestroyTime, kfDeathShrinkPower);
			f3Size.x *= fDeathScale;
			f3Size.y *= fDeathScale;
			f3Size.z *= fDeathScale;
		}

		// The position is local to the rendered cell; one conversion into the camera cell's frame feeds both the
		// model translation and the layout position.
		XMVECTOR vecPosition = engine::Rebase(rBasis, rCurrent.pVecPositions[i]);

		auto matScaling = XMMatrixScaling(f3Size.x, f3Size.y, f3Size.z);
		auto matTranslation = XMMatrixTranslationFromVector(vecPosition);
		auto matRotationZ = common::RotationMatrixFromDirection(rCurrent.pVecDirections[i], XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f));
		auto matRotationAccelerationX = XMMatrixRotationY(rCurrent.pfRotationAccelerationXs[i]);
		auto matRotationAccelerationY = XMMatrixRotationX(rCurrent.pfRotationAccelerationYs[i]);
		auto matTransform = XMMatrixMultiply(matPreMove, XMMatrixMultiply(matPreRotate, XMMatrixMultiply(matRotationZ, XMMatrixMultiply(matRotationAccelerationX, XMMatrixMultiply(matRotationAccelerationY, XMMatrixMultiply(matScaling, matTranslation))))));

		shaders::ModelLayout& rOmniLayout = pOmniLayouts[siRendered[iPipeline]];
		XMStoreFloat4(&rOmniLayout.f4Position, vecPosition);

		XMStoreFloat3x4(reinterpret_cast<XMFLOAT3X4*>(&rOmniLayout.f3x4Transform[0]), matTransform);
		XMStoreFloat3x4(reinterpret_cast<XMFLOAT3X4*>(&rOmniLayout.f3x4TransformNormal[0]), XMMatrixTranspose(XMMatrixInverse(nullptr, matTransform)));
		rOmniLayout.f4ColorAdd = {0.0f, 0.0f, 0.0f, 0.0f};
		rOmniLayout.uiMeshDataBase = 0;

		if (pAnimationData != nullptr)
		{
			rOmniLayout.uiMeshDataBase = static_cast<uint32_t>(pAnimationData->EvaluateInstance(skinningReservation, iInstance, rCurrent.pAnimationTimes[i].count()));
		}

		++iInstance;
		++siRendered[iPipeline];
	}
}

void OmnisInterpolate::Render(const FrameInterpolate& __restrict rFrameInterpolate, int64_t iCommandBuffer)
{
	engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerRenderOmnis);

	const OmnisInterpolate& rCurrent = *rFrameInterpolate.pOmnis;
	const engine::RenderBasis& rBasis = rFrameInterpolate.renderBasis;

	if (rCurrent.iCount == 0)
	{
		return;
	}

	OmnisRenderActiveGuard renderActiveGuard;

	// The interpolated rows align with the snapshot they were interpolated from, which holds each row's controller.
	const OmnisPostRender& rPostRender = *gpGame->RenderFrame(rBasis.coordinate).postRender.pOmnis;

	for (int64_t i = 0; i < kiPipelineCount; ++i)
	{
		RenderPipeline(rCurrent, rPostRender, rBasis, i, iCommandBuffer);
	}
}

void OmnisInterpolate::EndRender([[maybe_unused]] int64_t iCommandBuffer)
{
	for (int64_t i = 0; i < kiPipelineCount; ++i)
	{
		engine::gpPipelineManager->mDynamicPipelines.WriteModelPipelineIndirectBuffers(kuiPipelineCrcs[i], iCommandBuffer, siRendered[i]);
	}
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
	const Frame& rRenderFrame = gpGame->RenderFrame(coordinate);
	const OmnisPostRender& rPostRender = *rRenderFrame.postRender.pOmnis;

	for (int64_t i = 0; i < rOmnis.iCount; ++i)
	{
		XMVECTOR vecPosition = rOmnis.pVecPositions[i];
		RenderCombatAim(rFrameInterpolate, rRenderFrame.postRender, vecPosition, rPostRender.pVecWantedDirections[i], rPostRender.pAlignments[i]);
		if (rPostRender.pController[i] == ShipController::kFleet)
		{
			DebugRenderNavigation(rFrameInterpolate, rRenderFrame, vecPosition, rPostRender.pVecDebugNavigationWaypoints[i], rPostRender.pVecIslandDestinations[i], rPostRender.pFlags[i], rPostRender.pFlagshipGlobalIds[i], kfOmniRadius);
		}
	}
}

} // namespace game

#endif // BT_CLIENT
