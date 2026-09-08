#pragma once

#if defined(BT_CLIENT)

namespace game
{

void CommandDesyncProbe(const nlohmann::json& rParameters, nlohmann::json& rResult);

} // namespace game

#endif // BT_CLIENT
