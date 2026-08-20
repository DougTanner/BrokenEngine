#include "GraphicsSettingsWrappersBase.h"

namespace engine
{

Wrapper gFullscreen(true);
Wrapper gPresentMode(VK_PRESENT_MODE_FIFO_KHR, std::vector<VkPresentModeKHR> {VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_FIFO_KHR, VK_PRESENT_MODE_FIFO_LATEST_READY_KHR});
Wrapper gMultisampling(false);
Wrapper gSampleCount(VK_SAMPLE_COUNT_4_BIT, std::vector<VkSampleCountFlagBits> {VK_SAMPLE_COUNT_2_BIT, VK_SAMPLE_COUNT_4_BIT, VK_SAMPLE_COUNT_8_BIT, VK_SAMPLE_COUNT_16_BIT, VK_SAMPLE_COUNT_32_BIT, VK_SAMPLE_COUNT_64_BIT});
Wrapper gAnisotropy(true);
Wrapper gMaxAnisotropy(16.0f, 1.0f, 16.0f);
Wrapper gSampleShading(false);
Wrapper gMinSampleShading(0.6f, 0.0f, 1.0f);
Wrapper gMipLodBias(1.0f, 0.0f, 2.0f);
Wrapper gWaterShapeDetail(1.0f / 4.0f, 1.0f / 8.0f, 1.0f / 2.0f); // Fixed WaterDetailTextureSize scale; not exposed or persisted
Wrapper gSmokeSimulationPixels(1.0f, 0.5f, 1.5f);
Wrapper gSmokeSimulationArea(1.2f, 1.0f, 1.5f);
Wrapper gOpaqueUi(false);
Wrapper gUiOpacity(0.5f, 0.0f, 1.0f);
Wrapper gUiFontScale(1.0f, 0.5f, 3.0f);
Wrapper gUiTheme(UiTheme::kNavalSteel, std::vector<UiTheme> {UiTheme::kNavalSteel, UiTheme::kDarkAmber, UiTheme::kMidnightMauve});
Wrapper gSmokeEnabled(true);
Wrapper gWindEnabled(true);
Wrapper gLightingEnabled(true);
Wrapper gSunAngleOverride(1.15f, 0.0f, XM_2PI);

} // namespace engine
