#include "Pch.h"

#if defined(BT_CLIENT)

#include "Agent/AgentUiRegistry.h"

namespace engine
{

// Bounded copy into a fixed char buffer, always null-terminated.
static void CopyTruncate(std::span<char> destination, const char* pcSource)
{
	int64_t i = 0;
	for (; i < std::ssize(destination) - 1 && pcSource[i] != '\0'; ++i)
	{
		destination[i] = pcSource[i];
	}
	destination[i] = '\0';
}

// Length of the display portion of an ImGui label — everything before the "##" id separator (or the whole string).
static int64_t DisplayLength(std::string_view label)
{
	int64_t iSeparator = static_cast<int64_t>(label.find("##"));
	return iSeparator == static_cast<int64_t>(std::string_view::npos) ? static_cast<int64_t>(label.size()) : iSeparator;
}

static constexpr char LowerAscii(char cChar)
{
	return (cChar >= 'A' && cChar <= 'Z') ? static_cast<char>(cChar - 'A' + 'a') : cChar;
}

static bool ContainsCaseInsensitive(std::string_view haystack, std::string_view needle)
{
	if (needle.empty())
	{
		return true;
	}
	for (int64_t i = 0; i < std::ssize(haystack); ++i)
	{
		int64_t j = 0;
		while (j < std::ssize(needle) && i + j < std::ssize(haystack) && LowerAscii(haystack[i + j]) == LowerAscii(needle[j]))
		{
			++j;
		}
		if (j == std::ssize(needle))
		{
			return true;
		}
	}
	return false;
}

AgentUiRegistry::AgentUiRegistry()
: common::Singleton<AgentUiRegistry>(gpAgentUiRegistry)
{
}

int64_t AgentUiRegistry::FindPendingLabel(ImGuiID uiIdentifier) const
{
	for (int64_t i = 0; i < miPendingLabelCount; ++i)
	{
		if (mPendingLabels[i].uiIdentifier == uiIdentifier)
		{
			return i;
		}
	}
	return -1;
}

void AgentUiRegistry::RemovePendingLabel(int64_t iIndex)
{
	--miPendingLabelCount;
	if (iIndex != miPendingLabelCount)
	{
		mPendingLabels[iIndex] = mPendingLabels[miPendingLabelCount];
	}
	mPendingLabels[miPendingLabelCount] = {};
}

void AgentUiRegistry::HookItemAdd(ImGuiID uiIdentifier, const XMFLOAT4& rf4Rectangle, const char* pcWindow, bool bDisabled, bool bVisible)
{
	int64_t iPendingLabel = FindPendingLabel(uiIdentifier);
	bool bEmptyRectangle = rf4Rectangle.x >= rf4Rectangle.z || rf4Rectangle.y >= rf4Rectangle.w;
	if (iPendingLabel >= 0 && bEmptyRectangle)
	{
		// A tab reports a zero-size layout placeholder after its label and before its real tab rectangle.
		return;
	}

	int64_t& riCount = miItemCount[miWrite];
	if (riCount >= kiMaxItems)
	{
		return; // capacity bound — silently drop overflow (registry is best-effort snapshot)
	}
	AgentUiItem& rItem = mItems[miWrite][riCount];
	rItem.uiIdentifier = uiIdentifier;
	rItem.f4Rectangle = rf4Rectangle;
	rItem.iStatusFlags = bVisible ? static_cast<int64_t>(ImGuiItemStatusFlags_Visible) : 0;
	rItem.bDisabled = bDisabled;
	rItem.pcLabel[0] = '\0';
	rItem.pcValue[0] = '\0';
	CopyTruncate(rItem.pcWindow, pcWindow);
	if (iPendingLabel >= 0)
	{
		CopyTruncate(rItem.pcLabel, mPendingLabels[iPendingLabel].pcLabel);
		RemovePendingLabel(iPendingLabel);
	}
	++riCount;
}

void AgentUiRegistry::HookItemInfo(ImGuiID uiIdentifier, const char* pcLabel, int64_t iStatusFlags)
{
	// Search newest records first; tabs can emit ItemInfo before their ItemAdd.
	int64_t iCount = miItemCount[miWrite];
	for (int64_t i = iCount - 1; i >= 0; --i)
	{
		AgentUiItem& rItem = mItems[miWrite][i];
		if (rItem.uiIdentifier == uiIdentifier)
		{
			if (pcLabel != nullptr && pcLabel[0] != '\0')
			{
				CopyTruncate(rItem.pcLabel, pcLabel);
			}
			rItem.iStatusFlags |= iStatusFlags;
			return;
		}
	}

	if (pcLabel == nullptr || pcLabel[0] == '\0')
	{
		return;
	}

	int64_t iPendingLabel = FindPendingLabel(uiIdentifier);
	if (iPendingLabel < 0)
	{
		if (miPendingLabelCount >= kiMaxPendingLabels)
		{
			return; // capacity bound — silently drop overflow (registry is best-effort snapshot)
		}
		iPendingLabel = miPendingLabelCount++;
		mPendingLabels[iPendingLabel].uiIdentifier = uiIdentifier;
	}
	// The pre-ItemAdd status belongs to the prior item for tabs; only the label crosses into pending state.
	CopyTruncate(mPendingLabels[iPendingLabel].pcLabel, pcLabel);
}

void AgentUiRegistry::RecordItemValue(ImGuiID uiIdentifier, const char* pcValue)
{
	int64_t iCount = miItemCount[miWrite];
	for (int64_t i = iCount - 1; i >= 0; --i)
	{
		AgentUiItem& rItem = mItems[miWrite][i];
		if (rItem.uiIdentifier == uiIdentifier)
		{
			CopyTruncate(rItem.pcValue, pcValue);
			return;
		}
	}
}

void AgentUiRegistry::RecordItemChecked(ImGuiID uiIdentifier, bool bChecked)
{
	int64_t iCount = miItemCount[miWrite];
	for (int64_t i = iCount - 1; i >= 0; --i)
	{
		AgentUiItem& rItem = mItems[miWrite][i];
		if (rItem.uiIdentifier == uiIdentifier)
		{
			rItem.iStatusFlags |= static_cast<int64_t>(ImGuiItemStatusFlags_Checkable);
			if (bChecked)
			{
				rItem.iStatusFlags |= static_cast<int64_t>(ImGuiItemStatusFlags_Checked);
			}
			else
			{
				rItem.iStatusFlags &= ~static_cast<int64_t>(ImGuiItemStatusFlags_Checked);
			}
			return;
		}
	}
}

void AgentUiRegistry::Swap()
{
	// Snapshot the window list of the just-completed frame into the write buffer before flipping.
	ImGuiContext* pContext = ImGui::GetCurrentContext();
	int64_t& riWindowCount = miWindowCount[miWrite];
	riWindowCount = 0;
	for (ImGuiWindow* pWindow : pContext->Windows)
	{
		if (!pWindow->WasActive)
		{
			continue;
		}
		if (riWindowCount >= kiMaxWindows)
		{
			break;
		}
		AgentUiWindow& rWindow = mWindows[miWrite][riWindowCount];
		CopyTruncate(rWindow.pcName, pWindow->Name);
		rWindow.f4Rectangle = XMFLOAT4(pWindow->Pos.x, pWindow->Pos.y, pWindow->Pos.x + pWindow->Size.x, pWindow->Pos.y + pWindow->Size.y);
		rWindow.bFocused = (pContext->NavWindow == pWindow);
		++riWindowCount;
	}

	// Publish: the write buffer becomes the read buffer; reset the new write buffer's item count for the next frame.
	std::swap(miWrite, miRead);
	miItemCount[miWrite] = 0;
	for (int64_t i = 0; i < miPendingLabelCount; ++i)
	{
		mPendingLabels[i] = {};
	}
	miPendingLabelCount = 0;
}

int64_t AgentUiRegistry::ResolveLabel(const char* pcLabel, const char* pcWindow) const
{
	int64_t iCount = miItemCount[miRead];
	int64_t iNeedleDisplay = DisplayLength(pcLabel);

	// Three resolution tiers, tried in order; the first tier with any match decides (single -> hit, multiple -> ambiguous).
	for (int64_t k = 0; k < 3; ++k)
	{
		int64_t iFound = kiNotFound;
		int64_t iMatches = 0;
		for (int64_t i = 0; i < iCount; ++i)
		{
			const AgentUiItem& rItem = mItems[miRead][i];
			if (pcWindow != nullptr && std::strcmp(rItem.pcWindow, pcWindow) != 0)
			{
				continue;
			}
			if (rItem.pcLabel[0] == '\0')
			{
				continue;
			}

			bool bMatch = false;
			if (k == 0)
			{
				bMatch = std::strcmp(rItem.pcLabel, pcLabel) == 0;
			}
			else if (k == 1)
			{
				int64_t iItemDisplay = DisplayLength(rItem.pcLabel);
				bMatch = iItemDisplay == iNeedleDisplay && std::strncmp(rItem.pcLabel, pcLabel, static_cast<size_t>(iNeedleDisplay)) == 0;
			}
			else
			{
				bMatch = ContainsCaseInsensitive(rItem.pcLabel, pcLabel);
			}

			if (bMatch)
			{
				++iMatches;
				iFound = i;
			}
		}

		if (iMatches == 1)
		{
			return iFound;
		}
		if (iMatches > 1)
		{
			return kiAmbiguous;
		}
	}

	return kiNotFound;
}

} // namespace engine

