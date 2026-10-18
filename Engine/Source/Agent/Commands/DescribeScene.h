#pragma once

#if defined(BT_CLIENT)

namespace engine
{

// describe_scene: structured JSON view of the engine scene state — camera, UI/game state, tick, clientGridCoord,
// subscribed coords, per-cell reconciliation state (cellStates), and island placements. Reads no parameter. A game
// project that takes the command over validates its own parameters, calls this, then appends its own fields.
void CommandDescribeScene(const nlohmann::json& rParameters, nlohmann::json& rResult);

} // namespace engine

#endif // defined(BT_CLIENT)
