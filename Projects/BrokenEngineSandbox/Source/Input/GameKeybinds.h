#pragma once

#if defined(BT_CLIENT)

#include "Input/Keybinds.h"

namespace game
{

enum class GameKeyAction : uint8_t
{
	kToggleMap,

	kCount,
};

// ADL customization point for engine::InputPoll::Pressed / Held.
engine::VirtualKey KeybindFor(GameKeyAction eAction);
void SetKeybind(GameKeyAction eAction, engine::VirtualKey key);
void UnbindKey(engine::VirtualKey key);

// UI enumeration for KeybindsScreen's flat list.
std::span<const GameKeyAction> VisibleGameKeyActions();
engine::KeybindRowInfo DescribeKeybind(GameKeyAction eAction);

// Binary persistence uses kpcGameKeybindsPath and GameKeybindsFile::kiVersion.
bool LoadGameKeybinds();
void SaveGameKeybinds();
void ResetGameKeybinds();

} // namespace game

#endif // BT_CLIENT