// Global Dear ImGui test-engine hooks match imgui_internal.h and run from ItemAdd()/ItemInfo() with
// IMGUI_ENABLE_TEST_ENGINE and g.TestEngineHookItems active. The vendored ImGui wrapper enables the hooks; ImGuiManager
// enables item hooks for the agent layer. Client hooks forward to AgentUiRegistry, while the server links the same
// ImGui code and supplies no-op hooks in AgentUiRegistryServerStubs.cpp.

void ImGuiTestEngineHook_ItemAdd(ImGuiContext* pContext, ImGuiID uiIdentifier, const ImRect& rBoundingBox, [[maybe_unused]] const ImGuiLastItemData* pItemData)
{
	const char* pcWindow = pContext->CurrentWindow->Name;
	bool bDisabled = (pContext->CurrentItemFlags & ImGuiItemFlags_Disabled) != 0;
	// ItemAdd invokes this hook before it sets LastItemData.Visible; use its exact rectangle/clip predicate here.
	bool bVisible = rBoundingBox.Overlaps(pContext->CurrentWindow->ClipRect);
	engine::gpAgentUiRegistry->HookItemAdd(uiIdentifier, XMFLOAT4(rBoundingBox.Min.x, rBoundingBox.Min.y, rBoundingBox.Max.x, rBoundingBox.Max.y), pcWindow, bDisabled, bVisible);
}

void ImGuiTestEngineHook_ItemInfo(ImGuiContext* pContext, ImGuiID uiIdentifier, const char* pcLabel, ImGuiItemStatusFlags iStatusFlags)
{
	// Begin() registers window pseudo-items without ImGuiItemStatusFlags_Visible.
	// Empty labels omit them from describe_ui and label lookup (kiNotFound), avoiding visible:false entries and kClipped window-label clicks.
	// Window names remain queryable through the window snapshot.
	if (pContext->CurrentWindow != nullptr && uiIdentifier == pContext->CurrentWindow->ID)
	{
		return;
	}
	engine::gpAgentUiRegistry->HookItemInfo(uiIdentifier, pcLabel, static_cast<int64_t>(iStatusFlags));
}

void ImGuiTestEngineHook_Log([[maybe_unused]] ImGuiContext* pContext, [[maybe_unused]] const char* pcFormat, ...)
{
	// No-op: the registry needs item rects/labels only, not the test-engine log stream.
}

const char* ImGuiTestEngine_FindItemDebugLabel([[maybe_unused]] ImGuiContext* pContext, [[maybe_unused]] ImGuiID uiIdentifier)
{
	return nullptr;
}

#endif // defined(BT_CLIENT)
