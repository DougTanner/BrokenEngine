#pragma once

#if defined(BT_SERVER)

namespace engine
{

void ServerUpdateDisplayStats();
bool ServerDisplayContentChanged();
void PaintServerDisplay(HWND hWnd);
void HandleServerClick(HWND hWnd, int64_t iX, int64_t iY);

} // namespace engine

#endif // BT_SERVER
