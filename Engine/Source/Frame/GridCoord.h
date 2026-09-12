#pragma once

namespace engine
{

struct GridCoord
{
	int32_t x = 0;
	int32_t y = 0;

	constexpr bool operator==(const GridCoord&) const = default;

	constexpr uint64_t ToKey() const
	{
		return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32) |
		        static_cast<uint64_t>(static_cast<uint32_t>(y));
	}

	static constexpr GridCoord FromKey(uint64_t uiKey)
	{
		return {static_cast<int32_t>(uiKey >> 32), static_cast<int32_t>(uiKey)};
	}

	common::crc_t Crc() const
	{
		return common::Crc(ToKey());
	}

	void Write(std::ostream& rStream) const
	{
		common::Write(rStream, x);
		common::Write(rStream, y);
	}

	void Read(std::istream& rStream)
	{
		common::Read(rStream, x);
		common::Read(rStream, y);
	}
};

inline constexpr GridCoord kOriginCoord {0, 0};

inline constexpr float kfCellWidth = 900.0f;
inline constexpr float kfCellHeight = 900.0f;

// Per-cell elevation grid resolution. 1024 × 1024 floats = 4 MB/cell at ~0.88-unit
// spacing across kfCellWidth — sub-meter, fine enough that quantizing FrameElevation
// queries to grid-cell centers is gameplay-invisible. See IslandTerrain::BuildElevationGrid.
inline constexpr int64_t kiElevationGridDim = 1024;

inline constexpr float kfBaseAreaMinX = -kfCellWidth / 2.0f;
inline constexpr float kfBaseAreaMaxY = kfCellHeight / 2.0f;
inline constexpr float kfBaseAreaMaxX = kfCellWidth / 2.0f;
inline constexpr float kfBaseAreaMinY = -kfCellHeight / 2.0f;

#if defined(BT_CLIENT)
// Client-only presentation basis: the cell a value's positions are local to, plus the offset from the camera
// cell's origin to that cell's origin. coord answers cross-cell queries such as GlobalElevation, which need
// the cell identity rather than the offset. Both cells come from the small active set around the camera, so the
// offset is a small integer multiple of the 900-unit cell size and is therefore exact in float. Exactness rests
// on that multiple alone; a camera-coord fallback can put an active cell more than one cell away per axis.
struct RenderBasis
{
	GridCoord coord {};
	XMFLOAT2 f2Offset {};
};

// Move a position that is local to rBasis.coord into the camera cell's frame. Height is untouched: the offset
// is planar.
inline XMVECTOR XM_CALLCONV Rebase(const RenderBasis& rBasis, FXMVECTOR vecLocalPosition)
{
	return XMVectorAdd(vecLocalPosition, XMVectorSet(rBasis.f2Offset.x, rBasis.f2Offset.y, 0.0f, 0.0f));
}
#endif

// Pre-mix coord.ToKey() into a 32-bit seed where both x and y bits influence the result.
// Required because ToKey() packs x into bits 32-63: a naive `static_cast<uint32_t>(key)` would
// drop x entirely. The 64-bit multiply spreads every input bit through the upper half of the
// product, and the distinct multiplier per use case decorrelates offset and rotation streams.
// RandomEngine's constructor then runs full splitmix64 on the 32-bit seed to produce its state.
inline constexpr uint32_t SeedFromGridCoord(GridCoord coord, uint64_t uiMultiplier)
{
	return static_cast<uint32_t>((coord.ToKey() * uiMultiplier) >> 32);
}

} // namespace engine

template<>
struct std::hash<engine::GridCoord>
{
	std::size_t operator()(const engine::GridCoord& rCoord) const noexcept
	{
		return std::hash<uint64_t>{}(rCoord.ToKey());
	}
};
