#pragma once

#if defined(BT_CLIENT)

namespace engine
{

struct RawInput;

// Which synthetic-input script is running. Each agent input command (click / hover / set_slider / key / mouse)
// compiles to one of these; only one runs at a time.
enum class AgentScriptKind : uint8_t
{
	kClick,
	kHover,
	kSetSlider,
	kKey,
	kMouse,
};

enum class AgentMouseAction : uint8_t
{
	kMove,
	kDown,
	kUp,
	kClick,
	kWheel,
};

// Completion status polled by the deferred command response.
enum class AgentScriptStatus : uint8_t
{
	kPending,
	kDone,
	kTimeout,
	kNotFound,
	kAmbiguous,
	kNotInputable, // set_slider target resolved but lacks ImGuiItemStatusFlags_Inputable (e.g. a Button)
	kClipped, // target resolved but lacks ImGuiItemStatusFlags_Visible (scroll-clipped out of view)
};

// Immutable configuration a command hands to BeginScript(). Fixed-size (no heap); string params are bounded buffers.
struct AgentScript
{
	AgentScriptKind eKind = AgentScriptKind::kClick;

	char pcLabel[64] {};   // click / hover / set_slider target
	char pcWindow[32] {};  // optional window filter
	bool bHasWindow = false;

	char pcValueText[32] {}; // set_slider value text (ASCII, typed via ImGui temp-input)

	int64_t iTimeoutFrames = 120; // stabilization timeout (click / hover / set_slider)
	int64_t iHoldFrames = 2;      // hover / key hold duration

	int64_t iVirtualKey = 0; // key command: Win32 VK code driven through the RawInput overlay

	AgentMouseAction eMouseAction = AgentMouseAction::kMove;
	int64_t iImGuiMouseButton = 0; // 0 left / 1 right / 2 middle (ImGui IO button index)
	uint32_t uiOverlayMouseButtonBit = 0; // engine::MouseButtons bit for the RawInput overlay
	int64_t iWheelNotches = 0;
	float f2CoordinatePixels[2] {}; // mouse command raw pixel coords
	bool bHasCoordinate = false;
};

// Frame-stepped synthetic-input engine. Advanced one step per client main-loop iteration at the drain point
// (before ImGui NewFrame), so on a rendered frame injected events land in that frame. Two sinks: ImGui IO events (UI driving) and
// a RawInput snapshot overlay (engine key bindings). Zero steady-state heap — all state is fixed members.
class AgentInput : public common::Singleton<AgentInput>
{
public:

	AgentInput();

	AgentInput(const AgentInput&) = delete;
	AgentInput& operator=(const AgentInput&) = delete;

	// Start a script. Returns false if one is already running (caller answers "busy").
	bool BeginScript(const AgentScript& rScript);
	bool mbScriptActive = false;

	// True while a synthetic ImGui mouse pos is pinned (persists across a script's completion; cleared in BeginScript).
	// Consulted by input-suppression so the no-mouse sentinel is issued only when no synthetic pin already owns io.MousePos.
	// ImGui-IO mouse-pos pin (UI sink), separate from the overlay pos flag (game-world sink): the last synthetic
	// ImGui pos, re-issued after the Win32 backend in ImGuiManager::Prepare so it wins last-writer-wins. Cleared in
	// BeginScript only (never Finish) so it persists across a script's completion until the next script re-seeds it.
	bool mbImGuiMousePositionPinned = false;

	// At the client main-thread drain point before GameBase::Render and ImGui::NewFrame, advance the active script and
	// queue this frame's ImGui IO. The call is unconditional, so minimized frames still advance while Render takes the
	// swapchain-deferred skip and bypasses ImGuiManager::Prepare. While the client is not rendering (minimized or
	// swapchain recreate deferred), only the RawInput overlay takes effect; mouse ImGui events sent then never reach
	// ImGui, and key scripts send none.
	void AdvanceFrame();

	// End of RawInputManager::Update: OR the synthetic key / mouse-button / mouse-pos state onto the just-published
	// snapshot so game edge-detection fires as with hardware. The scroll accumulator is NOT added here — it is a
	// lifetime accumulator folded into iScrollWheelValue on every publish (see miSyntheticScrollAccumulator).
	void Overlay(RawInput& rRawInput);

	// Called by ImGuiManager::Prepare between the Win32 backend NewFrame and ImGui::NewFrame: if a synthetic ImGui mouse
	// pos is pinned, re-issue it so it is the frame's last mouse-pos event (the physical cursor would otherwise win
	// last-writer-wins in ImGui::NewFrame). Pin-valid gate lives inside the function.
	void ReissueImGuiMousePosition();

	// Persistent synthetic scroll offset added into the published lifetime iScrollWheelValue on EVERY publish (script
	// active or not) — consumers diff iScrollWheelValue, so the offset must never drop out of the published value.
	int64_t miSyntheticScrollAccumulator = 0;

	// True when the notch count's wheel product fits int32_t and adding it to the lifetime accumulator stays representable.
	bool WheelNotchesFit(int64_t iNotches) const;

	// Deferred responses poll meStatus; mbResolvedDisabled is read after successful completion.
	AgentScriptStatus meStatus = AgentScriptStatus::kPending;
	bool mbResolvedDisabled = false;

private:

	void Finish(AgentScriptStatus eStatus);
	// Resolve the target label to this frame's rect and drive the stabilization loop. Returns true once stable;
	// on not-found / ambiguous / timeout it calls Finish() and returns false.
	bool StabilizeTarget();
	void IssueImGuiMousePosition(float fX, float fY);

	AgentScript mScript {};

	int64_t miPhase = 0;
	int64_t miPhaseFrame = 0;
	int64_t miElapsedFrames = 0;
	int64_t miStableCount = 0;
	XMFLOAT4 mf4LastRectangle {};
	bool mbHaveLastRectangle = false;

	int64_t miResolvedStatusFlags = 0; // resolved target's ImGuiItemStatusFlags (set_slider Inputable pre-validation)
	float mf2TargetCenter[2] {}; // resolved rect center, re-pinned into ImGui each frame

	// Persistent synthetic state read by Overlay() (game-binding sink).
	bool mpbSyntheticKeys[0xFF] {};
	uint32_t muiSyntheticMouseButtons = 0;
	bool mbSyntheticMousePosValid = false;
	float mf2SyntheticMousePixels[2] {};

	float mf2ImGuiPinnedPixels[2] {};
};

inline AgentInput* gpAgentInput = nullptr;

} // namespace engine

#endif // defined(BT_CLIENT)
