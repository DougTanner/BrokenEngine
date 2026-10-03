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

void TweaksScreenBase::RenderMiscSection()
{
	int64_t iSection = giTweakSectionMisc;

	WrapperSeparatorText("Misc");
	WrapperSlider("Debug Texture Range", iSection);
}

} // namespace engine

#endif // BT_CLIENT
