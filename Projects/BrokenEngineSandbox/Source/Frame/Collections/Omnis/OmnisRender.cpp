#if defined(BT_CLIENT)

#include "Omnis.h"

#include "File/PackChunks.h"
#include "Graphics/Debug/DebugRender.h"

#include "Frame/Collections/Ships/ShipBehaviours.h"
#include "Frame/Collections/Ships/ShipHitboxes.h"
#include "Frame/Collections/Ships/ShipModels.h"
#include "Frame/Collections/Ships/ShipQueries.h"
#include "Profile/ProfileRows.h"
#include "Game.h"

namespace game
{

constexpr float kfDeathShrinkPower = 2.0f;

// One model pipeline per ShipController, indexed by it.
constexpr int64_t kiPipelineCount = std::to_underlying(ShipController::kCount);
constexpr std::array<const char*, kiPipelineCount> kpcPipelineNames {"OmnisFleet", "OmnisRandom"};
constexpr std::array<common::crc_t, kiPipelineCount> kuiPipelineCrcs {common::CrcConsteval("OmnisFleet"), common::CrcConsteval("OmnisRandom")};
const std::array<common::crc_t, kiPipelineCount> kuiModels {kuiOmniFleetModel, kuiOmniRandomModel};

constexpr int64_t kiDynamicBufferInitialCapacity = 256;

void OmnisInterpolate::GraphicsResources()
{
	for (int64_t i = 0; i < kiPipelineCount; ++i)
	{
		engine::gpPipelineManager->mDynamicPipelines.CreateModelPipelines(kuiPipelineCrcs[i], kpcPipelineNames[i], kuiModels[i], kiDynamicBufferInitialCapacity);
	}
}

// Every position here is local to the rendered cell, so each one converts into the camera cell's frame where it is
// handed to the debug renderer; the distances and directions are computed in the local frame, which is the same.
static void XM_CALLCONV RenderCombatAim(const FrameInterpolate& rFrameInterpolate, const FramePostRender& rRenderPostRender, FXMVECTOR vecPosition, FXMVECTOR vecWantedDirection, engine::AlignmentIdentifier alignment)
{
	const engine::RenderBasis& rBasis = rFrameInterpolate.renderBasis;
	float fClosestDistanceSquared = std::numeric_limits<float>::max();
	XMVECTOR vecClosestPosition = XMVectorZero();
	XMVECTOR vecClosestVelocity = XMVectorZero();
	bool bFound = false;

	ForEachEnemyShip(rFrameInterpolate, rRenderPostRender, alignment, [&]([[maybe_unused]] ShipLocator locator, [[maybe_unused]] engine::registry_id_t registryId, FXMVECTOR vecEnemyPosition, FXMVECTOR vecEnemyVelocity)
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

void OmnisInterpolate::RequestTextures(const std::unordered_map<engine::GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<engine::GridCoord>& rActiveCoordinates)
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
		const OmnisPostRender& rPostRender = *gpGame->RenderFrame(rCoordinate).postRender.pOmnis;
		for (int64_t i = 0; i < it->second.pOmnis->iCount; ++i)
		{
			bPresent[std::to_underlying(rPostRender.pController[i])] = true;
		}
	}

	engine::gpPipelineManager->mDynamicPipelines.RequestModelPipelineTextures(kuiPipelineCrcs, bPresent);
}

// Draws the rows whose controller selects iPipeline from pLayouts on and returns how many it wrote.
static int64_t RenderPipeline(const OmnisInterpolate& __restrict rCurrent, const OmnisPostRender& __restrict rPostRender, const engine::RenderBasis& rBasis, int64_t iPipeline, shaders::ModelLayout* pLayouts, int64_t iCommandBuffer)
{
	int64_t iCount = 0;
	for (int64_t i = 0; i < rCurrent.iCount; ++i)
	{
		iCount += std::to_underlying(rPostRender.pController[i]) == iPipeline ? 1 : 0;
	}

	const engine::AnimationData* pAnimationData = nullptr;
	engine::SkinningReservation skinningReservation {};
	if (engine::gAnimationDataMap.contains(kuiModels[iPipeline]))
	{
		pAnimationData = &engine::gAnimationDataMap.at(kuiModels[iPipeline]);
		skinningReservation = pAnimationData->ReserveSkinning(iCommandBuffer, iCount);
	}

	const std::unordered_map<common::crc_t, engine::EagerChunk>& rChunkMap = engine::gpFileManager->mpPackChunks->GetEagerChunkMap();
	const common::SceneHeader& rSceneHeader = rChunkMap.at(kuiModels[iPipeline]).pHeader->sceneHeader;
	ASSERT(rSceneHeader.modelCrc == kShipHitboxTypes[std::to_underlying(ShipType::kOmni)].models[iPipeline]);
	const engine::EagerChunk& rModelChunk = rChunkMap.at(rSceneHeader.modelCrc);
	XMMATRIX matAlignment = common::GetModelAlignment(rModelChunk.pHeader->modelHeader);
	engine::ModelShadowBounds shadowBounds = engine::ComputeModelShadowBounds(rModelChunk);

	float fRadius = gShipHitboxShapes[std::to_underlying(ShipType::kOmni)][iPipeline].fRadius;
	int64_t iInstance = 0;
	for (int64_t i = 0; i < rCurrent.iCount; ++i)
	{
		if (std::to_underlying(rPostRender.pController[i]) != iPipeline)
		{
			continue;
		}

		// The interpolated time can reach zero before the Destroy sweep removes the row, which would draw it at full scale.
		if (OmnisPostRender::IsFinishedDying(rPostRender.pLifecycleFlags[i], rCurrent.pDestroyedTimes[i]))
		{
			continue;
		}

		float fDeathScale = 1.0f;
		if (rCurrent.pDestroyedTimes[i] > std::chrono::duration<float>::zero())
		{
			fDeathScale = std::pow(rCurrent.pDestroyedTimes[i] / kDestroyTime, kfDeathShrinkPower);
		}

		// The position is local to the rendered cell; one conversion into the camera cell's frame feeds both the
		// model translation and the layout position.
		XMVECTOR vecPosition = engine::Rebase(rBasis, rCurrent.pVecPositions[i]);
		if (!engine::gpCamera->InVisibleArea(engine::gpCamera->mf4RenderVisibleArea, vecPosition, fRadius, fRadius, fRadius, fRadius))
		{
			continue;
		}

		auto matScaling = XMMatrixScaling(fDeathScale, fDeathScale, fDeathScale);
		auto matTranslation = XMMatrixTranslationFromVector(vecPosition);
		auto matRotationZ = common::RotationMatrixFromDirection(rCurrent.pVecDirections[i], XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f));
		auto matRotationAccelerationX = XMMatrixRotationY(rCurrent.pfRotationAccelerationXs[i]);
		auto matRotationAccelerationY = XMMatrixRotationX(rCurrent.pfRotationAccelerationYs[i]);
		auto matInstanceTransform = matScaling * matRotationZ * matRotationAccelerationX * matRotationAccelerationY * matTranslation;
		engine::FillModelLayout(shadowBounds, matAlignment, matInstanceTransform, vecPosition, pAnimationData, skinningReservation, iInstance, rCurrent.pAnimationTimes[i], pLayouts[iInstance]);

		++iInstance;
	}

