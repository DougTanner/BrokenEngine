#pragma once

#if defined(BT_SERVER)
#include "Frame/Collections/Players/Players.h" // kfPlayerRadius/kfPushMargin, the inputs the constexpr navigation values below derive from.
#endif // BT_SERVER

namespace engine
{
	struct CellStaticData;
} // namespace engine

namespace game
{

// Steering and terrain avoidance are game-owned because they control turn rates and flight characteristics.
// Terrain queries and tracing are engine-owned.
struct AiSteeringResult
{
	XMVECTOR vecArtificialIntelligenceDirection {};
};

AiSteeringResult XM_CALLCONV ComputeArtificialIntelligenceSteering(const engine::CellStaticData& rStaticData, FXMVECTOR vecPosition, FXMVECTOR vecCurrentDirection, FXMVECTOR vecCellCenter, float fDeltaTime, bool bAlternateContour);

float XM_CALLCONV ComputeTerrainAvoidance(const engine::CellStaticData& rStaticData, FXMVECTOR vecPosition, FXMVECTOR vecDirection, float fCurrentDeltaRotation);

#if defined(BT_SERVER)

// Engine startup passes the game's runtime clearance and elevation threshold unchanged to the navigation bake.
constexpr float NavigationClearanceMeters()
{
	return kfPlayerRadius + kfPushMargin;
}

constexpr float NavigationThresholdElevation(float fBaseHeight)
{
	return fBaseHeight - kfPlayerRadius - kfPushMargin;
}

#endif // BT_SERVER

} // namespace game
