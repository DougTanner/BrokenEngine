#include "Tweaks.h"

#include "Ui/Screens/TweaksScreen/TweaksSliderMap.h"

#include "Ui/MapWrappers.h"

#if defined(BT_CLIENT)

namespace game
{

const engine::TweaksSliderMapRegistrar gMapRegistrar
{
	{"Map Framing", &gMapFraming},
	{"Map Opaque Framing", &gMapOpaqueFraming},
	{"Map Blend Seconds", &gMapBlendSeconds},
};

void RenderMapSection(engine::TweaksScreen& rScreen)
{
	int64_t iSection = giTweakSectionMap;

	rScreen.WrapperSlider("Map Framing", iSection);
	rScreen.WrapperSlider("Map Opaque Framing", iSection);
	rScreen.WrapperSlider("Map Blend Seconds", iSection);
}

} // namespace game

#endif // BT_CLIENT
