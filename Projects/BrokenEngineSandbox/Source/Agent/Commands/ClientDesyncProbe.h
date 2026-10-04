#pragma once

#if defined(BT_CLIENT)

namespace game
{

void CommandDesynchronizationProbe(const nlohmann::json& rParameters, nlohmann::json& rResult);

} // namespace game

#endif // BT_CLIENT