	return iInstance;
}

void OmnisInterpolate::BeginRender(int64_t iCommandBuffer, const std::unordered_map<engine::GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<engine::GridCoord>& rActiveCoordinates)
{
	engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerRenderOmnis);

	int64_t iTotalCapacity = engine::AccumulateRenderCapacity(rRenderInterpolates, rActiveCoordinates, [](const game::FrameInterpolate& rInterpolate) -> const OmnisInterpolate&
	{
		return *rInterpolate.pOmnis;
	});

	// Any row can use either pipeline, so each reserves the whole capacity. Each cell's rows follow the previous cell's.
	for (int64_t i = 0; i < kiPipelineCount; ++i)
	{
		engine::gpPipelineManager->mDynamicPipelines.RenderModelPipelines(kuiPipelineCrcs[i], kpcPipelineNames[i], iCommandBuffer, iTotalCapacity, [&](shaders::ModelLayout* pLayouts)
		{
			int64_t iCount = 0;
			for (const engine::GridCoord& rCoordinate : rActiveCoordinates)
			{
				auto it = rRenderInterpolates.find(rCoordinate);
				if (it == rRenderInterpolates.end() || it->second.pOmnis->iCount == 0)
				{
					continue;
				}

				// The interpolated rows align with the snapshot they were interpolated from, which holds each row's controller.
				const Frame& rRenderFrame = gpGame->RenderFrame(rCoordinate);
				iCount += RenderPipeline(*it->second.pOmnis, *rRenderFrame.postRender.pOmnis, it->second.renderBasis, i, pLayouts + iCount, iCommandBuffer);
			}
			return iCount;
		});
	}
}

void OmnisInterpolate::Render(const FrameInterpolate& __restrict rFrameInterpolate, [[maybe_unused]] int64_t iCommandBuffer)
{
	engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerRenderOmnis);

	if constexpr (kbDebugRender)
	{
		const OmnisInterpolate& rCurrent = *rFrameInterpolate.pOmnis;
		const engine::RenderBasis& rBasis = rFrameInterpolate.renderBasis;
		const Frame& rRenderFrame = gpGame->RenderFrame(rBasis.coordinate);
		const OmnisPostRender& rPostRender = *rRenderFrame.postRender.pOmnis;

		// Entity positions MUST be read from rFrameInterpolate (the fully-interpolated frame),
		// never from PostRender. PostRender positions lag behind the rendered frame.
		// Only flags, metadata, and static world positions (nav waypoints, island destinations) come from PostRender.
		for (int64_t i = 0; i < rCurrent.iCount; ++i)
		{
			XMVECTOR vecPosition = rCurrent.pVecPositions[i];
			DebugRenderShipHitbox(rBasis, ShipType::kOmni, rPostRender.pController[i], vecPosition, rCurrent.pVecDirections[i]);
			RenderCombatAim(rFrameInterpolate, rRenderFrame.postRender, vecPosition, rPostRender.pVecWantedDirections[i], rPostRender.pAlignments[i]);
			if (rPostRender.pController[i] == ShipController::kFleet)
			{
				DebugRenderNavigation(rFrameInterpolate, rRenderFrame, vecPosition, rPostRender.pVecDebugNavigationWaypoints[i], rPostRender.pVecIslandDestinations[i], rPostRender.pFlags[i], rPostRender.pFlagshipGlobalIds[i], kfOmniRadius);
			}
		}
	}
}

} // namespace game

#endif // BT_CLIENT
