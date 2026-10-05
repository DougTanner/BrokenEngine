#include "RawInputManager.h"

#if defined(BT_CLIENT)

#include "Game.h"

namespace engine
{

RawInputManager::RawInputManager()
{
	ASSERT(gpRawInputManager == nullptr);

	gpRawInputManager = this;

	try
	{
		mpGamePad = std::make_unique<GamePad>();
	}
	catch ([[maybe_unused]] const std::exception& rException)
	{
		LOG(kInput, kError, "Failed GamePad: {}", rException.what());
	}
	catch (...)
	{
		LOG(kInput, kError, "Failed GamePad");
	}
}

RawInputManager::~RawInputManager()
{
	if (gpRawInputManager == this)
	{
		gpRawInputManager = nullptr;
	}
}

void RawInputManager::UpdateFocus(bool bHasFocus, HWND hWindow)
{
	mStateFlags.Set(RawInputStateFlags::kHasFocus, bHasFocus);
	mhWindow = hWindow;

	// Keyboard HID (usage page 0x01, usage 0x06). Register and unregister share the same device struct and
	// differ only in dwFlags: RIDEV_NOLEGACY adds the HID keyboard and ignores legacy keyboard messages;
	// RIDEV_REMOVE unregisters it.
	RAWINPUTDEVICE rawInputDevice
	{
		.usUsagePage = 0x01,
		.usUsage = 0x06,
		.dwFlags = static_cast<DWORD>(bHasFocus ? RIDEV_NOLEGACY : RIDEV_REMOVE),
		.hwndTarget = nullptr,
	};
	if (bHasFocus)
	{
		LOG(kInput, kInfo, "RegisterRawInputDevices");
		if (RegisterRawInputDevices(&rawInputDevice, 1, sizeof(RAWINPUTDEVICE)) == FALSE)
		{
			// Heap: common::LastErrorString() returns a std::string by value (exceeds SSO), and this LOG sits in the allocation-tracked main loop
			ScopedSuppressAllocationTracking suppress;
			LOG(kInput, kError, "Failed to register raw input: {}", common::LastErrorString().data());
		}

		if (mpGamePad != nullptr)
		{
			mpGamePad->Resume();
		}

		std::fill(std::begin(mpbKeyboardKeysDown), std::end(mpbKeyboardKeysDown), false);
	}
	else
	{
		TrapCursor(false);

		LOG(kInput, kInfo, "UnregisterRawInputDevices");
		if (RegisterRawInputDevices(&rawInputDevice, 1, sizeof(RAWINPUTDEVICE)) == FALSE)
		{
			// Heap: common::LastErrorString() returns a std::string by value (exceeds SSO), and this LOG sits in the allocation-tracked main loop
			ScopedSuppressAllocationTracking suppress;
			LOG(kInput, kError, "Failed to unregister raw input: {}", common::LastErrorString().data());
		}

		if (mpGamePad != nullptr)
		{
			mpGamePad->Suspend();
		}
	}
}

bool RawInputManager::SetVibration(int64_t iPlayer, float fLeftMotor, float fRightMotor, float fLeftTrigger, float fRightTrigger)
{
	if (mpGamePad != nullptr)
	{
		return mpGamePad->SetVibration(static_cast<int>(iPlayer), fLeftMotor, fRightMotor, fLeftTrigger, fRightTrigger);
	}

	return false;
}

void RawInputManager::TrapCursor(bool bTrap)
{
	if (bTrap)
	{
		RECT rectangle {};
		GetClientRect(mhWindow, &rectangle);
		POINT pointUpperLeft {};
		pointUpperLeft.x = rectangle.left;
		pointUpperLeft.y = rectangle.top;
		POINT pointLowerRight {};
		pointLowerRight.x = rectangle.right;
		pointLowerRight.y = rectangle.bottom;
		MapWindowPoints(mhWindow, nullptr, &pointUpperLeft, 1);
		MapWindowPoints(mhWindow, nullptr, &pointLowerRight, 1);
		rectangle.left = pointUpperLeft.x;
		rectangle.top = pointUpperLeft.y;
		rectangle.right = pointLowerRight.x;
		rectangle.bottom = pointLowerRight.y;
		ClipCursor(&rectangle);
	}
	else
	{
		ClipCursor(nullptr);
	}
}

void RawInputManager::Update(bool bLostFocus)
{
	if (bLostFocus) [[unlikely]]
	{
		TrapCursor(false);
	}
	else
	{
		// Suppressed harness client forces the trap off regardless of game setting so a physical cursor is never clipped to the window.
		TrapCursor(PhysicalInputSuppressed() ? false : !(game::gpGame->mGameFlags & engine::GameFlags::kMainMenu));
	}

	// A running agent input script relaxes the unfocused early-out so it can publish + overlay while the window is
	// unfocused (the norm for the harness). Off the script path this stays the plain focus gate.
	bool bScriptActive = gpAgentInput != nullptr && gpAgentInput->mbScriptActive;
	bool bHasFocus = mStateFlags & RawInputStateFlags::kHasFocus;
	if (!bHasFocus && !bScriptActive)
	{
		return;
	}

	if (bHasFocus)
	{
		std::copy(std::begin(mpbKeyboardKeysDown), std::end(mpbKeyboardKeysDown), std::begin(mRawInput.pbKeyboardKeys));
	}
	else
	{
		// Relaxed unfocused-publish path: the hardware keyboard is unregistered (RIDEV_REMOVE) while unfocused, so
		// mpbKeyboardKeysDown is frozen at its last focused state — zero the published keyboard so the overlay ORs
		// synthetic keys onto a clean snapshot instead of republishing stuck key bits for the whole script.
		std::fill(std::begin(mRawInput.pbKeyboardKeys), std::end(mRawInput.pbKeyboardKeys), false);
	}

	Mouse::State mouseState = mMouse.GetState();
	mRawInput.f2MousePosition.x = static_cast<float>(mouseState.x) / static_cast<float>(gpGraphics->mFramebufferVkExtent2D.width);
	mRawInput.f2MousePosition.y = static_cast<float>(mouseState.y) / static_cast<float>(gpGraphics->mFramebufferVkExtent2D.height);
	mRawInput.mouseButtons.Set(MouseButtons::kMouseButtonLeft, mouseState.leftButton);
	mRawInput.mouseButtons.Set(MouseButtons::kMouseButtonMiddle, mouseState.middleButton);
	mRawInput.mouseButtons.Set(MouseButtons::kMouseButtonRight, mouseState.rightButton);
	mRawInput.mouseButtons.Set(MouseButtons::kMouseButtonExtraOne, mouseState.xButton1);
	mRawInput.mouseButtons.Set(MouseButtons::kMouseButtonExtraTwo, mouseState.xButton2);
	mRawInput.iScrollWheelValue = mouseState.scrollWheelValue;

	// Consumers difference iScrollWheelValue, so retain the synthetic scroll offset on every publish to avoid phantom zoom when scripts end or focus returns.
	// AgentInput may be absent; this is the sole addition of its offset, and Overlay does not add it.
	if (gpAgentInput != nullptr)
	{
		mRawInput.iScrollWheelValue += gpAgentInput->miSyntheticScrollAccumulator;
	}

	// Game pad (only first game pad supported). Suppressed harness client skips the poll — the snapshot stays zero-initialized.
	if (mpGamePad != nullptr && !PhysicalInputSuppressed())
	{
		GamePad::State gamepadState = mpGamePad->GetState(0);
		if (gamepadState.IsConnected())
		{
			if (!(mStateFlags & RawInputStateFlags::kGamePadConnected))
			{
				mStateFlags.Set(RawInputStateFlags::kGamePadConnected);
				LOG(kInput, kInfo, "Game pad connected: {}", static_cast<int64_t>(mpGamePad->GetCapabilities(0).gamepadType));
			}

			mRawInput.f2LeftThumbstick.x = gamepadState.thumbSticks.leftX;
			mRawInput.f2LeftThumbstick.y = gamepadState.thumbSticks.leftY;
			mRawInput.f2RightThumbstick.x = gamepadState.thumbSticks.rightX;
			mRawInput.f2RightThumbstick.y = gamepadState.thumbSticks.rightY;

			mRawInput.f2DirectionalPad.x = gamepadState.dpad.left ? -1.0f : (gamepadState.dpad.right ? 1.0f : 0.0f);
			mRawInput.f2DirectionalPad.y = gamepadState.dpad.up ? 1.0f : (gamepadState.dpad.down ? -1.0f : 0.0f);

			mRawInput.gamepadButtons.Set(GamepadButtons::kGamepadButtonA, gamepadState.IsAPressed());
			mRawInput.gamepadButtons.Set(GamepadButtons::kGamepadButtonB, gamepadState.IsBPressed());
			mRawInput.gamepadButtons.Set(GamepadButtons::kGamepadButtonX, gamepadState.IsXPressed());
			mRawInput.gamepadButtons.Set(GamepadButtons::kGamepadButtonY, gamepadState.IsYPressed());
			mRawInput.gamepadButtons.Set(GamepadButtons::kGamepadLeftShoulder, gamepadState.IsLeftShoulderPressed());
			mRawInput.gamepadButtons.Set(GamepadButtons::kGamepadRightShoulder, gamepadState.IsRightShoulderPressed());
			mRawInput.gamepadButtons.Set(GamepadButtons::kGamepadStart, gamepadState.IsStartPressed());
			mRawInput.gamepadButtons.Set(GamepadButtons::kGamepadMenu, gamepadState.IsMenuPressed());
		}
		else
		{
			if (mStateFlags & RawInputStateFlags::kGamePadConnected)
			{
				mStateFlags.Set(RawInputStateFlags::kGamePadConnected, false);
				LOG(kInput, kInfo, "Game pad disconnected");
			}

			mRawInput.f2LeftThumbstick.x = 0.0f;
			mRawInput.f2LeftThumbstick.y = 0.0f;
			mRawInput.f2RightThumbstick.x = 0.0f;
			mRawInput.f2RightThumbstick.y = 0.0f;

			mRawInput.f2DirectionalPad.x = 0.0f;
			mRawInput.f2DirectionalPad.y = 0.0f;

			mRawInput.gamepadButtons = {};
		}
	}

	// Overlay ORs synthetic keys and mouse buttons and overrides synthetic mouse position; it does not add scroll.
	// Run it after the keyboard copy so that copy does not overwrite synthetic key bits.
	if (bScriptActive)
	{
		gpAgentInput->Overlay(mRawInput);
	}
}

void RawInputManager::HandleRawInput(LPARAM iMessageParameter)
{
	HRAWINPUT hRawInput = reinterpret_cast<HRAWINPUT>(iMessageParameter);

	// Only the fixed-size keyboard usage is registered (UpdateFocus), never variable-length RAWHID, so sizeof(RAWINPUT) bounds every packet
	RAWINPUT rawInput {};
	UINT uiRawInputBytes = sizeof(rawInput);
	if (GetRawInputData(hRawInput, RID_INPUT, &rawInput, &uiRawInputBytes, sizeof(RAWINPUTHEADER)) == static_cast<UINT>(-1))
	{
		// Heap: common::LastErrorString() returns a std::string by value (exceeds SSO), and this LOG sits in the allocation-tracked main loop
		ScopedSuppressAllocationTracking suppress;
		LOG(kInput, kWarning, "GetRawInputData failed: {}", common::LastErrorString().data());
		DEBUG_BREAK();
		return;
	}

	if (rawInput.header.dwType == RIM_TYPEKEYBOARD && !PhysicalInputSuppressed())
	{
		int64_t iKey = rawInput.data.keyboard.VKey;
		if (iKey < kiKeyboardKeyCount)
		{
			mpbKeyboardKeysDown[iKey] = (rawInput.data.keyboard.Flags & RI_KEY_BREAK) == 0;
		}
	}
}

} // namespace engine

#endif
