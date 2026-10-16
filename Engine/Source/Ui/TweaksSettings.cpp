#if defined(BT_CLIENT)

#include "TweaksSettings.h"

#include "Ui/Screens/TweaksScreen/TweaksScreenBase.h"
#include "Ui/GraphicsSettingsWrappersBase.h"

#include "Game.h"

namespace engine
{

struct TweaksSettings
{
	static constexpr int64_t kiVersion = 14;

	// uint8_t, not bool: the file is opaque input and a non-0/1 byte read into a bool is an invalid object representation
	uint8_t uiShowImGui = 0;
	uint8_t uiPadding[3] {};
	float fSunAngle = 1.15f;
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
	settings.uiShowImGui = static_cast<uint8_t>(game::gpGame->mbShowImGui);
	settings.fSunAngle = gSunAngleOverride.mfCurrent;
	gpImGuiManager->mpTweaksScreen->SaveState(settings.sectionState);

	WriteVersionedFile({FileFlags::kAppDataDirectory, FileFlags::kWrite}, kpcTweaksSettingsPath, settings);
}

void LoadTweaksSettings()
{
	if constexpr (!kbDebugInput)
	{
		return;
	}

	TweaksSettings settings {};
	if (!(ReadVersionedFile({FileFlags::kAppDataDirectory, FileFlags::kRead}, kpcTweaksSettingsPath, settings) && settings.uiShowImGui <= 1 && settings.fSunAngle >= gSunAngleOverride.mfMin && settings.fSunAngle <= gSunAngleOverride.mfMax))
	{
		LOG(kDefault, kWarning, "LoadTweaks FAILED to read file");
		return;
	}

	game::gpGame->mbShowImGui = settings.uiShowImGui != 0;
	gpImGuiManager->mpTweaksScreen->LoadState(settings.sectionState);
	gSunAngleOverride.Set(settings.fSunAngle);
}

} // namespace engine

#endif // BT_CLIENT
