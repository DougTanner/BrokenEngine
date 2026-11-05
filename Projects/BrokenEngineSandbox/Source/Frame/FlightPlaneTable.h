#pragma once

namespace game
{

// Borrowed by engine::FlightPlanes on both builds; absolute heights ascend from index 0.
inline constexpr std::array<engine::FlightPlane, 2> kFlightPlanes
{{
	{.fHeightMeters = 3.0f, .flags = engine::FlightPlaneFlags::kTerrainCollision},
	{.fHeightMeters = 70.0f},
}};

} // namespace game
