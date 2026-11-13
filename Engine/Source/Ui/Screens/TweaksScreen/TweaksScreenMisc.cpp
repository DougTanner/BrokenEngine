#include "TweaksScreen.h"

#if defined(BT_CLIENT)

#include "Ui/GraphicsSettingsWrappersBase.h"
#include "Ui/MenuUtils.h"
#include "Ui/MiscWrappersBase.h"

namespace engine
{

void RenderMiscellaneousSection(TweaksScreen& rScreen)
{
	int64_t iSection = giTweakSectionMiscellaneous;

	rScreen.WrapperSeparatorText("Misc");
	rScreen.WrapperSlider("Misc Debug Texture Linear Range", gMiscDebugTextureLinearRange, iSection);
	rScreen.WrapperSlider("Extra Area", gExtraArea, iSection);

	WrapperToggle("Orthographic Projection", &gOrthographicProjection);
	ImGui::BeginDisabled(gOrthographicProjection.Get<bool>());
	rScreen.WrapperSlider("Field of View", gFieldOfView, iSection);
	rScreen.WrapperSlider("Camera Tilt", gCameraTilt, iSection);
	ImGui::EndDisabled();
}

} // namespace engine

#endif // BT_CLIENT
