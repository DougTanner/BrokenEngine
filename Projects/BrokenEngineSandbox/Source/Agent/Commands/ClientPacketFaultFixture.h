#pragma once

#if defined(BT_CLIENT)

namespace game
{

class ClientSession;

void CommandClientPacketFaultFixture(const nlohmann::json& rParameters, nlohmann::json& rResult);
void InjectArmedClientPacketFault();
void ResetClientPacketFaultFixture(const ClientSession& rSession);

} // namespace game

#endif // BT_CLIENT
