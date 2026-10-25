#pragma once

#include "ExternalHeaders.h"

#if defined(BT_DEBUG)
inline constexpr bool kbAutoConnect = true;
#elif defined(BT_PROFILE)
inline constexpr bool kbAutoConnect = true;
#elif defined(BT_RELEASE)
inline constexpr bool kbAutoConnect = false;
#endif

inline constexpr bool kbAudioStreamingHarnessRig = true; // Compiles the agent audio streaming harness rig's pack-loading hooks (Debug clients)
inline constexpr bool kbProfileScreens = true; // Compiles the CPU, GPU, and Network profile overlay screens

#include "Log/LogTypes.h"
#include "EngineSwitches.h"

// Include order is load-bearing: ExternalHeaders.h (top of this file) supplies the DirectXMath/Vulkan/std symbols ShaderLayouts.h uses (the shared layout-header
// chain has no external includes of its own), and Shaders/ShaderLayouts.h must precede Engine.h — this is the repo's sole C++ include site for shaders:: constants; engine TUs consume them only via this PCH
#include "Common.h"
#include "Shaders/ShaderLayouts.h"
#include "Frame/Frame.h"
#include "Engine.h"
#include "GameBase.h"

#if defined(BT_CLIENT)
// Generic client agent commands: declared after GameBase.h because the interface names its UiState and GameFlags types.
#include "Agent/AgentCommandsClientGeneric.h"
#endif // BT_CLIENT

#if defined(BT_SERVER)
#include "Agent/AgentCommandsServerGeneric.h"
#endif // BT_SERVER
#include "Network/GameMessages.h"
