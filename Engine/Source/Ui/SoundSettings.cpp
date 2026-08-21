#if defined(BT_CLIENT)

#include "SoundSettings.h"

#include "Ui/SoundSettingsWrappersBase.h"

namespace engine
{

struct SoundSettings
{
	static constexpr int64_t kiVersion = 3;

	float fMasterVolume = 0.0f;
	float fMusicVolume = 0.0f;
	float fSoundVolume = 0.0f;
	uint8_t uiMuteInBackground = 0;
	uint8_t uiPad[3] {};
};
static_assert(std::is_trivially_copyable_v<SoundSettings>, "SoundSettings must stay trivially copyable — WriteVersionedFile stamps sizeof");
static_assert(std::is_standard_layout_v<SoundSettings>, "SoundSettings must stay standard-layout — BT_OFFSETOF above is only well-defined for standard-layout types");
static_assert(BT_OFFSETOF(SoundSettings, fMasterVolume) == 0, "SoundSettings::fMasterVolume offset changed — existing volume bytes move");
static_assert(BT_OFFSETOF(SoundSettings, fMusicVolume) == 4, "SoundSettings::fMusicVolume offset changed — existing volume bytes move");
static_assert(BT_OFFSETOF(SoundSettings, fSoundVolume) == 8, "SoundSettings::fSoundVolume offset changed — existing volume bytes move");
static_assert(BT_OFFSETOF(SoundSettings, uiMuteInBackground) == 12, "SoundSettings::uiMuteInBackground offset changed — mute byte is appended after volumes");
static_assert(BT_OFFSETOF(SoundSettings, uiPad) == 13, "SoundSettings padding changed — SoundSettings must remain 16 bytes");
static_assert(sizeof(SoundSettings) == 16, "SoundSettings::kiVersion must be bumped with this layout");
static constexpr char kpcSoundSettingsPath[] = "SoundSettings.bin";

void SaveSoundSettings()
{
	// Heap: file I/O allocates
	ScopedSuppressAllocationTracking suppress;

	SoundSettings soundSettings
	{
		.fMasterVolume = gMasterVolume.Get(),
		.fMusicVolume = gMusicVolume.Get(),
		.fSoundVolume = gSoundVolume.Get(),
		.uiMuteInBackground = static_cast<uint8_t>(gMuteInBackground.Get<bool>()),
	};

	WriteVersionedFile({FileFlags::kAppDataDirectory, FileFlags::kWrite}, kpcSoundSettingsPath, soundSettings);
}

void LoadSoundSettings()
{
	SoundSettings soundSettings {};

	if (ReadVersionedFile({FileFlags::kAppDataDirectory, FileFlags::kRead}, kpcSoundSettingsPath, soundSettings))
	{
		gMasterVolume.Set(soundSettings.fMasterVolume);
		gMusicVolume.Set(soundSettings.fMusicVolume);
		gSoundVolume.Set(soundSettings.fSoundVolume);
		gMuteInBackground.Set(soundSettings.uiMuteInBackground != 0);
	}

	if constexpr (kbRecording)
	{
		gMusicVolume.Set(0.0f);
	}
}

void ResetSoundSettings()
{
	gMasterVolume.ResetToDefault();
	gMusicVolume.ResetToDefault();
	gSoundVolume.ResetToDefault();
	gMuteInBackground.ResetToDefault();

	SaveSoundSettings();
}

} // namespace engine

#endif // BT_CLIENT
