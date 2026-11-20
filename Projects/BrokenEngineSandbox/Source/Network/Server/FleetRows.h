#pragma once

#if defined(BT_SERVER)

#include "Fleet.h"

namespace game
{

// The only code that copies fleet values onto ship rows. Each function queues nothing during replay playback, which takes
// every frame change from the recording.

// Completes spawnInfo with the member's fleet values and queues it for this tick's post-tick batch in coordinate.
template <typename SPAWN_INFO>
void QueueFleetSpawn(const engine::ClientGuid& rClientGuid, const Fleet& rFleet, const FleetMember& rMember, engine::GridCoord coordinate, SPAWN_INFO spawnInfo);
// Queues the fleet's wanted cell and pending ticks for every live member's row.
void QueueUpdateFleet(const Fleet& rFleet);
// Queues the squadron's settings for every live member row of that squadron.
void QueueUpdateSquadronOrders(const Fleet& rFleet, ShipType eType, uint8_t uiSquadron);
// Queues staged destruction of a live member's row.
void QueueDestroyShip(const FleetMember& rMember);

} // namespace game

#endif // BT_SERVER
