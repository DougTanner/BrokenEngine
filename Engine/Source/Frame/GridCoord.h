#pragma once

namespace engine
{

// Axial coordinates of a hexagonal cell: kCellDirections lists the q and r delta to each edge neighbour.
struct GridCoord
{
	int32_t iQ = 0;
	int32_t iR = 0;

	constexpr bool operator==(const GridCoord&) const = default;

	constexpr uint64_t ToKey() const
	{
		return (static_cast<uint64_t>(static_cast<uint32_t>(iQ)) << 32) |
		        static_cast<uint64_t>(static_cast<uint32_t>(iR));
	}

	static constexpr GridCoord FromKey(uint64_t uiKey)
	{
		return {.iQ = static_cast<int32_t>(uiKey >> 32), .iR = static_cast<int32_t>(uiKey)};
	}

	common::crc_t Crc() const
	{
		return common::Crc(ToKey());
	}

	void Write(std::ostream& rStream) const
	{
		common::Write(rStream, iQ);
		common::Write(rStream, iR);
	}

	void Read(std::istream& rStream)
	{
		common::Read(rStream, iQ);
		common::Read(rStream, iR);
	}
};

inline constexpr GridCoord kOriginCoordinate {.iQ = 0, .iR = 0};

inline constexpr float kfCellApothem = 450.0f;
inline constexpr common::HexDirections kCellDirections = common::MakeHexDirections(kfCellApothem);

// Half extents of the cell's bounding rectangle, which the elevation, collision, and pusher grids span.
inline constexpr float kfCellHalfWidth = kCellDirections.f2HalfExtents.x;
inline constexpr float kfCellHalfHeight = kCellDirections.f2HalfExtents.y;

// Per-cell elevation grids use 4 MiB for 1,024 × 1,024 floats spanning the bounding rectangle, at ~1.01-unit spacing in
// x and ~0.88-unit spacing in y. CellElevationSampler::Sample quantizes queries to these sample centers;
// IslandTerrain::BuildElevationGrid fills the grid.
inline constexpr int64_t kiElevationGridDimension = 1'024;

#if defined(BT_CLIENT)
// Client-only presentation basis: the cell a value's positions are local to, plus the offset from the camera
// cell's origin to that cell's origin. coordinate answers cross-cell queries such as GlobalElevation, which need
// the cell identity rather than the offset. The offset is the center offset of the two cells' axial delta, computed
// from the float lattice basis, so it is deterministic for a given pair of cells.
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

// Pre-mix coordinate.ToKey() into a 32-bit seed where both q and r bits influence the result.
// Required because ToKey() packs q into bits 32-63: a naive `static_cast<uint32_t>(key)` would
// drop q entirely. The 64-bit multiply spreads every input bit through the upper half of the
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
