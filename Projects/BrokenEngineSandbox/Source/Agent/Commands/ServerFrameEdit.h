#pragma once

#if defined(BT_SERVER)

namespace game
{

void CommandEditFrame(const nlohmann::json& rParameters, nlohmann::json& rResult);

} // namespace game

#endif // BT_SERVER
