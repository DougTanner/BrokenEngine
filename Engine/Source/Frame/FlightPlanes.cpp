#include "FlightPlanes.h"

#include "Frame/IslandTerrain.h"

namespace engine
{

FlightPlanes::FlightPlanes(std::span<const FlightPlane> planes)
: common::Singleton<FlightPlanes>(gpFlightPlanes, this)
, mPlanes(planes)
{
	// The header elevation ceiling bounds every heightmap texel, so no terrain reaches the highest plane.
	for ([[maybe_unused]] const auto& [rCrc, rTemplate] : gpIslandTerrain->mIslands)
	{
		ASSERT(rTemplate.fWorldElevationMeters + common::kfSeaBottomMeters < Highest().fHeightMeters);
	}
}

int64_t FlightPlanes::Count() const
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

} // namespace engine
