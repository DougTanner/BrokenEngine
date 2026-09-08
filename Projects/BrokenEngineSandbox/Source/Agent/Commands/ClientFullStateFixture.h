#pragma once

#if defined(BT_CLIENT)

namespace game
{

class ClientSession;

void CommandClientFullStateFixture(const nlohmann::json& rParams, nlohmann::json& rResult);
void DetachClientFullStateFixture(ClientSession& rSession);

} // namespace game

#endif // BT_CLIENT
