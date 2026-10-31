#pragma once

namespace game
{

inline constexpr std::array<engine::FlightPlane, 3> kFlightPlanes
{{
	{.fHeightMeters = 15.0f, .flags = engine::FlightPlaneFlags::kTerrainCollision},
	{.fHeightMeters = 40.0f, .flags = engine::FlightPlaneFlags::kTerrainCollision},
	{.fHeightMeters = 65.0f},
}};

} // namespace game
