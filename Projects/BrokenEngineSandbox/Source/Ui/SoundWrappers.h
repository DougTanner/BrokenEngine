#pragma once

#if defined(BT_CLIENT)

#include "Ui/WrapperBase.h"

namespace game
{

extern engine::Wrapper gOmniBlasterVolume;
extern engine::Wrapper gOmniBlasterPitchMinimum;
extern engine::Wrapper gOmniBlasterPitchRandom;

extern engine::Wrapper gFighterBlasterVolume;
extern engine::Wrapper gFighterBlasterPitchMinimum;
extern engine::Wrapper gFighterBlasterPitchRandom;

// Blasters - Terrain impact
extern engine::Wrapper gTerrainImpactVolume;

extern engine::Wrapper gMissileLaunchVolume;
extern engine::Wrapper gMissileLoopVolume;
extern engine::Wrapper gMissilePitchMin;
extern engine::Wrapper gMissilePitchRandom;

extern engine::Wrapper gExplosionVolume;

// Omnis - Shield
extern engine::Wrapper gShieldHitVolumeBase;
extern engine::Wrapper gShieldHitVolumeScale;
extern engine::Wrapper gShieldDownVolume;

// Omnis - Armor
extern engine::Wrapper gArmorHitVolumeBase;
extern engine::Wrapper gArmorHitVolumeScale;

extern engine::Wrapper gFighterDeathVolume;
extern engine::Wrapper gFighterDeathPitchMinimum;
extern engine::Wrapper gFighterDeathPitchRandom;
extern engine::Wrapper gFighterHitVolume;

} // namespace game

#endif // defined(BT_CLIENT)
