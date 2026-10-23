#pragma once

namespace game
{

inline constexpr std::string_view kGameName = "Broken Engine Model Viewer";
inline constexpr int64_t kiGameVersion = 1;

void RenderViewerUi();
struct FrameInterpolate;
void RenderViewerFrame(int64_t iCommandBuffer, const FrameInterpolate& rFrame);

} // namespace game
