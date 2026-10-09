#pragma once

#if defined(BT_CLIENT)

namespace game
{

class ClientSession;

void CommandClientSubscribeAcceptHarnessRig(const nlohmann::json& rParams, nlohmann::json& rResult);
void CommandClientStaleUpdateHarnessRig(const nlohmann::json& rParams, nlohmann::json& rResult);
void CommandClientCancelledSubscriptionHarnessRig(const nlohmann::json& rParams, nlohmann::json& rResult);
void DetachClientSubscriptionHarnessRigs(const ClientSession& rSession);

} // namespace game

#endif // BT_CLIENT
