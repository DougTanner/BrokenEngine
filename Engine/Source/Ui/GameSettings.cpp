#if defined(BT_CLIENT)

#include "GameSettings.h"

#include "Ui/GraphicsSettingsWrappersBase.h"
#include "Ui/LocalizationBase.h"

namespace engine
{

struct GameSettings
{
	static constexpr int64_t kiVersion = 2;

	// Fixed-width on disk: the Language enum's underlying type must not decide the file layout.
	int32_t iLanguage = static_cast<int32_t>(Language::kEnglish);
	float fUiFontScale = 1.0f;
	float fUiOpacity = 0.5f;
	// uint8_t, not bool: the file is opaque input and a non-0/1 byte read into a bool is an invalid object representation
	uint8_t uiOpaqueUi = 0;
	UiTheme eUiTheme = UiTheme::kNavalSteel;
	uint8_t uiPad[2] {};
};
static_assert(std::is_trivially_copyable_v<GameSettings>, "GameSettings must stay trivially copyable — WriteVersionedFile stamps sizeof");
static_assert(std::is_standard_layout_v<GameSettings>, "GameSettings must stay standard-layout — BT_OFFSETOF below is only well-defined for standard-layout types");
static_assert(BT_OFFSETOF(GameSettings, iLanguage) == 0, "GameSettings::iLanguage offset changed — existing language bytes move");
static_assert(BT_OFFSETOF(GameSettings, fUiFontScale) == 4, "GameSettings::fUiFontScale offset changed — existing font scale bytes move");
static_assert(BT_OFFSETOF(GameSettings, fUiOpacity) == 8, "GameSettings::fUiOpacity offset changed — existing opacity bytes move");
static_assert(BT_OFFSETOF(GameSettings, uiOpaqueUi) == 12, "GameSettings::uiOpaqueUi offset changed — the opaque-UI byte is appended after the floats");
static_assert(BT_OFFSETOF(GameSettings, eUiTheme) == 13, "GameSettings::eUiTheme offset changed — the theme byte follows the opaque-UI byte");
static_assert(BT_OFFSETOF(GameSettings, uiPad) == 14, "GameSettings padding changed — GameSettings must remain 16 bytes");
static_assert(sizeof(GameSettings) == 16, "GameSettings::kiVersion must be bumped with this layout");
static constexpr char kpcGameSettingsPath[] = "GameSettings.bin";

void SaveGameSettings()
{
	// Heap: file I/O allocates
	ScopedSuppressAllocationTracking suppress;

	GameSettings gameSettings
	{
		.iLanguage = static_cast<int32_t>(geLanguage),
		.fUiFontScale = gUiFontScale.Get(),
		.fUiOpacity = gUiOpacity.Get(),
		.uiOpaqueUi = static_cast<uint8_t>(gOpaqueUi.Get<bool>()),
		.eUiTheme = GetUiTheme(),
	};

	WriteVersionedFile({FileFlags::kAppDataDirectory, FileFlags::kWrite}, kpcGameSettingsPath, gameSettings);
}

namespace
{

// Returns the first invalid field's name, or nullptr when every checked field is valid.
const char* FindInvalidGameSetting(const GameSettings& rGameSettings)
{
	if (rGameSettings.iLanguage < 0 || rGameSettings.iLanguage >= kLanguageCount)
	{
		return "iLanguage";
	}

	if (!gUiFontScale.IsInRange(rGameSettings.fUiFontScale))
	{
		return "fUiFontScale";
	}

	if (!gUiOpacity.IsInRange(rGameSettings.fUiOpacity))
	{
		return "fUiOpacity";
	}

	return nullptr;
}

} // namespace

void LoadGameSettings()
{
	GameSettings gameSettings {};

	if (ReadVersionedFile({FileFlags::kAppDataDirectory, FileFlags::kRead}, kpcGameSettingsPath, gameSettings))
	{
		const char* pcInvalidField = FindInvalidGameSetting(gameSettings);
		if (pcInvalidField != nullptr)
		{
			LOG(kLoading, kWarning, "{} rejected: invalid {}", kpcGameSettingsPath, pcInvalidField);
		}
		else
		{
			geLanguage = static_cast<Language>(gameSettings.iLanguage);
			gUiFontScale.Set(gameSettings.fUiFontScale);
			gUiOpacity.Set(gameSettings.fUiOpacity);
			gOpaqueUi.Set(gameSettings.uiOpaqueUi != 0);
			gUiTheme.Set<UiTheme>(gameSettings.eUiTheme);
		}
	}
}

void ResetGameSettings()
{
	geLanguage = Language::kEnglish;
	gUiFontScale.ResetToDefault();
	gOpaqueUi.ResetToDefault();
	gUiOpacity.ResetToDefault();
	gUiTheme.ResetToDefault();

	SaveGameSettings();
}

} // namespace engine

#endif // BT_CLIENT
