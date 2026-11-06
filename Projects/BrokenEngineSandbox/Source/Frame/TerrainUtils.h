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
	XMVECTOR vecArtificialIntelligenceDirection {};
};

// Both read terrain relative to iFlightPlane, the ship's NavigationFlightPlane; on a plane without kTerrainCollision
// they ignore terrain.
AiSteeringResult XM_CALLCONV ComputeArtificialIntelligenceSteering(const engine::CellStaticData& rStaticData, FXMVECTOR vecPosition, FXMVECTOR vecCurrentDirection, FXMVECTOR vecCellCenter, float fDeltaTime, bool bAlternateContour, int64_t iFlightPlane);

float XM_CALLCONV ComputeTerrainAvoidance(const engine::CellStaticData& rStaticData, FXMVECTOR vecPosition, FXMVECTOR vecDirection, float fCurrentDeltaRotation, int64_t iFlightPlane);

// Each ship type navigates the graphs of one clearance tier, the smallest that covers its gameplay radius.
inline constexpr int64_t kiNavigationClearanceTierCount = 3;
static_assert(kiNavigationClearanceTierCount >= 2);

// Indexed by ShipType.
inline constexpr std::array<int64_t, kiShipTypeCount> kShipTypeHighestFlightPlanes {kiFighterHighestFlightPlane, kiOmniHighestFlightPlane, kiBattleshipHighestFlightPlane};

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

// One cell navigation graph; kNavigationGraphs entry i describes CellStaticData::navigationData index i.
struct NavigationGraph
{
	int64_t iClearanceTier = 0;
	int64_t iFlightPlane = 0;
};

// The planes, from plane 0 up, that tier iTier has graphs on: every plane for kiFiringClearanceTier, which firing line of
// sight tests for every shooter; otherwise up to the highest plane of the types mapped to it, none when no type is.
constexpr int64_t ClearanceTierFlightPlaneCount(int64_t iTier)
{
	if (iTier == kiFiringClearanceTier)
	{
		return std::ssize(kFlightPlanes);
	}
	int64_t iPlaneCount = 0;
	for (int64_t i = 0; i < kiShipTypeCount; ++i)
	{
		if (kShipTypeClearanceTiers.at(static_cast<size_t>(i)) == iTier)
		{
			iPlaneCount = std::max(iPlaneCount, kShipTypeHighestFlightPlanes.at(static_cast<size_t>(i)) + 1);
		}
	}
	return iPlaneCount;
}

inline constexpr int64_t kiNavigationGraphCount = []
{
	int64_t iGraphCount = 0;
	for (int64_t iTier = 0; iTier < kiNavigationClearanceTierCount; ++iTier)
	{
		iGraphCount += ClearanceTierFlightPlaneCount(iTier);
	}
	return iGraphCount;
}();

// Tier-major, each tier's planes ascending; engine startup bakes one cell graph per entry.
inline constexpr std::array<NavigationGraph, kiNavigationGraphCount> kNavigationGraphs = []
{
	std::array<NavigationGraph, kiNavigationGraphCount> navigationGraphs {};
	int64_t iGraph = 0;
	for (int64_t iTier = 0; iTier < kiNavigationClearanceTierCount; ++iTier)
	{
		for (int64_t iPlane = 0; iPlane < ClearanceTierFlightPlaneCount(iTier); ++iPlane)
		{
			navigationGraphs.at(static_cast<size_t>(iGraph)) = NavigationGraph {.iClearanceTier = iTier, .iFlightPlane = iPlane};
			++iGraph;
		}
	}
	return navigationGraphs;
}();

// Engine startup indexes the flight-plane table with each entry's plane.
static_assert(std::ranges::all_of(kNavigationGraphs, [](const NavigationGraph& rGraph)
{
	return rGraph.iFlightPlane >= 0 && rGraph.iFlightPlane < std::ssize(kFlightPlanes);
}));

// The index of tier iTier's graph on plane iPlane, or -1 when the tier has none there.
constexpr int64_t NavigationGraphIndex(int64_t iTier, int64_t iPlane)
{
	// std::views::enumerate cannot be used here: MSVC then reports C2872 on iter_move inside std::ranges::sort, between the
	// enumerate iterator's friend and the customization point, in any translation unit that includes this header and sorts.
	for (int64_t i = 0; const NavigationGraph& rGraph : kNavigationGraphs)
	{
		if (rGraph.iClearanceTier == iTier && rGraph.iFlightPlane == iPlane)
		{
			return i;
		}
		++i;
	}
	return -1;
}

#if defined(BT_SERVER)

// The elevation contour engine startup bakes tier iTier's graph on a plane of height fPlaneHeight from. The radius and
// kfPushMargin are subtracted in turn; subtracting their sum, NavigationClearanceMeters, rounds differently.
constexpr float NavigationThresholdElevation(int64_t iTier, float fPlaneHeight)
{
	return fPlaneHeight - ClearanceTierRadius(iTier) - kfPushMargin;
}

#endif // BT_SERVER

} // namespace game
