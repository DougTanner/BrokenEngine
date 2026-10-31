#pragma once

namespace game
{

// Read directly by engine::FlightPlanes at boot on both builds; index 0 is the lowest plane.
inline constexpr std::array<engine::FlightPlane, 3> kFlightPlanes
{{
	{.fHeightMeters = 2.0f, .flags = engine::FlightPlaneFlags::kTerrainCollision},
	{.fHeightMeters = 25.0f, .flags = engine::FlightPlaneFlags::kTerrainCollision},
	{.fHeightMeters = 100.0f},
}};

} // namespace game
