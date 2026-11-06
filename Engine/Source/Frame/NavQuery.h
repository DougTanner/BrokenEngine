#pragma once

namespace engine
{

struct NavData;

// An obstacle for one query, CCW in cell-local meters. An edge is tested against pf2Box; pf2Corners, slightly wider, are
// the A* nodes, so a leg round the box does not touch it.
struct NavRectangle
{
	XMFLOAT2 pf2Box[4] {};
	XMFLOAT2 pf2Corners[4] {};
	float fCrossingPenalty = 0.0f;
};

// A rectangle adds its fCrossingPenalty to the cost of every edge touching it and forbids none, so the search still takes
// the edge when no cheaper route exists. The caller has already left out any rectangle holding the start or destination.
XMVECTOR XM_CALLCONV NavQueryDirection(FXMVECTOR vecPosition, FXMVECTOR vecDestination, const NavData& rNavData, std::span<const NavRectangle> rectangles, XMVECTOR& rVecNextWaypoint);
XMVECTOR XM_CALLCONV NavQuerySnapToNavigable(FXMVECTOR vecPosition, const NavData& rNavData);

// Is the position inside a no-navigation polygon? Reads x/y only.
bool XM_CALLCONV NavQueryPointBlocked(FXMVECTOR vecPosition, const NavData& rNavData);
// Does the segment cross a no-navigation polygon edge? Reads x/y only; false on an empty graph.
bool XM_CALLCONV NavQuerySegmentBlocked(FXMVECTOR vecStart, FXMVECTOR vecEnd, const NavData& rNavData);

#if defined(BT_SERVER)
// Can the position, snapped clear of polygons, reach an open stretch of the cell edge shared with kCellDirections entry
// iDirection? The graph's clearance is already in its polygons, so an unblocked edge point is passable for that graph's
// tier. True on an empty graph. A found path whose first waypoint coincides with the start reads as a miss. Logs nothing:
// a miss here is an expected answer.
bool XM_CALLCONV NavQueryEdgeReachable(FXMVECTOR vecPosition, int64_t iDirection, const NavData& rNavData);
#endif // BT_SERVER

} // namespace engine
