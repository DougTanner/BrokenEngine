#pragma once

#include "Frame/FrameUtils.h"

namespace engine
{

struct CellStaticData;

SegmentHit XM_CALLCONV TracePointAgainstTerrain(const CellStaticData& rStaticData, FXMVECTOR vecStartPosition, FXMVECTOR vecEndPosition);

enum class DiscTerrainFlags : uint8_t
{
	kContact      = 1 << 0,
	kStartOverlap = 1 << 1,
};

// vecPosition is the resolved center, with the end position's Z and W = 1. vecNormal (Z = 0, W = 0) is the first
// contact's horizontal normal and holds only with kContact. kStartOverlap is set whenever the start overlapped
// blocking terrain; fStartOverlapDistance is how far the navigation snap moved it, 0 when the snap left it in place.
struct DiscTerrainResult
{
	XMVECTOR vecPosition {};
	XMVECTOR vecNormal {};
	common::Flags<DiscTerrainFlags> flags;
	float fStartOverlapDistance = 0.0f;
};

// Sim path. Sweeps a horizontal disc of fRadius from vecStart to vecEnd over the cell's own elevation grid, where a
// grid cell at or above fThresholdHeight blocks, and stops it at first contact; with bSlide the remaining motion,
// less its component into the contact normal, is swept once more. An overlapped start is first moved by
// NavQuerySnapToNavigable, the end moving with it, and the cells still overlapping the resolved start are left out
// of both sweeps. Reads only rStaticData, never mCells.
DiscTerrainResult XM_CALLCONV ResolveDiscAgainstTerrain(const CellStaticData& rStaticData, FXMVECTOR vecStart, FXMVECTOR vecEnd, float fRadius, float fThresholdHeight, bool bSlide);

} // namespace engine
