#pragma once

#if defined(BT_CLIENT)

namespace game
{

// describe_scene take-over: validates the params, calls engine::CommandDescribeScene for the engine scene fields,
// then appends fleets, per-collection counts, truncated, and the visible fighter/omni/battleship/blaster units
// ({coord, local} positions plus screen pixels from the rebased position via engine::CameraBase::WorldToScreen;
// blaster rows also carry the client-only wind-trail values). Params
// {"includeUnits"?:true,"maxUnits"?:200,"unitTypes"?:["fighter","omni","battleship","blaster"]}; an absent
// "unitTypes" emits every type. Throws std::runtime_error on bad params, caught by AgentCommandServer::Drain().
void CommandDescribeScene(const nlohmann::json& rParameters, nlohmann::json& rResult);

} // namespace game

#endif // defined(BT_CLIENT)
