#pragma once

#include "WrapperBase.h"

namespace engine
{

// UI color theme, selectable in the game's settings menu. Values are persisted (GameSettings.bin) — append only.
enum class UiTheme : uint8_t
{
	kNavalSteel = 0,
	kDarkAmber,
	kMidnightMauve,

	kCount,
};

inline Wrapper gFullscreen(true);
inline Wrapper gPresentMode(VK_PRESENT_MODE_FIFO_KHR, std::vector<VkPresentModeKHR> {VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_FIFO_KHR, VK_PRESENT_MODE_FIFO_LATEST_READY_KHR});
inline Wrapper gMultisampling(true);
inline Wrapper gSampleCount(VK_SAMPLE_COUNT_2_BIT, std::vector<VkSampleCountFlagBits> {VK_SAMPLE_COUNT_2_BIT, VK_SAMPLE_COUNT_4_BIT, VK_SAMPLE_COUNT_8_BIT, VK_SAMPLE_COUNT_16_BIT, VK_SAMPLE_COUNT_32_BIT, VK_SAMPLE_COUNT_64_BIT});
inline Wrapper gAnisotropy(true);
inline Wrapper gMaximumAnisotropy(16.0f, 1.0f, 16.0f);
inline Wrapper gSampleShading(true);
inline Wrapper gMinimumSampleShading(1.0f, 0.0f, 1.0f);
inline Wrapper gOrthographicProjection(false);
inline Wrapper gFieldOfView(15.0f, 5.0f, 25.0f); // True vertical angle in degrees
inline Wrapper gCameraTilt(20.0f, 0.0f, 45.0f); // Rotation about world X in degrees; positive looks north
inline Wrapper gWaterShapeDetail(1.0f / 4.0f, 1.0f / 8.0f, 1.0f / 2.0f); // Fixed WaterDetailTextureSize scale; not exposed or persisted
inline Wrapper gWaterFoamSystemEnabled(true);
inline Wrapper gBreakRepetition(true);
inline Wrapper gSmokeSimulationPixels(1.0f, 0.5f, 1.5f);
inline Wrapper gSmokeSimulationArea(1.2f, 1.0f, 1.5f);
inline Wrapper gOpaqueUi(false);
inline Wrapper gUiOpacity(0.5f, 0.0f, 1.0f);
inline Wrapper gUiFontScale(1.0f, 0.5f, 3.0f);
inline Wrapper gUiTheme(UiTheme::kNavalSteel, std::vector<UiTheme> {UiTheme::kNavalSteel, UiTheme::kDarkAmber, UiTheme::kMidnightMauve});
inline Wrapper gSmokeEnabled(true);
inline Wrapper gSmokeLayerStagger(true);
inline Wrapper gWindEnabled(true);
inline Wrapper gLightingEnabled(true);
inline Wrapper gSunAngleOverride(1.15f, 0.0f, XM_2PI);

// Clamps because the backing value crosses a trust boundary (persisted GameSettings.bin) and Wrapper::Set<T> soft-falls without clamping
inline UiTheme GetUiTheme()
{
	return static_cast<UiTheme>(std::clamp(gUiTheme.Get<int64_t>(), 0i64, static_cast<int64_t>(UiTheme::kCount) - 1));
}

} // namespace engine
