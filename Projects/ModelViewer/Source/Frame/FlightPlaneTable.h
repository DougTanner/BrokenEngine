#pragma once

namespace game
{

inline constexpr std::array<engine::FlightPlane, 3> kFlightPlanes
{{
	{.fHeightMeters = 2.0f, .flags = engine::FlightPlaneFlags::kTerrainCollision},
	{.fHeightMeters = 25.0f, .flags = engine::FlightPlaneFlags::kTerrainCollision},
	{.fHeightMeters = 100.0f},
}};

} // namespace game
