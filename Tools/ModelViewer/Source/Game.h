#pragma once

namespace game
{

inline constexpr std::string_view kGameName = "Broken Engine Model Viewer";
inline constexpr int64_t kiGameVersion = 1;
inline constexpr VkClearColorValue kVkHdrClearColor {{0.153f, 0.153f, 0.153f, 1.0f}};
inline constexpr float kfUiReferenceHeight = 2'160.0f;

void RenderViewerUi();

} // namespace game
