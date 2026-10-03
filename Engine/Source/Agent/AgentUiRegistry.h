#pragma once

#if defined(BT_CLIENT)

namespace engine
{

// One completed-frame ImGui widget record. Filled during ImGui::NewFrame..Render into the write table, then made
// readable after ImGui::Render() by Swap().
struct AgentUiItem
{
	ImGuiID uiIdentifier = 0;
	char pcLabel[64] {};
	char pcValue[64] {};
	char pcWindow[32] {};
	XMFLOAT4 f4Rectangle {}; // pixels: {minX, minY, maxX, maxY}
	int32_t iStatusFlags = 0; // ImGuiItemStatusFlags_ bits supplied by ItemAdd, ItemInfo, and menu helpers
	bool bDisabled = false;
};

// One completed-frame ImGui window record (name + pixel rect + focus), snapshotted from g.Windows at Swap().
struct AgentUiWindow
{
	char pcName[32] {};
	XMFLOAT4 f4Rectangle {}; // pixels: {minX, minY, maxX, maxY}
	bool bFocused = false;
};

// Label metadata emitted before ItemAdd (Dear ImGui tabs) waits here until a real rectangle arrives.
struct AgentUiPendingLabel
{
	ImGuiID uiIdentifier = 0;
	char pcLabel[64] {};
};

// Double-buffered fixed-capacity snapshot of every ImGui widget/window per completed frame. The imgui test-engine
// hooks fill item metadata and numeric wrappers attach displayed values; Swap() (after ImGui::Render()) publishes
// the write table, so readers (describe_ui, label resolution) always see the last COMPLETED frame. Zero steady-state
// heap: the tables are member arrays sized once at construction.
class AgentUiRegistry
{
public:

	AgentUiRegistry();
	~AgentUiRegistry();

	AgentUiRegistry(const AgentUiRegistry&) = delete;
	AgentUiRegistry& operator=(const AgentUiRegistry&) = delete;

	// Write-table fill (called from the imgui hooks during ImGui::NewFrame..Render).
	void HookItemAdd(ImGuiID uiIdentifier, const XMFLOAT4& rf4Rectangle, const char* pcWindow, bool bDisabled, bool bVisible);
	void HookItemInfo(ImGuiID uiIdentifier, const char* pcLabel, int32_t iStatusFlags);

	// Attach menu-control metadata to an item already submitted in the current write table.
	void RecordItemValue(ImGuiID uiIdentifier, const char* pcValue);
	void RecordItemChecked(ImGuiID uiIdentifier, bool bChecked);

	// Publish the write table as the read table and snapshot the window list. Call after ImGui::Render().
	void Swap();

	// Resolve a label to a read-table item index. Order: exact full label -> exact pre-"##" display portion ->
	// case-insensitive substring. Returns the index, kiNotFound (no match), or kiAmbiguous (>1 at the deciding tier).
	// pcWindow, when non-null, restricts matching to items in that window.
	static constexpr int64_t kiNotFound = -1;
	static constexpr int64_t kiAmbiguous = -2;
	int64_t ResolveLabel(const char* pcLabel, const char* pcWindow) const;

	static constexpr int64_t kiMaxItems = 1'024;
	static constexpr int64_t kiMaxWindows = 64;
	static constexpr int64_t kiMaxPendingLabels = 256;

	AgentUiItem mItems[2][kiMaxItems];
	int64_t miItemCount[2] {};
	AgentUiWindow mWindows[2][kiMaxWindows];
	int64_t miWindowCount[2] {};
	int64_t miRead = 1;

private:

	int64_t FindPendingLabel(ImGuiID uiIdentifier) const;
	void RemovePendingLabel(int64_t iIndex);

	AgentUiPendingLabel mPendingLabels[kiMaxPendingLabels];
	int64_t miPendingLabelCount = 0;

	int64_t miWrite = 0;
};

inline AgentUiRegistry* gpAgentUiRegistry = nullptr;

} // namespace engine

#endif // defined(BT_CLIENT)
