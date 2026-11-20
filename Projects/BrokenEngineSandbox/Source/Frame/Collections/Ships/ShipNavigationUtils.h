#pragma once

#include "Frame/GridCoord.h"

#include "Frame/Collections/Ships/ShipTypes.h"

namespace engine
{

struct CellStaticData;

} // namespace engine

namespace game
{

struct Frame;
class NavRectangleBoard;

// Fleet navigation: roaming, cross-cell movement, island destinations, following the row's own flagship, and a fleet
// Omni's patrol around its flagship while that flagship is in the cell.
void XM_CALLCONV ComputeNavigation(Frame& rFrame, const Frame& rPreviousFrame, const engine::CellStaticData& rStaticData, ShipLocator self, FXMVECTOR vecPosition, NavRectangleBoard& rNavRectangleBoard, engine::GridCoord fleetWantedCoordinate, int64_t iPendingFleetWantedCoordinateTicks, ShipOrders_t orders, int64_t& riNavigationDirection, int64_t& riNavigationWaypointIndex, XMVECTOR& rVecAiDirection, XMVECTOR& rVecIslandDestination, std::chrono::duration<float>& rCellChangeTimer);

} // namespace game
