#if defined(BT_CLIENT)

#include "TweaksSettings.h"

#include "Ui/Screens/TweaksScreen/TweaksScreen.h"

namespace engine
{

struct TweaksSettings
{
	static constexpr int64_t kiVersion = 15;

	// uint8_t, not bool: the file is opaque input and a non-0/1 byte read into a bool is an invalid object representation
	uint8_t uiShowImGui = 0;
	uint8_t puiPadding[7] {};
	// Engine-owned layout POD, embedded by value: one array bound and one sizeof for the whole program.
	TweakSectionState sectionState {};
};
static_assert(std::is_trivially_copyable_v<TweaksSettings>);
static_assert(sizeof(TweaksSettings) == 304, "kiVersion must be bumped with this layout");
constexpr char kpcTweaksSettingsPath[] = "TweaksSettings.bin";

void SaveTweaksSettings()
{
	if constexpr (!kbDebugInput)
	{
		return;
	}

	TweaksSettings settings {};
	settings.uiShowImGui = static_cast<uint8_t>(gbShowImGui);
	GetTweaksScreen().SaveState(settings.sectionState);

	WriteVersionedFile({FileFlags::kAppDataDirectory, FileFlags::kWrite}, kpcTweaksSettingsPath, settings);
}

void LoadTweaksSettings()
{
	if constexpr (!kbDebugInput)
	{
		return;
	}

	TweaksSettings settings {};
	if (!(ReadVersionedFile({FileFlags::kAppDataDirectory, FileFlags::kRead}, kpcTweaksSettingsPath, settings) && settings.uiShowImGui <= 1))
	{
		LOG(kDefault, kWarning, "LoadTweaks FAILED to read file");
		return;
	}

	gbShowImGui = settings.uiShowImGui != 0;
	GetTweaksScreen().LoadState(settings.sectionState);
}

} // namespace engine

#endif // BT_CLIENT
