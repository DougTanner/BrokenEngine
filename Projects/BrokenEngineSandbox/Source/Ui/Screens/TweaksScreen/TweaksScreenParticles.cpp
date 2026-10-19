#include "Tweaks.h"

#include "Ui/Screens/TweaksScreen/TweaksSliderMap.h"

#include "Ui/ParticleWrappers.h"

#if defined(BT_CLIENT)

namespace game
{

const engine::TweaksSliderMapRegistrar gParticlesRegistrar
{
	{"Missile Particle Width", &gMissileExplosionParticleWidth},
	{"Missile Particle Length", &gMissileExplosionParticleLength},
	{"Missile Particle Length Spread", &gMissileExplosionParticleLengthSpread},
	{"Missile Particle Position Jitter", &gMissileExplosionParticlePositionJitter},
	{"Missile Particle Velocity Base", &gMissileExplosionParticleVelocityBase},
	{"Missile Particle Velocity Spread", &gMissileExplosionParticleVelocitySpread},
	{"Missile Particle Vertical Velocity Base", &gMissileExplosionParticleVerticalVelocityBase},
	{"Missile Particle Vertical Velocity Spread", &gMissileExplosionParticleVerticalVelocitySpread},
	{"Missile Particle Velocity Decay", &gMissileExplosionParticleVelocityDecay},
	{"Missile Particle Gravity", &gMissileExplosionParticleGravity},
	{"Missile Particle Visible Intensity", &gMissileExplosionParticleVisibleIntensity},
	{"Missile Particle Intensity Spread", &gMissileExplosionParticleIntensitySpread},
	{"Missile Particle Intensity Decay", &gMissileExplosionParticleIntensityDecay},
	{"Missile Particle Intensity Power", &gMissileExplosionParticleIntensityPower},
	{"Omni Particle Width", &gOmniExplosionParticleWidth},
	{"Omni Particle Length", &gOmniExplosionParticleLength},
	{"Omni Particle Length Spread", &gOmniExplosionParticleLengthSpread},
	{"Omni Particle Position Jitter", &gOmniExplosionParticlePositionJitter},
	{"Omni Particle Velocity Base", &gOmniExplosionParticleVelocityBase},
	{"Omni Particle Velocity Spread", &gOmniExplosionParticleVelocitySpread},
	{"Omni Particle Vertical Velocity Base", &gOmniExplosionParticleVerticalVelocityBase},
	{"Omni Particle Vertical Velocity Spread", &gOmniExplosionParticleVerticalVelocitySpread},
	{"Omni Particle Velocity Decay", &gOmniExplosionParticleVelocityDecay},
	{"Omni Particle Gravity", &gOmniExplosionParticleGravity},
	{"Omni Particle Visible Intensity", &gOmniExplosionParticleVisibleIntensity},
	{"Omni Particle Intensity Spread", &gOmniExplosionParticleIntensitySpread},
	{"Omni Particle Intensity Decay", &gOmniExplosionParticleIntensityDecay},
	{"Omni Particle Intensity Power", &gOmniExplosionParticleIntensityPower},
	{"Fighter Particle Width", &gFighterExplosionParticleWidth},
	{"Fighter Particle Length", &gFighterExplosionParticleLength},
	{"Fighter Particle Length Spread", &gFighterExplosionParticleLengthSpread},
	{"Fighter Particle Position Jitter", &gFighterExplosionParticlePositionJitter},
	{"Fighter Particle Velocity Base", &gFighterExplosionParticleVelocityBase},
	{"Fighter Particle Velocity Spread", &gFighterExplosionParticleVelocitySpread},
	{"Fighter Particle Vertical Velocity Base", &gFighterExplosionParticleVerticalVelocityBase},
	{"Fighter Particle Vertical Velocity Spread", &gFighterExplosionParticleVerticalVelocitySpread},
	{"Fighter Particle Velocity Decay", &gFighterExplosionParticleVelocityDecay},
	{"Fighter Particle Gravity", &gFighterExplosionParticleGravity},
	{"Fighter Particle Visible Intensity", &gFighterExplosionParticleVisibleIntensity},
	{"Fighter Particle Intensity Spread", &gFighterExplosionParticleIntensitySpread},
	{"Fighter Particle Intensity Decay", &gFighterExplosionParticleIntensityDecay},
	{"Fighter Particle Intensity Power", &gFighterExplosionParticleIntensityPower},
};

void RenderParticlesSection(engine::TweaksScreen& rScreen)
{
	int64_t iSection = giTweakSectionParticles;

	if (ImGui::BeginTabBar("ParticlesTabs"))
	{
		if (rScreen.BeginSubtab("Missile", iSection, 0))
		{
			rScreen.WrapperSeparatorText("Size");
			rScreen.WrapperSlider("Width",           iSection, 1.0f, "Missile Particle Width");
			rScreen.WrapperSlider("Length",          iSection, 1.0f, "Missile Particle Length");
			rScreen.WrapperSlider("Length Spread",   iSection, 1.0f, "Missile Particle Length Spread");
			rScreen.WrapperSlider("Position Jitter", iSection, 1.0f, "Missile Particle Position Jitter");

			rScreen.WrapperSeparatorText("Speed");
			rScreen.WrapperSlider("Velocity Base",            iSection, 1.0f, "Missile Particle Velocity Base");
			rScreen.WrapperSlider("Velocity Spread",          iSection, 1.0f, "Missile Particle Velocity Spread");
			rScreen.WrapperSlider("Vertical Velocity Base",   iSection, 1.0f, "Missile Particle Vertical Velocity Base");
			rScreen.WrapperSlider("Vertical Velocity Spread", iSection, 1.0f, "Missile Particle Vertical Velocity Spread");
			rScreen.WrapperSlider("Velocity Decay",           iSection, 1.0f, "Missile Particle Velocity Decay");
			rScreen.WrapperSlider("Gravity",                  iSection, 1.0f, "Missile Particle Gravity");

			rScreen.WrapperSeparatorText("Visible");
			rScreen.WrapperSlider("Visible Intensity", iSection, 1.0f, "Missile Particle Visible Intensity");
			rScreen.WrapperSlider("Intensity Spread",  iSection, 1.0f, "Missile Particle Intensity Spread");
			rScreen.WrapperSlider("Intensity Decay",   iSection, 1.0f, "Missile Particle Intensity Decay");
			rScreen.WrapperSlider("Intensity Power",   iSection, 1.0f, "Missile Particle Intensity Power");

			ImGui::EndTabItem();
		}

		if (rScreen.BeginSubtab("Omni", iSection, 1))
		{
			rScreen.WrapperSeparatorText("Size");
			rScreen.WrapperSlider("Width",           iSection, 1.0f, "Omni Particle Width");
			rScreen.WrapperSlider("Length",          iSection, 1.0f, "Omni Particle Length");
			rScreen.WrapperSlider("Length Spread",   iSection, 1.0f, "Omni Particle Length Spread");
			rScreen.WrapperSlider("Position Jitter", iSection, 1.0f, "Omni Particle Position Jitter");

			rScreen.WrapperSeparatorText("Speed");
			rScreen.WrapperSlider("Velocity Base",            iSection, 1.0f, "Omni Particle Velocity Base");
			rScreen.WrapperSlider("Velocity Spread",          iSection, 1.0f, "Omni Particle Velocity Spread");
			rScreen.WrapperSlider("Vertical Velocity Base",   iSection, 1.0f, "Omni Particle Vertical Velocity Base");
			rScreen.WrapperSlider("Vertical Velocity Spread", iSection, 1.0f, "Omni Particle Vertical Velocity Spread");
			rScreen.WrapperSlider("Velocity Decay",           iSection, 1.0f, "Omni Particle Velocity Decay");
			rScreen.WrapperSlider("Gravity",                  iSection, 1.0f, "Omni Particle Gravity");

			rScreen.WrapperSeparatorText("Visible");
			rScreen.WrapperSlider("Visible Intensity", iSection, 1.0f, "Omni Particle Visible Intensity");
			rScreen.WrapperSlider("Intensity Spread",  iSection, 1.0f, "Omni Particle Intensity Spread");
			rScreen.WrapperSlider("Intensity Decay",   iSection, 1.0f, "Omni Particle Intensity Decay");
			rScreen.WrapperSlider("Intensity Power",   iSection, 1.0f, "Omni Particle Intensity Power");

			ImGui::EndTabItem();
		}

		if (rScreen.BeginSubtab("Fighter", iSection, 2))
		{
			rScreen.WrapperSeparatorText("Size");
			rScreen.WrapperSlider("Width",           iSection, 1.0f, "Fighter Particle Width");
			rScreen.WrapperSlider("Length",          iSection, 1.0f, "Fighter Particle Length");
			rScreen.WrapperSlider("Length Spread",   iSection, 1.0f, "Fighter Particle Length Spread");
			rScreen.WrapperSlider("Position Jitter", iSection, 1.0f, "Fighter Particle Position Jitter");

			rScreen.WrapperSeparatorText("Speed");
			rScreen.WrapperSlider("Velocity Base",            iSection, 1.0f, "Fighter Particle Velocity Base");
			rScreen.WrapperSlider("Velocity Spread",          iSection, 1.0f, "Fighter Particle Velocity Spread");
			rScreen.WrapperSlider("Vertical Velocity Base",   iSection, 1.0f, "Fighter Particle Vertical Velocity Base");
			rScreen.WrapperSlider("Vertical Velocity Spread", iSection, 1.0f, "Fighter Particle Vertical Velocity Spread");
			rScreen.WrapperSlider("Velocity Decay",           iSection, 1.0f, "Fighter Particle Velocity Decay");
			rScreen.WrapperSlider("Gravity",                  iSection, 1.0f, "Fighter Particle Gravity");

			rScreen.WrapperSeparatorText("Visible");
			rScreen.WrapperSlider("Visible Intensity", iSection, 1.0f, "Fighter Particle Visible Intensity");
			rScreen.WrapperSlider("Intensity Spread",  iSection, 1.0f, "Fighter Particle Intensity Spread");
			rScreen.WrapperSlider("Intensity Decay",   iSection, 1.0f, "Fighter Particle Intensity Decay");
			rScreen.WrapperSlider("Intensity Power",   iSection, 1.0f, "Fighter Particle Intensity Power");

			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}
}

} // namespace game

#endif // BT_CLIENT
