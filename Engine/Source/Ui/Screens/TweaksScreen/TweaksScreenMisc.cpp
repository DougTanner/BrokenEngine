#include "TweaksScreen.h"

#if defined(BT_CLIENT)

#include "Ui/GraphicsSettingsWrappersBase.h"
#include "Ui/MenuUtils.h"
#include "Ui/MiscWrappersBase.h"
#include "TweaksSliderMap.h"

namespace engine
{

const TweaksSliderMapRegistrar gMiscellaneousRegistrar
{
	{"Debug Texture Range", &gMiscDebugTextureLinearRange},
	{"Vertical Field of View", &gFieldOfView},
	{"Camera Tilt", &gCameraTilt},
};

void RenderMiscellaneousSection(TweaksScreen& rScreen)
{
	int64_t iSection = giTweakSectionMiscellaneous;

	rScreen.WrapperSeparatorText("Misc");
	rScreen.WrapperSlider("Debug Texture Range", iSection);

	WrapperToggle("Orthographic Projection", &gOrthographicProjection);
	ImGui::BeginDisabled(gOrthographicProjection.Get<bool>());
	rScreen.WrapperSlider("Vertical Field of View", iSection);
	ImGui::EndDisabled();
	rScreen.WrapperSlider("Camera Tilt", iSection);
}

} // namespace engine

#endif // BT_CLIENT
