#if defined(BT_CLIENT)

#include "Graphics/CameraBase.h"
#include "Ui/HeightLerpWrapperQuartet.h"
#include "Ui/LightingWrappersBase.h"
#include "Ui/MiscWrappersBase.h"
#include "Ui/PbrWrappersBase.h"
#include "Ui/ShadowWrappersBase.h"
#include "Ui/SmokeWrappersBase.h"
#include "Ui/SunMoonWrappersBase.h"
#include "Ui/TerrainWrappersBase.h"
#include "Ui/WaterWrappersBase.h"
#include "Ui/WrapperBase.h"
#include "Render.h"

namespace engine
{

// 0.0 outside the rise/set window (day side), 1.0 inside (night side), smooth lerps in the
// rise and set transition windows. Shape parameters are 4 sun-angle boundaries: rise opens at
// fRiseStart and finishes at fRiseEnd (transition from day to night side), set opens at
// fSetStart and finishes at fSetEnd (transition back).
static float ComputeNightEnvelope(float fSunAngle, float fRiseStart, float fRiseEnd, float fSetStart, float fSetEnd)
{
	if (fSunAngle >= fRiseStart && fSunAngle <= fRiseEnd)
	{
		return (fSunAngle - fRiseStart) / (fRiseEnd - fRiseStart);
	}
	if (fSunAngle > fRiseEnd || fSunAngle <= fSetStart)
	{
		return 1.0f;
	}
	if (fSunAngle >= fSetStart && fSunAngle <= fSetEnd)
	{
		return 1.0f - (fSunAngle - fSetStart) / (fSetEnd - fSetStart);
	}
	return 0.0f;
}

// Sun/Moon direction + tilt: stores the normalized sky direction in f4SunMoonNormal and returns it so the
// shadow-stretch translation (which also needs it) can be folded CPU-side without re-reading write-only memory.
static XMVECTOR XM_CALLCONV PopulateSunMoonDirection(shaders::GlobalLayout& rGlobalLayout, float fSunAngle)
{
	// Sun/Moon direction: night reverses across the sky from sunset back to sunrise
	float fDirectionAngle = fSunAngle < XM_PI ? fSunAngle : XM_2PI - fSunAngle;
	XMVECTOR vecSunMoonNormal = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
	XMMATRIX matSunRotation = XMMatrixRotationY(-fDirectionAngle);
	vecSunMoonNormal = XMVector4Transform(vecSunMoonNormal, matSunRotation);
	// Normal-only pitch tilt around world X. Shadows derive from raw fSunAngle and are unaffected.
	XMMATRIX matSunTilt = XMMatrixRotationX(gSunMoonNormalTilt.mfCurrent);
	vecSunMoonNormal = XMVector4Normalize(XMVector4Transform(vecSunMoonNormal, matSunTilt));
	XMStoreFloat4(&rGlobalLayout.f4SunMoonNormal, vecSunMoonNormal);

	// Uniform-only sun-normal products folded here (both need the finished sun/moon normal): Terrain.frag's
	// snow sun-normal tilt (fTerrainSnowBlend * normal) and Water.frag's skybox biased sun normal.
	XMStoreFloat4(&rGlobalLayout.f4TerrainSnowSunNormal, XMVectorScale(vecSunMoonNormal, gTerrainSnowBlend.mfCurrent));
	XMVECTOR vecBiasedSunNormal = XMVector3Normalize(XMVectorAdd(vecSunMoonNormal, XMVectorSet(0.0f, 0.0f, gLightingWaterSkyboxSunBias.mfCurrent, 0.0f)));
	XMStoreFloat4(&rGlobalLayout.f4WaterBiasedSunNormal, vecBiasedSunNormal);

	return vecSunMoonNormal;
}

// Piecewise day-cycle sun/moon color + ambient ramp, the sun/moon intensity stores, and the precomputed
// uniform-only sun/moon/ambient products consumed by SunLighting, the smoke helpers, and Water.frag. Returns
// the finished ambient color so the shadow-ambient split can be folded without re-reading write-only memory.
static XMVECTOR XM_CALLCONV PopulateDayCycleColors(shaders::GlobalLayout& rGlobalLayout, float fSunAngle)
{
	float fAmbientNight = gSunMoonMinimumAmbient.mfCurrent;
	float fAmbientMorning = std::max(0.075f, gSunMoonMinimumAmbient.mfCurrent);
	XMVECTOR vecSunMorning = XMVectorScale(XMVectorSet(1.0f, 219.0f / 255.0f, 0.0f, 1.0f), 0.5f);
	XMVECTOR vecAmbientMorning = XMVectorSet(fAmbientMorning, fAmbientMorning, fAmbientMorning, 1.0f);
	XMVECTOR vecSunNoon = XMVectorSet(0.8f, 0.8f, 0.8f, 1.0f);
	XMVECTOR vecAmbientNoon = XMVectorSet(0.2f, 0.2f, 0.2f, 1.0f);
	XMVECTOR vecSunEvening = XMVectorScale(XMVectorSet(0.8f, 0.4f, 0.4f, 1.0f), 0.75f);
	XMVECTOR vecAmbientEvening = XMVectorSet(fAmbientMorning, fAmbientMorning, fAmbientMorning, 1.0f);
	XMVECTOR vecMidnight = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
	XMVECTOR vecAmbientMidnight = XMVectorSet(fAmbientNight, fAmbientNight, fAmbientNight, 1.0f);
	XMVECTOR vecMoonFloor = XMVectorSet(1.0f, 1.0f, gSunMoonMoonBlueTint.mfCurrent, 0.0f);

	XMVECTOR vecSunMoon = vecMidnight;
	XMVECTOR vecAmbient = vecAmbientMidnight;

	float fMorning = gSunMoonMorning.mfCurrent;
	float fNoonStart = gSunMoonNoonStart.mfCurrent;
	float fNoonEnd = gSunMoonNoonEnd.mfCurrent;
	float fEvening = gSunMoonEvening.mfCurrent;
	float fNightStart = gSunMoonNightStart.mfCurrent;

	if (fSunAngle >= fMorning && fSunAngle < fNoonStart)
	{
		float fInterpolation = (fSunAngle - fMorning) / (fNoonStart - fMorning);
		vecSunMoon = XMVectorLerp(vecSunMorning, vecSunNoon, fInterpolation);
		vecAmbient = XMVectorLerp(vecAmbientMorning, vecAmbientNoon, fInterpolation);
	}
	else if (fSunAngle >= fNoonStart && fSunAngle < fNoonEnd)
	{
		vecSunMoon = vecSunNoon;
		vecAmbient = vecAmbientNoon;
	}
	else if (fSunAngle >= fNoonEnd && fSunAngle < fEvening)
	{
		float fInterpolation = (fSunAngle - fNoonEnd) / (fEvening - fNoonEnd);
		vecSunMoon = XMVectorLerp(vecSunNoon, vecSunEvening, fInterpolation);
		vecAmbient = XMVectorLerp(vecAmbientNoon, vecAmbientEvening, fInterpolation);
	}
	else if (fSunAngle >= fEvening && fSunAngle < fNightStart)
	{
		float fInterpolation = (fSunAngle - fEvening) / (fNightStart - fEvening);
		vecSunMoon = XMVectorLerp(vecSunEvening, vecMidnight, fInterpolation);
		vecAmbient = XMVectorLerp(vecAmbientEvening, vecAmbientMidnight, fInterpolation);
	}
	else if (fSunAngle >= fNightStart)
	{
		vecAmbient = vecAmbientMidnight;
	}
	else if (fSunAngle >= 0.0f)
	{
		float fInterpolation = fSunAngle / fMorning;
		vecSunMoon = XMVectorLerp(vecMidnight, vecSunMorning, fInterpolation);
		vecAmbient = XMVectorLerp(vecAmbientMidnight, vecAmbientMorning, fInterpolation);
	}
	else
	{
		DEBUG_BREAK();
	}

	// Sun: piecewise lerp goes naturally to (0,0,0) at midnight; no moon-floor clamp here.
	// Per-target sun intensity sliders scale at shader read sites, not here.
	XMStoreFloat4(&rGlobalLayout.f4SunColor, vecSunMoon);

	// Moon: floor color modulated by the moonrise/moonset envelope. Per-target moon intensity
	// sliders scale at shader read sites, not here.
	// Moon color follows its own rise/set schedule, independent of the shadow night gate.
	float fMoonAmount = ComputeNightEnvelope(fSunAngle, gSunMoonMoonriseStart.mfCurrent, gSunMoonMoonriseEnd.mfCurrent, gSunMoonMoonsetStart.mfCurrent, gSunMoonMoonsetEnd.mfCurrent);
	XMVECTOR vecMoon = XMVectorScale(vecMoonFloor, fMoonAmount);
	XMStoreFloat4(&rGlobalLayout.f4MoonColor, vecMoon);

	// Per-target sun/moon intensities. Terrain, Water, and Objects capture these values as locals and fold them into precomputed products;
	// Smoke remains a raw multiplier.
	float fSunIntensityTerrain = gSunMoonSunIntensityTerrain.mfCurrent;
	float fMoonIntensityTerrain = gSunMoonMoonIntensityTerrain.mfCurrent;
	float fSunIntensityWater = gSunMoonSunIntensityWater.mfCurrent;
	float fMoonIntensityWater = gSunMoonMoonIntensityWater.mfCurrent;
	float fSunIntensitySmoke = gSunMoonSunIntensitySmoke.mfCurrent;
	float fMoonIntensitySmoke = gSunMoonMoonIntensitySmoke.mfCurrent;
	float fSunIntensityObjects = gSunMoonSunIntensityObjects.mfCurrent;
	float fMoonIntensityObjects = gSunMoonMoonIntensityObjects.mfCurrent;
	rGlobalLayout.fSunIntensitySmoke    = fSunIntensitySmoke;
	rGlobalLayout.fMoonIntensitySmoke   = fMoonIntensitySmoke;

	vecAmbient = XMVectorSetW(XMVectorScale(vecAmbient, gSunMoonAmbientMultiplier.mfCurrent), 1.0f);
	XMStoreFloat4(&rGlobalLayout.f4AmbientColor, vecAmbient);

	// Terrain, smoke, and water shaders consume these day-cycle products; all operands are uniform across a draw.
	XMVECTOR vecRecommendation601 = XMVectorSet(0.299f, 0.587f, 0.114f, 0.0f);

	// Terrain sun/moon (SunLighting): per-target-scaled colors + Rec.601 magnitudes for the ambient-shadow blend.
	XMVECTOR vecSunTerrain = XMVectorScale(vecSunMoon, fSunIntensityTerrain);
	XMVECTOR vecMoonTerrain = XMVectorScale(vecMoon, fMoonIntensityTerrain);
	XMStoreFloat4(&rGlobalLayout.f4SunColorTerrain, vecSunTerrain);
	XMStoreFloat4(&rGlobalLayout.f4MoonColorTerrain, vecMoonTerrain);
	float fSunMagnitudeTerrain = XMVectorGetX(XMVector3Dot(vecSunTerrain, vecRecommendation601));
	float fMoonMagnitudeTerrain = XMVectorGetX(XMVector3Dot(vecMoonTerrain, vecRecommendation601));
	rGlobalLayout.fSunMagnitudeTerrain = fSunMagnitudeTerrain;
	rGlobalLayout.fMoonMagnitudeTerrain = fMoonMagnitudeTerrain;
	rGlobalLayout.fSunMoonMagnitudeSumInverseTerrain = 1.0f / std::max(fSunMagnitudeTerrain + fMoonMagnitudeTerrain, 0.001f);

	// Smoke base lighting (AddSmoke/BlendSmokePrecomputed): componentwise max(sun, moon) sky term + ambient.
	XMVECTOR vecSmokeBase = XMVectorAdd(XMVectorMax(XMVectorScale(vecSunMoon, fSunIntensitySmoke), XMVectorScale(vecMoon, fMoonIntensitySmoke)), vecAmbient);
	XMStoreFloat4(&rGlobalLayout.f4SmokeBaseLighting, vecSmokeBase);

	// Water sun/moon (Water.frag): max-combine for skybox tint + its /3 scalar, the /3 ambient scalar, and the
	// Rec.601 effective-shadow weights with their guarded reciprocal-sum.
	XMVECTOR vecWaterSun = XMVectorScale(vecSunMoon, fSunIntensityWater);
	XMVECTOR vecWaterMoon = XMVectorScale(vecMoon, fMoonIntensityWater);
	XMVECTOR vecWaterSunOrMoon = XMVectorMax(vecWaterSun, vecWaterMoon);
	XMStoreFloat4(&rGlobalLayout.f4WaterSunOrMoon, vecWaterSunOrMoon);
	rGlobalLayout.fWaterSunScalar = (XMVectorGetX(vecWaterSunOrMoon) + XMVectorGetY(vecWaterSunOrMoon) + XMVectorGetZ(vecWaterSunOrMoon)) / 3.0f;
	rGlobalLayout.fWaterAmbientScalar = (XMVectorGetX(vecAmbient) + XMVectorGetY(vecAmbient) + XMVectorGetZ(vecAmbient)) / 3.0f;
	float fWaterSunWeight = XMVectorGetX(XMVector3Dot(vecWaterSun, vecRecommendation601));
	float fWaterMoonWeight = XMVectorGetX(XMVector3Dot(vecWaterMoon, vecRecommendation601));
	rGlobalLayout.fWaterSunWeight = fWaterSunWeight;
	rGlobalLayout.fWaterMoonWeight = fWaterMoonWeight;
	rGlobalLayout.fWaterShadowWeightSumInverse = 1.0f / std::max(0.001f, fWaterSunWeight + fWaterMoonWeight);

	// Fold Main-phase PBR tunables here while day-cycle colors are CPU-local; reading mapped write-combined uniforms back stalls. Direct BRDF
	// products include Object intensity and fPhysicallyBasedRenderingSun. IBL uses separate unscaled Rec.709 luminance / fPhysicallyBasedRenderingDayBrightness so it stays linear in
	// fPhysicallyBasedRenderingSun; scaling that luminance compounds to fPhysicallyBasedRenderingSun^3. The brightness reciprocal is intentionally unguarded.
	XMVECTOR vecRecommendation709 = XMVectorSet(0.2126f, 0.7152f, 0.0722f, 0.0f);
	float fPhysicallyBasedRenderingSun = gPhysicallyBasedRenderingSun.mfCurrent;
	float fPhysicallyBasedRenderingDayBrightnessInverse = 1.0f / gPhysicallyBasedRenderingDayBrightness.mfCurrent;
	XMVECTOR vecPhysicallyBasedRenderingSunColorObjects = XMVectorScale(vecSunMoon, fSunIntensityObjects * fPhysicallyBasedRenderingSun);
	XMVECTOR vecPhysicallyBasedRenderingMoonColorObjects = XMVectorScale(vecMoon, fMoonIntensityObjects * fPhysicallyBasedRenderingSun);
	XMStoreFloat4(&rGlobalLayout.f4PhysicallyBasedRenderingSunColorObjects, vecPhysicallyBasedRenderingSunColorObjects);
	XMStoreFloat4(&rGlobalLayout.f4PhysicallyBasedRenderingMoonColorObjects, vecPhysicallyBasedRenderingMoonColorObjects);
	XMStoreFloat4(&rGlobalLayout.f4PhysicallyBasedRenderingSunColorObjectsImageBasedLighting, XMVectorScale(vecPhysicallyBasedRenderingSunColorObjects, XMVectorGetX(XMVector3Dot(vecSunMoon, vecRecommendation709)) * fPhysicallyBasedRenderingDayBrightnessInverse));
	XMStoreFloat4(&rGlobalLayout.f4PhysicallyBasedRenderingMoonColorObjectsImageBasedLighting, XMVectorScale(vecPhysicallyBasedRenderingMoonColorObjects, XMVectorGetX(XMVector3Dot(vecMoon, vecRecommendation709)) * fPhysicallyBasedRenderingDayBrightnessInverse));

	return vecAmbient;
}

static void PopulateDayCycleFeatherWindows(float fSunAngle, float& rfDayPercent, float& rfNoonPercent)
{
	static constexpr float kfNoonFeatherEnd = XM_PIDIV8;
	rfNoonPercent = 0.0f;
	if (fSunAngle >= kfNoonFeatherEnd && fSunAngle <= XM_PIDIV2)
	{
		rfNoonPercent = (fSunAngle - kfNoonFeatherEnd) / (XM_PIDIV2 - kfNoonFeatherEnd);
	}
	else if (fSunAngle > XM_PIDIV2 && fSunAngle <= (XM_PI - kfNoonFeatherEnd))
	{
		rfNoonPercent = 1.0f - (fSunAngle - XM_PIDIV2) / (XM_PIDIV2 - kfNoonFeatherEnd);
	}

	rfDayPercent = 0.0f;
	if (fSunAngle >= 0.0f && fSunAngle <= XM_PIDIV2)
	{
		rfDayPercent = fSunAngle / XM_PIDIV2;
	}
	else if (fSunAngle > XM_PIDIV2 && fSunAngle <= XM_PI)
	{
		rfDayPercent = 1.0f - (fSunAngle - XM_PIDIV2) / XM_PIDIV2;
	}
}

static void PopulateSunAndLighting(shaders::GlobalLayout& rGlobalLayout, float fSunAngle, float& rfDayPercent, float& rfNoonPercent, XMVECTOR& rvecSunMoonNormalOut, XMVECTOR& rvecAmbientColorOut)
{
	rvecSunMoonNormalOut = PopulateSunMoonDirection(rGlobalLayout, fSunAngle);
	rvecAmbientColorOut = PopulateDayCycleColors(rGlobalLayout, fSunAngle);
	PopulateDayCycleFeatherWindows(fSunAngle, rfDayPercent, rfNoonPercent);

	float fLightingTimeOfDayMultiplier = rfDayPercent * gLightingDayFinalMultiplier.mfCurrent + (1.0f - rfDayPercent) * gLightingNightFinalMultiplier.mfCurrent;
	rGlobalLayout.fLightingTimeOfDayMultiplier = fLightingTimeOfDayMultiplier;
	// Smoke lighting multiplier folded with time-of-day (AddSmoke/BlendSmokePrecomputed): both inputs are known
	// here (fSmokeLightingMultiplier source from RenderSmokeGlobal earlier this frame, time-of-day just above).
	rGlobalLayout.fSmokeLightingCombinedMultiplier = fLightingTimeOfDayMultiplier * gSmokeLightingMultiplier.mfCurrent;
	rGlobalLayout.fLightingWaterSkyboxOne = gLightingWaterSkyboxOne.mfCurrent + (1.0f - rfDayPercent) * 1.5f * gLightingWaterSkyboxOne.mfCurrent;
	// Skybox normal soften blends a sunrise/sunset (low-sun) value toward a noon value by the noon feather (1 at solar noon, 0 toward both horizons / night).
	rGlobalLayout.fLightingWaterSkyboxNormalSoften = (1.0f - rfNoonPercent) * gLightingWaterSkyboxNormalSoftenSunrise.mfCurrent + rfNoonPercent * gLightingWaterSkyboxNormalSoftenNoon.mfCurrent;
}

static void XM_CALLCONV PopulateShadowStretch(shaders::GlobalLayout& rGlobalLayout, float fSunAngle, FXMVECTOR vecSunMoonNormal)
{
	static constexpr float kfSunriseStretchBegin = XM_2PI - XM_PIDIV4;
	static constexpr float kfSunriseStretchEnd = XM_PIDIV2 - XM_PIDIV16;
	static constexpr float kfSunriseStretchTotal = (XM_2PI - kfSunriseStretchBegin) + kfSunriseStretchEnd;
	float fSunriseStretch = 0.0f;
	static constexpr float kfSunsetStretchBegin = XM_PIDIV2 + XM_PIDIV16;
	static constexpr float kfSunsetStretchEnd = XM_PI + XM_PIDIV4;
	static constexpr float kfSunsetStretchTotal = kfSunsetStretchEnd - kfSunsetStretchBegin;
	float fSunsetStretch = 0.0f;
	if (fSunAngle >= kfSunriseStretchBegin || fSunAngle < 0.0f)
	{
		fSunriseStretch = 1.0f - (fSunAngle - kfSunriseStretchBegin) / (kfSunriseStretchTotal);
	}
	else if (fSunAngle >= 0.0f && fSunAngle < kfSunriseStretchEnd)
	{
		fSunriseStretch = 1.0f - (fSunAngle + (XM_2PI - kfSunriseStretchBegin)) / kfSunriseStretchTotal;
	}
	else if (fSunAngle >= kfSunriseStretchEnd && fSunAngle < kfSunsetStretchBegin)
	{
		// No horizon stretch between the sunrise and sunset ramps.
	}
	else if (fSunAngle >= kfSunsetStretchBegin && fSunAngle < kfSunsetStretchEnd)
	{
		fSunsetStretch = (fSunAngle - kfSunsetStretchBegin) / kfSunsetStretchTotal;
	}
	else if (fSunAngle >= kfSunsetStretchEnd)
	{
		fSunriseStretch = 1.0f;
		fSunsetStretch = 1.0f;
	}

	// ShadowStretchProjection consumes the uniform stretch cubes and translation; only its position-dependent fStretchX remains shader-side.
	float fStretchScale = gObjectShadowsSunsetStretch.mfCurrent;
	float fSunriseStretchScaled = fSunriseStretch * fStretchScale;
	float fSunsetStretchScaled = fSunsetStretch * fStretchScale;
	float fSunriseStretchCubed = fSunriseStretchScaled * fSunriseStretchScaled * fSunriseStretchScaled;
	float fSunsetStretchCubed = fSunsetStretchScaled * fSunsetStretchScaled * fSunsetStretchScaled;
	rGlobalLayout.fShadowSunriseStretchCubed = fSunriseStretchCubed;
	rGlobalLayout.fShadowSunsetStretchCubed = fSunsetStretchCubed;
	float fSumCubes = fSunriseStretchCubed + fSunsetStretchCubed;
	rGlobalLayout.f2ShadowStretchTranslation.x = -fSumCubes * XMVectorGetX(vecSunMoonNormal);
	rGlobalLayout.f2ShadowStretchTranslation.y = -fSumCubes * XMVectorGetY(vecSunMoonNormal);
}

static void PopulateShadowArea(shaders::GlobalLayout& rGlobalLayout, float fShadowTextureSizeWidth, float fShadowTextureSizeHeight, float& rfWorldTexelX, float& rfFullWidth)
{
	// Shadow texels use a coverage-safe eye-height reference: outward zoom expands it immediately, inward zoom contracts it at the existing
	// rate, and it never falls below live height. Derive texel size from the analytic straight-down gFieldOfView/aspect frustum, not the snapped render
	// area, so the grid is bit-stable at settled height and integer-texel XY pan does not shimmer. textureWidth / kfShadowHeadroomMultiplier
	// pixels span that frustum; the headroom covers the live area throughout zoom transitions. Use the actual clamped extent for
	// device-independent coverage and center f4ShadowArea on the camera, snapped to the texel grid.
	WorldSizedTexelArea area = ComputeWorldSizedTexelArea(engine::CameraBase::kfShadowHeadroomMultiplier, engine::gpCamera->mfShadowTexelEyeHeight, fShadowTextureSizeWidth, fShadowTextureSizeHeight, gpSwapchainManager->mfAspectRatio, gFieldOfView.mfCurrent, engine::gpCamera->mVecPosition);
	rGlobalLayout.f4ShadowArea = area.f4Area;

	// Latch the previous world area. A recreate makes the previous area current and forces pure-current temporal
	// output; otherwise ShadowTemporal.comp rejects reprojected samples that land outside the previous footprint.
	static TemporalAreaLatch sTemporalAreaLatch {};
	// The retained rectangle is in the camera cell's frame; follow a camera cell change before it is compared with
	// this frame's area, and let a multi-cell jump take the existing reset.
	static RetainedAreaBasis sRetainedAreaBasis {};
	if (std::optional<XMFLOAT2> of2Shift = sRetainedAreaBasis.Advance(engine::gpCamera->mBasisCoordinate))
	{
		ShiftArea(sTemporalAreaLatch.f4PreviousArea, *of2Shift);
	}
	else
	{
		gbShadowTemporalReset = true;
		++gPresentationContinuity.shadow.iHistoryResets;
	}
	// The latch publishes the previous area through a local so the probe snapshot can report the same rectangle the
	// mapped write-only layout receives.
	XMFLOAT4 f4PreviousArea {};
	rGlobalLayout.fShadowTemporalBlend = sTemporalAreaLatch.Update(area.f4Area, gbShadowTemporalReset, gShadowTemporalBlend.mfCurrent, f4PreviousArea);
	rGlobalLayout.f4ShadowAreaPrevious = f4PreviousArea;
	gPresentationContinuity.shadow.f4CurrentArea = area.f4Area;
	gPresentationContinuity.shadow.f4PreviousArea = f4PreviousArea;

	rfWorldTexelX = area.fWorldTexelX;
	rfFullWidth = area.fFullWidth;
}

static void PopulateShadowSunExtension(shaders::GlobalLayout& rGlobalLayout, float fSunAngle, float fWorldTexelX, float fFullWidth, float fShadowElevationTextureSizeWidth, float fShadowTextureSizeWidth, float& rfShadowSunAngle)
{
	// Sun-direction extension: extend by half the shadow-area width on the sun side. The elevation
	// texture is 1.5x wider than the shadow texture and rasterizes f4ShadowAreaExtra; the extension
	// fills exactly the extra half.
	rGlobalLayout.f4ShadowAreaExtra = rGlobalLayout.f4ShadowArea;
	float fHalfWidth = fFullWidth * 0.5f;
	if (fSunAngle >= XM_PI + XM_PIDIV2 || fSunAngle < XM_PIDIV2)
	{
		rGlobalLayout.f4ShadowAreaExtra.z += fHalfWidth;

		rGlobalLayout.fShadowWidthScale = fWorldTexelX;
		if (fSunAngle >= 0.0f && fSunAngle < XM_PIDIV2)
		{
			rfShadowSunAngle = fSunAngle;
		}
		else
		{
			rfShadowSunAngle = XM_2PI - fSunAngle;
		}
		rGlobalLayout.fShadowDirectionMultiplier = 1.0f;

		rGlobalLayout.iShadowElevationSize = static_cast<int32_t>(fShadowElevationTextureSizeWidth);
		rGlobalLayout.iShadowIncrement = 1;
		rGlobalLayout.iShadowStartOffset = 0;
	}
	else
	{
		rGlobalLayout.f4ShadowAreaExtra.x -= fHalfWidth;

		rGlobalLayout.fShadowWidthScale = -fWorldTexelX;
		rfShadowSunAngle = fSunAngle >= XM_PI ? fSunAngle - XM_PI : XM_PI - fSunAngle;
		rGlobalLayout.fShadowDirectionMultiplier = -1.0f;

		rGlobalLayout.iShadowElevationSize = 0;
		rGlobalLayout.iShadowIncrement = -1;
		rGlobalLayout.iShadowStartOffset = static_cast<int32_t>(fShadowElevationTextureSizeWidth - fShadowTextureSizeWidth); // Elevation extension half-width, from real extents.
	}
}

static void XM_CALLCONV PopulateShadowParameters(shaders::GlobalLayout& rGlobalLayout, float fSunAngle, float fDayPercent, float fNoonPercent, FXMVECTOR vecSunMoonNormal, FXMVECTOR vecAmbientColor)
{
	VkExtent3D vkShadowTextureExtent = gpTextureManager->mRenderTargetTextures.mShadowTexture.mInfo.vkExtent3D;
	float fShadowTextureSizeWidth = static_cast<float>(vkShadowTextureExtent.width);
	float fShadowTextureSizeHeight = static_cast<float>(vkShadowTextureExtent.height);
	// 1.5x-wide elevation texture: read the created extent so the headroom factor has a single owner at the
	// allocation site (RenderTargetTextures::CreateShadowTextures), like the shadow extent read just above.
	float fShadowElevationTextureSizeWidth = static_cast<float>(gpTextureManager->mRenderTargetTextures.mShadowElevationTexture.mInfo.vkExtent3D.width);

	float fShadowNoon = std::pow(fDayPercent, gShadowFeatherPower.mfCurrent);
	float fShadowEvening = 1.0f - fShadowNoon;
	float fOffsetNoon = std::pow(fNoonPercent, 2.0f);

	rGlobalLayout.fShadowFeather = 1.0f / (fShadowNoon * gShadowFeatherNoon.mfCurrent + fShadowEvening * gShadowFeatherSunset.mfCurrent);
	// fShadowNoonOffset contributes to fShadowAngleOffsetSum after the sun extension resolves the shadow sun angle.
	float fShadowNoonOffset = fOffsetNoon * gShadowFeatherNoonOffset.mfCurrent;

	float fShadowDistanceFalloff = gShadowDistanceFalloff.mfCurrent;
	rGlobalLayout.fShadowDistanceFalloff = fShadowDistanceFalloff;
	rGlobalLayout.fShadowDistanceFalloffInverse = 1.0f / fShadowDistanceFalloff;

	// Separable Gaussian blur kernel precomputed for ShadowBlurH/V: the kernel is symmetric, so store the
	// half-kernel [0, radius] and the reciprocal of the full-span weight sum (center counts once, each side twice).
	float fShadowBlurSigma = std::max(gShadowBlurSigma.mfCurrent, 1.0e-6f);
	float fShadowBlurWeightSum = 0.0f;
	for (int64_t i = 0; i <= shaders::kiShadowBlurRadius; ++i)
	{
		float fNormalizedOffset = static_cast<float>(i) / fShadowBlurSigma;
		float fWeight = std::exp(-0.5f * fNormalizedOffset * fNormalizedOffset);
		rGlobalLayout.pfShadowBlurWeights[i] = fWeight;
		fShadowBlurWeightSum += (i == 0) ? fWeight : 2.0f * fWeight;
	}
	rGlobalLayout.fShadowBlurWeightSumInverse = 1.0f / fShadowBlurWeightSum;

	rGlobalLayout.fObjectShadowsBlurSigma = gObjectShadowsBlurSigma.mfCurrent;
	rGlobalLayout.iObjectShadowsBlurRadius = static_cast<int32_t>(gObjectShadowsBlurRadius.mfCurrent);
	rGlobalLayout.fObjectShadowsGrow = gObjectShadowsGrow.mfCurrent;
	rGlobalLayout.fObjectShadowsIntensity = fDayPercent * gObjectShadowsNoon.mfCurrent + (1.0f - fDayPercent) * gObjectShadowsSunset.mfCurrent;
	rGlobalLayout.fObjectShadowsIntensity *= std::pow(fDayPercent, 0.1f);
	// fShadowSunsetOffset contributes to fShadowAngleOffsetSum after the sun extension resolves the shadow sun angle.
	float fShadowSunsetOffset = fShadowEvening * gShadowFeatherSunsetOffset.mfCurrent;

	PopulateShadowStretch(rGlobalLayout, fSunAngle, vecSunMoonNormal);
	// fShadowAffectAmbient and the two f4AmbientColor products (SunLighting ambient split) are uniform-only;
	// fold the split here where both fShadowAffectAmbient and the finished ambient color are CPU-local.
	float fShadowAffectAmbient = std::pow(fDayPercent, 0.25f) * gShadowAffectAmbient.mfCurrent;
	rGlobalLayout.fShadowAffectAmbient = fShadowAffectAmbient;
	XMStoreFloat4(&rGlobalLayout.f4AmbientUnshadowed, XMVectorScale(vecAmbientColor, 1.0f - fShadowAffectAmbient));
	XMStoreFloat4(&rGlobalLayout.f4AmbientShadowed, XMVectorScale(vecAmbientColor, fShadowAffectAmbient));

	rGlobalLayout.f2ShadowTextureSizeInverse.x = 1.0f / fShadowTextureSizeWidth;
	rGlobalLayout.f2ShadowTextureSizeInverse.y = 1.0f / fShadowTextureSizeHeight;
	rGlobalLayout.f2ShadowElevationTextureSizeInverse.x = 1.0f / fShadowElevationTextureSizeWidth;
	rGlobalLayout.f2ShadowElevationTextureSizeInverse.y = 1.0f / fShadowTextureSizeHeight; // Elevation texture shares the shadow texture's height.

	float fShadowHeightFadeTop = gShadowHeightFadeTop.mfCurrent;
	float fShadowHeightFadeBottom = gShadowHeightFadeBottom.mfCurrent;
	rGlobalLayout.fShadowHeightFadeBottom = fShadowHeightFadeBottom;
	// Fade top is [0,20] and bottom is [-30,0], so the range is nonnegative; top == bottom == 0 yields +inf, which Shadow.comp's final clamp
	// absorbs.
	rGlobalLayout.fShadowHeightFadeRangeInverse = 1.0f / (fShadowHeightFadeTop - fShadowHeightFadeBottom);

	float fWorldTexelX = 0.0f;
	float fFullWidth = 0.0f;
	PopulateShadowArea(rGlobalLayout, fShadowTextureSizeWidth, fShadowTextureSizeHeight, fWorldTexelX, fFullWidth);

	float fShadowSunAngle = 0.0f;
	PopulateShadowSunExtension(rGlobalLayout, fSunAngle, fWorldTexelX, fFullWidth, fShadowElevationTextureSizeWidth, fShadowTextureSizeWidth, fShadowSunAngle);

	// Shadow.comp's per-occluder feather term uses the sum of the shadow sun angle and noon/sunset offsets.
	rGlobalLayout.fShadowAngleOffsetSum = fShadowSunAngle + fShadowNoonOffset + fShadowSunsetOffset;

	// Shadow night-gate timing is independent of the moon-color envelope.
	// Shadow night-gate envelope — drives fShadowMoonMultiplier (Shadow.comp output scaling).
	rGlobalLayout.fShadowMoonMultiplier = std::lerp(1.0f, gSunMoonShadowNightMultiplier.mfCurrent, ComputeNightEnvelope(fSunAngle, gSunMoonShadowSunsetStart.mfCurrent, gSunMoonShadowSunsetEnd.mfCurrent, gSunMoonShadowSunriseStart.mfCurrent, gSunMoonShadowSunriseEnd.mfCurrent));
}

static void PopulateTerrainParameters(shaders::GlobalLayout& rGlobalLayout, float fDayPercent, float fNoonPercent)
{
	// Terrain. Heightmap pixel values carry absolute meters directly, so the shader applies no
	// height/depth multiplier.
	rGlobalLayout.fIslandAmbientOcclusion = fNoonPercent * gIslandAmbientOcclusion.mfCurrent;

	// PopulateSunMoonDirection folds fTerrainSnowBlend into f4TerrainSnowSunNormal.
	rGlobalLayout.fTerrainSnowAmbientOcclusionExclusion = gTerrainSnowAmbientOcclusionExclusion.mfCurrent;
	float fTerrainDetailNormalsMultiplier = engine::LerpAtHeight(engine::gpCamera->mfCameraEyeHeight, gTerrainDetailNormalsMultiplier.startHeight.mfCurrent, gTerrainDetailNormalsMultiplier.endHeight.mfCurrent, gTerrainDetailNormalsMultiplier.low.mfCurrent, gTerrainDetailNormalsMultiplier.high.mfCurrent);

	rGlobalLayout.fTerrainRockSize = gTerrainRockSize.mfCurrent;
	rGlobalLayout.fTerrainRockBlend = gTerrainRockBlend.mfCurrent;
	rGlobalLayout.fTerrainRockNormalsSizeOne = gTerrainRockNormalsSizeOne.mfCurrent;
	rGlobalLayout.fTerrainRockNormalsSizeTwo = gTerrainRockNormalsSizeTwo.mfCurrent;
	rGlobalLayout.fTerrainRockNormalsSizeThree = gTerrainRockNormalsSizeThree.mfCurrent;
	rGlobalLayout.fTerrainRockNormalsBlend = fTerrainDetailNormalsMultiplier * gTerrainRockNormalsBlend.mfCurrent;

	rGlobalLayout.fTerrainBeachSandSize = gTerrainBeachSandSize.mfCurrent;
	rGlobalLayout.fTerrainBeachSandBlend = gTerrainBeachSandBlend.mfCurrent;
	rGlobalLayout.fTerrainBeachNormalsSizeOne = gTerrainBeachNormalsSizeOne.mfCurrent;
	rGlobalLayout.fTerrainBeachNormalsSizeTwo = gTerrainBeachNormalsSizeTwo.mfCurrent;
	rGlobalLayout.fTerrainBeachNormalsSizeThree = gTerrainBeachNormalsSizeThree.mfCurrent;
	rGlobalLayout.fTerrainBeachNormalsBlend = std::max(fDayPercent * fDayPercent, 0.25f) * gTerrainBeachNormalsBlend.mfCurrent * fTerrainDetailNormalsMultiplier;
}

void RenderFrameGlobal(int64_t iCommandBuffer, std::chrono::duration<float> currentTime)
{
	RenderLightingGlobal(iCommandBuffer);
	RenderSmokeGlobal(iCommandBuffer);
	RenderWindGlobal(iCommandBuffer);

	float fSunAngle = engine::gpCamera->SunAngle();

	shaders::GlobalLayout& rGlobalLayout = *reinterpret_cast<shaders::GlobalLayout*>(&gpBufferManager->mGlobalLayoutUniformBuffers.at(iCommandBuffer).mpMappedMemory[0]);

	rGlobalLayout.fElapsedTime = currentTime.count();
	// Smoke curl time offset (SmokeCurlOffset): the swirl noise's circular time offset (0.04 noise UV/s in SmokeSpread)
	// scaled by kfSmokeCurlTimeScale to 0.001 noise UV/s, about one glass feature per 10 s at Smoke Curl Speed 1.
	static constexpr float kfSmokeCurlTimeScale = 0.025f;
	float fSmokeCurlTime = 0.01f * currentTime.count();
	float fSmokeCurlTimeOffsetScale = gSmokeCurlSpeed.mfCurrent * kfSmokeCurlTimeScale * 2.0f;
	rGlobalLayout.f2SmokeCurlTimeOffset.x = fSmokeCurlTimeOffsetScale * (-1.0f + 2.0f * std::sin(fSmokeCurlTime));
	rGlobalLayout.f2SmokeCurlTimeOffset.y = fSmokeCurlTimeOffsetScale * (-1.0f + 2.0f * std::cos(fSmokeCurlTime));
	float fBaseHeight = gBaseHeight.mfCurrent;
	rGlobalLayout.fBaseHeight = fBaseHeight;
	rGlobalLayout.fBaseHeightInverse = 1.0f / std::max(fBaseHeight, 0.001f);
	rGlobalLayout.fAspectRatioInverse = 1.0f / gpSwapchainManager->mfAspectRatio;

	XMFLOAT4A f4CameraPositionGlobal {};
	XMStoreFloat4A(&f4CameraPositionGlobal, engine::gpCamera->mVecPosition);
	rGlobalLayout.f2CameraPosition.x = f4CameraPositionGlobal.x;
	rGlobalLayout.f2CameraPosition.y = f4CameraPositionGlobal.y;
	rGlobalLayout.f4VisibleArea = engine::gpCamera->mf4RenderVisibleArea;

	float fDayPercent = 0.0f;
	float fNoonPercent = 0.0f;
	XMVECTOR vecSunMoonNormal = XMVectorZero();
	XMVECTOR vecAmbientColor = XMVectorZero();
	PopulateSunAndLighting(rGlobalLayout, fSunAngle, fDayPercent, fNoonPercent, vecSunMoonNormal, vecAmbientColor);
	PopulateShadowParameters(rGlobalLayout, fSunAngle, fDayPercent, fNoonPercent, vecSunMoonNormal, vecAmbientColor);
	PopulateTerrainParameters(rGlobalLayout, fDayPercent, fNoonPercent);
	PopulateWaterParameters(rGlobalLayout, fSunAngle, fDayPercent, currentTime);

	rGlobalLayout.fDebugTextureIndex = gDebugTextureIndex.mfCurrent;
	rGlobalLayout.fDebugTextureFormat = static_cast<float>(gpTextureManager->mRenderTargetTextures.mpiDebugTextureFormats[static_cast<int64_t>(gDebugTextureIndex.mfCurrent)]);
	rGlobalLayout.fDebugTextureLinearRange = gMiscDebugTextureLinearRange.mfCurrent;

	float fSeaFloorElevation = gpIslandTerrain->mfSeaFloorElevation;
	rGlobalLayout.fSeaFloorElevation = fSeaFloorElevation;
	rGlobalLayout.fSeaFloorElevationInverse = 1.0f / fSeaFloorElevation;
	// Zero-out line Terrain.vert uses to sink each island's submerged verts to the sea floor. Single source
	// of truth = the same constant DataPacker bakes the valid-area hull / texture masking from.
	rGlobalLayout.fUnderwaterMaskThreshold = common::kfUnderwaterMaskThresholdMeters;
	float fHigh = 0.0f;
	for (const auto& [rCrc, rIsland] : gpIslandTerrain->mIslands)
	{
		if (rIsland.bGpuResident)
		{
			fHigh = std::max(fHigh, rIsland.fWorldElevationMeters);
		}
	}
	rGlobalLayout.fDebugTerrainElevationHigh = fHigh;
}

} // namespace engine

#endif // defined(BT_CLIENT)
