#pragma once

#include "ExternalHeaders.h"

inline constexpr bool kbInvincibility = true;

#if defined(BT_DEBUG)
inline constexpr bool kbAutoConnect = true;
#elif defined(BT_PROFILE)
inline constexpr bool kbAutoConnect = true;
#elif defined(BT_RELEASE)
inline constexpr bool kbAutoConnect = false;
#endif

#include "Log/LogTypes.h"
#include "EngineSwitches.h"

// Include order is load-bearing: ExternalHeaders.h (top of this file) supplies the DirectXMath/Vulkan/std symbols ShaderLayouts.h uses (the shared layout-header
// chain has no external includes of its own), and Shaders/ShaderLayouts.h must precede Engine.h — this is the repo's sole C++ include site for shaders:: constants; engine TUs consume them only via this PCH
#include "Common.h"
#include "Shaders/ShaderLayouts.h"
#include "Frame/Frame.h"
#include "Engine.h"
#include "Network/GameMessages.h"
