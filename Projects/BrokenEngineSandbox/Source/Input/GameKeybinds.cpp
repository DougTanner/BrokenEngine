#include "Input/GameKeybinds.h"

#if defined(BT_CLIENT)

namespace game
{

struct GameKeybindsFile
{
	static constexpr int64_t kiVersion = 1;

	engine::VirtualKey pKeys[static_cast<size_t>(GameKeyAction::kCount)] {};
};
static_assert(std::is_trivially_copyable_v<GameKeybindsFile>, "GameKeybindsFile must stay trivially copyable — WriteVersionedFile stamps sizeof");
static_assert(std::is_standard_layout_v<GameKeybindsFile>, "GameKeybindsFile must stay standard-layout — BT_OFFSETOF below is only well-defined for standard-layout types");
static_assert(BT_OFFSETOF(GameKeybindsFile, pKeys) == 0, "GameKeybindsFile::pKeys offset changed");
static_assert(sizeof(GameKeybindsFile) == static_cast<size_t>(GameKeyAction::kCount), "GameKeybindsFile::kiVersion must be bumped with this layout");

constexpr char kpcGameKeybindsPath[] = "GameKeybinds.bin";

constexpr engine::VirtualKey kpDefaultGameKeybinds[static_cast<size_t>(GameKeyAction::kCount)]
{
	engine::VirtualKey::FromWin32(VK_TAB), // kToggleMap
};

static engine::VirtualKey spGameKeybinds[static_cast<size_t>(GameKeyAction::kCount)]
{
	kpDefaultGameKeybinds[0],
};

engine::VirtualKey KeybindFor(GameKeyAction eAction)
{
	return spGameKeybinds[static_cast<size_t>(eAction)];
}

void SetKeybind(GameKeyAction eAction, engine::VirtualKey key)
{
	spGameKeybinds[static_cast<size_t>(eAction)] = key;
}

void UnbindKey(engine::VirtualKey key)
{
	for (engine::VirtualKey& rKey : spGameKeybinds)
	{
		if (rKey == key)
		{
			rKey = engine::VirtualKey::Unbound();
		}
	}
}

std::span<const GameKeyAction> VisibleGameKeyActions()
{
	static constexpr GameKeyAction kpActions[]
	{
		GameKeyAction::kToggleMap,
	};
	return kpActions;
}

engine::KeybindRowInfo DescribeKeybind(GameKeyAction eAction)
{
	return engine::KeybindRowInfo
	{
		.slot = engine::KeybindSlot {.eDomain = engine::KeybindDomain::kGame, .uiAction = static_cast<uint8_t>(eAction)},
		.pcId = "KeybindToggleMap",
		.label = U"TOGGLE MAP",
		.key = KeybindFor(eAction),
	};
}

bool LoadGameKeybinds()
{
	// Heap: file I/O allocates
	ScopedSuppressAllocationTracking suppress;

	std::fstream fileStream = engine::gpFileManager->OpenFile({engine::FileFlags::kRead}, kpcGameKeybindsPath);
	if (!fileStream.is_open())
	{
		return false;
	}

	int64_t iVersion = 0;
	int64_t iSize = 0;
	bool bHeaderValid = engine::ReadAndValidateVersionHeader<GameKeybindsFile>(fileStream, iVersion, iSize);
	if (!bHeaderValid || !fileStream.good())
	{
		LOG(kLoading, kWarning, "{} rejected: invalid header (version {} size {})", kpcGameKeybindsPath, iVersion, iSize);
		return false;
	}

	GameKeybindsFile fileData {};
	common::Read(fileStream, fileData);
	if (fileStream.gcount() != static_cast<std::streamsize>(sizeof(GameKeybindsFile)) || fileStream.fail() || fileStream.peek() != std::char_traits<char>::eof())
	{
		LOG(kLoading, kWarning, "{} rejected: invalid payload size", kpcGameKeybindsPath);
		return false;
	}

	bool pbSeen[engine::kiKeyboardKeyCount] {};
	for (engine::VirtualKey key : fileData.pKeys)
	{
		if (!key.IsBound())
		{
			continue;
		}
		if (!engine::IsBindableKey(key) || pbSeen[key.uiCode])
		{
			LOG(kLoading, kWarning, "{} rejected: invalid or duplicate virtual key {}", kpcGameKeybindsPath, static_cast<int64_t>(key.uiCode));
			return false;
		}
		pbSeen[key.uiCode] = true;
	}

	std::copy(std::begin(fileData.pKeys), std::end(fileData.pKeys), std::begin(spGameKeybinds));
	return true;
}

void SaveGameKeybinds()
{
	// Heap: file I/O allocates
	ScopedSuppressAllocationTracking suppress;

	GameKeybindsFile fileData {};
	std::copy(std::begin(spGameKeybinds), std::end(spGameKeybinds), std::begin(fileData.pKeys));
	engine::WriteVersionedFile(kpcGameKeybindsPath, fileData);
}

void ResetGameKeybinds()
{
	std::copy(std::begin(kpDefaultGameKeybinds), std::end(kpDefaultGameKeybinds), std::begin(spGameKeybinds));
}

} // namespace game

#endif // BT_CLIENT
