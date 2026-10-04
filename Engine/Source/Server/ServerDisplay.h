#pragma once

#if defined(BT_SERVER)

namespace engine
{

void ServerUpdateDisplayStatistics();
bool ServerDisplayContentChanged();
void PaintServerDisplay(HWND hWindow);
void HandleServerClick(HWND hWindow, int64_t iX, int64_t iY);

} // namespace engine

#endif // BT_SERVER
