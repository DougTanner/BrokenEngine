#include "Spaceships.h"

#include "Frame/Collections/Pushers/Pushers.h"
#include "Frame/CellStaticData.h"
#include "Frame/FlightPlanes.h"

#include "Frame/Collections/Omnis/Omnis.h"
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
constexpr float kfSpaceshipTerrainBounceVelocity = 4.0f;

// Terrain avoidance (omni-proximity skip constants; sampling constants in TerrainUtils.cpp)
constexpr float kfSpaceshipAvoidTerrainOmniAngle = 0.4f;
constexpr float kfSpaceshipAvoidTerrainOmniDistance = 40.0f;

// Forward declarations for shared helpers (defined in Spaceships.cpp)
[[nodiscard]] bool XM_CALLCONV NearestAliveOmniPosition(const OmnisInterpolate& rOmnis, const OmnisPostRender& rOmnisPostRender, FXMVECTOR vecFrom, XMVECTOR& rVecResult);

void XM_CALLCONV SpaceshipsPostRender::ComputeSteering(std::span<const XMFLOAT4> islandCandidates, FXMVECTOR vecPosition, FXMVECTOR vecDirection, bool bOmniAlive, FXMVECTOR vecNearestOmni, std::chrono::duration<float> deltaTime, SpaceshipFlags_t& rFlags, float& rfDeltaRotation)
{
	XMVECTOR vecToOmni = bOmniAlive ? XMVectorSubtract(vecNearestOmni, vecPosition) : XMVectorZero();
	float fOmniDistance = bOmniAlive ? XMVectorGetX(XMVector3Length(vecToOmni)) : kfSpaceshipFleeEndDistance + 1.0f;
	if (fOmniDistance < kfSpaceshipFleeStartDistance)
	{
		rFlags.Set(kFleeOmni);
	}
	else if (fOmniDistance > kfSpaceshipFleeEndDistance)
	{
		rFlags.Set(kFleeOmni, false);
	}

	if (islandCandidates.empty())
	{
		rFlags.Set(kReturnToIslandCenter, false);
		if (bOmniAlive)
		{
			XMVECTOR vecToDestinationNormal = XMVector3Normalize(XMVectorSubtract(vecNearestOmni, vecPosition));
			float fDirectionDestinationCrossZ = XMVectorGetZ(XMVector3Cross(vecDirection, vecToDestinationNormal));
			float fWantedDeltaRotation = fDirectionDestinationCrossZ > 0.0f ? kfSpaceshipChaseTurnRate : -kfSpaceshipChaseTurnRate;
			if (rFlags & kFleeOmni)
			{
				fWantedDeltaRotation = fDirectionDestinationCrossZ > 0.0f ? -kfSpaceshipFleeTurnRate : kfSpaceshipFleeTurnRate;
			}

			rfDeltaRotation = std::lerp(fWantedDeltaRotation, rfDeltaRotation, common::ExponentialDecay(kfSpaceshipSteeringSmoothing, deltaTime.count()));
		}
		rfDeltaRotation = common::ExponentialDecay(kfSpaceshipSteeringDecay, deltaTime.count()) * rfDeltaRotation;
		return;
	}

	// Return-to-island steering selects the nearest center from candidates precomputed once per coord in Update.
	// The tick-invariant candidates use the placement's x/y, the lowest flight plane's height, and w=1 for every ship.
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

	XMVECTOR vecDestination = bOmniAlive ? vecNearestOmni : vecIslandCenter;
	if (rFlags & kReturnToIslandCenter)
	{
		vecDestination = vecIslandCenter;
	}
	XMVECTOR vecToDestinationNormal = XMVector3Normalize(XMVectorSubtract(vecDestination, vecPosition));
	float fDirectionDestinationCrossZ = XMVectorGetZ(XMVector3Cross(vecDirection, vecToDestinationNormal));
	float fWantedDeltaRotation = fDirectionDestinationCrossZ > 0.0f ? kfSpaceshipChaseTurnRate : -kfSpaceshipChaseTurnRate;
	if (!(rFlags & kReturnToIslandCenter) && rFlags & kFleeOmni)
	{
		fWantedDeltaRotation = fDirectionDestinationCrossZ > 0.0f ? -kfSpaceshipFleeTurnRate : kfSpaceshipFleeTurnRate;
	}

	rfDeltaRotation = std::lerp(fWantedDeltaRotation, rfDeltaRotation, common::ExponentialDecay(kfSpaceshipSteeringSmoothing, deltaTime.count()));
	rfDeltaRotation = common::ExponentialDecay(kfSpaceshipSteeringDecay, deltaTime.count()) * rfDeltaRotation;
}

