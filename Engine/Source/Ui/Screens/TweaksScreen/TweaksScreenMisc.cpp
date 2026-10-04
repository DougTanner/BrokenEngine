#include "TweaksScreenBase.h"

#if defined(BT_CLIENT)

#include "Ui/MiscWrappersBase.h"
#include "TweaksSliderMap.h"

namespace engine
{

const TweaksSliderMapRegistrar gMiscellaneousRegistrar
{
	{"Debug Texture Range", &gMiscDebugTextureLinearRange},
};

void RenderMiscellaneousSection(TweaksScreenBase& rScreen)
{
	int64_t iSection = giTweakSectionMiscellaneous;

	rScreen.WrapperSeparatorText("Misc");
	rScreen.WrapperSlider("Debug Texture Range", iSection);
}

} // namespace engine

#endif // BT_CLIENT
