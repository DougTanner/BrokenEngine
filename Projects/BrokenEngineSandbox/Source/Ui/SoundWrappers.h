#pragma once

#if defined(BT_CLIENT)

#include "Ui/WrapperBase.h"

namespace game
{

// Volume defaults are the tuned per-channel mix; adjust each one via the Sound Tweaks tab.
// Pitch defaults are real (pitch=0 would be silent-DC, not zero-volume).

inline engine::Wrapper gOmniBlasterVolume(0.1f, 0.0f, 0.2f);
inline engine::Wrapper gOmniBlasterPitchMinimum(0.75f, 0.5f, 1.5f);
inline engine::Wrapper gOmniBlasterPitchRandom(0.5f, 0.0f, 1.0f);

inline engine::Wrapper gFighterBlasterVolume(0.1f, 0.0f, 0.2f);
inline engine::Wrapper gFighterBlasterPitchMinimum(1.5f, 0.5f, 2.0f);
inline engine::Wrapper gFighterBlasterPitchRandom(1.0f, 0.0f, 1.5f);

// Blasters - Terrain impact
inline engine::Wrapper gTerrainImpactVolume(0.075f, 0.0f, 0.2f);

inline engine::Wrapper gMissileLaunchVolume(0.05f, 0.0f, 0.2f);
inline engine::Wrapper gMissileLoopVolume(0.04f, 0.0f, 0.2f);
inline engine::Wrapper gMissilePitchMin(1.0f, 0.5f, 1.5f);
inline engine::Wrapper gMissilePitchRandom(1.0f, 0.0f, 1.0f);

inline engine::Wrapper gExplosionVolume(0.075f, 0.0f, 0.2f);

// Omnis - Shield
inline engine::Wrapper gShieldHitVolumeBase(0.05f, 0.0f, 0.2f);
inline engine::Wrapper gShieldHitVolumeScale(0.0f, 0.0f, 0.2f);
inline engine::Wrapper gShieldDownVolume(0.0f, 0.0f, 0.2f);

// Omnis - Armor
inline engine::Wrapper gArmorHitVolumeBase(0.0f, 0.0f, 0.2f);
inline engine::Wrapper gArmorHitVolumeScale(0.0f, 0.0f, 0.2f);

inline engine::Wrapper gFighterDeathVolume(0.1f, 0.0f, 0.2f);
inline engine::Wrapper gFighterDeathPitchMinimum(0.75f, 0.5f, 1.5f);
inline engine::Wrapper gFighterDeathPitchRandom(0.5f, 0.0f, 1.0f);
inline engine::Wrapper gFighterHitVolume(0.075f, 0.0f, 0.2f);

} // namespace game

#endif // defined(BT_CLIENT)