void XM_CALLCONV SpaceshipsPostRender::ApplyMovement(const Frame& __restrict rFrame, const SpaceshipsInterpolate& __restrict rCurrentInterpolate, int64_t i, SpaceshipFlags_t flags, std::chrono::duration<float> deltaTime, XMVECTOR& rVecVelocity)
{
	float fAcceleration = flags & kReturnToIslandCenter ? kfSpaceshipReturnAcceleration
	                    : flags & kFleeOmni ? kfSpaceshipFleeAcceleration
	                    : kfSpaceshipChaseAcceleration;
	rVecVelocity = engine::ApplyMovement<true>(rVecVelocity, rCurrentInterpolate.pVecDirections[i], deltaTime.count(), fAcceleration, kfSpaceshipDrag, kfSpaceshipMaxSpeed, kfSpaceshipVelocityToDirection);
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

void SpaceshipsPostRender::ApplyTerrainBounce(const engine::CellStaticData& rStaticData, const SpaceshipsInterpolate& rPreviousInterpolate, SpaceshipsInterpolate& __restrict rCurrentInterpolate, int64_t i, std::chrono::duration<float> deltaTime, float& rfDeltaRotation, XMVECTOR& rVecVelocity)
{
	engine::DiscTerrainResult result = engine::ResolveDiscAgainstTerrain(rStaticData, rPreviousInterpolate.pVecPositions[i], rCurrentInterpolate.pVecPositions[i], kfSpaceshipRadius, engine::gpFlightPlanes->Lowest().fHeightMeters, false);
	if (result.flags & engine::DiscTerrainFlags::kStartOverlap) [[unlikely]]
	{
		LOG(kDefault, kWarning, "Spaceship terrain start overlap Cell: ({},{}) Index: {} Start: {} Moved: {}", rStaticData.coordinate.iX, rStaticData.coordinate.iY, i, common::WbV2(rPreviousInterpolate.pVecPositions[i], 1), common::Wb(result.fStartOverlapDistance, 1));
	}

	// The resolved position carries W=1.0 — this write bypasses the main integration clamp.
	rCurrentInterpolate.pVecPositions[i] = result.vecPosition;
	if (result.flags & engine::DiscTerrainFlags::kContact) [[unlikely]]
	{
		float fDirectionTerrainCrossZ = XMVectorGetZ(XMVector3Cross(rCurrentInterpolate.pVecDirections[i], result.vecNormal));
		rfDeltaRotation = fDirectionTerrainCrossZ > 0.0f ? kfSpaceshipTerrainBounceRotation : -kfSpaceshipTerrainBounceRotation;

		// Velocity W stays exactly 0 because ResolveDiscAgainstTerrain returns the contact normal with W=0.
		rVecVelocity = XMVector3Reflect(rVecVelocity, result.vecNormal);
		rVecVelocity = XMVectorMultiplyAdd(XMVectorReplicate(deltaTime.count() * kfSpaceshipTerrainBounceVelocity), result.vecNormal, rVecVelocity);
	}
}

void SpaceshipsPostRender::AvoidTerrain([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const Frame& __restrict rPreviousFrame, const engine::CellStaticData& rStaticData, int64_t iStart, int64_t iEnd)
{
	SpaceshipsInterpolate& rCurrentInterpolate = *rFrame.interpolate.pSpaceships;
	SpaceshipsPostRender& rCurrentPostRender = *rFrame.postRender.pSpaceships;

	for (int64_t i = iStart; i < iEnd; ++i)
	{
		if (rCurrentPostRender.pLifecycleFlags[i] & engine::LifecycleFlags::kDying) [[unlikely]]
		{
			continue;
		}

		if (rCurrentPostRender.pArrivalGracePeriods[i] > std::chrono::duration<float>::zero())
		{
			continue;
		}

		XMVECTOR vecNearestOmni = XMVectorZero();
		if (NearestAliveOmniPosition(*rFrame.interpolate.pOmnis, *rFrame.postRender.pOmnis, rCurrentInterpolate.pVecPositions[i], vecNearestOmni))
		{
			XMVECTOR vecToOmni = XMVectorSubtract(vecNearestOmni, rCurrentInterpolate.pVecPositions[i]);
			float fDistanceToOmni = XMVectorGetX(XMVector3Length(vecToOmni));
			if (fDistanceToOmni < kfSpaceshipAvoidTerrainOmniDistance)
			{
				XMVECTOR vecToOmniNormal = XMVector3Normalize(vecToOmni);
				float fAngleToOmni = XMVectorGetX(XMVector3AngleBetweenNormals(rCurrentInterpolate.pVecDirections[i], vecToOmniNormal));
				if (fAngleToOmni < kfSpaceshipAvoidTerrainOmniAngle)
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
