#include "FlightPlanes.h"

#include "Frame/IslandTerrain.h"

#include "Frame/FlightPlaneTable.h"

namespace engine
{

FlightPlanes::FlightPlanes()
: common::Singleton<FlightPlanes>(gpFlightPlanes, this)
, mPlanes(game::kFlightPlanes)
{
	// Every height is positive and above its predecessor. These run first because the terrain check below reads
	// Highest(), which needs a non-empty table.
	ASSERT(!mPlanes.empty());
	ASSERT(std::ssize(mPlanes) <= kiMaxFlightPlanes);
	float fPreviousHeightMeters = 0.0f;
	for (const FlightPlane& rPlane : mPlanes)
	{
		ASSERT(rPlane.fHeightMeters > fPreviousHeightMeters);
		fPreviousHeightMeters = rPlane.fHeightMeters;
	}

	// The header elevation ceiling bounds every heightmap texel, so no terrain reaches the highest plane.
	for ([[maybe_unused]] const auto& [rCrc, rTemplate] : gpIslandTerrain->mIslands)
	{
		ASSERT(rTemplate.fWorldElevationMeters + common::kfSeaBottomMeters < Highest().fHeightMeters);
	}
}

_Ret_range_(1, kiMaxFlightPlanes) int64_t FlightPlanes::Count() const
{
	return std::ssize(mPlanes);
}

const FlightPlane& FlightPlanes::Plane(int64_t iIndex) const
{
	return mPlanes[static_cast<size_t>(iIndex)];
}

const FlightPlane& FlightPlanes::Lowest() const
{
	return mPlanes.front();
}

const FlightPlane& FlightPlanes::Highest() const
{
	return mPlanes.back();
}

int64_t FlightPlanes::NearestPlaneIndex(float fHeight) const
{
	int64_t iNearest = 0;
	float fNearestDistance = std::abs(fHeight - Plane(0).fHeightMeters);
	for (int64_t i = 1; i < Count(); ++i)
	{
		float fDistance = std::abs(fHeight - Plane(i).fHeightMeters);
		if (fDistance < fNearestDistance)
		{
			iNearest = i;
			fNearestDistance = fDistance;
		}
	}

	return iNearest;
}

} // namespace engine
