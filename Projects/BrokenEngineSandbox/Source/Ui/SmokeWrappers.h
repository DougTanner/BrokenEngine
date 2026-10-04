#pragma once

#if defined(BT_CLIENT)

#include "Ui/WrapperBase.h"

namespace game
{

extern engine::Wrapper gExplosionPrimaryPuffAreaOne;
extern engine::Wrapper gExplosionPrimaryPuffAreaTwo;
extern engine::Wrapper gExplosionPrimaryPuffIntensityOne;
extern engine::Wrapper gExplosionPrimaryPuffIntensityTwo;

extern engine::Wrapper gExplosionSecondaryPuffAreaOne;
extern engine::Wrapper gExplosionSecondaryPuffAreaTwo;
extern engine::Wrapper gExplosionSecondaryPuffIntensityOne;
extern engine::Wrapper gExplosionSecondaryPuffIntensityTwo;

extern engine::Wrapper gExplosionPrimaryTrailIntensity;
extern engine::Wrapper gExplosionPrimaryTrailLength;
extern engine::Wrapper gExplosionPrimaryTrailDuration;

extern engine::Wrapper gExplosionSecondaryTrailIntensity;
extern engine::Wrapper gExplosionSecondaryTrailLength;
extern engine::Wrapper gExplosionSecondaryTrailDuration;

// Blasters - Terrain puff
extern engine::Wrapper gBlasterPuffAreaStart;
extern engine::Wrapper gBlasterPuffAreaEnd;
extern engine::Wrapper gBlasterPuffIntensityStart;
extern engine::Wrapper gBlasterPuffIntensityEnd;

extern engine::Wrapper gPlayerImpactPuffAreaOne;
extern engine::Wrapper gPlayerImpactPuffAreaTwo;
extern engine::Wrapper gPlayerImpactPuffIntensityOne;
extern engine::Wrapper gPlayerImpactPuffIntensityTwo;

extern engine::Wrapper gMissileTrailIntensity;

} // namespace game

#endif // defined(BT_CLIENT)
