#pragma once

#if defined(BT_CLIENT)

namespace game
{

// describe_scene: structured JSON view of the rendered scene — camera, UI/game state, visible player/spaceship
// units (world + screen positions via engine::Camera::WorldToScreen), fleets, per-collection counts, and island
// placements. Client-only. Params {"includeUnits"?:true,"maxUnits"?:200}. Throws std::runtime_error on bad
// params (trust boundary), caught by AgentCommandServer::Drain(). nlohmann::json arrives via the game Pch.
void CommandDescribeScene(const nlohmann::json& rParams, nlohmann::json& rResult);

} // namespace game

#endif // defined(BT_CLIENT)
