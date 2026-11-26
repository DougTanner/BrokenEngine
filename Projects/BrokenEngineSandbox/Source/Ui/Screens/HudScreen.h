#pragma once

#if defined(BT_CLIENT)

#include "Ui/MenuUtils.h"

namespace game
{

class HudScreen
{
public:

	void Render();

private:

	void RenderFleetPanel(float fTarget);
	void RenderFocusedOwnedEntityPanel(float fTarget);

	static float PanelWidth();
	engine::SlidePanelState mFleetSlide {};
	engine::SlidePanelState mFocusedOwnedEntitySlide {};
	std::chrono::duration<float> mTimeWantingForceOpen = 0s;
	bool mbPreviousForceOpen = false;
};

} // namespace game

#endif // BT_CLIENT
