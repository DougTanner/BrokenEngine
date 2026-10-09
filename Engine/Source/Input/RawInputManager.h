#pragma once

namespace engine
{

inline constexpr int64_t kiKeyboardKeyCount = 0xFF;

enum class MouseButtons : uint32_t
{
	kMouseButtonLeft     = 1ui32 << 0,
	kMouseButtonMiddle   = 1ui32 << 1,
	kMouseButtonRight    = 1ui32 << 2,
	kMouseButtonExtraOne = 1ui32 << 3,
	kMouseButtonExtraTwo = 1ui32 << 4,
};

enum class GamepadButtons : uint32_t
{
	kGamepadButtonA       = 1ui32 << 0,
	kGamepadButtonB       = 1ui32 << 1,
	kGamepadButtonX       = 1ui32 << 2,
	kGamepadButtonY       = 1ui32 << 3,

	kGamepadLeftShoulder  = 1ui32 << 4,
	kGamepadRightShoulder = 1ui32 << 5,

	kGamepadStart         = 1ui32 << 6,
	kGamepadMenu          = 1ui32 << 7,
};

struct RawInput
{
	bool pbKeyboardKeys[kiKeyboardKeyCount] {};

	common::Flags<MouseButtons> mouseButtons {};
	XMFLOAT2 f2MousePosition {};
	int64_t iScrollWheelValue = 0;

	common::Flags<GamepadButtons> gamepadButtons {};
	XMFLOAT2 f2LeftThumbstick {};
	XMFLOAT2 f2RightThumbstick {};
	XMFLOAT2 f2DirectionalPad {};
};

#if defined(BT_CLIENT)
class RawInputManager : public common::Singleton<RawInputManager>
{
public:

	RawInputManager();

	void HandleRawInput(LPARAM iMessageParameter);
	void UpdateFocus(bool bHasFocus, HWND hWindow);
	bool SetVibration(float fLeftMotor, float fRightMotor);

	void Update(bool bLostFocus);

	RawInput mRawInput {};

private:

	enum class RawInputStateFlags : uint8_t
	{
		kGamePadConnected = 1 << 0,
		kHasFocus         = 1 << 1,
	};

	void TrapCursor(bool bTrap);

	HWND mhWindow = nullptr;

	bool mpbKeyboardKeysDown[kiKeyboardKeyCount] {};
	Mouse mMouse;

	std::unique_ptr<GamePad> mpGamePad;
	common::Flags<RawInputStateFlags> mStateFlags;
};

inline RawInputManager* gpRawInputManager = nullptr;
#endif // BT_CLIENT

} // namespace engine
