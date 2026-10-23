#pragma once

#include "Frame/FrameBase.h"

namespace game
{

struct Frame;
struct FramePostRender;
struct StatusChange;

struct FrameInterpolate
{
	static void GraphicsResources();
	int64_t iFrame = 0;
};

} // namespace game
