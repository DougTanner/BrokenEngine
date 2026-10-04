#pragma once

#if defined(BT_CLIENT)
#include "Ui/MenuUtils.h"
#endif

namespace game
{

class HudScreen
{
public:

	void Render();

private:

	void RenderFleetPanel(float fTarget);
	void RenderFocusedPlayerPanel(float fTarget);

#if defined(BT_CLIENT)
	static float PanelWidth();
	engine::SlidePanelState mFleetSlide {};
	engine::SlidePanelState mFocusedPlayerSlide {};
	std::chrono::duration<float> mTimeWantingForceOpen = 0s;
	bool mbPreviousForceOpen = false;
#endif
};

} // namespace game
