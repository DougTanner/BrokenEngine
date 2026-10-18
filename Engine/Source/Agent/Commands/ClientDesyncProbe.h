#pragma once

#if defined(BT_CLIENT)

namespace engine
{

void CommandDesynchronizationProbe(const nlohmann::json& rParameters, nlohmann::json& rResult);

} // namespace engine

#endif // BT_CLIENT
