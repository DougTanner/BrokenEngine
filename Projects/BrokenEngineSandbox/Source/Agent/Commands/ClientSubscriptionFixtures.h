#pragma once

#if defined(BT_CLIENT)

namespace game
{

class ClientSession;

void CommandClientSubscribeAcceptFixture(const nlohmann::json& rParams, nlohmann::json& rResult);
void CommandClientStaleUpdateFixture(const nlohmann::json& rParams, nlohmann::json& rResult);
void CommandClientCancelledSubscriptionFixture(const nlohmann::json& rParams, nlohmann::json& rResult);
void DetachClientSubscriptionFixtures(ClientSession& rSession);

} // namespace game

#endif // BT_CLIENT
