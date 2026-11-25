#include "Omnis.h"

#include "Frame/Collections/Pushers/Pushers.h"
#include "Frame/CellStaticData.h"

#include "Frame/Collections/Ships/FleetCoordination.h"
#include "Frame/Collections/Ships/ShipBehaviours.h"
#include "Frame/Collections/Ships/ShipHitboxes.h"
#include "Frame/Collections/Ships/ShipNavigationUtils.h"
#include "Frame/Collections/Ships/ShipQueries.h"
#include "Frame/HealthDamage.h"
#include "Frame/TerrainUtils.h"
#if defined(BT_CLIENT)
#include "Frame/Collections/Ships/ShipModels.h"
#include "Ui/LightingWrappers.h"
#include "Ui/WindDepositsWrappers.h"
#endif

namespace game
{

constexpr float kfRotateTowardsSpeed = 10.0f;

constexpr float kfOmniCirclingDistance = 30.0f;
// Cosine and sine of the 27-degree lead of the circling point ahead of the Omni.
constexpr float kfOmniCirclingLeadCos = 0.8910065f;
constexpr float kfOmniCirclingLeadSin = 0.4539905f;

#if defined(BT_CLIENT)
constexpr float kfShieldShrinkSpeed = 1.5f;
constexpr float kfShieldRotationSpeed = 4.0f;

constexpr float kfHexShieldSizeScale = kfOmniRadius * 1.334f;
constexpr float kfHexShieldColorMix = 0.85f;
constexpr float kfHexShieldFadeThreshold = 0.25f;

constexpr float kfRotationTiltFactor = 0.015f;
constexpr float kfRotationTiltMaximum = 0.4f;
#endif // BT_CLIENT

void OmnisInterpolate::Update([[maybe_unused]] FrameInterpolate& __restrict rFrameInterpolate, [[maybe_unused]] const Frame& __restrict rPreviousFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	OmnisInterpolate& rCurrent = *rFrameInterpolate.pOmnis;
	const OmnisInterpolate& rPrevious = *rPreviousFrame.interpolate.pOmnis;
	const OmnisPostRender& rPreviousPostRender = *rPreviousFrame.postRender.pOmnis;
	std::chrono::duration<float> deltaTime(rFrameInterpolate.fDeltaTime);

	for (int64_t i = 0; i < rCurrent.iCount; ++i)
	{
		XMVECTOR vecPosition = rPrevious.pVecPositions[i];
		XMVECTOR vecDirection = rPrevious.pVecDirections[i];
		std::chrono::duration<float> destroyedTime = rPrevious.pDestroyedTimes[i];

		if (!(rPreviousPostRender.pLifecycleFlags[i] & engine::LifecycleFlags::kDying)) [[likely]]
		{
			vecPosition = XMVectorMultiplyAdd(XMVectorReplicate(deltaTime.count()), rPreviousPostRender.pVecVelocities[i], vecPosition);
		}
		// Enforce W=1.0 — prevents drift via MultiplyAdd's 4-lane propagation (pos.W += dt * vel.W).
		vecPosition = XMVectorSetW(vecPosition, 1.0f);

		// The hull turns only about Z: it follows the horizontal part of a wanted direction that may aim at another
		// height, and holds when the target is exactly overhead.
		XMVECTOR vecWantedHorizontal = XMVectorSetZ(rPreviousPostRender.pVecWantedDirections[i], 0.0f);
		if (XMVectorGetX(XMVector3LengthSq(vecWantedHorizontal)) > 0.0f)
		{
			vecDirection = common::RotateTowardsPercent(vecDirection, XMVector3Normalize(vecWantedHorizontal), common::ExponentialInterpolant(kfRotateTowardsSpeed, deltaTime.count()));
		}

		if (rPreviousPostRender.pLifecycleFlags[i] & engine::LifecycleFlags::kDying) [[unlikely]]
		{
			destroyedTime = std::max(destroyedTime - deltaTime, std::chrono::duration<float>::zero());
		}

		rCurrent.pVecPositions[i] = vecPosition;
		rCurrent.pVecDirections[i] = vecDirection;
		rCurrent.pDestroyedTimes[i] = destroyedTime;

		engine::PushersInterpolate::Sync(rFrameInterpolate, rCurrent.pPushers[i],
		{
			.vecPosition = vecPosition,
			.fRadius = gShipHitboxShapes[std::to_underlying(ShipType::kOmni)][std::to_underlying(rPreviousPostRender.pController[i])].fPusherRadius,
			.fIntensity = kfOmniPusherIntensity,
			.uiPower = kuiOmniPusherPower,
			.flags = {engine::PusherFlags::kTypeDefault},
		});

#if defined(BT_CLIENT)
		float fRotationAccelerationX = std::clamp(kfRotationTiltFactor * XMVectorGetX(rPreviousPostRender.pVecVelocities[i]), -kfRotationTiltMaximum, kfRotationTiltMaximum);
		float fRotationAccelerationY = std::clamp(-kfRotationTiltFactor * XMVectorGetY(rPreviousPostRender.pVecVelocities[i]), -kfRotationTiltMaximum, kfRotationTiltMaximum);
		rCurrent.pfRotationAccelerationXs[i] = fRotationAccelerationX;
		rCurrent.pfRotationAccelerationYs[i] = fRotationAccelerationY;

		float fShieldRotation = rPrevious.pfShieldRotations[i];
		float fShieldShrink = rPrevious.pfShieldShrinks[i];
		fShieldRotation += deltaTime.count() * kfShieldRotationSpeed;
		fShieldShrink = std::clamp(fShieldShrink + (rPreviousPostRender.pfShields[i] > 0.0f ? deltaTime.count() * kfShieldShrinkSpeed : -deltaTime.count() * kfShieldShrinkSpeed), 0.0f, 1.0f);
		rCurrent.pfShieldRotations[i] = fShieldRotation;
		rCurrent.pfShieldShrinks[i] = fShieldShrink;

		// Each row's clock advances through its own controller's model.
		std::chrono::duration<float> animationTime = rPrevious.pAnimationTimes[i];
		common::crc_t uiModel = rPreviousPostRender.pController[i] == ShipController::kFleet ? kuiOmniFleetModel : kuiOmniRandomModel;
		if (engine::gAnimationDataMap.contains(uiModel))
		{
			animationTime = engine::gAnimationDataMap.at(uiModel).AdvanceLoopingTime(animationTime, deltaTime);
		}
		rCurrent.pAnimationTimes[i] = animationTime;

		if ((rCurrent.pWindTrails[i].uuid.iValue != 0))
		{
			engine::WindTrailsInterpolate::Sync(rFrameInterpolate, rCurrent.pWindTrails[i],
			{
				.vecPosition = vecPosition,
				.fIntensity = game::gWindDepositOmniIntensity.mfCurrent,
				.fWidth = game::gWindDepositOmniWidth.mfCurrent,
				.fLengthMultiplier = game::gWindDepositOmniLengthMultiplier.mfCurrent,
			});
		}

		HexShieldDirections hexShieldDirections = rPrevious.pHexShieldDirections[i];
		HexShieldIntensities hexShieldVertexIntensities {};
		HexShieldIntensities hexShieldFragmentIntensities {};
		for (int64_t j = 0; j < shaders::kiHexShieldDirections; ++j)
		{
			hexShieldVertexIntensities.pfData[j] = std::max(rPrevious.pHexShieldVertexIntensities[i].pfData[j] - gHexShieldIntensityDecay.mfCurrent * deltaTime.count(), 0.0f);
			hexShieldFragmentIntensities.pfData[j] = std::max(rPrevious.pHexShieldFragmentIntensities[i].pfData[j] - gHexShieldIntensityDecay.mfCurrent * deltaTime.count(), 0.0f);
		}
		rCurrent.pHexShieldDirections[i] = hexShieldDirections;
		rCurrent.pHexShieldVertexIntensities[i] = hexShieldVertexIntensities;
		rCurrent.pHexShieldFragmentIntensities[i] = hexShieldFragmentIntensities;

		if ((rCurrent.pHexShields[i].uuid.iValue != 0) && !(rPreviousPostRender.pLifecycleFlags[i] & engine::LifecycleFlags::kDying))
		{
			XMMATRIX matRotation = XMMatrixRotationZ(rCurrent.pfShieldRotations[i]);
			XMFLOAT3X4 f3x4Transform {};
			XMFLOAT3X4 f3x4TransformNormal {};
			XMStoreFloat3x4(&f3x4Transform, matRotation);
			XMStoreFloat3x4(&f3x4TransformNormal, XMMatrixTranspose(XMMatrixInverse(nullptr, matRotation)));

			engine::HexShieldsInterpolate::SyncData syncData
			{
				.vecPosition = rCurrent.pVecPositions[i],
				.pf4Transforms =
				{
					{f3x4Transform._11, f3x4Transform._12, f3x4Transform._13, f3x4Transform._14},
					{f3x4Transform._21, f3x4Transform._22, f3x4Transform._23, f3x4Transform._24},
					{f3x4Transform._31, f3x4Transform._32, f3x4Transform._33, f3x4Transform._34},
				},
				.pf4TransformNormals =
				{
					{f3x4TransformNormal._11, f3x4TransformNormal._12, f3x4TransformNormal._13, f3x4TransformNormal._14},
					{f3x4TransformNormal._21, f3x4TransformNormal._22, f3x4TransformNormal._23, f3x4TransformNormal._24},
					{f3x4TransformNormal._31, f3x4TransformNormal._32, f3x4TransformNormal._33, f3x4TransformNormal._34},
				},
				.pf4Directions = {},
				.pfVertexIntensities = {},
				.pfFragmentIntensities = {},
				.fLightingIntensity = gHexShieldLightingIntensity.mfCurrent,
				.fSize = rCurrent.pfShieldShrinks[i] * kfHexShieldSizeScale,
				.fColorMix = kfHexShieldColorMix,
			};

			// Copy direction arrays (per-slot smoothstep tail so fade-out eases to zero)
			for (int64_t j = 0; j < shaders::kiHexShieldDirections; ++j)
			{
				float fFragmentIntensity = rCurrent.pHexShieldFragmentIntensities[i].pfData[j];
				float fFade = std::clamp(fFragmentIntensity / kfHexShieldFadeThreshold, 0.0f, 1.0f);
				float fSmoothFade = fFade * fFade * (3.0f - 2.0f * fFade);
				syncData.pf4Directions[j] = rCurrent.pHexShieldDirections[i].pf4Data[j];
				syncData.pfVertexIntensities[j] = rCurrent.pHexShieldVertexIntensities[i].pfData[j] * fSmoothFade;
				syncData.pfFragmentIntensities[j] = fFragmentIntensity * fSmoothFade;
			}

			engine::HexShieldsInterpolate::Sync(rFrameInterpolate, rCurrent.pHexShields[i], syncData);
		}

		// The flagship row of this tick is not updated yet, so the beam starts at its previous-frame position, about one
		// tick behind. Without the flagship the light keeps its last corners until PostRender Update releases it.
		if ((rCurrent.pHealBeamAreaLights[i].uuid.iValue != 0))
		{
			std::optional<ShipLocator> optionalFlagship = FindShipByGlobalId(rPreviousFrame, rPreviousPostRender.pFlagshipGlobalIds[i]);
			if (optionalFlagship)
			{
				float fTime = static_cast<float>(rFrameInterpolate.iTick) * engine::kfDeltaTime + rFrameInterpolate.fDeltaTime;
				float fPulse = 0.5f + 0.5f * std::sin(XM_2PI * kfOmniHealBeamPulseFrequency * fTime);
				SyncFighterBeam(rFrameInterpolate, rCurrent.pHealBeamAreaLights[i], siHealBeamAreaLightTypeIndex, ShipPosition(rPreviousFrame.interpolate, *optionalFlagship), vecPosition, kfOmniHealBeamHalfWidth, std::lerp(kfOmniHealBeamPulseMinimum, 1.0f, fPulse));
			}
		}
#endif // BT_CLIENT
	}
}

// vecTarget while the Omni is farther than kfOmniCirclingDistance from it in x/y; within it, the point on that circle the
// lead angle clockwise ahead of the Omni, so chasing it circles the target clockwise. An Omni exactly over its target gets
// vecTarget.
static XMVECTOR XM_CALLCONV OmniCirclingPoint(FXMVECTOR vecPosition, FXMVECTOR vecTarget)
{
	XMVECTOR vecOffset = XMVectorSetZ(XMVectorSubtract(vecPosition, vecTarget), 0.0f);
	if (XMVectorGetX(XMVector2LengthSq(vecOffset)) > kfOmniCirclingDistance * kfOmniCirclingDistance)
	{
		return vecTarget;
	}

	// XMVector2Normalize divides every lane by the x/y length, so Z is zeroed above; a zero offset normalizes to zero.
	XMVECTOR vecNormal = XMVector2Normalize(vecOffset);
	XMVECTOR vecLeadNormal = common::TransformDirection(vecNormal, XMMATRIX(kfOmniCirclingLeadCos, -kfOmniCirclingLeadSin, 0.0f, 0.0f, kfOmniCirclingLeadSin, kfOmniCirclingLeadCos, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f));
	return XMVectorAdd(vecTarget, XMVectorScale(vecLeadNormal, kfOmniCirclingDistance));
}

// OmnisPostRender::Update orchestrates the shared per-row ship behaviours.

void OmnisPostRender::Update([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const Frame& __restrict rPreviousFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	OmnisPostRender& __restrict rCurrent = *rFrame.postRender.pOmnis;
	const OmnisPostRender& rPrevious = *rPreviousFrame.postRender.pOmnis;
	const OmnisInterpolate& rPreviousInterpolate = *rPreviousFrame.interpolate.pOmnis;
	std::chrono::duration<float> deltaTime(rFrame.interpolate.fDeltaTime);

	if (rCurrent.iCount == 0)
	{
		return;
	}

	FleetBoard fleetBoard(rPreviousFrame);
	NavRectangleBoard navRectangleBoard(rPreviousFrame, ShipType::kOmni);

	for (int64_t i = 0; i < rCurrent.iCount; ++i)
	{
		ShipOrders_t orders = rPrevious.pOrders[i];
		ShipFlags_t flags = rPrevious.pFlags[i];
		// The fire request lasts one tick: only this tick's AcquireTarget sets it, which a transfer-locked row skips.
		flags.Set(ShipFlags::kFireBlaster, false);
		std::chrono::duration<float> nextBlasterFireTime = rPrevious.pNextBlasterFireTimes[i];
		XMVECTOR vecVelocity = rPrevious.pVecVelocities[i];
		XMVECTOR vecWantedDirection = rPrevious.pVecWantedDirections[i];
		float fArmor = rPrevious.pfArmors[i];
		float fShield = rPrevious.pfShields[i];
		std::chrono::duration<float> shieldCooldown = std::max(std::chrono::duration<float>::zero(), rPrevious.pShieldCooldowns[i] - deltaTime);
#if defined(BT_CLIENT)
		std::chrono::duration<float> destroyedExplosionTime = std::max(std::chrono::duration<float>::zero(), rPrevious.pDestroyedExplosionTimes[i] - deltaTime);
#endif
		std::chrono::duration<float> shieldDownSoundCooldown = std::max(std::chrono::duration<float>::zero(), rPrevious.pShieldDownSoundCooldowns[i] - deltaTime);
		XMVECTOR vecAiDirection = rPrevious.pVecAiDirections[i];
		std::chrono::duration<float> transferLockTimer = rPrevious.pTransferLockTimers[i];
		std::chrono::duration<float> arrivalGracePeriod = std::max(std::chrono::duration<float>::zero(), rPrevious.pArrivalGracePeriods[i] - deltaTime);
		std::chrono::duration<float> cellChangeTimer = rPrevious.pCellChangeTimers[i];
		engine::GridCoord fleetWantedCoordinate = rPrevious.pFleetWantedCoordinates[i];
		int64_t iPendingFleetWantedCoordinateTicks = rPrevious.puiPendingFleetWantedCoordinateTicks[i];
		int64_t iNavigationDirection = GetNavigationDirection(flags);
		int64_t iNavigationWaypointIndex = GetNavigationWaypointIndex(flags);
		XMVECTOR vecIslandDestination = rPrevious.pVecIslandDestinations[i];
		engine::registry_id_t registryTarget = rPrevious.pRegistryTargets[i];
		XMVECTOR vecPosition = rPreviousInterpolate.pVecPositions[i];
		// Within kfOmniPatrolRadius of its flagship in x/y on a tick outside transfer lock; only a healing omni there heals.
		bool bWithinPatrolRadius = false;

#if defined(BT_CLIENT)
		if constexpr (kbDebugRender)
		{
			// Debug nav waypoint persists across the staggered pathfind throttle: ComputeNavigation
			// overwrites it only on a recompute tick, so carry the previous tick's value forward here.
			// Also covers the transfer-lock path, where ComputeNavigation is skipped. Without this the
			// non-recompute ticks render stale buffer contents (line flashes to random positions).
			rCurrent.pVecDebugNavigationWaypoints[i] = rPrevious.pVecDebugNavigationWaypoints[i];
		}
#endif // BT_CLIENT

		CountdownPendingOrders(iPendingFleetWantedCoordinateTicks);

		// Transfer lock skips navigation, target acquisition, and movement acceleration.
		if (transferLockTimer > std::chrono::duration<float>::zero())
		{
			transferLockTimer -= deltaTime;
		}
		else
		{
			if (rPrevious.pController[i] == ShipController::kFleet)
			{
				// A healing omni patrols whatever its fleet's stance.
				FleetView view = fleetBoard.Find(rPrevious.pFlagshipGlobalIds[i]);
				if (flags & ShipFlags::kHealing)
				{
					view.eStance = FleetStance::kEscort;
				}
				ShipTactic tactic = ChooseFleetTactic(ShipType::kOmni, rPreviousFrame, rStaticData, view, rPrevious.pGlobalIds[i], vecPosition, navRectangleBoard, rPrevious.pAlignments[i], registryTarget);
				if (tactic.bHasDestination)
				{
					if (tactic.bAttacking)
					{
						tactic.vecDestination = OmniCirclingPoint(vecPosition, tactic.vecDestination);
					}
					// A tactic tick leaves the row roaming, so the next navigation tick re-steers by roaming or changes mode
					// and recomputes its path.
					AimAiDirection(XMVectorSubtract(tactic.vecDestination, vecPosition), vecAiDirection);
					iNavigationDirection = -1;
				}
				else
				{
					// A healing omni escorts whatever its squadron setting, so it stays where it can heal; its stored orders
					// keep kPatrolCell.
					ShipOrders_t navigationOrders = orders;
					navigationOrders.Set(ShipOrders::kPatrolCell, (orders & ShipOrders::kPatrolCell) && !(flags & ShipFlags::kHealing));
					ComputeNavigation(rFrame, rPreviousFrame, rStaticData, {.eType = ShipType::kOmni, .iRow = i}, vecPosition, navRectangleBoard, fleetWantedCoordinate, iPendingFleetWantedCoordinateTicks, navigationOrders, iNavigationDirection, iNavigationWaypointIndex, vecAiDirection, vecIslandDestination, cellChangeTimer);
				}
				flags.Set(ShipFlags::kNoAiDirection, false);
				bWithinPatrolRadius = view.flagshipGlobalId.iValue != 0 && XMVectorGetX(XMVector2LengthSq(XMVectorSubtract(vecPosition, view.vecFlagshipPosition))) <= kfOmniPatrolRadius * kfOmniPatrolRadius;
			}
			else if (XMVECTOR vecFlagship = XMVectorZero(); (orders & ShipOrders::kTargetFlagship) && FindNearestEnemyFlagship(rPreviousFrame, vecPosition, rPrevious.pAlignments[i], registryTarget, vecFlagship))
			{
				// A flagship chase never returns to an island.
				flags.Set(ShipFlags::kReturnToIslandCenter, false);
				XMVECTOR vecDestination = HasShipLineOfSight(rPreviousFrame, rStaticData, ShipTypeClearanceTier(ShipType::kOmni), vecPosition, vecFlagship) ? OmniCirclingPoint(vecPosition, vecFlagship) : NextRouteWaypoint(rStaticData, ShipTypeClearanceTier(ShipType::kOmni), vecPosition, vecFlagship, navRectangleBoard);
				AimAiDirection(XMVectorSubtract(vecDestination, vecPosition), vecAiDirection);
				flags.Set(ShipFlags::kNoAiDirection, false);
			}
			else
			{
				XMVECTOR vecTarget = XMVectorZero();
				bool bInLineOfSight = false;
				bool bEnemyFound = FindAttackTarget(rPreviousFrame, rStaticData, vecPosition, vecPosition, {}, rPrevious.pAlignments[i], registryTarget, vecTarget, bInLineOfSight);
				bool bHasDestination = false;
				XMVECTOR vecDestination = ComputeReturnDestination(rStaticData, ShipTypeClearanceTier(ShipType::kOmni), vecPosition, bEnemyFound, vecTarget, navRectangleBoard, flags, bHasDestination);
				if (bEnemyFound && !(flags & ShipFlags::kReturnToIslandCenter))
				{
					// Circling reads the pick's sight, tested on kiFiringClearanceTier, so that must be the Omni's own tier.
					static_assert(ShipTypeClearanceTier(ShipType::kOmni) == kiFiringClearanceTier);
					vecDestination = bInLineOfSight ? OmniCirclingPoint(vecPosition, vecDestination) : NextRouteWaypoint(rStaticData, ShipTypeClearanceTier(ShipType::kOmni), vecPosition, vecTarget, navRectangleBoard);
				}
				if (bHasDestination)
				{
					AimAiDirection(XMVectorSubtract(vecDestination, vecPosition), vecAiDirection);
					flags.Set(ShipFlags::kNoAiDirection, false);
				}
			}

			bool bLookTargetFound = false;
			XMVECTOR vecLookPosition = XMVectorZero();
			AcquireTarget(rPreviousFrame, rStaticData, vecPosition, rPrevious.pAlignments[i], registryTarget, kfOmniBlastersSpeed, kfBlasterTargetRange, flags, bLookTargetFound, vecLookPosition);

			float fAccelerationMultiplier = (1.0f - kfOmniJitterRange * 0.5f) + common::Random<kfOmniJitterRange>(rFrame.postRender.randomEngine);
			float fDecayMultiplier = (1.0f - kfOmniJitterRange * 0.5f) + common::Random<kfOmniJitterRange>(rFrame.postRender.randomEngine);
			ApplyAcceleratedMovement(iNavigationDirection, (flags & ShipFlags::kNoAiDirection) ? XMVectorZero() : vecAiDirection, deltaTime, fAccelerationMultiplier, fDecayMultiplier, kOmniMovement, vecVelocity);

			if (bLookTargetFound)
			{
				// The reticle uses this unsmoothed lead-intercept direction; OmnisInterpolate::Update smooths hull rotation with kfRotateTowardsSpeed.
				vecWantedDirection = XMVector3Normalize(XMVectorSubtract(vecLookPosition, vecPosition));
			}
		}

		bool bHealing = bWithinPatrolRadius && (flags & ShipFlags::kHealing);
		if (bHealing)
		{
			fArmor = std::min(fArmor + kfOmniHealRate * deltaTime.count(), kfOmniArmor);
			fShield = std::min(fShield + kfOmniHealRate * deltaTime.count(), kfOmniShield);
			if (fArmor >= kfOmniArmor && fShield >= kfOmniShield)
			{
				flags.Set(ShipFlags::kHealing, false);
			}
		}

#if defined(BT_CLIENT)
		// The heal beam shows exactly while the omni heals; the light it holds stays out of the CRC.
		if (bHealing && (flags & ShipFlags::kHealing))
		{
			if (rFrame.interpolate.pOmnis->pHealBeamAreaLights[i].uuid.iValue == 0)
			{
				engine::AreaLightsPostRender::Add(rFrame, rFrame.interpolate.pOmnis->pHealBeamAreaLights[i], OmnisInterpolate::siHealBeamAreaLightTypeIndex);
			}
		}
		else
		{
			engine::ReleaseIndexableElementAndClearHandle(rFrame, rFrame.interpolate.pOmnis->pHealBeamAreaLights[i]);
		}
#endif // BT_CLIENT

		// Shield regeneration and pushes also run while transfer-locked. Terrain contact uses the sweep end's height.
		float fHeight = XMVectorGetZ(rFrame.interpolate.pOmnis->pVecPositions[i]);
		RegenerateShield(deltaTime, shieldCooldown, kfOmniShield, kfOmniShieldRegeneration, fShield);
		ApplyTerrainPush(rStaticData, vecPosition, kfOmniRadius, fHeight, kfPushMargin, vecVelocity);
		float fPositionCorrectionX = 0.0f;
		ApplyPusherPush(rFrame, rFrame.interpolate.pOmnis->pPushers[i], kfOmniMaximumPusherPushVelocity, vecVelocity, fPositionCorrectionX);
		if (rPrevious.pLifecycleFlags[i] & engine::LifecycleFlags::kDying)
		{
			ResolveTerrainContact(rStaticData, {.eType = ShipType::kOmni, .iRow = i}, vecPosition, rFrame.interpolate.pOmnis->pVecPositions[i], kfOmniRadius, vecVelocity);
		}
		else
		{
			float fIgnoredDeltaRotation = 0.0f;
			ApplyTerrainBounce(rStaticData, {.eType = ShipType::kOmni, .iRow = i}, vecPosition, rFrame.interpolate.pOmnis->pVecPositions[i], rFrame.interpolate.pOmnis->pVecDirections[i], kfOmniRadius, deltaTime, fIgnoredDeltaRotation, vecVelocity);
		}
		rFrame.interpolate.pOmnis->pVecPositions[i] = XMVectorSetX(rFrame.interpolate.pOmnis->pVecPositions[i], XMVectorGetX(rFrame.interpolate.pOmnis->pVecPositions[i]) + fPositionCorrectionX);
		// Height changes step the position directly.
		vecVelocity = XMVectorSetZ(vecVelocity, 0.0f);

		SetNavigationDirection(flags, iNavigationDirection);
		SetNavigationWaypointIndex(flags, iNavigationWaypointIndex);
		rCurrent.pOrders[i] = orders;
		rCurrent.pFlags[i] = flags;
		rCurrent.pNextBlasterFireTimes[i] = nextBlasterFireTime;
		rCurrent.pVecVelocities[i] = vecVelocity;
		rCurrent.pVecWantedDirections[i] = vecWantedDirection;
		rCurrent.pfArmors[i] = fArmor;
		rCurrent.pfShields[i] = fShield;
		rCurrent.pShieldCooldowns[i] = shieldCooldown;
#if defined(BT_CLIENT)
		rCurrent.pDestroyedExplosionTimes[i] = destroyedExplosionTime;
#endif
		rCurrent.pShieldDownSoundCooldowns[i] = shieldDownSoundCooldown;
		rCurrent.pVecAiDirections[i] = vecAiDirection;
		rCurrent.pTransferLockTimers[i] = transferLockTimer;
		rCurrent.pArrivalGracePeriods[i] = arrivalGracePeriod;
		rCurrent.pCellChangeTimers[i] = cellChangeTimer;
		rCurrent.pVecIslandDestinations[i] = vecIslandDestination;
		rCurrent.pRegistryTargets[i] = registryTarget;
		rCurrent.pFleetWantedCoordinates[i] = fleetWantedCoordinate;
		rCurrent.puiPendingFleetWantedCoordinateTicks[i] = static_cast<uint8_t>(iPendingFleetWantedCoordinateTicks);
	}
}

} // namespace game
