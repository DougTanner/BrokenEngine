#pragma once

namespace engine
{

struct GridCoord
{
	int32_t iX = 0;
	int32_t iY = 0;

	constexpr bool operator==(const GridCoord&) const = default;

	constexpr uint64_t ToKey() const
	{
		return (static_cast<uint64_t>(static_cast<uint32_t>(iX)) << 32) |
		        static_cast<uint64_t>(static_cast<uint32_t>(iY));
	}

	static constexpr GridCoord FromKey(uint64_t uiKey)
	{
		return {.iX = static_cast<int32_t>(uiKey >> 32), .iY = static_cast<int32_t>(uiKey)};
	}

	common::crc_t Crc() const
	{
		return common::Crc(ToKey());
	}

	void Write(std::ostream& rStream) const
	{
		common::Write(rStream, iX);
		common::Write(rStream, iY);
	}

	void Read(std::istream& rStream)
	{
		common::Read(rStream, iX);
		common::Read(rStream, iY);
	}
};

inline constexpr GridCoord kOriginCoordinate {.iX = 0, .iY = 0};

inline constexpr float kfCellWidth = 900.0f;
inline constexpr float kfCellHeight = 900.0f;

// Per-cell elevation grids use 4 MiB for 1,024 × 1,024 floats at ~0.88-unit spacing.
// CellElevationSampler::Sample quantizes queries to these sub-meter sample centers; IslandTerrain::BuildElevationGrid fills the grid.
inline constexpr int64_t kiElevationGridDimension = 1'024;

inline constexpr float kfBaseAreaMinimumX = -kfCellWidth / 2.0f;
inline constexpr float kfBaseAreaMaximumY = kfCellHeight / 2.0f;
inline constexpr float kfBaseAreaMaximumX = kfCellWidth / 2.0f;
inline constexpr float kfBaseAreaMinimumY = -kfCellHeight / 2.0f;

#if defined(BT_CLIENT)
// Client-only presentation basis: the cell a value's positions are local to, plus the offset from the camera
// cell's origin to that cell's origin. coordinate answers cross-cell queries such as GlobalElevation, which need
// the cell identity rather than the offset. Both cells come from the small active set around the camera, so the
// offset is a small integer multiple of the 900-unit cell size and is therefore exact in float. Exactness rests
// on that multiple alone; a camera-coord fallback can put an active cell more than one cell away per axis.
struct RenderBasis
{
	GridCoord coordinate {};
	XMFLOAT2 f2Offset {};
};

// Move a position that is local to rBasis.coordinate into the camera cell's frame. Height is untouched: the offset
// is planar.
inline XMVECTOR XM_CALLCONV Rebase(const RenderBasis& rBasis, FXMVECTOR vecLocalPosition)
{
	return XMVectorAdd(vecLocalPosition, XMVectorSet(rBasis.f2Offset.x, rBasis.f2Offset.y, 0.0f, 0.0f));
}
#endif

// Pre-mix coordinate.ToKey() into a 32-bit seed where both x and y bits influence the result.
// Required because ToKey() packs x into bits 32-63: a naive `static_cast<uint32_t>(key)` would
// drop x entirely. The 64-bit multiply spreads every input bit through the upper half of the
// product, and the distinct multiplier per use case decorrelates offset and rotation streams.
// RandomEngine's constructor then runs full splitmix64 on the 32-bit seed to produce its state.
inline constexpr uint32_t SeedFromGridCoordinate(GridCoord coordinate, uint64_t uiMultiplier)
{
	return static_cast<uint32_t>((coordinate.ToKey() * uiMultiplier) >> 32);
}

} // namespace engine

template<>
struct std::hash<engine::GridCoord>
{
	std::size_t operator()(const engine::GridCoord& rCoordinate) const noexcept
	{
		return std::hash<uint64_t>{}(rCoordinate.ToKey());
	}
};
