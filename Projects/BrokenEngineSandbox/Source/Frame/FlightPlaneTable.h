#pragma once

namespace game
{

// Copied by engine::FlightPlanes at boot on both builds; index 0 is the lowest plane. The highest entry's height
// is its clearance above the tallest island peak, which engine::FlightPlanes resolves at boot.
inline constexpr std::array<engine::FlightPlane, 2> kFlightPlanes
{{
	{.fHeightMeters = 5.0f, .flags = engine::FlightPlaneFlags::kTerrainCollision},
	{.fHeightMeters = 5.0f},
}};

} // namespace game
