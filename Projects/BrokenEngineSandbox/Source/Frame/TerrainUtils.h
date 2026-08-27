#pragma once

#if defined(BT_SERVER)
#include "Frame/Collections/Players/Players.h" // kfPlayerRadius/kfPushMargin, the inputs the constexpr navigation values below derive from.
#endif // BT_SERVER

namespace engine { struct FrameStaticData; }

namespace game
{

// Steering and terrain avoidance stay game-owned on purpose: a real game rebuilds its movement code per flight model,
// so nothing that affects turn rate or flight characteristics belongs in the engine.
// The engine-worthy layer below it is already engine-owned: terrain queries (engine::IslandTerrain elevation, normal,
// sampler) and terrain tracing.
struct AiSteeringResult
{
	XMVECTOR vecAiDirection;
};

AiSteeringResult XM_CALLCONV ComputeAiSteering(const engine::FrameStaticData& rStaticData, FXMVECTOR vecPosition, FXMVECTOR vecCurrentDirection, FXMVECTOR vecFrameCenter, float fDeltaTime, bool bAlternateContour);

float XM_CALLCONV ComputeTerrainAvoidance(const engine::FrameStaticData& rStaticData, FXMVECTOR vecPosition, FXMVECTOR vecDirection, float fCurrentDeltaRotation);

#if defined(BT_SERVER)

// The game supplies navigation clearance and the elevation threshold to engine startup as runtime values,
// so a game can derive them however it wants (per ship type, for example). The engine passes both straight
// through to the nav bake and performs no arithmetic on them, which keeps the bake bit-identical.
constexpr float NavClearanceMeters()
{
	return kfPlayerRadius + kfPushMargin;
}

constexpr float NavThresholdElevation(float fBaseHeight)
{
	return fBaseHeight - kfPlayerRadius - kfPushMargin;
}

#endif // BT_SERVER

} // namespace game
