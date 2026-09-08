#pragma once

#if defined(BT_CLIENT)

namespace game
{

class ClientSession;

void CommandClientPacketFaultFixture(const nlohmann::json& rParams, nlohmann::json& rResult);
void InjectArmedClientPacketFault();
void ResetClientPacketFaultFixture(ClientSession& rSession);
void DetachClientPacketFaultFixture(ClientSession& rSession);

} // namespace game

#endif // BT_CLIENT
