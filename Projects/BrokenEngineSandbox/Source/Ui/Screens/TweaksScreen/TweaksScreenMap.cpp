#include "Tweaks.h"

#include "Ui/MapWrappers.h"

#if defined(BT_CLIENT)

namespace game
{

void RenderMapSection(engine::TweaksScreen& rScreen)
{
	int64_t iSection = giTweakSectionMap;

	rScreen.WrapperSlider("Map Framing", gMapFraming, iSection);
	rScreen.WrapperSlider("Map Opaque Framing", gMapOpaqueFraming, iSection);
	rScreen.WrapperSlider("Map Blend Seconds", gMapBlendSeconds, iSection);
}

} // namespace game

#endif // BT_CLIENT
