#pragma once

namespace game
{

inline constexpr std::array<engine::FlightPlane, 3> kFlightPlanes
{{
	{.fHeightMeters = 6.0f, .flags = engine::FlightPlaneFlags::kTerrainCollision},
	{.fHeightMeters = 50.0f, .flags = engine::FlightPlaneFlags::kTerrainCollision},
	{.fHeightMeters = 100.0f},
}};

} // namespace game
