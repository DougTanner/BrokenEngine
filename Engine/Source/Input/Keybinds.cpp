#include "Input/Keybinds.h"

#if defined(BT_CLIENT)

#include "Ui/LocalizationBase.h"

namespace engine
{

struct EngineKeybindsFile
{
	static constexpr int64_t kiVersion = 1;

	VirtualKey pKeys[static_cast<size_t>(KeyAction::kCount)] {};
};
static_assert(std::is_trivially_copyable_v<EngineKeybindsFile>, "EngineKeybindsFile must stay trivially copyable — WriteVersionedFile stamps sizeof");
static_assert(std::is_standard_layout_v<EngineKeybindsFile>, "EngineKeybindsFile must stay standard-layout — BT_OFFSETOF below is only well-defined for standard-layout types");
static_assert(BT_OFFSETOF(EngineKeybindsFile, pKeys) == 0, "EngineKeybindsFile::pKeys offset changed");
static_assert(sizeof(EngineKeybindsFile) == static_cast<size_t>(KeyAction::kCount), "EngineKeybindsFile::kiVersion must be bumped with this layout");

constexpr char kpcEngineKeybindsPath[] = "EngineKeybinds.bin";

constexpr VirtualKey kpDefaultEngineKeybinds[static_cast<size_t>(KeyAction::kCount)]
{
	VirtualKey::FromWin32(VK_F1),     // kToggleFullscreen
	VirtualKey::FromWin32(VK_RETURN), // kOpenChat
	VirtualKey::FromWin32(VK_F9),     // kToggleScreenshots
	VirtualKey::FromWin32('W'),       // kCameraMoveForward
	VirtualKey::FromWin32('S'),       // kCameraMoveBack
	VirtualKey::FromWin32('A'),       // kCameraMoveLeft
	VirtualKey::FromWin32('D'),       // kCameraMoveRight
};

static VirtualKey spEngineKeybinds[static_cast<size_t>(KeyAction::kCount)]
{
	kpDefaultEngineKeybinds[0],
	kpDefaultEngineKeybinds[1],
	kpDefaultEngineKeybinds[2],
	kpDefaultEngineKeybinds[3],
	kpDefaultEngineKeybinds[4],
	kpDefaultEngineKeybinds[5],
	kpDefaultEngineKeybinds[6],
};

struct EngineKeyActionDescriptor
{
	const char* pcId = nullptr;
	StandardString eLabel = StandardString::kCount;
};

constexpr EngineKeyActionDescriptor kpEngineKeyActionDescriptors[static_cast<size_t>(KeyAction::kCount)]
{
	{.pcId = "KeybindFullscreen",    .eLabel = StandardString::kStringFullscreen},
	{.pcId = "KeybindOpenChat",      .eLabel = StandardString::kStringOpenChat},
	{.pcId = "KeybindScreenshot",    .eLabel = StandardString::kStringScreenshot},
	{.pcId = "KeybindCameraForward", .eLabel = StandardString::kStringCameraForward},
	{.pcId = "KeybindCameraBack",    .eLabel = StandardString::kStringCameraBack},
	{.pcId = "KeybindCameraLeft",    .eLabel = StandardString::kStringCameraLeft},
	{.pcId = "KeybindCameraRight",   .eLabel = StandardString::kStringCameraRight},
};

static std::string_view CopyKeyNameToBuffer(std::string_view text, std::span<char> buffer)
{
	int64_t iCopyLength = std::min(static_cast<int64_t>(text.size()), static_cast<int64_t>(buffer.size()) - 1);
	std::copy_n(text.data(), iCopyLength, buffer.data());
	buffer[static_cast<size_t>(iCopyLength)] = '\0';
	return std::string_view(buffer.data(), static_cast<size_t>(iCopyLength));
}

static std::string_view EncodeKeyNameToBuffer(std::u32string_view text, std::span<char> buffer)
{
	int64_t iWrite = 0;
	int64_t iLimit = static_cast<int64_t>(buffer.size()) - 1;
	for (char32_t cCodePoint : text)
	{
		char pcEncoded[5] {};
		int64_t iByteCount = ImTextCharToUtf8(pcEncoded, static_cast<unsigned int>(cCodePoint));
		if (iWrite + iByteCount > iLimit)
		{
			break;
		}
		std::copy_n(pcEncoded, iByteCount, buffer.data() + iWrite);
		iWrite += iByteCount;
	}

	buffer[static_cast<size_t>(iWrite)] = '\0';
	return std::string_view(buffer.data(), static_cast<size_t>(iWrite));
}

bool IsBindableKey(VirtualKey key)
{
	if (!key.IsBound() || !key.IsValid())
	{
		return false;
	}

	uint8_t uiCode = key.uiCode;
	if ((uiCode >= 'A' && uiCode <= 'Z') || (uiCode >= '0' && uiCode <= '9') || (uiCode >= VK_F1 && uiCode <= VK_F24)
	 || (uiCode >= VK_NUMPAD0 && uiCode <= VK_NUMPAD9) || (uiCode >= VK_OEM_1 && uiCode <= VK_OEM_3)
	 || (uiCode >= VK_OEM_4 && uiCode <= VK_OEM_8))
	{
		return true;
	}

	switch (uiCode)
	{
		case VK_TAB:
		case VK_RETURN:
		case VK_SHIFT:
		case VK_CONTROL:
		case VK_CAPITAL:
		case VK_SPACE:
		case VK_PRIOR:
		case VK_NEXT:
		case VK_END:
		case VK_HOME:
		case VK_LEFT:
		case VK_UP:
		case VK_RIGHT:
		case VK_DOWN:
		case VK_INSERT:
		case VK_MULTIPLY:
		case VK_ADD:
		case VK_SUBTRACT:
		case VK_DECIMAL:
		case VK_DIVIDE:
		case VK_OEM_102:
			return true;

		default:
			return false;
	}
}

std::string_view FormatVirtualKeyName(VirtualKey key, std::span<char> buffer)
{
	if (buffer.size() <= 1)
	{
		if (buffer.size() == 1)
		{
			buffer[0] = '\0';
		}
		return {};
	}

	if (!key.IsBound())
	{
		return EncodeKeyNameToBuffer(TranslatedString(StandardString::kStringUnbound), buffer);
	}

	uint8_t uiCode = key.uiCode;
	if ((uiCode >= 'A' && uiCode <= 'Z') || (uiCode >= '0' && uiCode <= '9'))
	{
		char pcSingle[2] {static_cast<char>(uiCode), '\0'};
		return CopyKeyNameToBuffer(std::string_view(pcSingle, 1), buffer);
	}

	if (uiCode >= VK_F1 && uiCode <= VK_F24)
	{
		int64_t iLength = std::snprintf(buffer.data(), buffer.size(), "F%d", static_cast<int>(uiCode - VK_F1 + 1));
		int64_t iClamped = std::clamp(iLength, 0i64, static_cast<int64_t>(buffer.size()) - 1);
		return std::string_view(buffer.data(), static_cast<size_t>(iClamped));
	}

	if (uiCode >= VK_NUMPAD0 && uiCode <= VK_NUMPAD9)
	{
		int64_t iLength = std::snprintf(buffer.data(), buffer.size(), "NUM %d", static_cast<int>(uiCode - VK_NUMPAD0));
		int64_t iClamped = std::clamp(iLength, 0i64, static_cast<int64_t>(buffer.size()) - 1);
		return std::string_view(buffer.data(), static_cast<size_t>(iClamped));
	}

	switch (uiCode)
	{
		case VK_TAB:      return CopyKeyNameToBuffer("TAB", buffer);
		case VK_RETURN:   return CopyKeyNameToBuffer("ENTER", buffer);
		case VK_SHIFT:    return CopyKeyNameToBuffer("SHIFT", buffer);
		case VK_CONTROL:  return CopyKeyNameToBuffer("CTRL", buffer);
		case VK_CAPITAL:  return CopyKeyNameToBuffer("CAPS LOCK", buffer);
		case VK_SPACE:    return CopyKeyNameToBuffer("SPACE", buffer);
		case VK_PRIOR:    return CopyKeyNameToBuffer("PAGE UP", buffer);
		case VK_NEXT:     return CopyKeyNameToBuffer("PAGE DOWN", buffer);
		case VK_END:      return CopyKeyNameToBuffer("END", buffer);
		case VK_HOME:     return CopyKeyNameToBuffer("HOME", buffer);
		case VK_LEFT:     return CopyKeyNameToBuffer("LEFT", buffer);
		case VK_UP:       return CopyKeyNameToBuffer("UP", buffer);
		case VK_RIGHT:    return CopyKeyNameToBuffer("RIGHT", buffer);
		case VK_DOWN:     return CopyKeyNameToBuffer("DOWN", buffer);
		case VK_INSERT:   return CopyKeyNameToBuffer("INSERT", buffer);
		case VK_MULTIPLY: return CopyKeyNameToBuffer("NUM *", buffer);
		case VK_ADD:      return CopyKeyNameToBuffer("NUM +", buffer);
		case VK_SUBTRACT: return CopyKeyNameToBuffer("NUM -", buffer);
		case VK_DECIMAL:  return CopyKeyNameToBuffer("NUM .", buffer);
		case VK_DIVIDE:   return CopyKeyNameToBuffer("NUM /", buffer);
		default:          break;
	}

	uint32_t uiScanCode = MapVirtualKeyW(uiCode, MAPVK_VK_TO_VSC);
	if (uiScanCode != 0)
	{
		wchar_t pcWideName[64] {};
		int64_t iWideLength = GetKeyNameTextW(static_cast<LONG>(uiScanCode << 16), pcWideName, static_cast<int>(std::size(pcWideName)));
		if (iWideLength > 0)
		{
			for (int64_t i = 0; i < iWideLength; ++i)
			{
				pcWideName[i] = static_cast<wchar_t>(std::towupper(static_cast<wint_t>(pcWideName[i])));
			}

			int64_t iUtf8Length = WideCharToMultiByte(CP_UTF8, 0, pcWideName, static_cast<int>(iWideLength), buffer.data(), static_cast<int>(buffer.size()) - 1, nullptr, nullptr);
			if (iUtf8Length > 0)
			{
				buffer[static_cast<size_t>(iUtf8Length)] = '\0';
				std::string_view keyName(buffer.data(), static_cast<size_t>(iUtf8Length));
				if (keyName == "#")
				{
					return CopyKeyNameToBuffer("HASH", buffer);
				}
				if (keyName.back() != '#' && !keyName.contains("##"))
				{
					return keyName;
				}
			}
		}
	}

	int64_t iLength = std::snprintf(buffer.data(), buffer.size(), "KEY 0x%02X", static_cast<unsigned int>(uiCode));
	int64_t iClamped = std::clamp(iLength, 0i64, static_cast<int64_t>(buffer.size()) - 1);
	return std::string_view(buffer.data(), static_cast<size_t>(iClamped));
}

VirtualKey KeybindFor(KeyAction eAction)
{
	return spEngineKeybinds[static_cast<size_t>(eAction)];
}

void SetKeybind(KeyAction eAction, VirtualKey key)
{
	spEngineKeybinds[static_cast<size_t>(eAction)] = key;
}

void UnbindKey(VirtualKey key)
{
	for (VirtualKey& rKey : spEngineKeybinds)
	{
		if (rKey == key)
		{
			rKey = VirtualKey::Unbound();
		}
	}
}

std::span<const KeyAction> VisibleEngineKeyActions()
{
	if constexpr (kbFreeCamera)
	{
		static constexpr KeyAction kpActions[]
		{
			KeyAction::kToggleFullscreen,
			KeyAction::kOpenChat,
			KeyAction::kToggleScreenshots,
			KeyAction::kCameraMoveForward,
			KeyAction::kCameraMoveBack,
			KeyAction::kCameraMoveLeft,
			KeyAction::kCameraMoveRight,
		};
		return kpActions;
	}
	else
	{
		static constexpr KeyAction kpActions[]
		{
			KeyAction::kToggleFullscreen,
			KeyAction::kOpenChat,
			KeyAction::kToggleScreenshots,
		};
		return kpActions;
	}
}

KeybindRowInfo DescribeKeybind(KeyAction eAction)
{
	const EngineKeyActionDescriptor& rDescriptor = kpEngineKeyActionDescriptors[static_cast<size_t>(eAction)];
	return KeybindRowInfo
	{
		.slot = KeybindSlot::Engine(eAction),
		.pcId = rDescriptor.pcId,
		.label = TranslatedString(rDescriptor.eLabel),
		.key = KeybindFor(eAction),
	};
}

bool LoadEngineKeybinds()
{
	// Heap: file I/O allocates
	ScopedSuppressAllocationTracking suppress;

	std::fstream fileStream = gpFileManager->OpenFile({FileFlags::kRead}, kpcEngineKeybindsPath);
	if (!fileStream.is_open())
	{
		return false;
	}

	int64_t iVersion = 0;
	int64_t iSize = 0;
	bool bHeaderValid = ReadAndValidateVersionHeader<EngineKeybindsFile>(fileStream, iVersion, iSize);
	if (!bHeaderValid || !fileStream.good())
	{
		LOG(kLoading, kWarning, "{} rejected: invalid header (version {} size {})", kpcEngineKeybindsPath, iVersion, iSize);
		return false;
	}

	EngineKeybindsFile fileData {};
	common::Read(fileStream, fileData);
	if (fileStream.gcount() != static_cast<std::streamsize>(sizeof(EngineKeybindsFile)) || fileStream.fail() || fileStream.peek() != std::char_traits<char>::eof())
	{
		LOG(kLoading, kWarning, "{} rejected: invalid payload size", kpcEngineKeybindsPath);
		return false;
	}

	bool pbSeen[kiKeyboardKeyCount] {};
	for (VirtualKey key : fileData.pKeys)
	{
		if (!key.IsBound())
		{
			continue;
		}
		if (!IsBindableKey(key) || pbSeen[key.uiCode])
		{
			LOG(kLoading, kWarning, "{} rejected: invalid or duplicate virtual key {}", kpcEngineKeybindsPath, static_cast<int64_t>(key.uiCode));
			return false;
		}
		pbSeen[key.uiCode] = true;
	}

	std::copy(std::begin(fileData.pKeys), std::end(fileData.pKeys), std::begin(spEngineKeybinds));
	return true;
}

void SaveEngineKeybinds()
{
	// Heap: file I/O allocates
	ScopedSuppressAllocationTracking suppress;

	EngineKeybindsFile fileData {};
	std::copy(std::begin(spEngineKeybinds), std::end(spEngineKeybinds), std::begin(fileData.pKeys));
	WriteVersionedFile(kpcEngineKeybindsPath, fileData);
}

void ResetEngineKeybinds()
{
	std::copy(std::begin(kpDefaultEngineKeybinds), std::end(kpDefaultEngineKeybinds), std::begin(spEngineKeybinds));
}

} // namespace engine

#endif // BT_CLIENT
