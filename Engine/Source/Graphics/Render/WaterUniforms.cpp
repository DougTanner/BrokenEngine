#if defined(BT_CLIENT)

#include "Graphics/EngineCamera.h"
#include "Ui/HeightLerpWrapperQuartet.h"
#include "Ui/WaterWrappersBase.h"
#include "Render.h"

namespace engine
{

static void PopulateWaterSunsetFade(shaders::GlobalLayout& rGlobalLayout, float fSunAngle)
{
	float fSunsetFade = 1.0f;
	if (fSunAngle >= XM_PIDIV16 && fSunAngle < XM_PIDIV2)
	{
		fSunsetFade = 1.0f - (fSunAngle - XM_PIDIV16) / (XM_PIDIV2 - XM_PIDIV16);
	}
	else if (fSunAngle >= XM_PIDIV2 && fSunAngle < XM_PI - XM_PIDIV16)
	{
		fSunsetFade = (fSunAngle - XM_PIDIV2) / (XM_PI - XM_PIDIV16 - XM_PIDIV2);
	}
	rGlobalLayout.fWaterDepthLookupTableSunsetFade = gWaterDepthLookupTableSunsetFadeIntensity.mfCurrent * std::pow(fSunsetFade, gWaterDepthLookupTableSunsetFadePower.mfCurrent);
}

static void PopulateWaterDirectional(shaders::GlobalLayout& rGlobalLayout, float fSunAngle)
{
	float fDirectional = 1.0f;
	if (fSunAngle >= 0.0f && fSunAngle < XM_PIDIV2)
	{
		fDirectional = 1.0f - (fSunAngle) / XM_PIDIV2;
	}
	else if (fSunAngle >= XM_PIDIV2 && fSunAngle < XM_PI)
	{
		fDirectional = (fSunAngle - XM_PIDIV2) / XM_PIDIV2;
	}
	rGlobalLayout.fWaterDirectional = std::pow(fDirectional, 2.0f);
}

// Reduced-time accumulators require exactly one call per frame.
static void PopulateWaterReducedUv(shaders::GlobalLayout& rGlobalLayout, std::chrono::duration<float> elapsedTime)
{
	// Reduce normal-map origins modulo 10.0: the shader's noninteger octave multipliers turn these wraps
	// into integer UV shifts absorbed by fract(); modulo 1.0 produces visible UV jumps.
	XMFLOAT4A f4CameraPosition {};
	XMStoreFloat4A(&f4CameraPosition, engine::gpCamera->mVecPosition);
	rGlobalLayout.fWaterOriginX = f4CameraPosition.x;
	rGlobalLayout.fWaterOriginY = f4CameraPosition.y;
	gPresentationContinuity.cameraBasisCoordinate = engine::gpCamera->mBasisCoordinate;
	gPresentationContinuity.f2WaterOrigin = {f4CameraPosition.x, f4CameraPosition.y};

	double fSizeBaseOne = static_cast<double>(gLightingSampledNormalsOneSize.mfCurrent);
	double fSizeBaseTwo = static_cast<double>(gLightingSampledNormalsTwoSize.mfCurrent);
	double fSizeBaseThree = static_cast<double>(gLightingSampledNormalsThreeSize.mfCurrent);
	// Camera-height-driven speed lerp — single-sourced fade endpoint shared with LightingUniforms.cpp.
	float fCameraHeightZoomFactor = engine::LerpAtHeight(engine::gpCamera->mfCameraEyeHeight, engine::Camera::kfCameraEyeHeightDefault, engine::Camera::kfWaveFadeEndHeight, 0.0f, 1.0f);
	double fSpeedOne = static_cast<double>(std::lerp(gLightingSampledNormalsSpeedOneMinimum.mfCurrent, gLightingSampledNormalsSpeedOneMaximum.mfCurrent, fCameraHeightZoomFactor));
	double fSpeedTwo = static_cast<double>(std::lerp(gLightingSampledNormalsSpeedTwoMinimum.mfCurrent, gLightingSampledNormalsSpeedTwoMaximum.mfCurrent, fCameraHeightZoomFactor));
	double fSpeedThree = static_cast<double>(std::lerp(gLightingSampledNormalsSpeedThreeMinimum.mfCurrent, gLightingSampledNormalsSpeedThreeMaximum.mfCurrent, fCameraHeightZoomFactor));
	float fRotationOne = gWaterNormalRotationOne.mfCurrent;
	float fRotationTwo = gWaterNormalRotationTwo.mfCurrent;
	float fRotationThree = gWaterNormalRotationThree.mfCurrent;
	// A zero scroll-direction angle moves along world (1,1), independently of pattern rotation.
	float fSpeedDirectionOne = gWaterNormalSpeedDirectionOne.mfCurrent;
	float fSpeedDirectionTwo = gWaterNormalSpeedDirectionTwo.mfCurrent;
	float fSpeedDirectionThree = gWaterNormalSpeedDirectionThree.mfCurrent;
	// Integrate size * speed * dt in double precision to keep phase continuous as size or speed changes.
	// CPU R(phi-theta) and shader R(theta) give world scroll R(phi)(1,1), with magnitude sqrt(2).
	// Wrap each time component modulo 10.0 so the shader's speed multipliers give integral shifts absorbed by fract().
	static double sfReducedTimeOneX = 0.0;
	static double sfReducedTimeOneY = 0.0;
	static double sfReducedTimeTwoX = 0.0;
	static double sfReducedTimeTwoY = 0.0;
	static double sfReducedTimeThreeX = 0.0;
	static double sfReducedTimeThreeY = 0.0;
	static std::chrono::duration<float> sPreviousElapsedTime(0.0f);
	std::chrono::duration<float> deltaTime = std::max(std::chrono::duration<float>(0.0f), elapsedTime - sPreviousElapsedTime);
	sPreviousElapsedTime = elapsedTime;
	double fPreciseDeltaTime = static_cast<double>(deltaTime.count());
	double fDeltaOne = fSizeBaseOne * fSpeedOne * fPreciseDeltaTime;
	double fCosineOne = static_cast<double>(std::cos(fRotationOne));
	double fSineOne = static_cast<double>(std::sin(fRotationOne));
	double fScrollGammaOne = static_cast<double>(fSpeedDirectionOne) - static_cast<double>(fRotationOne);
	double fScrollCosineOne = std::cos(fScrollGammaOne);
	double fScrollSineOne = std::sin(fScrollGammaOne);
	sfReducedTimeOneX = std::fmod(sfReducedTimeOneX + fDeltaOne * (fScrollCosineOne - fScrollSineOne), 10.0);
	sfReducedTimeOneY = std::fmod(sfReducedTimeOneY + fDeltaOne * (fScrollCosineOne + fScrollSineOne), 10.0);
	double fDeltaTwo = fSizeBaseTwo * fSpeedTwo * fPreciseDeltaTime;
	double fCosineTwo = static_cast<double>(std::cos(fRotationTwo));
	double fSineTwo = static_cast<double>(std::sin(fRotationTwo));
	double fScrollGammaTwo = static_cast<double>(fSpeedDirectionTwo) - static_cast<double>(fRotationTwo);
	double fScrollCosineTwo = std::cos(fScrollGammaTwo);
	double fScrollSineTwo = std::sin(fScrollGammaTwo);
	sfReducedTimeTwoX = std::fmod(sfReducedTimeTwoX + fDeltaTwo * (fScrollCosineTwo - fScrollSineTwo), 10.0);
	sfReducedTimeTwoY = std::fmod(sfReducedTimeTwoY + fDeltaTwo * (fScrollCosineTwo + fScrollSineTwo), 10.0);
	double fDeltaThree = fSizeBaseThree * fSpeedThree * fPreciseDeltaTime;
	double fCosineThree = static_cast<double>(std::cos(fRotationThree));
	double fSineThree = static_cast<double>(std::sin(fRotationThree));
	rGlobalLayout.f4WaterNormalRotationOne = {static_cast<float>(fCosineOne), -static_cast<float>(fSineOne), static_cast<float>(fSineOne), static_cast<float>(fCosineOne)};
	rGlobalLayout.f4WaterNormalRotationTwo = {static_cast<float>(fCosineTwo), -static_cast<float>(fSineTwo), static_cast<float>(fSineTwo), static_cast<float>(fCosineTwo)};
	rGlobalLayout.f4WaterNormalRotationThree = {static_cast<float>(fCosineThree), -static_cast<float>(fSineThree), static_cast<float>(fSineThree), static_cast<float>(fCosineThree)};
	double fScrollGammaThree = static_cast<double>(fSpeedDirectionThree) - static_cast<double>(fRotationThree);
	double fScrollCosineThree = std::cos(fScrollGammaThree);
	double fScrollSineThree = std::sin(fScrollGammaThree);
	sfReducedTimeThreeX = std::fmod(sfReducedTimeThreeX + fDeltaThree * (fScrollCosineThree - fScrollSineThree), 10.0);
	sfReducedTimeThreeY = std::fmod(sfReducedTimeThreeY + fDeltaThree * (fScrollCosineThree + fScrollSineThree), 10.0);
	// Phase must follow the camera across a cell change, so the reductions below run on the absolute camera position,
	// reconstructed as a double from the camera cell and the camera's local position. This is client-only CPU work:
	// fWaterOriginX/Y above stays in the rebased frame the shaders add it back to.
	double fCameraX = static_cast<double>(engine::gpCamera->mBasisCoordinate.iX) * static_cast<double>(kfCellWidth) + static_cast<double>(f4CameraPosition.x);
	double fCameraY = static_cast<double>(engine::gpCamera->mBasisCoordinate.iY) * static_cast<double>(kfCellHeight) + static_cast<double>(f4CameraPosition.y);

	// Rotate cameraXY by R(-theta) before fmod on the CPU: rotation is already encoded when shader fract() absorbs the integral sizeMult*10
	// wrap. Rotating an already-reduced origin gives nonintegral wrap shifts except at multiples of pi/2, causing normal-pattern jumps at each
	// wrap.
	auto RotatedCamera = [&](double fCosine, double fSine, double& rfOutputX, double& rfOutputY)
	{
		rfOutputX = fCosine * fCameraX + fSine * fCameraY;
		rfOutputY = -fSine * fCameraX + fCosine * fCameraY;
	};
	double fRotatedCameraXOne = 0.0;
	double fRotatedCameraYOne = 0.0;
	double fRotatedCameraXTwo = 0.0;
	double fRotatedCameraYTwo = 0.0;
	double fRotatedCameraXThree = 0.0;
	double fRotatedCameraYThree = 0.0;
	RotatedCamera(fCosineOne, fSineOne, fRotatedCameraXOne, fRotatedCameraYOne);
	RotatedCamera(fCosineTwo, fSineTwo, fRotatedCameraXTwo, fRotatedCameraYTwo);
	RotatedCamera(fCosineThree, fSineThree, fRotatedCameraXThree, fRotatedCameraYThree);

	rGlobalLayout.fWaterReducedNormalOriginX = static_cast<float>(std::fmod(fSizeBaseOne * fRotatedCameraXOne, 10.0));
	rGlobalLayout.fWaterReducedNormalOriginY = static_cast<float>(std::fmod(fSizeBaseOne * fRotatedCameraYOne, 10.0));
	rGlobalLayout.fWaterReducedNormalTimeX = static_cast<float>(sfReducedTimeOneX);
	rGlobalLayout.fWaterReducedNormalTimeY = static_cast<float>(sfReducedTimeOneY);
	rGlobalLayout.fWaterReducedNormalOriginTwoX = static_cast<float>(std::fmod(fSizeBaseTwo * fRotatedCameraXTwo, 10.0));
	rGlobalLayout.fWaterReducedNormalOriginTwoY = static_cast<float>(std::fmod(fSizeBaseTwo * fRotatedCameraYTwo, 10.0));
	rGlobalLayout.fWaterReducedNormalTimeTwoX = static_cast<float>(sfReducedTimeTwoX);
	rGlobalLayout.fWaterReducedNormalTimeTwoY = static_cast<float>(sfReducedTimeTwoY);
	rGlobalLayout.fWaterReducedNormalOriginThreeX = static_cast<float>(std::fmod(fSizeBaseThree * fRotatedCameraXThree, 10.0));
	rGlobalLayout.fWaterReducedNormalOriginThreeY = static_cast<float>(std::fmod(fSizeBaseThree * fRotatedCameraYThree, 10.0));
	rGlobalLayout.fWaterReducedNormalTimeThreeX = static_cast<float>(sfReducedTimeThreeX);
	rGlobalLayout.fWaterReducedNormalTimeThreeY = static_cast<float>(sfReducedTimeThreeY);

	// Modulo 10.0 requires noise and normal-map octave multipliers in tenths for seamless fract() wraps.
	double fNoiseFrequency = static_cast<double>(gWaterColorNoiseFrequency.mfCurrent);
	XMFLOAT2 f2ReducedNoiseOrigin {static_cast<float>(std::fmod(fNoiseFrequency * fCameraX, 10.0)), static_cast<float>(std::fmod(fNoiseFrequency * fCameraY, 10.0))};
	rGlobalLayout.fWaterReducedNoiseOriginX = f2ReducedNoiseOrigin.x;
	rGlobalLayout.fWaterReducedNoiseOriginY = f2ReducedNoiseOrigin.y;
	gPresentationContinuity.f2ReducedNoiseOrigin = f2ReducedNoiseOrigin;
	gPresentationContinuity.fNoiseFrequency = static_cast<float>(fNoiseFrequency);
	++gPresentationContinuity.iPublishedFrames;
}

void PopulateWaterParameters(shaders::GlobalLayout& rGlobalLayout, float fSunAngle, float fDayPercent, std::chrono::duration<float> elapsedTime)
{
	rGlobalLayout.fWaterEarlyOut = gWaterEarlyOut.mfCurrent;
	rGlobalLayout.fWaterHeight = gWaterHeight.mfCurrent;
	rGlobalLayout.fWaterTerrainHeight = gWaterTerrainHeight.mfCurrent;
	rGlobalLayout.fWaterTerrainFadeInverse = 1.0f / gWaterTerrainFade.mfCurrent;
	rGlobalLayout.fWaterTerrainFadeClamp = gWaterTerrainFadeClamp.mfCurrent;

	rGlobalLayout.fWaterDepthLookupTableFeather = gWaterDepthLookupTableFeather.mfCurrent;
	rGlobalLayout.fWaterDepthColorFeather = gWaterDepthColorFeather.mfCurrent;
	rGlobalLayout.fWaterDepthColorFloor = gWaterDepthColorFloor.mfCurrent;
	rGlobalLayout.fWaterUnderseaCompressionInverse = 1.0f / gWaterUnderseaCompression.mfCurrent; // TerrainElevation.frag undersea depth-curve exponent
	rGlobalLayout.fWaterDepthReflectionFeatherInverse = 1.0f / (fDayPercent * gWaterDepthReflectionFeather.mfCurrent); // unguarded: night dayPercent=0 -> +inf absorbed by the shader clamp
	rGlobalLayout.fWaterColorNoiseFrequency = gWaterColorNoiseFrequency.mfCurrent;

	PopulateWaterSunsetFade(rGlobalLayout, fSunAngle);

	rGlobalLayout.fWaterFresnel = std::pow(fDayPercent, 0.5f) * gWaterFresnel.mfCurrent;
	rGlobalLayout.fWaterColorBottom = gWaterColorBottom.mfCurrent;
	rGlobalLayout.fWaterColorHeightInv = 1.0f / gWaterColorHeight.mfCurrent;
	rGlobalLayout.fWaterColorNoiseAmount = gWaterColorNoiseAmount.mfCurrent;
	rGlobalLayout.fWaterColorNoiseWeightOne = gWaterColorNoiseWeightOne.mfCurrent;
	rGlobalLayout.fWaterColorNoiseWeightTwo = gWaterColorNoiseWeightTwo.mfCurrent;
	rGlobalLayout.fWaterColorNoiseMultiplierOne = gWaterColorNoiseMultiplierOne.mfCurrent;
	rGlobalLayout.fWaterColorNoiseMultiplierTwo = gWaterColorNoiseMultiplierTwo.mfCurrent;

	PopulateWaterDirectional(rGlobalLayout, fSunAngle);

	float fBreakStart = gWaterBreakStartDepth.mfCurrent;
	float fBreakEnd = gWaterBreakEndDepth.mfCurrent;
	float fEffectiveBreakStart = std::min(fBreakStart, fBreakEnd);
	float fBreakRange = fBreakEnd - fEffectiveBreakStart;
	float fMediumShoreWidth = gWaterMediumShoreSoftness.mfCurrent;
	rGlobalLayout.fWaterBreakEndDepthInverse = 1.0f / fBreakEnd;
	rGlobalLayout.fWaterBreakBlendStart = fEffectiveBreakStart;
	rGlobalLayout.fWaterBreakBlendInverseRange = fBreakRange > 0.0f ? 1.0f / fBreakRange : 0.0f;
	rGlobalLayout.fWaterBreakBlendCurve = gWaterBreakBlendCurve.mfCurrent;
	rGlobalLayout.fWaterMediumShoreFadeInverseWidth = fMediumShoreWidth > 0.0f ? 1.0f / fMediumShoreWidth : 0.0f;
	rGlobalLayout.fWaterLowSteepness = gWaterLowSteepness.mfCurrent;

	rGlobalLayout.fWaterMediumSteepness = gWaterMediumSteepness.mfCurrent;
	rGlobalLayout.fWaterWaveNormalBlend = gWaterWaveNormalBlend.mfCurrent;
	rGlobalLayout.fWaterLowAmplitude = gWaterLowAmplitude.mfCurrent;
	rGlobalLayout.fWaterMediumAmplitude = gWaterMediumAmplitude.mfCurrent;

	PopulateWaterReducedUv(rGlobalLayout, elapsedTime);
}

} // namespace engine

#endif // defined(BT_CLIENT)
