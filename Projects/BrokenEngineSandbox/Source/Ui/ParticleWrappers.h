#pragma once

#include "Ui/WrapperBase.h"

namespace game
{

// Per-explosion-type particle multipliers (Tweaks → Particles tab).
// A multiplier of 1.0f leaves the corresponding base ExplosionType value unchanged.
// Declaration order must match slider order in TweaksScreenParticles.cpp.

extern engine::Wrapper gMissileExplosionParticleWidth;
extern engine::Wrapper gMissileExplosionParticleLength;
extern engine::Wrapper gMissileExplosionParticleLengthSpread;
extern engine::Wrapper gMissileExplosionParticlePositionJitter;
extern engine::Wrapper gMissileExplosionParticleVelocityBase;
extern engine::Wrapper gMissileExplosionParticleVelocitySpread;
extern engine::Wrapper gMissileExplosionParticleVerticalVelocityBase;
extern engine::Wrapper gMissileExplosionParticleVerticalVelocitySpread;
extern engine::Wrapper gMissileExplosionParticleVelocityDecay;
extern engine::Wrapper gMissileExplosionParticleGravity;
extern engine::Wrapper gMissileExplosionParticleVisibleIntensity;
extern engine::Wrapper gMissileExplosionParticleIntensitySpread;
extern engine::Wrapper gMissileExplosionParticleIntensityDecay;
extern engine::Wrapper gMissileExplosionParticleIntensityPower;

extern engine::Wrapper gPlayerExplosionParticleWidth;
extern engine::Wrapper gPlayerExplosionParticleLength;
extern engine::Wrapper gPlayerExplosionParticleLengthSpread;
extern engine::Wrapper gPlayerExplosionParticlePositionJitter;
extern engine::Wrapper gPlayerExplosionParticleVelocityBase;
extern engine::Wrapper gPlayerExplosionParticleVelocitySpread;
extern engine::Wrapper gPlayerExplosionParticleVerticalVelocityBase;
extern engine::Wrapper gPlayerExplosionParticleVerticalVelocitySpread;
extern engine::Wrapper gPlayerExplosionParticleVelocityDecay;
extern engine::Wrapper gPlayerExplosionParticleGravity;
extern engine::Wrapper gPlayerExplosionParticleVisibleIntensity;
extern engine::Wrapper gPlayerExplosionParticleIntensitySpread;
extern engine::Wrapper gPlayerExplosionParticleIntensityDecay;
extern engine::Wrapper gPlayerExplosionParticleIntensityPower;

extern engine::Wrapper gSpaceshipExplosionParticleWidth;
extern engine::Wrapper gSpaceshipExplosionParticleLength;
extern engine::Wrapper gSpaceshipExplosionParticleLengthSpread;
extern engine::Wrapper gSpaceshipExplosionParticlePositionJitter;
extern engine::Wrapper gSpaceshipExplosionParticleVelocityBase;
extern engine::Wrapper gSpaceshipExplosionParticleVelocitySpread;
extern engine::Wrapper gSpaceshipExplosionParticleVerticalVelocityBase;
extern engine::Wrapper gSpaceshipExplosionParticleVerticalVelocitySpread;
extern engine::Wrapper gSpaceshipExplosionParticleVelocityDecay;
extern engine::Wrapper gSpaceshipExplosionParticleGravity;
extern engine::Wrapper gSpaceshipExplosionParticleVisibleIntensity;
extern engine::Wrapper gSpaceshipExplosionParticleIntensitySpread;
extern engine::Wrapper gSpaceshipExplosionParticleIntensityDecay;
extern engine::Wrapper gSpaceshipExplosionParticleIntensityPower;

} // namespace game
