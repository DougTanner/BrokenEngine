#include "Tweaks.h"

#include "Ui/ParticleWrappers.h"

#if defined(BT_CLIENT)

namespace game
{

void RenderParticlesSection(engine::TweaksScreen& rScreen)
{
	int64_t iSection = giTweakSectionParticles;

	if (ImGui::BeginTabBar("ParticlesTabs"))
	{
		if (rScreen.BeginSubtab("Missile", iSection, 0))
		{
			rScreen.WrapperSeparatorText("Size");
			rScreen.WrapperSlider("Missile Explosion Particle Width", gMissileExplosionParticleWidth, iSection, 1.0f);
			rScreen.WrapperSlider("Missile Explosion Particle Length", gMissileExplosionParticleLength, iSection, 1.0f);
			rScreen.WrapperSlider("Missile Explosion Particle Length Spread", gMissileExplosionParticleLengthSpread, iSection, 1.0f);
			rScreen.WrapperSlider("Missile Explosion Particle Position Jitter", gMissileExplosionParticlePositionJitter, iSection, 1.0f);

			rScreen.WrapperSeparatorText("Speed");
			rScreen.WrapperSlider("Missile Explosion Particle Velocity Base", gMissileExplosionParticleVelocityBase, iSection, 1.0f);
			rScreen.WrapperSlider("Missile Explosion Particle Velocity Spread", gMissileExplosionParticleVelocitySpread, iSection, 1.0f);
			rScreen.WrapperSlider("Missile Explosion Particle Vertical Velocity Base", gMissileExplosionParticleVerticalVelocityBase, iSection, 1.0f);
			rScreen.WrapperSlider("Missile Explosion Particle Vertical Velocity Spread", gMissileExplosionParticleVerticalVelocitySpread, iSection, 1.0f);
			rScreen.WrapperSlider("Missile Explosion Particle Velocity Decay", gMissileExplosionParticleVelocityDecay, iSection, 1.0f);
			rScreen.WrapperSlider("Missile Explosion Particle Gravity", gMissileExplosionParticleGravity, iSection, 1.0f);

			rScreen.WrapperSeparatorText("Visible");
			rScreen.WrapperSlider("Missile Explosion Particle Intensity Spread", gMissileExplosionParticleIntensitySpread, iSection, 1.0f);
			rScreen.WrapperSlider("Missile Explosion Particle Intensity Decay", gMissileExplosionParticleIntensityDecay, iSection, 1.0f);
			rScreen.WrapperSlider("Missile Explosion Particle Intensity Power", gMissileExplosionParticleIntensityPower, iSection, 1.0f);

			ImGui::EndTabItem();
		}

		if (rScreen.BeginSubtab("Omni", iSection, 1))
		{
			rScreen.WrapperSeparatorText("Size");
			rScreen.WrapperSlider("Omni Explosion Particle Width", gOmniExplosionParticleWidth, iSection, 1.0f);
			rScreen.WrapperSlider("Omni Explosion Particle Length", gOmniExplosionParticleLength, iSection, 1.0f);
			rScreen.WrapperSlider("Omni Explosion Particle Length Spread", gOmniExplosionParticleLengthSpread, iSection, 1.0f);
			rScreen.WrapperSlider("Omni Explosion Particle Position Jitter", gOmniExplosionParticlePositionJitter, iSection, 1.0f);

			rScreen.WrapperSeparatorText("Speed");
			rScreen.WrapperSlider("Omni Explosion Particle Velocity Base", gOmniExplosionParticleVelocityBase, iSection, 1.0f);
			rScreen.WrapperSlider("Omni Explosion Particle Velocity Spread", gOmniExplosionParticleVelocitySpread, iSection, 1.0f);
			rScreen.WrapperSlider("Omni Explosion Particle Vertical Velocity Base", gOmniExplosionParticleVerticalVelocityBase, iSection, 1.0f);
			rScreen.WrapperSlider("Omni Explosion Particle Vertical Velocity Spread", gOmniExplosionParticleVerticalVelocitySpread, iSection, 1.0f);
			rScreen.WrapperSlider("Omni Explosion Particle Velocity Decay", gOmniExplosionParticleVelocityDecay, iSection, 1.0f);
			rScreen.WrapperSlider("Omni Explosion Particle Gravity", gOmniExplosionParticleGravity, iSection, 1.0f);

			rScreen.WrapperSeparatorText("Visible");
			rScreen.WrapperSlider("Omni Explosion Particle Visible Intensity", gOmniExplosionParticleVisibleIntensity, iSection, 1.0f);
			rScreen.WrapperSlider("Omni Explosion Particle Intensity Spread", gOmniExplosionParticleIntensitySpread, iSection, 1.0f);
			rScreen.WrapperSlider("Omni Explosion Particle Intensity Decay", gOmniExplosionParticleIntensityDecay, iSection, 1.0f);
			rScreen.WrapperSlider("Omni Explosion Particle Intensity Power", gOmniExplosionParticleIntensityPower, iSection, 1.0f);

			ImGui::EndTabItem();
		}

		if (rScreen.BeginSubtab("Fighter", iSection, 2))
		{
			rScreen.WrapperSeparatorText("Size");
			rScreen.WrapperSlider("Fighter Explosion Particle Width", gFighterExplosionParticleWidth, iSection, 1.0f);
			rScreen.WrapperSlider("Fighter Explosion Particle Length", gFighterExplosionParticleLength, iSection, 1.0f);
			rScreen.WrapperSlider("Fighter Explosion Particle Length Spread", gFighterExplosionParticleLengthSpread, iSection, 1.0f);
			rScreen.WrapperSlider("Fighter Explosion Particle Position Jitter", gFighterExplosionParticlePositionJitter, iSection, 1.0f);

			rScreen.WrapperSeparatorText("Speed");
			rScreen.WrapperSlider("Fighter Explosion Particle Velocity Base", gFighterExplosionParticleVelocityBase, iSection, 1.0f);
			rScreen.WrapperSlider("Fighter Explosion Particle Velocity Spread", gFighterExplosionParticleVelocitySpread, iSection, 1.0f);
			rScreen.WrapperSlider("Fighter Explosion Particle Vertical Velocity Base", gFighterExplosionParticleVerticalVelocityBase, iSection, 1.0f);
			rScreen.WrapperSlider("Fighter Explosion Particle Vertical Velocity Spread", gFighterExplosionParticleVerticalVelocitySpread, iSection, 1.0f);
			rScreen.WrapperSlider("Fighter Explosion Particle Velocity Decay", gFighterExplosionParticleVelocityDecay, iSection, 1.0f);
			rScreen.WrapperSlider("Fighter Explosion Particle Gravity", gFighterExplosionParticleGravity, iSection, 1.0f);

			rScreen.WrapperSeparatorText("Visible");
			rScreen.WrapperSlider("Fighter Explosion Particle Intensity Spread", gFighterExplosionParticleIntensitySpread, iSection, 1.0f);
			rScreen.WrapperSlider("Fighter Explosion Particle Intensity Decay", gFighterExplosionParticleIntensityDecay, iSection, 1.0f);
			rScreen.WrapperSlider("Fighter Explosion Particle Intensity Power", gFighterExplosionParticleIntensityPower, iSection, 1.0f);

			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}
}

} // namespace game

#endif // BT_CLIENT
