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

// The game's flight-plane table, handed over once at boot on both builds and immutable afterwards, so tick code on
// worker threads reads it without synchronization. Index 0 is the lowest plane; heights ascend.
class FlightPlanes : public common::Singleton<FlightPlanes>
{
public:

	explicit FlightPlanes(std::span<const FlightPlane> planes);

	[[nodiscard]] int64_t Count() const;
	[[nodiscard]] const FlightPlane& Plane(int64_t iIndex) const;
	[[nodiscard]] const FlightPlane& Lowest() const;
	[[nodiscard]] const FlightPlane& Highest() const;

private:

	std::span<const FlightPlane> mPlanes;
};

inline FlightPlanes* gpFlightPlanes = nullptr;

} // namespace engine
