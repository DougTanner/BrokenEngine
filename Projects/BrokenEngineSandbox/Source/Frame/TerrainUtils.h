#pragma once

#include "Frame/Collections/Ships/ShipHitboxes.h"

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
	XMVECTOR vecAiDirection {};
};

AiSteeringResult XM_CALLCONV ComputeAiSteering(const engine::CellStaticData& rStaticData, FXMVECTOR vecPosition, FXMVECTOR vecCurrentDirection, FXMVECTOR vecCellCenter, float fDeltaTime, bool bAlternateContour);

float XM_CALLCONV ComputeTerrainAvoidance(const engine::CellStaticData& rStaticData, FXMVECTOR vecPosition, FXMVECTOR vecDirection, float fCurrentDeltaRotation);

// Each ship type navigates the graphs of one clearance tier, the smallest that covers its gameplay radius.
inline constexpr int64_t kiNavigationClearanceTierCount = 3;
static_assert(kiNavigationClearanceTierCount >= 2);

// The ship radius tier iTier covers: kfOmniRadius at tier 0, the largest gameplay radius at the last tier, evenly spaced
// between. The blend returns both ends exactly, so the types with those radii map to them (kShipTypeClearanceTiers).
constexpr float ClearanceTierRadius(int64_t iTier)
{
	float fLargestRadius = 0.0f;
	for (const ShipHitboxType& rType : kShipHitboxTypes)
	{
		fLargestRadius = std::max(fLargestRadius, rType.fGameplayRadius);
	}
	float fBlend = static_cast<float>(iTier) / static_cast<float>(kiNavigationClearanceTierCount - 1);
	return kfOmniRadius * (1.0f - fBlend) + fLargestRadius * fBlend;
}

// Engine startup bakes each of the tier's graphs with this clearance.
constexpr float NavigationClearanceMeters(int64_t iTier)
{
	return ClearanceTierRadius(iTier) + kfPushMargin;
}

// An obstacle polygon inflated into a cell must come from an island that cell lists.
static_assert(NavigationClearanceMeters(kiNavigationClearanceTierCount - 1) < engine::kfCellLandMarginMeters);

// Indexed by ShipType: the smallest tier whose clearance covers the type's gameplay radius plus kfPushMargin.
inline constexpr std::array<int64_t, kiShipTypeCount> kShipTypeClearanceTiers = []
{
	std::array<int64_t, kiShipTypeCount> clearanceTiers {};
	for (int64_t i = 0; i < kiShipTypeCount; ++i)
	{
		int64_t& riTier = clearanceTiers.at(static_cast<size_t>(i));
		while (NavigationClearanceMeters(riTier) < kShipHitboxTypes.at(static_cast<size_t>(i)).fGameplayRadius + kfPushMargin)
		{
			++riTier;
		}
	}
	return clearanceTiers;
}();

constexpr int64_t ShipTypeClearanceTier(ShipType eType)
{
	return kShipTypeClearanceTiers.at(std::to_underlying(eType));
}

// One navigation graph per clearance tier at the base flight height; entry i describes CellStaticData::navigationData
// index i. The threshold subtracts ClearanceTierRadius and kfPushMargin in turn because subtracting their sum,
// NavigationClearanceMeters, rounds differently. Debug colors blend yellow at tier 0 to cyan at the last tier so
// overlapping tiers stay distinguishable.
inline constexpr std::array<engine::NavigationGraphSpec, kiNavigationClearanceTierCount> kNavigationGraphSpecs = []
{
	std::array<engine::NavigationGraphSpec, kiNavigationClearanceTierCount> navigationGraphSpecs {};
	for (int64_t iTier = 0; iTier < kiNavigationClearanceTierCount; ++iTier)
	{
		float fTierBlend = static_cast<float>(iTier) / static_cast<float>(kiNavigationClearanceTierCount - 1);
		navigationGraphSpecs.at(static_cast<size_t>(iTier)) = engine::NavigationGraphSpec
		{
			.fThresholdElevation = kfBaseHeightMeters - ClearanceTierRadius(iTier) - kfPushMargin,
			.fClearanceMeters = NavigationClearanceMeters(iTier),
			.fDebugDrawZ = kfBaseHeightMeters,
			.f4DebugColor = {1.0f - fTierBlend, 1.0f, fTierBlend, 1.0f},
		};
	}
	return navigationGraphSpecs;
}();

} // namespace game
