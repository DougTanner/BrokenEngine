#pragma once

#if defined(BT_CLIENT)

namespace game
{

class ClientSession;

void CommandClientFullStateHarnessRig(const nlohmann::json& rParameters, nlohmann::json& rResult);
void DetachClientFullStateHarnessRig(ClientSession& rSession);

} // namespace game

#endif // BT_CLIENT
