#include "Spaceships.h"

#include "Frame/Collections/Pushers/Pushers.h"
#include "Frame/CellStaticData.h"

#include "Frame/Collections/Players/Players.h"
#include "Frame/TerrainUtils.h"

namespace game
{

using enum SpaceshipFlags;

constexpr float kfSpaceshipChaseAcceleration = 4.0f;
constexpr float kfSpaceshipFleeAcceleration = 6.0f;
constexpr float kfSpaceshipReturnAcceleration = kfSpaceshipAcceleration;

constexpr float kfSpaceshipSteeringSmoothing = 0.064f;
constexpr float kfSpaceshipSteeringDecay = 6.0f;
constexpr float kfSpaceshipChaseTurnRate = 32.0f;
constexpr float kfSpaceshipFleeTurnRate = 32.0f;

// Velocity-to-direction blend (airplane constraint)
constexpr float kfSpaceshipVelocityToDirection = 4.0f;

// Flee/return hysteresis
constexpr float kfSpaceshipFleeStartDistance = 15.0f;
constexpr float kfSpaceshipFleeEndDistance = 25.0f;
constexpr float kfSpaceshipReturnDistance = 180.0f;
constexpr float kfSpaceshipReturnedDistance = kfSpaceshipReturnDistance - 20.0f;

constexpr float kfSpaceshipTerrainBounceRotation = 8.0f;
constexpr float kfSpaceshipTerrainBounceMove = kfSpaceshipRadius * 2.0f;
constexpr float kfSpaceshipTerrainBounceVelocity = 4.0f;

// Terrain avoidance (player-proximity skip constants; sampling constants in TerrainUtils.cpp)
constexpr float kfSpaceshipAvoidTerrainPlayerAngle = 0.4f;
constexpr float kfSpaceshipAvoidTerrainPlayerDistance = 40.0f;

// Forward declarations for shared helpers (defined in Spaceships.cpp)
[[nodiscard]] bool XM_CALLCONV NearestAlivePlayerPosition(const PlayersInterpolate& rPlayers, const PlayersPostRender& rPlayersPostRender, FXMVECTOR vecFrom, XMVECTOR& rVecResult);

void XM_CALLCONV SpaceshipsPostRender::ComputeSteering(std::span<const XMFLOAT4> islandCandidates, FXMVECTOR vecPosition, FXMVECTOR vecDirection, bool bPlayerAlive, FXMVECTOR vecNearestPlayer, float fDeltaTime, SpaceshipFlags_t& rFlags, float& rfDeltaRotation)
{
	XMVECTOR vecToPlayer = bPlayerAlive ? XMVectorSubtract(vecNearestPlayer, vecPosition) : XMVectorZero();
	float fPlayerDistance = bPlayerAlive ? XMVectorGetX(XMVector3Length(vecToPlayer)) : kfSpaceshipFleeEndDistance + 1.0f;
	if (fPlayerDistance < kfSpaceshipFleeStartDistance)
	{
		rFlags.Set(kFleePlayer);
	}
	else if (fPlayerDistance > kfSpaceshipFleeEndDistance)
	{
		rFlags.Set(kFleePlayer, false);
	}

	if (islandCandidates.empty())
	{
		rFlags.Set(kReturnToIslandCenter, false);
		if (bPlayerAlive)
		{
			XMVECTOR vecToDestinationNormal = XMVector3Normalize(XMVectorSubtract(vecNearestPlayer, vecPosition));
			float fDirectionDestinationCrossZ = XMVectorGetZ(XMVector3Cross(vecDirection, vecToDestinationNormal));
			float fWantedDeltaRotation = fDirectionDestinationCrossZ > 0.0f ? kfSpaceshipChaseTurnRate : -kfSpaceshipChaseTurnRate;
			if (rFlags & kFleePlayer)
			{
				fWantedDeltaRotation = fDirectionDestinationCrossZ > 0.0f ? -kfSpaceshipFleeTurnRate : kfSpaceshipFleeTurnRate;
			}

			rfDeltaRotation = std::lerp(fWantedDeltaRotation, rfDeltaRotation, common::ExponentialDecay(kfSpaceshipSteeringSmoothing, fDeltaTime));
		}
		rfDeltaRotation = common::ExponentialDecay(kfSpaceshipSteeringDecay, fDeltaTime) * rfDeltaRotation;
		return;
	}

	// Return-to-island steering selects the nearest center from candidates precomputed once per coord in Update.
	// The tick-invariant candidates use the placement's x/y, gBaseHeight, and w=1 for every ship.
	XMVECTOR vecIslandCenter = XMLoadFloat4(&islandCandidates[0]);
	float fNearestDistanceSquared = std::numeric_limits<float>::max();
	for (const XMFLOAT4& rf4Candidate : islandCandidates)
	{
		XMVECTOR vecCandidate = XMLoadFloat4(&rf4Candidate);
		float fDistanceSquared = XMVectorGetX(XMVector3LengthSq(XMVectorSubtract(vecCandidate, vecPosition)));
		if (fDistanceSquared < fNearestDistanceSquared)
		{
			fNearestDistanceSquared = fDistanceSquared;
			vecIslandCenter = vecCandidate;
		}
	}
	float fDistanceFromIslandCenter = common::Distance(vecPosition, vecIslandCenter);
	if (fDistanceFromIslandCenter > kfSpaceshipReturnDistance)
	{
		rFlags.Set(kReturnToIslandCenter);
	}
	else if (fDistanceFromIslandCenter < kfSpaceshipReturnedDistance)
	{
		rFlags.Set(kReturnToIslandCenter, false);
	}

	XMVECTOR vecDestination = bPlayerAlive ? vecNearestPlayer : vecIslandCenter;
	if (rFlags & kReturnToIslandCenter)
	{
		vecDestination = vecIslandCenter;
	}
	XMVECTOR vecToDestinationNormal = XMVector3Normalize(XMVectorSubtract(vecDestination, vecPosition));
	float fDirectionDestinationCrossZ = XMVectorGetZ(XMVector3Cross(vecDirection, vecToDestinationNormal));
	float fWantedDeltaRotation = fDirectionDestinationCrossZ > 0.0f ? kfSpaceshipChaseTurnRate : -kfSpaceshipChaseTurnRate;
	if (!(rFlags & kReturnToIslandCenter) && rFlags & kFleePlayer)
	{
		fWantedDeltaRotation = fDirectionDestinationCrossZ > 0.0f ? -kfSpaceshipFleeTurnRate : kfSpaceshipFleeTurnRate;
	}

	rfDeltaRotation = std::lerp(fWantedDeltaRotation, rfDeltaRotation, common::ExponentialDecay(kfSpaceshipSteeringSmoothing, fDeltaTime));
	rfDeltaRotation = common::ExponentialDecay(kfSpaceshipSteeringDecay, fDeltaTime) * rfDeltaRotation;
}

void XM_CALLCONV SpaceshipsPostRender::ApplyMovement(const Frame& __restrict rFrame, const SpaceshipsInterpolate& __restrict rCurrentInterpolate, int64_t i, SpaceshipFlags_t flags, float fDeltaTime, XMVECTOR& rVecVelocity)
{
	float fAcceleration = flags & kReturnToIslandCenter ? kfSpaceshipReturnAcceleration
	                    : flags & kFleePlayer ? kfSpaceshipFleeAcceleration
	                    : kfSpaceshipChaseAcceleration;
	rVecVelocity = engine::ApplyMovement<true>(rVecVelocity, rCurrentInterpolate.pVecDirections[i], fDeltaTime, fAcceleration, kfSpaceshipDrag, kfSpaceshipMaxSpeed, kfSpaceshipVelocityToDirection);
	ApplyPusherResponse(rFrame, rCurrentInterpolate, i, rVecVelocity);
}

void XM_CALLCONV SpaceshipsPostRender::ApplyPusherResponse(const Frame& __restrict rFrame, const SpaceshipsInterpolate& __restrict rCurrentInterpolate, int64_t i, XMVECTOR& rVecVelocity)
{
	// Apply push from nearby pushers (pass own pusher ID to ignore self-push)
	XMVECTOR vecPush = engine::PushersInterpolate::ApplyPush(rFrame.interpolate, rCurrentInterpolate.pVecPositions[i], rCurrentInterpolate.puiPushers[i]);

	float fPushLength = XMVectorGetX(XMVector3Length(vecPush));
	if (fPushLength > 0.0f)
	{
		XMVECTOR vecPushDirection = XMVectorDivide(vecPush, XMVectorReplicate(fPushLength));
		rVecVelocity = engine::ApplyClampedPush(rVecVelocity, vecPushDirection, fPushLength, kfSpaceshipMaxPusherPushVelocity);
	}
}

void SpaceshipsPostRender::ApplyTerrainBounce(const engine::CellStaticData& rStaticData, SpaceshipsInterpolate& __restrict rCurrentInterpolate, int64_t i, float fDeltaTime, float& rfDeltaRotation, XMVECTOR& rVecVelocity)
{
	float fTerrainElevation = engine::gpIslandTerrain->MakeCellElevationSampler(rStaticData).Sample(rCurrentInterpolate.pVecPositions[i]);
	if (fTerrainElevation >= XMVectorGetZ(rCurrentInterpolate.pVecPositions[i])) [[unlikely]]
	{
		XMVECTOR vecTerrainNormal = XMVector3Normalize(XMVectorSetZ(engine::gpIslandTerrain->CellNormal(rStaticData, rCurrentInterpolate.pVecPositions[i]), 0.0f));

		rCurrentInterpolate.pVecPositions[i] = XMVectorMultiplyAdd(XMVectorReplicate(fDeltaTime * kfSpaceshipTerrainBounceMove), vecTerrainNormal, rCurrentInterpolate.pVecPositions[i]);
		// Enforce W=1.0 — terrain-bounce bypasses the main integration clamp.
		rCurrentInterpolate.pVecPositions[i] = XMVectorSetW(rCurrentInterpolate.pVecPositions[i], 1.0f);

		float fDirectionTerrainCrossZ = XMVectorGetZ(XMVector3Cross(rCurrentInterpolate.pVecDirections[i], vecTerrainNormal));
		rfDeltaRotation = fDirectionTerrainCrossZ > 0.0f ? kfSpaceshipTerrainBounceRotation : -kfSpaceshipTerrainBounceRotation;

		// Velocity W stays exactly 0 because vecTerrainNormal inherits W=0 from XMVector3Cross in IslandTerrain.cpp's NormalFromElevation.
		rVecVelocity = XMVector3Reflect(rVecVelocity, vecTerrainNormal);
		rVecVelocity = XMVectorMultiplyAdd(XMVectorReplicate(fDeltaTime * kfSpaceshipTerrainBounceVelocity), vecTerrainNormal, rVecVelocity);
	}
}

void SpaceshipsPostRender::AvoidTerrain([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const Frame& __restrict rPreviousFrame, const engine::CellStaticData& rStaticData, int64_t iStart, int64_t iEnd)
{
	SpaceshipsInterpolate& rCurrentInterpolate = *rFrame.interpolate.pSpaceships;
	SpaceshipsPostRender& rCurrentPostRender = *rFrame.postRender.pSpaceships;

	for (int64_t i = iStart; i < iEnd; ++i)
	{
		if (rCurrentPostRender.pFlags[i] & kExploding) [[unlikely]]
		{
			continue;
		}

		if (rCurrentPostRender.pfArrivalGracePeriods[i] > 0.0f)
		{
			continue;
		}

		XMVECTOR vecNearestPlayer = XMVectorZero();
		if (NearestAlivePlayerPosition(*rFrame.interpolate.pPlayers, *rFrame.postRender.pPlayers, rCurrentInterpolate.pVecPositions[i], vecNearestPlayer))
		{
			XMVECTOR vecToPlayer = XMVectorSubtract(vecNearestPlayer, rCurrentInterpolate.pVecPositions[i]);
			float fDistanceToPlayer = XMVectorGetX(XMVector3Length(vecToPlayer));
			if (fDistanceToPlayer < kfSpaceshipAvoidTerrainPlayerDistance)
			{
				XMVECTOR vecToPlayerNormal = XMVector3Normalize(vecToPlayer);
				float fAngleToPlayer = XMVectorGetX(XMVector3AngleBetweenNormals(rCurrentInterpolate.pVecDirections[i], vecToPlayerNormal));
				if (fAngleToPlayer < kfSpaceshipAvoidTerrainPlayerAngle)
				{
					continue;
				}
			}
		}

		rCurrentInterpolate.pfDeltaRotations[i] = ComputeTerrainAvoidance(rStaticData, rCurrentInterpolate.pVecPositions[i], rCurrentInterpolate.pVecDirections[i], rCurrentInterpolate.pfDeltaRotations[i]);

		rCurrentInterpolate.pfDeltaRotations[i] = common::ClampMagnitude(rCurrentInterpolate.pfDeltaRotations[i], kfSpaceshipMaxTurnRate);
	}
}

} // namespace game
