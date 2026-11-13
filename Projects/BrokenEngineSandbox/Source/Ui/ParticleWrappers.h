#pragma once

#include "Ui/WrapperBase.h"

namespace game
{

// Per-explosion-type particle multipliers (Tweaks → Particles tab).
// A multiplier of 1.0f leaves the corresponding base ExplosionType value unchanged.
// Declaration order must match slider order in TweaksScreenParticles.cpp.

inline engine::Wrapper gMissileExplosionParticleWidth(0.75f, 0.0f, 3.0f);
inline engine::Wrapper gMissileExplosionParticleLength(2.0f, 0.0f, 3.0f);
inline engine::Wrapper gMissileExplosionParticleLengthSpread(2.0f, 0.0f, 6.0f);
inline engine::Wrapper gMissileExplosionParticlePositionJitter(0.5f, 0.0f, 10.0f);
inline engine::Wrapper gMissileExplosionParticleVelocityBase(4.0f, 0.0f, 10.0f);
inline engine::Wrapper gMissileExplosionParticleVelocitySpread(5.0f, 0.0f, 10.0f);
inline engine::Wrapper gMissileExplosionParticleVerticalVelocityBase(0.25f, 0.0f, 3.0f);
inline engine::Wrapper gMissileExplosionParticleVerticalVelocitySpread(3.0f, 0.0f, 3.0f);
inline engine::Wrapper gMissileExplosionParticleVelocityDecay(5.0f, 0.0f, 6.0f);
inline engine::Wrapper gMissileExplosionParticleGravity(2.0f, 0.0f, 5.0f);
inline engine::Wrapper gMissileExplosionParticleIntensitySpread(5.0f, 0.0f, 6.0f);
inline engine::Wrapper gMissileExplosionParticleIntensityDecay(2.5f, 0.0f, 5.0f);
inline engine::Wrapper gMissileExplosionParticleIntensityPower(0.75f, 0.0f, 5.0f);

inline engine::Wrapper gOmniExplosionParticleWidth(1.0f, 0.0f, 3.0f);
inline engine::Wrapper gOmniExplosionParticleLength(1.0f, 0.0f, 3.0f);
inline engine::Wrapper gOmniExplosionParticleLengthSpread(0.0f, 0.0f, 3.0f);
inline engine::Wrapper gOmniExplosionParticlePositionJitter(1.0f, 0.0f, 3.0f);
inline engine::Wrapper gOmniExplosionParticleVelocityBase(1.0f, 0.0f, 3.0f);
inline engine::Wrapper gOmniExplosionParticleVelocitySpread(1.0f, 0.0f, 3.0f);
inline engine::Wrapper gOmniExplosionParticleVerticalVelocityBase(1.0f, 0.0f, 3.0f);
inline engine::Wrapper gOmniExplosionParticleVerticalVelocitySpread(1.0f, 0.0f, 3.0f);
inline engine::Wrapper gOmniExplosionParticleVelocityDecay(1.0f, 0.0f, 5.0f);
inline engine::Wrapper gOmniExplosionParticleGravity(1.0f, 0.0f, 5.0f);
inline engine::Wrapper gOmniExplosionParticleVisibleIntensity(1.0f, 0.0f, 4.0f);
inline engine::Wrapper gOmniExplosionParticleIntensitySpread(1.0f, 0.0f, 3.0f);
inline engine::Wrapper gOmniExplosionParticleIntensityDecay(1.0f, 0.0f, 5.0f);
inline engine::Wrapper gOmniExplosionParticleIntensityPower(1.0f, 0.0f, 5.0f);

inline engine::Wrapper gFighterExplosionParticleWidth(0.75f, 0.0f, 3.0f);
inline engine::Wrapper gFighterExplosionParticleLength(0.5f, 0.0f, 3.0f);
inline engine::Wrapper gFighterExplosionParticleLengthSpread(2.0f, 0.0f, 6.0f);
inline engine::Wrapper gFighterExplosionParticlePositionJitter(0.5f, 0.0f, 10.0f);
inline engine::Wrapper gFighterExplosionParticleVelocityBase(4.0f, 0.0f, 10.0f);
inline engine::Wrapper gFighterExplosionParticleVelocitySpread(5.0f, 0.0f, 10.0f);
inline engine::Wrapper gFighterExplosionParticleVerticalVelocityBase(0.25f, 0.0f, 3.0f);
inline engine::Wrapper gFighterExplosionParticleVerticalVelocitySpread(3.0f, 0.0f, 3.0f);
inline engine::Wrapper gFighterExplosionParticleVelocityDecay(5.0f, 0.0f, 6.0f);
inline engine::Wrapper gFighterExplosionParticleGravity(2.0f, 0.0f, 5.0f);
inline engine::Wrapper gFighterExplosionParticleIntensitySpread(5.0f, 0.0f, 6.0f);
inline engine::Wrapper gFighterExplosionParticleIntensityDecay(1.5f, 0.0f, 5.0f);
inline engine::Wrapper gFighterExplosionParticleIntensityPower(0.75f, 0.0f, 5.0f);

} // namespace game
