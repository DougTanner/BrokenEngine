#pragma once

#include "Frame/FrameBase.h"

namespace game
{

inline constexpr float kfBaseHeightMeters = 2.0f;

struct Frame;
struct FramePostRender;
struct StatusChange;

struct FrameInterpolate
{
	static void GraphicsResources();
	int64_t iFrame = 0;
};

} // namespace game
