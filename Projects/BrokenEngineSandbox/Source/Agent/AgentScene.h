#pragma once

#if defined(BT_CLIENT)

namespace game
{

// describe_scene: structured JSON view of the rendered scene — camera, UI/game state, visible player/spaceship/
// blaster units (world + screen positions via engine::Camera::WorldToScreen; blaster rows also carry the
// client-only wind-trail values), fleets, per-collection counts, and island placements. Client-only. Params
// {"includeUnits"?:true,"maxUnits"?:200,"unitTypes"?:["player","spaceship","blaster"]}; an absent "unitTypes"
// emits every type. Throws std::runtime_error on bad params (trust boundary), caught by
// AgentCommandServer::Drain(). nlohmann::json arrives via the game Pch.
void CommandDescribeScene(const nlohmann::json& rParams, nlohmann::json& rResult);

} // namespace game

#endif // defined(BT_CLIENT)
