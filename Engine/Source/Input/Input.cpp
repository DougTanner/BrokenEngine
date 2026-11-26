#include "Input.h"

#if defined(BT_CLIENT)

#include "Ui/MiscWrappersBase.h"

#include "Game.h"
#include "Input/GameKeybinds.h"

namespace engine
{

using enum MenuInputFlags;

constexpr float kfGamepadThreshold = 0.1f;

InputPoll::InputPoll(bool bLostFocus, bool bMenuVisible, MenuInput& rMenuInput)
: mrCurrent(gpRawInputManager->mRawInput)
, mrPrevious(gpInput->mPreviousRawInput)
{
	gpInput->BeginPoll(bLostFocus, bMenuVisible, rMenuInput, *this);
}

InputPoll::~InputPoll()
{
	gpInput->EndPoll();
}

XMFLOAT2 InputPoll::MoveAxis() const
{
	XMFLOAT2 f2Move {};
	static constexpr float kfFreeCameraAxis = 0.5f;
	if (Held(KeyAction::kCameraMoveForward))
	{
		f2Move.y += kfFreeCameraAxis;
	}
	if (Held(KeyAction::kCameraMoveBack))
	{
		f2Move.y -= kfFreeCameraAxis;
	}
	if (Held(KeyAction::kCameraMoveLeft))
	{
		f2Move.x -= kfFreeCameraAxis;
	}
	if (Held(KeyAction::kCameraMoveRight))
	{
		f2Move.x += kfFreeCameraAxis;
	}
	return f2Move;
}

bool InputPoll::PressedKey(VirtualKey key) const
{
	if (!key.IsBound())
	{
		return false;
	}

	return KeyboardPressed(static_cast<int64_t>(key.uiCode));
}

bool InputPoll::HeldKey(VirtualKey key) const
{
	if (!key.IsBound())
	{
		return false;
	}
	if ((muiPollFlags & (kuiFlagCaptureSuppressed | kuiFlagUserInterfaceOwnsKeyboard)) != 0)
	{
		return false;
	}

	return mrCurrent.pbKeyboardKeys[key.uiCode];
}

bool InputPoll::KeyboardPressed(int64_t iKey) const
{
	if ((muiPollFlags & kuiFlagCaptureSuppressed) != 0)
	{
		return false;
	}

	bool bUserInterfaceOwnsKeyboard = (muiPollFlags & kuiFlagUserInterfaceOwnsKeyboard) != 0;
	return (iKey == VK_TAB || !bUserInterfaceOwnsKeyboard) && mrCurrent.pbKeyboardKeys[iKey] && !mrPrevious.pbKeyboardKeys[iKey];
}

bool InputPoll::MousePressed(MouseButtons eButton) const
{
	return (mrCurrent.mouseButtons & eButton) && !(mrPrevious.mouseButtons & eButton);
}

bool InputPoll::GamepadPressed(GamepadButtons eButton) const
{
	return (mrCurrent.gamepadButtons & eButton) && !(mrPrevious.gamepadButtons & eButton);
}

Input::Input()
: common::Singleton<Input>(gpInput, this)
{
}

void Input::LoadAllKeybinds()
{
	bool bEngineLoaded = LoadEngineKeybinds();
	bool bGameLoaded = game::LoadGameKeybinds();

	// Resolve cross-domain startup conflicts: a loaded file overrides a missing/rejected file's
	// defaults, and when both files loaded and bind the same key, the engine binding wins with one warning.
	if (!bEngineLoaded && bGameLoaded)
	{
		for (int64_t i = 0; i < static_cast<int64_t>(game::GameKeyAction::kCount); ++i)
		{
			VirtualKey key = game::KeybindFor(static_cast<game::GameKeyAction>(i));
			if (key.IsBound())
			{
				UnbindKey(key);
			}
		}
	}
	else
	{
		for (int64_t i = 0; i < static_cast<int64_t>(KeyAction::kCount); ++i)
		{
			VirtualKey engineKey = KeybindFor(static_cast<KeyAction>(i));
			if (!engineKey.IsBound())
			{
				continue;
			}

			for (int64_t j = 0; j < static_cast<int64_t>(game::GameKeyAction::kCount); ++j)
			{
				game::GameKeyAction eGameAction = static_cast<game::GameKeyAction>(j);
				if (game::KeybindFor(eGameAction) == engineKey)
				{
					game::SetKeybind(eGameAction, VirtualKey::Unbound());
					if (bGameLoaded)
					{
						LOG(kLoading, kWarning, "Cross-domain keybind conflict on startup: engine keybind took precedence over game keybind");
					}
				}
			}
		}
	}
}

void Input::BeginKeybindCapture(KeybindSlot slot)
{
	mCaptureSlot = slot;
	mStateFlags.Set(InputStateFlags::kCapturingKeybind);
}

void Input::CancelKeybindCapture()
{
	mStateFlags.Set(InputStateFlags::kCapturingKeybind, false);
}

bool Input::IsCapturingKeybind() const
{
	return (mStateFlags & InputStateFlags::kCapturingKeybind) != 0;
}

bool Input::IsCapturingKeybind(KeybindSlot slot) const
{
	return IsCapturingKeybind() && mCaptureSlot == slot;
}

void Input::RebindSlot(KeybindSlot slot, VirtualKey key)
{
	if (key.IsBound())
	{
		UnbindKey(key);
		game::UnbindKey(key);
	}

	if (slot.eDomain == KeybindDomain::kEngine)
	{
		SetKeybind(static_cast<KeyAction>(slot.uiAction), key);
	}
	else
	{
		game::SetKeybind(static_cast<game::GameKeyAction>(slot.uiAction), key);
	}
}

void Input::ResetAllKeybinds()
{
	CancelKeybindCapture();
	ResetEngineKeybinds();
	game::ResetGameKeybinds();
}

void Input::BeginPoll(bool bLostFocus, bool bMenuVisible, MenuInput& rMenuInput, InputPoll& rInputPoll)
{
	gpRawInputManager->Update(bLostFocus);
	const RawInput& rRawInput = gpRawInputManager->mRawInput;

	if (mSuppressedChatOpenKey.IsBound() && (bLostFocus || !rRawInput.pbKeyboardKeys[mSuppressedChatOpenKey.uiCode]))
	{
		mSuppressedChatOpenKey = VirtualKey::Unbound();
	}

	// Seed only the wheel baseline; the other previous-snapshot fields remain zero so keys already held at startup
	// report fresh presses.
	if (!(mStateFlags & InputStateFlags::kScrollWheelInitialized))
	{
		mPreviousRawInput.iScrollWheelValue = rRawInput.iScrollWheelValue;
		mStateFlags.Set(InputStateFlags::kScrollWheelInitialized);
	}

	// The keyboard belongs to the UI while the last rendered frame had an editable text field active. ImGui resets
	// PlatformImeData.WantTextInput in NewFrame and sets it during that frame, so at poll time it describes the frame
	// just rendered; io.WantTextInput is one frame staler and io.WantCaptureKeyboard is also set by mouse-dragged sliders.
	const ImGuiContext* pImGuiContext = ImGui::GetCurrentContext();
	bool bUserInterfaceOwnsKeyboard = pImGuiContext->PlatformImeData.WantTextInput;
	rInputPoll.muiPollFlags = bUserInterfaceOwnsKeyboard ? InputPoll::kuiFlagUserInterfaceOwnsKeyboard : 0;

	bool bAltF4Quit = rRawInput.pbKeyboardKeys[VK_MENU] && rRawInput.pbKeyboardKeys[VK_F4] && !mPreviousRawInput.pbKeyboardKeys[VK_F4];
	if (mStateFlags & InputStateFlags::kCapturingKeybind)
	{
		// Suppress normal bindings and menu-back for the entire frame capture is active (including completion/cancel)
		// while preserving raw Alt+F4 quit.
		rInputPoll.muiPollFlags |= InputPoll::kuiFlagCaptureSuppressed;

		if (!bAltF4Quit)
		{
			bool bEscapePressed = rRawInput.pbKeyboardKeys[VK_ESCAPE] && !mPreviousRawInput.pbKeyboardKeys[VK_ESCAPE];
			bool bUnbindPressed = (rRawInput.pbKeyboardKeys[VK_BACK] && !mPreviousRawInput.pbKeyboardKeys[VK_BACK])
			                   || (rRawInput.pbKeyboardKeys[VK_DELETE] && !mPreviousRawInput.pbKeyboardKeys[VK_DELETE]);
			if (bEscapePressed)
			{
				CancelKeybindCapture();
			}
			else if (bUnbindPressed)
			{
				KeybindSlot slot = mCaptureSlot;
				CancelKeybindCapture();
				RebindSlot(slot, VirtualKey::Unbound());
			}
			else
			{
				for (int64_t i = 1; i < kiKeyboardKeyCount; ++i)
				{
					if (!rRawInput.pbKeyboardKeys[i] || mPreviousRawInput.pbKeyboardKeys[i])
					{
						continue;
					}
					VirtualKey candidate = VirtualKey::FromWin32(i);
					if (IsBindableKey(candidate))
					{
						KeybindSlot slot = mCaptureSlot;
						CancelKeybindCapture();
						RebindSlot(slot, candidate);
						break;
					}
				}
			}
		}
	}

	auto IsActionRawDown = [&rRawInput](KeyAction eAction) -> bool
	{
		VirtualKey key = KeybindFor(eAction);
		return key.IsBound() && rRawInput.pbKeyboardKeys[key.uiCode];
	};

	bool bKeyboardMouse = (rRawInput.mouseButtons & MouseButtons::kMouseButtonLeft)
	                   || (rRawInput.mouseButtons & MouseButtons::kMouseButtonRight) || IsActionRawDown(KeyAction::kCameraMoveLeft)
	                   || IsActionRawDown(KeyAction::kCameraMoveRight) || IsActionRawDown(KeyAction::kCameraMoveForward)
	                   || IsActionRawDown(KeyAction::kCameraMoveBack) || rRawInput.pbKeyboardKeys[VK_LEFT]
	                   || rRawInput.pbKeyboardKeys[VK_RIGHT] || rRawInput.pbKeyboardKeys[VK_UP] || rRawInput.pbKeyboardKeys[VK_DOWN]
	                   || rRawInput.pbKeyboardKeys[VK_NUMPAD1] || rRawInput.pbKeyboardKeys[VK_NUMPAD3]
	                   || rRawInput.pbKeyboardKeys[VK_NUMPAD5] || rRawInput.pbKeyboardKeys[VK_NUMPAD2];

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

	bool bQuit = bAltF4Quit;
	if constexpr (kbDebugInput)
	{
		bQuit |= rInputPoll.KeyboardPressed(VK_F4);
	}
	rMenuInput.flags.Set(kQuit, bQuit);
	rMenuInput.flags.Set(kMouseIsDown, rRawInput.mouseButtons & MouseButtons::kMouseButtonLeft);
	rMenuInput.flags.Set(kMouseClick, rInputPoll.MousePressed(MouseButtons::kMouseButtonLeft));
	rMenuInput.flags.Set(kGamepadButton, rInputPoll.GamepadPressed(GamepadButtons::kGamepadButtonA));
	if constexpr (kbProfiling)
	{
		rMenuInput.flags.Set(kToggleProfileText, rInputPoll.KeyboardPressed('P'));
	}
	if constexpr (kbDebugRender)
	{
		rMenuInput.flags.Set(kToggleDebugRender, rInputPoll.KeyboardPressed('Q'));
	}
	if constexpr (kbDebugInput)
	{
		rMenuInput.flags.Set(kTogglePauseFrame, rInputPoll.KeyboardPressed(VK_SPACE));
		rMenuInput.flags.Set(kQuicksave, rInputPoll.KeyboardPressed(VK_F5));
		rMenuInput.flags.Set(kQuickload, rInputPoll.KeyboardPressed(VK_F6));
		rMenuInput.flags.Set(kSaveReplay, rInputPoll.KeyboardPressed(VK_F7));
		rMenuInput.flags.Set(kLoadReplay, rInputPoll.KeyboardPressed(VK_F8));
		rMenuInput.flags.Set(kSlowTime, rInputPoll.KeyboardPressed(VK_OEM_MINUS));
		rMenuInput.flags.Set(kSpeedUpTime, rInputPoll.KeyboardPressed(VK_OEM_PLUS));
		rMenuInput.flags.Set(kSingleStep, rInputPoll.KeyboardPressed(VK_TAB));
	}

	rMenuInput.f2Mouse = rRawInput.f2MousePosition;
	rMenuInput.f2Gamepad = rRawInput.f2LeftThumbstick;

	bool bCaptureSuppressed = (rInputPoll.muiPollFlags & InputPoll::kuiFlagCaptureSuppressed) != 0;
	rMenuInput.flags.Set(kPauseMenu, !bCaptureSuppressed && (rInputPoll.KeyboardPressed(VK_ESCAPE) || rInputPoll.MousePressed(MouseButtons::kMouseButtonMiddle) || rInputPoll.GamepadPressed(GamepadButtons::kGamepadMenu) || rInputPoll.GamepadPressed(GamepadButtons::kGamepadButtonB)));
	if constexpr (kbDebugInput)
	{
		rMenuInput.flags.Set(kMenuDebugTexture, rInputPoll.KeyboardPressed(VK_F2));
		rMenuInput.flags.Set(kDebugTextureNext, gpCamera->meObserverState != CameraBase::ObserverState::kObserving && rInputPoll.KeyboardPressed(VK_RIGHT));
		rMenuInput.flags.Set(kDebugTexturePrevious, gpCamera->meObserverState != CameraBase::ObserverState::kObserving && rInputPoll.KeyboardPressed(VK_LEFT));
		rMenuInput.flags.Set(kMenuTweaks, rInputPoll.KeyboardPressed(VK_F3));
	}

	const ImGuiWindow* pHoveredWindow = pImGuiContext->HoveredWindow;

	XMFLOAT2 f2Move {};
	if (gpCamera->meObserverState == CameraBase::ObserverState::kObserving)
	{
		bool bMapOpen = game::gpGame->MapOpen();
		if ((!bLostFocus || (gpAgentInput != nullptr && gpAgentInput->mbScriptActive)) && !bUserInterfaceOwnsKeyboard && !bMenuVisible && !bMapOpen)
		{
			if (rRawInput.pbKeyboardKeys[VK_UP])
			{
				f2Move.y += 1.0f;
			}
			if (rRawInput.pbKeyboardKeys[VK_DOWN])
			{
				f2Move.y -= 1.0f;
			}
			if (rRawInput.pbKeyboardKeys[VK_LEFT])
			{
				f2Move.x -= 1.0f;
			}
			if (rRawInput.pbKeyboardKeys[VK_RIGHT])
			{
				f2Move.x += 1.0f;
			}
		}

		float fEdgeMargin = gObserverEdgeScrollMargin.mfCurrent;
		if (!bLostFocus && !PhysicalInputSuppressed() && !bMenuVisible && !(mStateFlags & InputStateFlags::kGamepadMode) && !bMapOpen && pHoveredWindow == nullptr && fEdgeMargin > 0.0f)
		{
			float fWidth = static_cast<float>(gpGraphics->mFramebufferVkExtent2D.width);
			float fHeight = static_cast<float>(gpGraphics->mFramebufferVkExtent2D.height);
			if (fWidth > 0.0f && fHeight > 0.0f)
			{
				float fMouseX = rRawInput.f2MousePosition.x * fWidth;
				float fMouseY = rRawInput.f2MousePosition.y * fHeight;
				if (fMouseX >= 0.0f && fMouseX <= fWidth && fMouseY >= 0.0f && fMouseY <= fHeight)
				{
					if (fMouseX < fEdgeMargin)
					{
						f2Move.x -= (fEdgeMargin - fMouseX) / fEdgeMargin;
					}
					else if (fMouseX > fWidth - fEdgeMargin)
					{
						f2Move.x += (fEdgeMargin - (fWidth - fMouseX)) / fEdgeMargin;
					}

					if (fMouseY < fEdgeMargin)
					{
						f2Move.y += (fEdgeMargin - fMouseY) / fEdgeMargin;
					}
					else if (fMouseY > fHeight - fEdgeMargin)
					{
						f2Move.y -= (fEdgeMargin - (fHeight - fMouseY)) / fEdgeMargin;
					}
				}
			}
		}

		float fLengthSquared = f2Move.x * f2Move.x + f2Move.y * f2Move.y;
		if (fLengthSquared > 1.0f)
		{
			float fInverseLength = 1.0f / std::sqrt(fLengthSquared);
			f2Move.x *= fInverseLength;
			f2Move.y *= fInverseLength;
		}
	}
	else if constexpr (kbFreeCamera)
	{
		f2Move = rInputPoll.MoveAxis();
	}
	gpCamera->mCameraInput.f2Move = f2Move;

	// ImGui owns the wheel when MouseWheelY has an owner, WheelingWindow holds a target, or the hovered window can
	// scroll and accepts wheel input. Non-scrolling panels, modals, and background leave it with the camera regardless
	// of open menus. This mirrors UpdateMouseWheel for this root-window-only UI; child-window bubbling is absent.
	// WheelingWindow retains its earlier target, and ImPlot's owner suppresses notches for two polls after exit; the
	// next owner-free poll can zoom. Hover resolves in NewFrame after this poll, so panel-edge crossings use the prior
	// hover.
	bool bUserInterfaceOwnsScroll = pImGuiContext->WheelingWindow != nullptr
	                             || !ImGui::TestKeyOwner(ImGuiKey_MouseWheelY, ImGuiKeyOwner_NoOwner)
	                             || (pHoveredWindow != nullptr && !pHoveredWindow->Collapsed && pHoveredWindow->ScrollMax.y != 0.0f && (pHoveredWindow->Flags & (ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMouseInputs)) == 0);
	// The previous snapshot advances after every edge consumer even when the wheel is swallowed, so suppressed
	// notches cannot surface later as zoom.
	gpCamera->mCameraInput.iScrollDelta = bUserInterfaceOwnsScroll ? 0 : rRawInput.iScrollWheelValue - mPreviousRawInput.iScrollWheelValue;

	// Populate ImGui gamepad inputs when menus are visible.
	if (bMenuVisible)
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
}

void Input::EndPoll()
{
	mPreviousRawInput = gpRawInputManager->mRawInput;
}

} // namespace engine

#endif // BT_CLIENT
