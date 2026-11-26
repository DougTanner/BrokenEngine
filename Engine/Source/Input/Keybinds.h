#pragma once

#if defined(BT_CLIENT)

#include "Input/RawInputManager.h"

namespace engine
{

// Zero means unbound. Nonzero codes below kiKeyboardKeyCount are Win32 virtual-key codes.
struct VirtualKey
{
	uint8_t uiCode = 0;

	constexpr bool IsBound() const
	{
		return uiCode != 0;
	}

	constexpr bool IsValid() const
	{
		return uiCode < static_cast<uint8_t>(kiKeyboardKeyCount);
	}

	constexpr bool operator==(const VirtualKey&) const = default;

	static constexpr VirtualKey Unbound()
	{
		return VirtualKey {};
	}

	static constexpr VirtualKey FromWin32(int64_t iVirtualKey)
	{
		return VirtualKey {.uiCode = static_cast<uint8_t>(iVirtualKey)};
	}
};
static_assert(sizeof(VirtualKey) == 1 && std::is_trivially_copyable_v<VirtualKey> && std::is_standard_layout_v<VirtualKey>, "VirtualKey must remain a 1-byte trivially copyable standard-layout wrapper");

// Engine-owned player-facing remappable actions only.
// Fixed inputs (Esc menu-back/cancel, Alt+F4 quit) and debug/profiling/debug-render
// shortcuts stay outside this enum.
enum class KeyAction : uint8_t
{
	kToggleFullscreen,
	kOpenChat,
	kToggleScreenshots,
	kCameraMoveForward,
	kCameraMoveBack,
	kCameraMoveLeft,
	kCameraMoveRight,

	kCount,
};

enum class KeybindDomain : uint8_t
{
	kEngine,
	kGame,
};

// Identifies one remappable action across either domain without exposing game
// enum types in engine headers.
struct KeybindSlot
{
	KeybindDomain eDomain = KeybindDomain::kEngine;
	uint8_t uiAction = 0;

	constexpr bool operator==(const KeybindSlot&) const = default;

	static constexpr KeybindSlot Engine(KeyAction eAction)
	{
		return KeybindSlot {.eDomain = KeybindDomain::kEngine, .uiAction = static_cast<uint8_t>(eAction)};
	}
};

// Non-owning view used by KeybindsScreen to render engine and game binds in one
// uniform flat list without section headers.
struct KeybindRowInfo
{
	KeybindSlot slot {};
	const char* pcId = nullptr;       // Stable ASCII suffix for ImGui/harness "##<id>"
	std::u32string_view label {};     // Localized uppercase display label
	VirtualKey key {};
};

// Single source of truth for bindable keyboard keys (shared by live capture and
// binary file validation). Rejects unbound (0), mouse VKs, VK_ESCAPE, VK_BACK,
// VK_DELETE, VK_MENU, system keys, and out-of-range codes; VK_F4 remains
// allowed in all builds.
bool IsBindableKey(VirtualKey key);

// Formats a key's player-facing uppercase name ("UNBOUND", "TAB", "F1", "W", ...)
// into caller-supplied buffer with zero heap allocation.
std::string_view FormatVirtualKeyName(VirtualKey key, std::span<char> buffer);

// InputPoll::Pressed and InputPoll::Held resolve KeybindFor through ADL.
VirtualKey KeybindFor(KeyAction eAction);
void SetKeybind(KeyAction eAction, VirtualKey key);
void UnbindKey(VirtualKey key);

// UI enumeration (omits kCameraMove* when !kbFreeCamera).
std::span<const KeyAction> VisibleEngineKeyActions();
KeybindRowInfo DescribeKeybind(KeyAction eAction);

// Versioned binary persistence (EngineKeybinds.bin, kiVersion = 1).
// LoadEngineKeybinds returns true when an existing file validated and loaded,
// false when missing or rejected (leaving defaults in place).
bool LoadEngineKeybinds();
void SaveEngineKeybinds();
void ResetEngineKeybinds();

} // namespace engine

#endif // BT_CLIENT
