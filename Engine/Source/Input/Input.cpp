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

	// Seed only the wheel baseline; the other previous-snapshot fields remain zero so keys already held at startup
	// report fresh presses.
	if (!(mStateFlags & InputStateFlags::kScrollWheelInitialized))
	{
		mPreviousRawInput.iScrollWheelValue = rRawInput.iScrollWheelValue;
		mStateFlags.Set(InputStateFlags::kScrollWheelInitialized);
	}

	InputPoll inputPoll(rRawInput, mPreviousRawInput);

	bool bKeyboardMouse = (rRawInput.mouseButtons & MouseButtons::kMouseButtonLeft) || (rRawInput.mouseButtons & MouseButtons::kMouseButtonRight) || rRawInput.pbKeyboardKeys['A'] || rRawInput.pbKeyboardKeys['D'] || rRawInput.pbKeyboardKeys['W'] || rRawInput.pbKeyboardKeys['S'] || rRawInput.pbKeyboardKeys[VK_LEFT] || rRawInput.pbKeyboardKeys[VK_RIGHT] || rRawInput.pbKeyboardKeys[VK_UP] || rRawInput.pbKeyboardKeys[VK_DOWN] || rRawInput.pbKeyboardKeys[VK_NUMPAD1] || rRawInput.pbKeyboardKeys[VK_NUMPAD3] || rRawInput.pbKeyboardKeys[VK_NUMPAD5] || rRawInput.pbKeyboardKeys[VK_NUMPAD2];

	if (bKeyboardMouse)
	{
		mStateFlags.Set(InputStateFlags::kGamepadMode, false);
	}
	else if (std::abs(rRawInput.f2LeftThumbstick.x) + std::abs(rRawInput.f2LeftThumbstick.y) > kfGamepadThreshold || std::abs(rRawInput.f2RightThumbstick.x) + std::abs(rRawInput.f2RightThumbstick.y) > kfGamepadThreshold)
	{
		mStateFlags.Set(InputStateFlags::kGamepadMode);
	}
	else if (!::operator==(rRawInput.f2MousePosition, mPreviousRawInput.f2MousePosition))
	{
		mStateFlags.Set(InputStateFlags::kGamepadMode, false);
	}

	rMenuInput.bGamepad = mStateFlags & InputStateFlags::kGamepadMode;

	rMenuInput.flags.Set(kQuit, rRawInput.pbKeyboardKeys[VK_MENU] && inputPoll.KeyboardPressed(VK_F4));
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

	rMenuInput.flags.Set(kPauseMenu, inputPoll.KeyboardPressed(VK_ESCAPE) || inputPoll.MousePressed(MouseButtons::kMouseButtonMiddle) || inputPoll.GamepadPressed(GamepadButtons::kGamepadMenu) || inputPoll.GamepadPressed(GamepadButtons::kGamepadButtonB));
	if constexpr (kbDebugInput)
	{
		rMenuInput.flags.Set(kMenuDebugTexture, inputPoll.KeyboardPressed(VK_F2));
		rMenuInput.flags.Set(kDebugTextureNext, inputPoll.KeyboardPressed(VK_RIGHT));
		rMenuInput.flags.Set(kDebugTexturePrevious, inputPoll.KeyboardPressed(VK_LEFT));
		rMenuInput.flags.Set(kMenuTweaks, inputPoll.KeyboardPressed(VK_F3));
	}

	XMFLOAT2 f2Move {};
	if constexpr (kbFreeCamera)
	{
		static constexpr float kfFreeCameraAxis = 0.5f;
		if (rRawInput.pbKeyboardKeys['W'])
		{
			f2Move.y += kfFreeCameraAxis;
		}
		if (rRawInput.pbKeyboardKeys['S'])
		{
			f2Move.y -= kfFreeCameraAxis;
		}
		if (rRawInput.pbKeyboardKeys['A'])
		{
			f2Move.x -= kfFreeCameraAxis;
		}
		if (rRawInput.pbKeyboardKeys['D'])
		{
			f2Move.x += kfFreeCameraAxis;
		}
	}
	gpCamera->mCameraInput.f2Move = f2Move;

	// ImGui owns the wheel when MouseWheelY has an owner, WheelingWindow holds a target, or the hovered window can
	// scroll and accepts wheel input. Non-scrolling panels, modals, and background leave it with the camera regardless
	// of open menus. This mirrors UpdateMouseWheel for this root-window-only UI; child-window bubbling is absent.
	// WheelingWindow retains its earlier target, and ImPlot's owner suppresses notches for two polls after exit; the
	// next owner-free poll can zoom. Hover resolves in NewFrame after this poll, so panel-edge crossings use the prior
	// hover.
	const ImGuiContext* pImGuiContext = ImGui::GetCurrentContext();
	const ImGuiWindow* pHoveredWindow = pImGuiContext != nullptr ? pImGuiContext->HoveredWindow : nullptr;
	bool bUserInterfaceOwnsScroll = (pImGuiContext != nullptr && pImGuiContext->WheelingWindow != nullptr)
	                             || (pImGuiContext != nullptr && !ImGui::TestKeyOwner(ImGuiKey_MouseWheelY, ImGuiKeyOwner_NoOwner))
	                             || (pHoveredWindow != nullptr && !pHoveredWindow->Collapsed && pHoveredWindow->ScrollMax.y != 0.0f && !(pHoveredWindow->Flags & (ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMouseInputs)));
	// The previous snapshot advances after every edge consumer even when the wheel is swallowed, so suppressed
	// notches cannot surface later as zoom.
	gpCamera->mCameraInput.iScrollDelta = bUserInterfaceOwnsScroll ? 0 : rRawInput.iScrollWheelValue - mPreviousRawInput.iScrollWheelValue;

	// Populate ImGui gamepad inputs when menus are visible. Guard GetIO(): the ImGui context can be destroyed across a
	// multi-frame deferred swapchain recreate (minimized client), and this fires every deferred frame if a menu was open.
	if (bMenuVisible && ImGui::GetCurrentContext() != nullptr)
	{
		ImGuiIO& rInputOutput = ImGui::GetIO();
		rInputOutput.BackendFlags |= ImGuiBackendFlags_HasGamepad;

		rInputOutput.AddKeyEvent(ImGuiKey_GamepadFaceDown, rRawInput.gamepadButtons & GamepadButtons::kGamepadButtonA);
		rInputOutput.AddKeyEvent(ImGuiKey_GamepadFaceRight, rRawInput.gamepadButtons & GamepadButtons::kGamepadButtonB);
		rInputOutput.AddKeyEvent(ImGuiKey_GamepadStart, rRawInput.gamepadButtons & GamepadButtons::kGamepadMenu);

		rInputOutput.AddKeyEvent(ImGuiKey_GamepadDpadUp, rRawInput.f2DirectionalPad.y > 0.5f);
		rInputOutput.AddKeyEvent(ImGuiKey_GamepadDpadDown, rRawInput.f2DirectionalPad.y < -0.5f);
		rInputOutput.AddKeyEvent(ImGuiKey_GamepadDpadLeft, rRawInput.f2DirectionalPad.x < -0.5f);
		rInputOutput.AddKeyEvent(ImGuiKey_GamepadDpadRight, rRawInput.f2DirectionalPad.x > 0.5f);

		rInputOutput.AddKeyAnalogEvent(ImGuiKey_GamepadLStickUp, rRawInput.f2LeftThumbstick.y > 0.1f, rRawInput.f2LeftThumbstick.y);
		rInputOutput.AddKeyAnalogEvent(ImGuiKey_GamepadLStickDown, rRawInput.f2LeftThumbstick.y < -0.1f, -rRawInput.f2LeftThumbstick.y);
		rInputOutput.AddKeyAnalogEvent(ImGuiKey_GamepadLStickLeft, rRawInput.f2LeftThumbstick.x < -0.1f, -rRawInput.f2LeftThumbstick.x);
		rInputOutput.AddKeyAnalogEvent(ImGuiKey_GamepadLStickRight, rRawInput.f2LeftThumbstick.x > 0.1f, rRawInput.f2LeftThumbstick.x);
	}

	return inputPoll;
}

} // namespace engine

#endif // BT_CLIENT
