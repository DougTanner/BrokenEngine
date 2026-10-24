#pragma once

#if defined(BT_CLIENT)

namespace engine
{

struct RawInput;

// Which synthetic-input script is running. Each agent input command (click / set_slider / key / wheel / type_text)
// compiles to one of these; only one runs at a time.
enum class AgentScriptKind : uint8_t
{
	kClick,
	kSetSlider,
	kKey,
	kWheel,
	kTypeText,
};

// Completion status polled by the deferred command response.
enum class AgentScriptStatus : uint8_t
{
	kPending,
	kDone,
	kNotFound,
	kAmbiguous,
	kNotInputable, // set_slider target resolved but lacks ImGuiItemStatusFlags_Inputable (e.g. a Button)
};

// Immutable configuration a command hands to BeginScript(). Fixed-size (no heap); string params are bounded buffers.
struct AgentScript
{
	AgentScriptKind eKind = AgentScriptKind::kClick;

	char pcLabel[64] {};   // click / set_slider target
	char pcWindow[32] {};  // optional window filter
	bool bHasWindow = false;

	char pcValueText[32] {}; // set_slider value text (ASCII, typed via ImGui temp-input)

	char pcTypedText[NetworkMessages::ClientChatMessage::kiMaxTextBytes + 1] {}; // type_text UTF-8 text, typed into the active ImGui text field

	int64_t iHoldFrames = 2; // key hold duration

	int64_t iVirtualKey = 0; // key command: Win32 VK code driven through the RawInput overlay

	int64_t iWheelNotches = 0;
};

// Frame-stepped synthetic-input engine. Advanced one step per client main-loop iteration at the drain point
// (before ImGui NewFrame), so on a rendered frame injected events land in that frame. Sinks: ImGui (item activation,
// typed text, and keys for UI driving), a RawInput snapshot overlay (engine key bindings), and the lifetime scroll
// accumulator RawInputManager folds into the published wheel value (camera zoom). It never sends an ImGui mouse event.
// Zero steady-state heap — all state is fixed members.
class AgentInput : public common::Singleton<AgentInput>
{
public:

	AgentInput();

	AgentInput(const AgentInput&) = delete;
	AgentInput& operator=(const AgentInput&) = delete;

	// Start a script. Returns false if one is already running (caller answers "busy").
	bool BeginScript(const AgentScript& rScript);
	bool mbScriptActive = false;

	// At the client main-thread drain point before GameBase::Render and ImGui::NewFrame, advance the active script and
	// queue this frame's ImGui input. The call is unconditional, so minimized frames still advance while Render takes the
	// swapchain-deferred skip and bypasses ImGuiManager::Prepare. While the client is not rendering (minimized or
	// swapchain recreate deferred), only the RawInput overlay takes effect; ImGui input queued then never reaches
	// ImGui, and key scripts send none.
	void AdvanceFrame();

	// End of RawInputManager::Update: OR the synthetic keys onto the just-published snapshot so game edge-detection
	// fires as with hardware. The scroll accumulator is NOT added here — it is a lifetime accumulator folded into
	// iScrollWheelValue on every publish (see miSyntheticScrollAccumulator).
	void Overlay(RawInput& rRawInput);

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
	// Resolve the target label in the registry's last completed frame and queue ImGui activation of its ID, which
	// this frame's ImGui::NewFrame applies. On not-found / ambiguous / a non-inputable set_slider target it calls
	// Finish() with that error, and on a disabled target it calls Finish(kDone) without activating; both return false.
	bool ActivateTarget();

	AgentScript mScript {};

	int64_t miPhase = 0;
	int64_t miPhaseFrame = 0;

	// Persistent synthetic state read by Overlay() (game-binding sink).
	bool mpbSyntheticKeys[0xFF] {};
};

inline AgentInput* gpAgentInput = nullptr;

} // namespace engine

#endif // defined(BT_CLIENT)
