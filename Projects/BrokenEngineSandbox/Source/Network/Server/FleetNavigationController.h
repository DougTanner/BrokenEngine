#pragma once

#if defined(BT_SERVER)

#include "Fleet.h"

namespace game
{

class FleetNavigationController
{
public:

	void TickFleetTimers(std::unordered_map<engine::ClientGuid, std::vector<Fleet>>& rFleets, common::RandomEngine& rRandom);
};

} // namespace game

#endif // BT_SERVER
