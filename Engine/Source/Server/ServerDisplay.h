#pragma once

#if defined(BT_SERVER)

namespace engine
{

void ServerUpdateDisplayStatistics();
bool ServerDisplayContentChanged();
void PaintServerDisplay(HWND hwnd);
void HandleServerClick(HWND hwnd, int64_t iX, int64_t iY);

} // namespace engine

#endif // BT_SERVER
