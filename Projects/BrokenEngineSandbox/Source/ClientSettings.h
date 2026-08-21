#pragma once

#if defined(BT_CLIENT)

namespace game
{

void SaveTweaksSettings();
void LoadTweaksSettings();

void SaveClientState();
void LoadClientState();

} // namespace game

#endif // BT_CLIENT
