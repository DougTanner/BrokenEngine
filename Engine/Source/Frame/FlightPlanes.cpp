#include "FlightPlanes.h"

#include "Frame/FlightPlaneTable.h"

namespace engine
{

FlightPlanes::FlightPlanes()
: common::Singleton<FlightPlanes>(gpFlightPlanes, this)
, mPlanes(game::kFlightPlanes)
{
	ASSERT(!mPlanes.empty());
	ASSERT(std::ssize(mPlanes) <= kiMaxFlightPlanes);

	float fPreviousHeightMeters = 0.0f;
	for (const FlightPlane& rPlane : mPlanes)
	{
		ASSERT(rPlane.fHeightMeters > fPreviousHeightMeters);
		fPreviousHeightMeters = rPlane.fHeightMeters;
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

std::pair<int64_t, int64_t> FlightPlanes::OccupiedPlanes(float fHeight) const
{
	for (int64_t i = 0; i < Count(); ++i)
	{
		float fPlaneHeight = Plane(i).fHeightMeters;
		if (fHeight <= fPlaneHeight)
		{
			return {i == 0 || fHeight == fPlaneHeight ? i : i - 1, i};
		}
	}

	return {Count() - 1, Count() - 1};
}

} // namespace engine
