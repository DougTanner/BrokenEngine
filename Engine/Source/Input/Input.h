#pragma once

#if defined(BT_CLIENT)

#include "Input/Keybinds.h"
#include "Input/RawInputManager.h"

namespace engine
{

// MenuInputFlags carries fixed and debug inputs. Query remappable fullscreen, chat, and screenshot actions
// through InputPoll::Pressed(KeyAction).
enum class MenuInputFlags : uint64_t
{
	kPauseMenu            = 0x00000001,
	kMouseIsDown          = 0x00000004,
	kMouseClick           = 0x00000008,
	kGamepadButton        = 0x00000010,
	kQuit                 = 0x00000020,
	kToggleProfileText    = 0x00000040,
	kTogglePauseFrame     = 0x00000080,
	kQuicksave            = 0x00000200,
	kQuickload            = 0x00000400,
	kSaveReplay           = 0x00000800,
	kLoadReplay           = 0x00001000,
	kSlowTime             = 0x00002000,
	kSpeedUpTime          = 0x00004000,
	kSingleStep           = 0x00008000,
	kMenuTweaks           = 0x00020000,
	kMenuDebugTexture     = 0x00200000,
	kDebugTextureNext     = 0x00400000,
	kDebugTexturePrevious = 0x00800000,
	kToggleDebugRender    = 0x01000000,
};
using MenuInputFlags_t = common::Flags<MenuInputFlags>;

struct MenuInput
{
	bool bGamepad = false;
	MenuInputFlags_t flags {};
	XMFLOAT2 f2Mouse {};
	XMFLOAT2 f2Gamepad {};
};

// Scoped RAII display-frame poll guard. Construction publishes the RawInput
// snapshot, resolves active keybind capture (if any), and fills MenuInput and
// gpCamera->mCameraInput through this single owning guard; destruction advances
// Input::mPreviousRawInput after all consumers (including early-return paths
// and game::ProcessGameMenuInput).
class [[nodiscard]] InputPoll
{
public:

	InputPoll(bool bLostFocus, bool bMenuVisible, MenuInput& rMenuInput);
	~InputPoll();

	InputPoll(const InputPoll&) = delete;
	InputPoll& operator=(const InputPoll&) = delete;

	// Game action enums supply KeybindFor via ADL. Both queries return false for unbound actions and during
	// capture. When UI owns the keyboard, only Pressed can report VK_TAB.
	template <typename ACTION_ENUM>
	bool Pressed(ACTION_ENUM eAction) const
	{
		return PressedKey(KeybindFor(eAction));
	}

	template <typename ACTION_ENUM>
	bool Held(ACTION_ENUM eAction) const
	{
		return HeldKey(KeybindFor(eAction));
	}

	// Combines the four configured free-camera KeyAction directions using
	// kfFreeCameraAxis. Returns {0, 0} while UI owns keyboard or capture runs.
	XMFLOAT2 MoveAxis() const;

	// Low-level key/device queries for fixed and debug shortcuts; keyboard
	// queries are automatically suppressed while keybind capture is active.
	bool PressedKey(VirtualKey key) const;
	bool HeldKey(VirtualKey key) const;
	bool KeyboardPressed(int64_t iKey) const;
	bool MousePressed(MouseButtons eButton) const;
	bool GamepadPressed(GamepadButtons eButton) const;

private:

	friend class Input;

	static constexpr uint8_t kuiFlagUserInterfaceOwnsKeyboard = 1ui32 << 0;
	static constexpr uint8_t kuiFlagCaptureSuppressed         = 1ui32 << 1;

	const RawInput& mrCurrent;
	const RawInput& mrPrevious;
	uint8_t muiPollFlags = 0; // UI keyboard ownership + capture suppression bits
};

class Input : public common::Singleton<Input>
{
public:

	Input();

	// Loads EngineKeybinds.bin and GameKeybinds.bin at startup and resolves any
	// cross-domain duplicate keys (loaded file overrides default; engine wins
	// tie between two loaded files with one kWarning).
	void LoadAllKeybinds();

	void BeginKeybindCapture(KeybindSlot slot);
	void CancelKeybindCapture();
	bool IsCapturingKeybind() const;
	bool IsCapturingKeybind(KeybindSlot slot) const;

	// Unbinds any previous owner across both engine and game tables, then
	// assigns key (or VirtualKey::Unbound()) to slot.
	void RebindSlot(KeybindSlot slot, VirtualKey key);

	// Cancels active capture and resets both engine and game keybinds to defaults.
	void ResetAllKeybinds();

	// Keeps the opening key's WM_KEYDOWN and WM_SYSKEYDOWN repeats out of chat until release or focus loss.
	VirtualKey mSuppressedChatOpenKey {};

private:

	friend class InputPoll;

	enum class InputStateFlags : uint8_t
	{
		kScrollWheelInitialized = 1 << 0,
		kGamepadMode            = 1 << 1,
		kCapturingKeybind       = 1 << 2,
	};

	void BeginPoll(bool bLostFocus, bool bMenuVisible, MenuInput& rMenuInput, InputPoll& rInputPoll);
	void EndPoll();

	RawInput mPreviousRawInput {};
	KeybindSlot mCaptureSlot {};
	common::Flags<InputStateFlags> mStateFlags {};
};

inline Input* gpInput = nullptr;

} // namespace engine

#endif // BT_CLIENT
