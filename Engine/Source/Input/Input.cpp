#include "Input.h"

#if defined(BT_CLIENT)

namespace engine
{

using enum MenuInputFlags;

constexpr float kfGamepadThreshold = 0.1f;

InputPoll Input::BeginPoll(bool bLostFocus, bool bMenuVisible, MenuInput& rMenuInput)
{
	gpRawInputManager->Update(bLostFocus);
	const RawInput& rRawInput = gpRawInputManager->mRawInput;

	// Seed only the wheel baseline: the rest of the previous snapshot stays zeroed on the first poll so a key
	// already held at startup still reads as a fresh press, exactly as before.
	if (!(mStateFlags & InputStateFlags::kScrollWheelInitialized))
	{
		mPreviousRawInput.iScrollWheelValue = rRawInput.iScrollWheelValue;
		mStateFlags.Set(InputStateFlags::kScrollWheelInitialized);
	}

	InputPoll inputPoll(rRawInput, mPreviousRawInput);

	// Check all keyboard and mouse buttons to detect keyboard/mouse mode
	bool bKeyboardMouse = (rRawInput.mouseButtons & MouseButtons::kMouseButtonLeft) || (rRawInput.mouseButtons & MouseButtons::kMouseButtonRight) || rRawInput.pKeyboardKeys['A'] || rRawInput.pKeyboardKeys['D'] || rRawInput.pKeyboardKeys['W'] || rRawInput.pKeyboardKeys['S'] || rRawInput.pKeyboardKeys[VK_LEFT] || rRawInput.pKeyboardKeys[VK_RIGHT] || rRawInput.pKeyboardKeys[VK_UP] || rRawInput.pKeyboardKeys[VK_DOWN] || rRawInput.pKeyboardKeys[VK_NUMPAD1] || rRawInput.pKeyboardKeys[VK_NUMPAD3] || rRawInput.pKeyboardKeys[VK_NUMPAD5] || rRawInput.pKeyboardKeys[VK_NUMPAD2];

	if (bKeyboardMouse)
	{
		mStateFlags.Clear(InputStateFlags::kGamepadMode);
	}
	else if (std::abs(rRawInput.f2LeftThumbstick.x) + std::abs(rRawInput.f2LeftThumbstick.y) > kfGamepadThreshold || std::abs(rRawInput.f2RightThumbstick.x) + std::abs(rRawInput.f2RightThumbstick.y) > kfGamepadThreshold)
	{
		mStateFlags.Set(InputStateFlags::kGamepadMode);
	}
	else if (!::operator==(rRawInput.f2MousePosition, mPreviousRawInput.f2MousePosition))
	{
		mStateFlags.Clear(InputStateFlags::kGamepadMode);
	}

	rMenuInput.bGamepad = mStateFlags & InputStateFlags::kGamepadMode;

	// Menu
	rMenuInput.flags.Set(kQuit, rRawInput.pKeyboardKeys[VK_MENU] && inputPoll.KeyboardPressed(VK_F4));
	rMenuInput.flags.Set(kToggleFullscreen, inputPoll.KeyboardPressed(VK_F1));
	rMenuInput.flags.Set(kMouseIsDown, rRawInput.mouseButtons & MouseButtons::kMouseButtonLeft);
	rMenuInput.flags.Set(kMouseClick, inputPoll.MousePressed(MouseButtons::kMouseButtonLeft));
	rMenuInput.flags.Set(kGamepadButton, inputPoll.GamepadPressed(GamepadButtons::kGamepadButtonA));
	if constexpr (kbProfiling)
	{
		rMenuInput.flags.Set(kToggleProfileText, inputPoll.KeyboardPressed('P'));
	}
	if constexpr (kbDebugRender)
	{
		rMenuInput.flags.Set(kToggleDebugRender, inputPoll.KeyboardPressed('Q'));
	}
	if constexpr (kbDebugInput)
	{
		rMenuInput.flags.Set(kQuit, inputPoll.KeyboardPressed(VK_F4));
		rMenuInput.flags.Set(kTogglePauseFrame, inputPoll.KeyboardPressed(VK_SPACE));
		rMenuInput.flags.Set(kQuicksave, inputPoll.KeyboardPressed(VK_F5));
		rMenuInput.flags.Set(kQuickload, inputPoll.KeyboardPressed(VK_F6));
		rMenuInput.flags.Set(kSaveReplay, inputPoll.KeyboardPressed(VK_F7));
		rMenuInput.flags.Set(kLoadReplay, inputPoll.KeyboardPressed(VK_F8));
		rMenuInput.flags.Set(kSlowTime, inputPoll.KeyboardPressed(VK_OEM_MINUS));
		rMenuInput.flags.Set(kSpeedUpTime, inputPoll.KeyboardPressed(VK_OEM_PLUS));
		rMenuInput.flags.Set(kSingleStep, inputPoll.KeyboardPressed(VK_TAB));
	}
	if constexpr (kbScreenshots)
	{
		rMenuInput.flags.Set(kToggleScreenshots, inputPoll.KeyboardPressed(VK_F9));
	}

	rMenuInput.f2Mouse = rRawInput.f2MousePosition;
	rMenuInput.f2Gamepad = rRawInput.f2LeftThumbstick;

	// Menus
	rMenuInput.flags.Set(kPauseMenu, inputPoll.KeyboardPressed(VK_ESCAPE) || inputPoll.MousePressed(kMouseButtonMiddle) || inputPoll.GamepadPressed(kGamepadMenu) || inputPoll.GamepadPressed(kGamepadButtonB));
	if constexpr (kbDebugInput)
	{
		rMenuInput.flags.Set(kMenuDebugTexture, inputPoll.KeyboardPressed(VK_F2));
		rMenuInput.flags.Set(kDebugTextureNext, inputPoll.KeyboardPressed(VK_RIGHT));
		rMenuInput.flags.Set(kDebugTexturePrev, inputPoll.KeyboardPressed(VK_LEFT));
		rMenuInput.flags.Set(kMenuTweaks, inputPoll.KeyboardPressed(VK_F3));
	}

	// Camera
	XMFLOAT2 f2Move {};
	if constexpr (kbFreeCamera)
	{
		constexpr float kfFreeCameraAxis = 0.5f;
		if (rRawInput.pKeyboardKeys['W']) { f2Move.y += kfFreeCameraAxis; }
		if (rRawInput.pKeyboardKeys['S']) { f2Move.y -= kfFreeCameraAxis; }
		if (rRawInput.pKeyboardKeys['A']) { f2Move.x -= kfFreeCameraAxis; }
		if (rRawInput.pKeyboardKeys['D']) { f2Move.x += kfFreeCameraAxis; }
	}
	gpCamera->mCameraInput.f2Move = f2Move;

	// The UI owns the wheel when ImGui's MouseWheelY owner test fails, when the cursor is over a window that can
	// actually scroll, or while WheelingWindow holds an earlier target. A non-scrolling panel, modal, or empty
	// background leaves the notch with the camera, menus open or not. This mirrors the routing ImGui::UpdateMouseWheel
	// performs, so a notch is never both scrolled and zoomed: for a root window its scroll target is the hovered window
	// (its bubble loop only walks child windows, and this UI creates none), and WheelingWindow covers the lock during
	// which ImGui keeps feeding an earlier target regardless of current hover. ImPlot's owner remains visible for the
	// first two input polls after leaving a plot; those owner-visible polls conservatively suppress background notches,
	// while the following poll first sees no owner and is camera-eligible.
	// Hover is one frame old — ImGui resolves it in NewFrame, after this poll — so a notch arriving on the frame the
	// cursor crosses a panel edge is judged against the previous hover.
	const ImGuiContext* pImGuiContext = ImGui::GetCurrentContext();
	const ImGuiWindow* pHoveredWindow = pImGuiContext != nullptr ? pImGuiContext->HoveredWindow : nullptr;
	bool bUserInterfaceOwnsScroll = (pImGuiContext != nullptr && pImGuiContext->WheelingWindow != nullptr) ||
		(pImGuiContext != nullptr && !ImGui::TestKeyOwner(ImGuiKey_MouseWheelY, ImGuiKeyOwner_NoOwner)) ||
		(pHoveredWindow != nullptr && pHoveredWindow->ScrollMax.y != 0.0f && !(pHoveredWindow->Flags & (ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMouseInputs)));
	// CompletePoll advances the baseline whether or not the notch was swallowed, so a suppressed notch can never
	// surface later as zoom applied after the fact.
	gpCamera->mCameraInput.iScrollDelta = bUserInterfaceOwnsScroll ? 0 : rRawInput.iScrollWheelValue - mPreviousRawInput.iScrollWheelValue;

	// Populate ImGui gamepad inputs when menus are visible. Guard GetIO(): the ImGui context can be destroyed across a
	// multi-frame deferred swapchain recreate (minimized client), and this fires every deferred frame if a menu was open.
	if (bMenuVisible && ImGui::GetCurrentContext() != nullptr)
	{
		ImGuiIO& rIo = ImGui::GetIO();
		rIo.BackendFlags |= ImGuiBackendFlags_HasGamepad;

		// Map gamepad buttons to ImGui keys
		rIo.AddKeyEvent(ImGuiKey_GamepadFaceDown, rRawInput.gamepadButtons & kGamepadButtonA);
		rIo.AddKeyEvent(ImGuiKey_GamepadFaceRight, rRawInput.gamepadButtons & kGamepadButtonB);
		rIo.AddKeyEvent(ImGuiKey_GamepadStart, rRawInput.gamepadButtons & kGamepadMenu);

		// D-pad stored as analog values in f2Dpad
		rIo.AddKeyEvent(ImGuiKey_GamepadDpadUp, rRawInput.f2Dpad.y > 0.5f);
		rIo.AddKeyEvent(ImGuiKey_GamepadDpadDown, rRawInput.f2Dpad.y < -0.5f);
		rIo.AddKeyEvent(ImGuiKey_GamepadDpadLeft, rRawInput.f2Dpad.x < -0.5f);
		rIo.AddKeyEvent(ImGuiKey_GamepadDpadRight, rRawInput.f2Dpad.x > 0.5f);

		// Left stick for navigation
		rIo.AddKeyAnalogEvent(ImGuiKey_GamepadLStickUp, rRawInput.f2LeftThumbstick.y > 0.1f, rRawInput.f2LeftThumbstick.y);
		rIo.AddKeyAnalogEvent(ImGuiKey_GamepadLStickDown, rRawInput.f2LeftThumbstick.y < -0.1f, -rRawInput.f2LeftThumbstick.y);
		rIo.AddKeyAnalogEvent(ImGuiKey_GamepadLStickLeft, rRawInput.f2LeftThumbstick.x < -0.1f, -rRawInput.f2LeftThumbstick.x);
		rIo.AddKeyAnalogEvent(ImGuiKey_GamepadLStickRight, rRawInput.f2LeftThumbstick.x > 0.1f, rRawInput.f2LeftThumbstick.x);
	}

	return inputPoll;
}

void Input::CompletePoll()
{
	// Runs after every edge consumer, including the game callback and the modal path that skips it; moving this
	// earlier silently breaks edge detection for whatever still has to run.
	mPreviousRawInput = gpRawInputManager->mRawInput;
}

} // namespace engine

#endif // BT_CLIENT
