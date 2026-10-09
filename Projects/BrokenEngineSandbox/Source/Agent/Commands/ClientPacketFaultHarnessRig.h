#pragma once

#if defined(BT_CLIENT)

namespace game
{

class ClientSession;

void CommandClientPacketFaultHarnessRig(const nlohmann::json& rParameters, nlohmann::json& rResult);
void InjectArmedClientPacketFault();
void ResetClientPacketFaultHarnessRig(const ClientSession& rSession);

} // namespace game

#endif // BT_CLIENT
