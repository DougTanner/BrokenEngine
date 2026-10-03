#include "Pch.h"

#if defined(BT_SERVER)

// The vendored imgui unity unit (ThirdParty/Prebuilts/Source/Engine/ImGui.cpp) is compiled once, with
// IMGUI_ENABLE_TEST_ENGINE, into the single shared static lib linked by both targets. That define makes
// ImGui::Begin / DebugLogV / ShowIDStackToolWindow emit calls to the four test-engine hook externs. The real
// implementations live in the client-only engine::AgentUiRegistry (Agent/AgentUiRegistry.cpp); the server links
// the same imgui object code but has no registry, so it provides no-op stubs here to resolve the externs.
// Signatures must match imgui_internal.h:3951-3954 so the mangled names line up (ImGuiID -> unsigned int,
// ImGuiItemStatusFlags -> int; the internal struct types, only ever passed by pointer/reference, are
// forward-declared to keep imgui internals out of the server PCH).

struct ImRect;
struct ImGuiLastItemData;

void ImGuiTestEngineHook_ItemAdd([[maybe_unused]] ImGuiContext* pContext, [[maybe_unused]] ImGuiID uiIdentifier, [[maybe_unused]] const ImRect& rBoundingBox, [[maybe_unused]] const ImGuiLastItemData* pItemData)
{
}

void ImGuiTestEngineHook_ItemInfo([[maybe_unused]] ImGuiContext* pContext, [[maybe_unused]] ImGuiID uiIdentifier, [[maybe_unused]] const char* pcLabel, [[maybe_unused]] int iFlags)
{
}

void ImGuiTestEngineHook_Log([[maybe_unused]] ImGuiContext* pContext, [[maybe_unused]] const char* pcFormat, ...)
{
}

const char* ImGuiTestEngine_FindItemDebugLabel([[maybe_unused]] ImGuiContext* pContext, [[maybe_unused]] ImGuiID uiIdentifier)
{
	return nullptr;
}

#endif // defined(BT_SERVER)
