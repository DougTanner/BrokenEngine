#pragma once

#if defined(BT_SERVER)

#include "Fleet.h"

namespace game
{

struct PendingFlagshipUpdate
{
	engine::ClientGuid clientGuid {};
	FleetGuid fleetGuid {};
	engine::GridCoord newWantedCoordinate {};
	uint8_t uiPendingFleetWantedCoordinateTicks = 0;
};

class FleetNavigationController
{
public:

	void TickFleetTimers(std::unordered_map<engine::ClientGuid, std::vector<Fleet>>& rFleets, common::RandomEngine& rRandom);
	void ProcessFlagshipUpdates(const std::unordered_map<engine::ClientGuid, std::vector<Fleet>>& rFleets);


	void ShiftFlagshipAfterDeath(const engine::ClientGuid& rGuid, Fleet& rFleet);

	std::vector<PendingFlagshipUpdate> mPendingFlagshipUpdates;
};

} // namespace game

#endif // BT_SERVER
