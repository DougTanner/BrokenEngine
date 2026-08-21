#pragma once

namespace engine
{

// Values are pinned: they are persisted as an int32_t in the game settings file.
enum class Language : int32_t
{
	kEnglish = 0,
	kChinese = 1,
	kSpanish = 2,
	kPortuguese = 3,
	kFrench = 4,
	kGerman = 5,
};

inline constexpr int64_t kLanguageCount = 6;

struct LanguageOption
{
	Language eLanguage;
	const char* pcLabel;
};

// Each label stays in its own language, so it is never routed through the translation table.
inline constexpr LanguageOption kLanguageOptions[]
{
	{.eLanguage = Language::kEnglish, .pcLabel = "ENGLISH"},
	{.eLanguage = Language::kChinese, .pcLabel = "中文"},
	{.eLanguage = Language::kSpanish, .pcLabel = "ESPANOL"},
	{.eLanguage = Language::kPortuguese, .pcLabel = "PORTUGUES"},
	{.eLanguage = Language::kFrench, .pcLabel = "FRANCAIS"},
	{.eLanguage = Language::kGerman, .pcLabel = "DEUTSCH"},
};
static_assert(std::size(kLanguageOptions) == static_cast<size_t>(kLanguageCount));

inline Language geLanguage = Language::kEnglish;

enum class StandardString : uint8_t
{
	kStringComplete,
	kStringDefaults,
	kStringGameOver,
	kStringGamepad,
	kStringGameSettings,
	kStringMainMenu,
	kStringMouse,
	kStringMoveWith,
	kStringGraphics,
	kStringOr,
	kStringLocalServer,
	kStringRemoteServer,
	kStringStart,
	kStringReady,
	kStringRespawn,
	kStringResume,
	kStringAudio,
	kStringQuit,
	kStringPaused,

	kCount,
};

std::u32string_view TranslatedString(StandardString eString);
void InitializeLocalization();

} // namespace engine
