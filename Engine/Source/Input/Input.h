#pragma once

#if defined(BT_CLIENT)

#include "Input/RawInputManager.h"

namespace engine
{

// Menu
enum class MenuInputFlags : uint64_t
{
	kPauseMenu         = 0x00000001,
	kToggleFullscreen  = 0x00000002,
	kMouseIsDown       = 0x00000004,
	kMouseClick        = 0x00000008,
	kGamepadButton     = 0x00000010,
	kQuit              = 0x00000020,
	kToggleProfileText = 0x00000040,
	kTogglePauseFrame  = 0x00000080,
	kQuicksave         = 0x00000200,
	kQuickload         = 0x00000400,
	kSaveReplay        = 0x00000800,
	kLoadReplay        = 0x00001000,
	kSlowTime          = 0x00002000,
	kSpeedUpTime       = 0x00004000,
	kSingleStep        = 0x00008000,
	kMenuTweaks        = 0x00020000,
	kToggleScreenshots = 0x00040000,
	kMenuDebugTexture  = 0x00200000,
	kDebugTextureNext  = 0x00400000,
	kDebugTexturePrev  = 0x00800000,
	kToggleDebugRender = 0x01000000,
};
using MenuInputFlags_t = common::Flags<MenuInputFlags>;

struct MenuInput
{
	bool bGamepad = false;
	MenuInputFlags_t flags {};
	XMFLOAT2 f2Mouse {};
	XMFLOAT2 f2Gamepad {};
};

// Borrowed view of the current and previous raw snapshots, handed out by BeginPoll so engine policy and the
// game callback detect edges against the same pair. It stores no state of its own and is only valid until
// CompletePoll advances the previous snapshot, so it must never outlive GameBase::ProcessInput.
class InputPoll
{
public:

	bool KeyboardPressed(int64_t iKey) const { return mrCurrent.pKeyboardKeys[iKey] && !mrPrevious.pKeyboardKeys[iKey]; }
	bool MousePressed(MouseButtons eButton) const { return (mrCurrent.mouseButtons & eButton) && !(mrPrevious.mouseButtons & eButton); }
	bool GamepadPressed(GamepadButtons eButton) const { return (mrCurrent.gamepadButtons & eButton) && !(mrPrevious.gamepadButtons & eButton); }

private:

	friend class Input;

	InputPoll(const RawInput& rCurrent, const RawInput& rPrevious) : mrCurrent(rCurrent), mrPrevious(rPrevious) {}

	const RawInput& mrCurrent;
	const RawInput& mrPrevious;
};

// Input manager class
class Input
{
public:

	InputPoll BeginPoll(bool bLostFocus, bool bMenuVisible, MenuInput& rMenuInput);
	void CompletePoll();

private:

	enum class InputStateFlags : uint8_t
	{
		kScrollWheelInitialized = 1 << 0,
		kGamepadMode            = 1 << 1,
	};

	// Previous display frame's published snapshot. Also carries the scroll-wheel baseline, since the raw
	// snapshot exposes only a lifetime accumulator.
	RawInput mPreviousRawInput {};

	common::Flags<InputStateFlags> mStateFlags;
};

inline Input* gpInput = nullptr;

} // namespace engine

#endif // BT_CLIENT
