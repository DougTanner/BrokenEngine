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
	rScreen.WrapperSlider("Misc Debug Texture Linear Range", iSection, 2.0f, "Debug Texture Range");

	WrapperToggle("Orthographic Projection", &gOrthographicProjection);
	ImGui::BeginDisabled(gOrthographicProjection.Get<bool>());
	rScreen.WrapperSlider("Field of View", iSection, 2.0f, "Vertical Field of View");
	rScreen.WrapperSlider("Camera Tilt", iSection);
	ImGui::EndDisabled();
}

} // namespace engine

#endif // BT_CLIENT
