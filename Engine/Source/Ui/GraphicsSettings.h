#pragma once

#if defined(BT_CLIENT)

namespace engine
{

void SaveGraphicsSettings();
bool LoadGraphicsSettings();
void ResetGraphicsSettings();

} // namespace engine

#endif // defined(BT_CLIENT)
