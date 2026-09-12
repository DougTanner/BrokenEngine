#pragma once

#if defined(BT_CLIENT)

namespace engine
{

void CommandPresentationContinuityProbe(const nlohmann::json& rParams, nlohmann::json& rResult);

} // namespace engine

#endif // defined(BT_CLIENT)
