#pragma once

inline constexpr bool kbDesynchronizationRecovery = true;
inline constexpr bool kbDesynchronizationDebugFrames = false; // Manual client/server diagnostic switch; both targets must match
inline constexpr bool kbFrameDispatch = true;
inline constexpr bool kbLogging = true;
inline constexpr bool kbRenderThread = true;

inline constexpr bool kbAlsoLogToPrintf = false;
inline constexpr bool kbGraphicsProcessingUnitAssistedValidation = false;
inline constexpr bool kbRecording = false;
inline constexpr bool kbRenderDoc = true; // Compile-time gate for RenderDoc in-app capture; runtime activation still requires --renderdoc
inline constexpr bool kbScreenshots = true;
inline constexpr bool kbAgent = true; // Compile-time gate for the agent command layer; runtime activation still requires --agent-port
inline constexpr bool kbVulkanPipelineCache = true;
inline constexpr bool kbVulkanWireframe = false;
inline constexpr bool kbFramebufferClearColor = kbVulkanWireframe;

#if defined(BT_SERVER)
inline constexpr bool kbSingleInstance = true;
#else
inline constexpr bool kbSingleInstance = false;
#endif

#if defined(BT_DEBUG)
inline constexpr const char* kpcBuildConfigurationName = "Debug";
inline constexpr bool kbMimallocDiagnostics = true;

inline constexpr bool kbDebugBreak = true;
inline constexpr bool kbDebugInput = true;
inline constexpr bool kbDebugNavigationCrossingCheck = false;
inline constexpr bool kbDebugRender = true;
inline constexpr bool kbDxDiag = true;
inline constexpr bool kbFreeCamera = true;
inline constexpr bool kbProfiling = true;
inline constexpr bool kbProfilingDump = false;
inline constexpr bool kbProfilingFrameSpike = false;
inline constexpr bool kbReplayFullFrames = true;
inline constexpr bool kbVulkanDebugLayers = true;
inline constexpr bool kbRandomlyInvalidatePhysicallyBasedRenderingCubemapCache = true;
inline constexpr bool kbShowProfileTextByDefault = false;
#elif defined(BT_PROFILE)
inline constexpr const char* kpcBuildConfigurationName = "Profile";
inline constexpr bool kbMimallocDiagnostics = false;

inline constexpr bool kbDebugBreak = false;
inline constexpr bool kbDebugInput = false;
inline constexpr bool kbDebugNavigationCrossingCheck = false;
inline constexpr bool kbDebugRender = false;
inline constexpr bool kbDxDiag = false;
inline constexpr bool kbFreeCamera = true;
inline constexpr bool kbProfiling = true;
inline constexpr bool kbProfilingDump = true;
inline constexpr bool kbProfilingFrameSpike = true;
inline constexpr bool kbReplayFullFrames = false;
inline constexpr bool kbVulkanDebugLayers = false;
inline constexpr bool kbRandomlyInvalidatePhysicallyBasedRenderingCubemapCache = false;
inline constexpr bool kbShowProfileTextByDefault = true;
#elif defined(BT_RELEASE)
inline constexpr const char* kpcBuildConfigurationName = "Release";
inline constexpr bool kbMimallocDiagnostics = false;

inline constexpr bool kbDebugBreak = false;
inline constexpr bool kbDebugInput = false;
inline constexpr bool kbDebugNavigationCrossingCheck = false;
inline constexpr bool kbDebugRender = false;
inline constexpr bool kbDxDiag = true;
inline constexpr bool kbFreeCamera = false;
inline constexpr bool kbProfiling = false;
inline constexpr bool kbProfilingDump = false;
inline constexpr bool kbProfilingFrameSpike = false;
inline constexpr bool kbReplayFullFrames = false;
inline constexpr bool kbVulkanDebugLayers = false;
inline constexpr bool kbRandomlyInvalidatePhysicallyBasedRenderingCubemapCache = false;
inline constexpr bool kbShowProfileTextByDefault = false;
#endif

inline constexpr LogLevel keLogLevelTemporary = kVerbose;
inline constexpr LogLevel keLogLevelReplay = kVerbose; // Keeps the per-tick replay checksum lines in the binary; their runtime level stays default-silent

inline constexpr LogLevel keLogLevelDefault = kDebug; // Compile floor; runtime threshold (gpeLogRuntimeLevels, default kInfo) gates emission live via set_log_level
inline constexpr LogLevel keLogLevelAudio = keLogLevelDefault;
inline constexpr LogLevel keLogLevelGraphics = keLogLevelDefault;
inline constexpr LogLevel keLogLevelLoading = keLogLevelDefault;
inline constexpr LogLevel keLogLevelNavigationData = keLogLevelDefault;
inline constexpr LogLevel keLogLevelNetwork = keLogLevelDefault;
inline constexpr LogLevel keLogLevelInput = keLogLevelDefault;
