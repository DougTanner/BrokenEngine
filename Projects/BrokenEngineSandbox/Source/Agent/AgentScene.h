#pragma once

#if defined(BT_CLIENT)

namespace game
{

// describe_scene: structured JSON view of the rendered scene — camera, UI/game state, visible player/spaceship/
// blaster units ({coord, local} positions plus screen pixels from the rebased position via
// engine::Camera::WorldToScreen; blaster rows also carry the
// client-only wind-trail values), fleets, per-collection counts, and island placements. Params
// {"includeUnits"?:true,"maxUnits"?:200,"unitTypes"?:["player","spaceship","blaster"]}; an absent "unitTypes"
// emits every type. Throws std::runtime_error on bad params, caught by
// AgentCommandServer::Drain().
void CommandDescribeScene(const nlohmann::json& rParameters, nlohmann::json& rResult);

} // namespace game

#endif // defined(BT_CLIENT)
