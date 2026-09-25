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
	float mfTimeWantingForceOpen = 0.0f;
	bool mbPreviousForceOpen = false;
#endif
};

} // namespace game
