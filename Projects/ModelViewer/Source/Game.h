#pragma once

namespace game
{

inline constexpr std::string_view kGameName = "Broken Engine Model Viewer";
inline constexpr int64_t kiGameVersion = 1;
// The negative alpha makes HdrResolve show the clear ungraded.
inline constexpr VkClearColorValue kVkHdrClearColor {{0.267f, 0.267f, 0.267f, -1.0f}};
inline constexpr float kfUiReferenceHeight = 2'160.0f;

} // namespace game
