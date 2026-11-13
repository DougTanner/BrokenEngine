#pragma once

#if defined(BT_CLIENT)

#include "Ui/WrapperBase.h"

namespace game
{

// Lighting wrappers control scene-light contributions; Visible wrappers control visual area and intensity.

inline engine::Wrapper gExplosionPrimaryVisibleAreaOne(4.0f, 0.1f, 5.0f);
inline engine::Wrapper gExplosionPrimaryVisibleAreaTwo(2.0f, 0.1f, 5.0f);
inline engine::Wrapper gExplosionPrimaryVisibleAreaThree(0.0f, 0.0f, 5.0f);
inline engine::Wrapper gExplosionPrimaryVisibleIntensityOne(3.0f, 0.0f, 3.0f);
inline engine::Wrapper gExplosionPrimaryVisibleIntensityTwo(1.0f, 0.0f, 3.0f);
inline engine::Wrapper gExplosionPrimaryVisibleIntensityThree(0.0f, 0.0f, 3.0f);
inline engine::Wrapper gExplosionPrimaryLightingAreaOne(16.0f, 1.0f, 32.0f);
inline engine::Wrapper gExplosionPrimaryLightingAreaTwo(8.0f, 1.0f, 16.0f);
inline engine::Wrapper gExplosionPrimaryLightingAreaThree(0.0f, 0.0f, 4.0f);
inline engine::Wrapper gExplosionPrimaryLightingIntensityOne(0.6f, 0.0f, 1.0f);
inline engine::Wrapper gExplosionPrimaryLightingIntensityTwo(0.15f, 0.0f, 1.0f);
inline engine::Wrapper gExplosionPrimaryLightingIntensityThree(0.0f, 0.0f, 2.0f);

inline engine::Wrapper gExplosionSecondaryVisibleAreaOne(2.0f, 0.0f, 4.0f);
inline engine::Wrapper gExplosionSecondaryVisibleAreaTwo(0.5f, 0.1f, 4.0f);
inline engine::Wrapper gExplosionSecondaryVisibleAreaThree(0.0f, 0.0f, 4.0f);
inline engine::Wrapper gExplosionSecondaryVisibleIntensityOne(0.5f, 0.0f, 3.0f);
inline engine::Wrapper gExplosionSecondaryVisibleIntensityTwo(0.25f, 0.0f, 3.0f);
inline engine::Wrapper gExplosionSecondaryVisibleIntensityThree(0.0f, 0.0f, 3.0f);
inline engine::Wrapper gExplosionSecondaryLightingAreaOne(8.0f, 0.0f, 16.0f);
inline engine::Wrapper gExplosionSecondaryLightingAreaTwo(2.0f, 1.0f, 4.0f);
inline engine::Wrapper gExplosionSecondaryLightingAreaThree(0.0f, 0.0f, 4.0f);
inline engine::Wrapper gExplosionSecondaryLightingIntensityOne(0.5f, 0.0f, 1.0f);
inline engine::Wrapper gExplosionSecondaryLightingIntensityTwo(0.1f, 0.0f, 1.0f);
inline engine::Wrapper gExplosionSecondaryLightingIntensityThree(0.0f, 0.0f, 2.0f);

// Blasters - Terrain crater
inline engine::Wrapper gCraterVisibleAreaOne(0.35f, 0.0f, 2.0f);
inline engine::Wrapper gCraterVisibleAreaTwo(0.35f, 0.0f, 2.0f);
inline engine::Wrapper gCraterVisibleAreaThree(0.25f, 0.03f, 1.5f);
inline engine::Wrapper gCraterVisibleAreaFour(0.0f, 0.0f, 1.5f);
inline engine::Wrapper gCraterVisibleIntensityOne(2.0f, 0.2f, 10.0f);
inline engine::Wrapper gCraterVisibleIntensityTwo(2.0f, 0.2f, 10.0f);
inline engine::Wrapper gCraterVisibleIntensityThree(1.0f, 0.1f, 5.0f);
inline engine::Wrapper gCraterVisibleIntensityFour(0.0f, 0.0f, 5.0f);
inline engine::Wrapper gCraterLightingAreaOne(8.0f, 1.0f, 16.0f);
inline engine::Wrapper gCraterLightingAreaTwo(2.0f, 1.0f, 4.0f);
inline engine::Wrapper gCraterLightingAreaThree(1.0f, 1.0f, 4.0f);
inline engine::Wrapper gCraterLightingAreaFour(0.0f, 0.0f, 4.0f);
inline engine::Wrapper gCraterLightingIntensityOne(0.3f, 0.0f, 1.0f);
inline engine::Wrapper gCraterLightingIntensityTwo(0.1f, 0.0f, 2.0f);
inline engine::Wrapper gCraterLightingIntensityThree(0.05f, 0.0f, 2.0f);
inline engine::Wrapper gCraterLightingIntensityFour(0.0f, 0.0f, 2.0f);

// Omnis - Area light
inline engine::Wrapper gOmniAreaLightVisibleIntensity(1.0f, 0.1f, 1.0f);
inline engine::Wrapper gOmniBlasterLightingArea(3.0f, 1.0f, 8.0f);
inline engine::Wrapper gOmniBlasterLightingIntensity(2.5f, 0.0f, 6.0f);

// Omnis - Impact point light
inline engine::Wrapper gOmniImpactVisibleAreaOne(0.675f, 0.07f, 3.5f);
inline engine::Wrapper gOmniImpactVisibleAreaTwo(0.0f, 0.0f, 3.5f);
inline engine::Wrapper gOmniImpactVisibleIntensityOne(1.0f, 0.1f, 5.0f);
inline engine::Wrapper gOmniImpactVisibleIntensityTwo(0.5f, 0.0f, 2.5f);
inline engine::Wrapper gOmniImpactLightingAreaOne(2.0f, 1.0f, 4.0f);
inline engine::Wrapper gOmniImpactLightingAreaTwo(0.0f, 0.0f, 4.0f);
inline engine::Wrapper gOmniImpactLightingIntensityOne(0.0f, 0.0f, 2.0f);
inline engine::Wrapper gOmniImpactLightingIntensityTwo(0.0f, 0.0f, 2.0f);

// Omnis - Hex shield
inline engine::Wrapper gHexShieldIntensityDecay(1.25f, 0.1f, 6.0f);
inline engine::Wrapper gHexShieldLightingIntensity(4.0f, 0.0f, 8.0f);

inline engine::Wrapper gMissileExhaustVisibleIntensity(1.0f, 0.1f, 5.0f);
inline engine::Wrapper gMissileExhaustLightingArea(3.0f, 1.0f, 4.0f);
inline engine::Wrapper gMissileExhaustLightingIntensity(3.5f, 0.0f, 5.0f);

// Fighters - Hit flash
inline engine::Wrapper gHitFlashVisibleAreaOne(0.375f, 0.04f, 1.9f);
inline engine::Wrapper gHitFlashVisibleAreaTwo(0.0f, 0.0f, 1.9f);
inline engine::Wrapper gHitFlashVisibleIntensityOne(1.0f, 0.1f, 5.0f);
inline engine::Wrapper gHitFlashVisibleIntensityTwo(0.0f, 0.0f, 5.0f);
inline engine::Wrapper gHitFlashLightingAreaOne(2.0f, 1.0f, 4.0f);
inline engine::Wrapper gHitFlashLightingAreaTwo(0.0f, 0.0f, 4.0f);
inline engine::Wrapper gHitFlashLightingIntensityOne(0.0f, 0.0f, 2.0f);
inline engine::Wrapper gHitFlashLightingIntensityTwo(0.0f, 0.0f, 2.0f);

// Fighters - Beam visible
inline engine::Wrapper gFighterBeamImpactVisibleAreaOne(4.0f, 0.4f, 19.0f);
inline engine::Wrapper gFighterBeamImpactVisibleAreaTwo(0.0f, 0.0f, 19.0f);
inline engine::Wrapper gFighterBeamImpactVisibleIntensityOne(1.0f, 0.1f, 5.0f);
inline engine::Wrapper gFighterBeamImpactVisibleIntensityTwo(0.0f, 0.0f, 5.0f);
inline engine::Wrapper gFighterBeamWidth(0.3f, 0.02f, 3.0f);

// Fighters - Beam lighting
inline engine::Wrapper gFighterBeamImpactLightingAreaOne(2.0f, 1.0f, 4.0f);
inline engine::Wrapper gFighterBeamImpactLightingAreaTwo(0.0f, 0.0f, 4.0f);
inline engine::Wrapper gFighterBeamImpactLightingIntensityOne(2.0f, 0.0f, 16.0f);
inline engine::Wrapper gFighterBeamImpactLightingIntensityTwo(0.0f, 0.0f, 4.0f);
inline engine::Wrapper gFighterBeamLightingArea(8.0f, 1.0f, 10.0f);
inline engine::Wrapper gFighterBeamLightingIntensity(20.0f, 0.0f, 100.0f);

} // namespace game

#endif // defined(BT_CLIENT)
