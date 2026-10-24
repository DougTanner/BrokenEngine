#pragma once

#define GLM_FORCE_SWIZZLE
#define GLM_FORCE_CTOR_INIT
#include <glm/glm.hpp>

namespace glm
{
using float_t = float;
}

#define CPM_ARC_BALL_NS arc_ball
#include "cpm-arc-ball/arc-ball/ArcBall.hpp"
