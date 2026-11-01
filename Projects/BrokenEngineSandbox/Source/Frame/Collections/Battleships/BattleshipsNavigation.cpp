#include "Battleships.h"

#include "Frame/CellStaticData.h"

#include "Frame/Collections/Ships/ShipQueries.h"
#include "Frame/TerrainUtils.h"

namespace game
{

// Terrain avoidance (enemy-proximity skip constants; sampling constants in TerrainUtils.cpp)
constexpr float kfBattleshipAvoidTerrainEnemyAngle = 0.4f;
constexpr float kfBattleshipAvoidTerrainEnemyDistance = 40.0f;

// Reads the previous frame's ships, so the tuple order never hands it an enemy row this tick has not updated yet.
void BattleshipsPostRender::AvoidTerrain([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const Frame& __restrict rPreviousFrame, const engine::CellStaticData& rStaticData, int64_t iStart, int64_t iEnd)
{
	BattleshipsInterpolate& rCurrentInterpolate = *rFrame.interpolate.pBattleships;
	BattleshipsPostRender& rCurrentPostRender = *rFrame.postRender.pBattleships;

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

		if (rCurrentPostRender.pController[i] == ShipController::kFleet && !IsWithinHeadingCone(rCurrentInterpolate.pVecDirections[i], rCurrentPostRender.pVecAiDirections[i]))
		{
			continue;
		}

		XMVECTOR vecNearestEnemy = XMVectorZero();
		if (FindNearestEnemyShip(rPreviousFrame, rCurrentInterpolate.pVecPositions[i], rCurrentPostRender.pAlignments[i], vecNearestEnemy))
		{
			XMVECTOR vecToEnemy = XMVectorSubtract(vecNearestEnemy, rCurrentInterpolate.pVecPositions[i]);
			float fDistanceToEnemy = XMVectorGetX(XMVector3Length(vecToEnemy));
			if (fDistanceToEnemy < kfBattleshipAvoidTerrainEnemyDistance)
			{
				XMVECTOR vecToEnemyNormal = XMVector3Normalize(vecToEnemy);
				float fAngleToEnemy = XMVectorGetX(XMVector3AngleBetweenNormals(rCurrentInterpolate.pVecDirections[i], vecToEnemyNormal));
				if (fAngleToEnemy < kfBattleshipAvoidTerrainEnemyAngle && HasLineOfSight(rStaticData, rCurrentInterpolate.puiFlightPlanes[i], rCurrentInterpolate.pVecPositions[i], vecNearestEnemy))
				{
					continue;
				}
			}
		}

		int64_t iNavigationPlane = NavigationFlightPlane(XMVectorGetZ(rCurrentInterpolate.pVecPositions[i]), GetTargetFlightPlane(rCurrentPostRender.pOrders[i]));
		float fAvoidanceDeltaRotation = ComputeTerrainAvoidance(rStaticData, rCurrentInterpolate.pVecPositions[i], rCurrentInterpolate.pVecDirections[i], rCurrentInterpolate.pfDeltaRotations[i], iNavigationPlane);

		// Rows still align with the previous frame here: Update calls this before Transfer, Destroy, and Spawn.
		rCurrentInterpolate.pfDeltaRotations[i] = RampBattleshipTurnRate(rPreviousFrame.interpolate.pBattleships->pfDeltaRotations[i], fAvoidanceDeltaRotation, std::chrono::duration<float>(rFrame.interpolate.fDeltaTime));
	}
}

} // namespace game
