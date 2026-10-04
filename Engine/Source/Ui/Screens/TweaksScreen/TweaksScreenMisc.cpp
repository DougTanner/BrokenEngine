#include "TweaksScreenBase.h"

#if defined(BT_CLIENT)

#include "Ui/MiscWrappersBase.h"
#include "TweaksSliderMap.h"

namespace engine
{

const TweaksSliderMapRegistrar gMiscRegistrar
{
	{"Debug Texture Range", &gMiscDebugTextureLinearRange},
};

void RenderMiscSection(TweaksScreenBase& rScreen)
{
	int64_t iSection = giTweakSectionMisc;

	rScreen.WrapperSeparatorText("Misc");
	rScreen.WrapperSlider("Debug Texture Range", iSection);
}

} // namespace engine

#endif // BT_CLIENT
