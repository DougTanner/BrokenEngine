#pragma once

namespace game
{

// Handed to engine::FlightPlanes at boot on both builds; index 0 is the lowest plane.
inline constexpr std::array<engine::FlightPlane, 3> kFlightPlanes
{{
	{.fHeightMeters = 6.0f, .flags = engine::FlightPlaneFlags::kTerrainCollision},
	{.fHeightMeters = 50.0f, .flags = engine::FlightPlaneFlags::kTerrainCollision},
	{.fHeightMeters = 100.0f},
}};

// Every height is positive and above its predecessor.
constexpr bool FlightPlanesAscend(std::span<const engine::FlightPlane> planes)
{
	float fPreviousHeightMeters = 0.0f;
	for (const engine::FlightPlane& rPlane : planes)
	{
		if (rPlane.fHeightMeters <= fPreviousHeightMeters)
		{
			return false;
		}
		fPreviousHeightMeters = rPlane.fHeightMeters;
	}
	return true;
}

static_assert(std::ssize(kFlightPlanes) >= 1);
static_assert(std::ssize(kFlightPlanes) <= engine::kiMaxFlightPlanes);
static_assert(FlightPlanesAscend(kFlightPlanes));

} // namespace game
