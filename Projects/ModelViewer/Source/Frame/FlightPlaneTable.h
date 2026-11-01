#pragma once

namespace game
{

inline constexpr std::array<engine::FlightPlane, 2> kFlightPlanes
{{
	{.fHeightMeters = 5.0f, .flags = engine::FlightPlaneFlags::kTerrainCollision},
	{.fHeightMeters = 5.0f},
}};

} // namespace game
