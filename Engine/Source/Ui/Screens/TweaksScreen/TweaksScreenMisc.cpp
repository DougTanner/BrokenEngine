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

	rScreen.WrapperSeparatorText("Observer Camera");
	rScreen.WrapperSlider("Observer Pan Speed", gObserverPanSpeed, iSection);
	rScreen.WrapperSlider("Observer Pan Acceleration", gObserverPanAcceleration, iSection);
	rScreen.WrapperSlider("Observer Pan Deceleration", gObserverPanDeceleration, iSection);
	rScreen.WrapperSlider("Observer Edge Scroll Margin", gObserverEdgeScrollMargin, iSection);
	rScreen.WrapperSlider("Observer Zoom Speed Scaling", gObserverZoomSpeedScaling, iSection);
}

} // namespace engine

#endif // BT_CLIENT
