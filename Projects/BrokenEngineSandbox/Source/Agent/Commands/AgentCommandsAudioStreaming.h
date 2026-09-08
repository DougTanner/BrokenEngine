#pragma once

#if defined(BT_CLIENT)

namespace game
{

void CommandAudioStreamingFixture(const nlohmann::json& rParams, nlohmann::json& rResult);

} // namespace game

#endif // defined(BT_CLIENT)
