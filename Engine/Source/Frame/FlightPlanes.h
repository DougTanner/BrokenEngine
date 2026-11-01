#pragma once

namespace engine
{

enum class FlightPlaneFlags : uint8_t
{
	kTerrainCollision = 0x01,
};
using FlightPlaneFlags_t = common::Flags<FlightPlaneFlags>;

struct FlightPlane
{
	float fHeightMeters = 0.0f;
	FlightPlaneFlags_t flags;
};

inline constexpr int64_t kiMaxFlightPlanes = 8;

// The game's flight-plane table is copied at boot on both builds with its highest plane raised by the tallest island
// peak, and stays immutable afterwards, so tick code on worker threads reads it without synchronization. Index 0 is the
// lowest plane; heights ascend.
class FlightPlanes : public common::Singleton<FlightPlanes>
{
public:

	FlightPlanes();

	[[nodiscard]] _Ret_range_(1, kiMaxFlightPlanes) int64_t Count() const;
	[[nodiscard]] const FlightPlane& Plane(int64_t iIndex) const;
	[[nodiscard]] const FlightPlane& Lowest() const;
	[[nodiscard]] const FlightPlane& Highest() const;
	// The index of the plane nearest fHeight; a height equally near two planes takes the lower one.
	[[nodiscard]] int64_t NearestPlaneIndex(float fHeight) const;

private:

	std::vector<FlightPlane> mPlanes;
};

inline FlightPlanes* gpFlightPlanes = nullptr;

} // namespace engine
