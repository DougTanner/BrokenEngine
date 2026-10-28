#if defined(BT_CLIENT)

#include "Graphics/CameraBase.h"
#include "Ui/GraphicsQualityWrappersBase.h"
#include "Ui/GraphicsSettingsWrappersBase.h"
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
	float fCameraHeightZoomFactor = engine::LerpAtHeight(engine::gpCamera->GetRenderEyeHeight(), engine::CameraBase::kfCameraEyeHeightDefault, engine::CameraBase::kfWaveFadeEndHeight, 0.0f, 1.0f);
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
	auto [fCellX, fCellY] = CellCenterOffsetDouble(kOriginCoordinate, engine::gpCamera->mBasisCoordinate);
	double fCameraX = fCellX + static_cast<double>(f4CameraPosition.x);
	double fCameraY = fCellY + static_cast<double>(f4CameraPosition.y);

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
	if (gWaterFoamSystemEnabled.Get<bool>() && gWaterLevel.Get<int64_t>() > static_cast<int64_t>(GraphicsQualityLevel::kLow))
	{
		double fFoamTextureInverseTileSize = 1.0 / static_cast<double>(gWaterFoamTextureTileSize.mfCurrent);
		XMFLOAT2 f2FoamReducedTextureOrigin {static_cast<float>(std::fmod(fFoamTextureInverseTileSize * fCameraX, 2.0)), static_cast<float>(std::fmod(fFoamTextureInverseTileSize * fCameraY, 2.0))};
		rGlobalLayout.f2WaterFoamReducedTextureOrigin.x = f2FoamReducedTextureOrigin.x;
		rGlobalLayout.f2WaterFoamReducedTextureOrigin.y = f2FoamReducedTextureOrigin.y;
		rGlobalLayout.fWaterFoamTextureInverseTileSize = static_cast<float>(fFoamTextureInverseTileSize);
		rGlobalLayout.fWaterFoamTextureStrength = gWaterFoamEnabled.Get<bool>() ? gWaterFoamTextureStrength.mfCurrent : 0.0f;
		gPresentationContinuity.f2FoamReducedTextureOrigin = f2FoamReducedTextureOrigin;
		gPresentationContinuity.fFoamTextureInverseTileSize = static_cast<float>(fFoamTextureInverseTileSize);
		double fFoamBreakupInverseTileSize = 1.0 / static_cast<double>(gWaterFoamBreakupTileSize.mfCurrent);
		XMFLOAT2 f2FoamBreakupReducedTextureOrigin {static_cast<float>(std::fmod(fFoamBreakupInverseTileSize * fCameraX, 2.0)), static_cast<float>(std::fmod(fFoamBreakupInverseTileSize * fCameraY, 2.0))};
		rGlobalLayout.f2WaterFoamBreakupReducedTextureOrigin.x = f2FoamBreakupReducedTextureOrigin.x;
		rGlobalLayout.f2WaterFoamBreakupReducedTextureOrigin.y = f2FoamBreakupReducedTextureOrigin.y;
		rGlobalLayout.fWaterFoamBreakupInverseTileSize = static_cast<float>(fFoamBreakupInverseTileSize);
		rGlobalLayout.fWaterFoamBreakupAmount = gWaterFoamBreakupAmount.mfCurrent;
		rGlobalLayout.uiWaterFoamTextureIndex = static_cast<uint32_t>(std::clamp(gWaterFoamTextureIndex.GetIndex(), 0i64, static_cast<int64_t>(shaders::kiWaterFoamTextureCount - 1)));
		rGlobalLayout.uiWaterFoamBreakupTextureIndex = static_cast<uint32_t>(std::clamp(gWaterFoamBreakupTextureIndex.GetIndex(), 0i64, static_cast<int64_t>(shaders::kiWaterFoamBreakupTextureCount - 1)));
		gPresentationContinuity.f2FoamBreakupReducedTextureOrigin = f2FoamBreakupReducedTextureOrigin;
		gPresentationContinuity.fFoamBreakupInverseTileSize = static_cast<float>(fFoamBreakupInverseTileSize);
		double fFoamSpawnNoiseInverseTileSize = 1.0 / static_cast<double>(gWaterFoamSpawnNoiseTileSize.mfCurrent);
		XMFLOAT2 f2FoamSpawnNoiseReducedOrigin {static_cast<float>(std::fmod(fFoamSpawnNoiseInverseTileSize * fCameraX, 2.0)), static_cast<float>(std::fmod(fFoamSpawnNoiseInverseTileSize * fCameraY, 2.0))};
		rGlobalLayout.f2WaterFoamSpawnNoiseReducedOrigin.x = f2FoamSpawnNoiseReducedOrigin.x;
		rGlobalLayout.f2WaterFoamSpawnNoiseReducedOrigin.y = f2FoamSpawnNoiseReducedOrigin.y;
		rGlobalLayout.fWaterFoamSpawnNoiseInverseTileSize = static_cast<float>(fFoamSpawnNoiseInverseTileSize);
		gPresentationContinuity.f2FoamSpawnNoiseReducedOrigin = f2FoamSpawnNoiseReducedOrigin;
		gPresentationContinuity.fFoamSpawnNoiseInverseTileSize = static_cast<float>(fFoamSpawnNoiseInverseTileSize);
	}
	else
	{
		rGlobalLayout.fWaterFoamTextureStrength = 0.0f;
	}
	++gPresentationContinuity.iPublishedFrames;
}

static void PopulateWaterFoam(shaders::GlobalLayout& rGlobalLayout, std::chrono::duration<float> elapsedTime)
{
	static bool sbInitialized = false;
	static bool sbWasActive = false;
	static RetainedAreaBasis sRetainedBasis {};
	static XMFLOAT4 sf4PreviousArea {};
	static std::chrono::duration<float> sPreviousElapsedTime(0.0f);
	static std::chrono::steady_clock::time_point sPreviousUpdate {};

	bool bActive = gWaterFoamSystemEnabled.Get<bool>() && gWaterLevel.Get<int64_t>() > static_cast<int64_t>(GraphicsQualityLevel::kLow);
	if (!bActive)
	{
		rGlobalLayout.iWaterFoamEnabled = 0;
		rGlobalLayout.iWaterFoamReset = 1;
		sbInitialized = false;
		sbWasActive = false;
		return;
	}

	std::chrono::steady_clock::time_point updateTime = std::chrono::steady_clock::now();
	std::optional<XMFLOAT2> optionalOffset = sRetainedBasis.Advance(engine::gpCamera->mBasisCoordinate);
	bool bBasisReset = sbInitialized && !optionalOffset.has_value();
	bool bClockReset = elapsedTime < sPreviousElapsedTime;
	bool bReset = !sbInitialized || gbWaterFoamReset || bBasisReset || bClockReset || !sbWasActive;
	XMFLOAT4 f4Area = engine::gpCamera->mf4RenderVisibleArea;
	XMFLOAT4 f4PreviousArea = sf4PreviousArea;
	if (bReset)
	{
		f4PreviousArea = f4Area;
	}
	else
	{
		ShiftArea(f4PreviousArea, *optionalOffset);
	}
	// Wave elapsedTime follows simulation time; foam lifetime uses wall time across render gaps.
	float fDeltaTime = sbInitialized && !bClockReset ? std::chrono::duration<float>(updateTime - sPreviousUpdate).count() : 0.0f;
	float fJStart = gWaterFoamJStart.mfCurrent;
	float fJFull = std::min(gWaterFoamJFull.mfCurrent, fJStart - 0.001f);
	rGlobalLayout.f4WaterFoamArea = f4Area;
	rGlobalLayout.f4WaterFoamPreviousArea = f4PreviousArea;
	rGlobalLayout.f2WaterFoamAreaSize.x = f4Area.z - f4Area.x;
	rGlobalLayout.f2WaterFoamAreaSize.y = f4Area.y - f4Area.w;
	rGlobalLayout.f2WaterFoamAreaInverseSize.x = 1.0f / (f4Area.z - f4Area.x);
	rGlobalLayout.f2WaterFoamAreaInverseSize.y = 1.0f / (f4Area.y - f4Area.w);
	rGlobalLayout.f2WaterFoamPreviousAreaInverseSize.x = 1.0f / (f4PreviousArea.z - f4PreviousArea.x);
	rGlobalLayout.f2WaterFoamPreviousAreaInverseSize.y = 1.0f / (f4PreviousArea.y - f4PreviousArea.w);
	rGlobalLayout.fWaterFoamEmissionRate = gWaterFoamEmissionRate.mfCurrent;
	rGlobalLayout.fWaterFoamJStart = fJStart;
	rGlobalLayout.fWaterFoamJInverseRange = 1.0f / (fJStart - fJFull);
	rGlobalLayout.fWaterFoamSpawnNoiseAmount = gWaterFoamSpawnNoiseAmount.mfCurrent;
	rGlobalLayout.fWaterFoamDecayRate = std::log(2.0f) / gWaterFoamHalfLife.mfCurrent;
	rGlobalLayout.fWaterFoamDeltaTime = fDeltaTime;
	rGlobalLayout.iWaterFoamReset = bReset ? 1 : 0;
	rGlobalLayout.iWaterFoamEnabled = bActive ? 1 : 0;
	gPresentationContinuity.waterFoam.f4CurrentArea = f4Area;
	gPresentationContinuity.waterFoam.f4PreviousArea = f4PreviousArea;
	if (bBasisReset)
	{
		++gPresentationContinuity.waterFoam.iHistoryResets;
	}
	sf4PreviousArea = f4Area;
	sPreviousElapsedTime = elapsedTime;
	sPreviousUpdate = updateTime;
	sbInitialized = true;
	sbWasActive = bActive;
	gbWaterFoamReset = false;
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
	rGlobalLayout.fWaterColorHeightInverse = 1.0f / gWaterColorHeight.mfCurrent;
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

	PopulateWaterFoam(rGlobalLayout, elapsedTime);
	PopulateWaterReducedUv(rGlobalLayout, elapsedTime);
}

// Gerstner wave phase-reduction modulus (shared by both bands).
constexpr double kfWaveTwoPi = 2.0 * 3.14159265358979323846;

// CPU-side staging + frame-invariant cache for one Gerstner wave band (`vec4` == XMFLOAT4, 16-byte
// stride — matches the mapped layout arrays exactly for a straight memcpy). All wave math reads and
// writes this cached (normal, cacheable) copy instead of the write-combined mapped uniform buffer,
// whose readbacks each stall on memory latency; the populate finishes with one memcpy per array
// region into rMainLayout. Directions (pf4WavesOne), omega (pf4WavesTwo.x), phi (pf4WavesTwo.z), and
// the clamped-but-unscaled base amplitude (pfBaseAmplitude) are frame-invariant — rebuilt only when
// the consumed tunables change. The per-frame pass rewrites only pf4WavesTwo.y (base amplitude x the
// eye-height fade scale) and pf4WavesTwo.w (the fmod phase term). Sized to the 256-entry shader maxima.
struct GerstnerWaveBandStaging
{
	XMFLOAT4 pf4WavesOne[256];
	XMFLOAT4 pf4WavesTwo[256];
	float pfBaseAmplitude[256] {};
};

// Consumed low/medium tunables snapshot; inequality vs last frame triggers an invariant rebuild.
// iCount defaults to -1 (never a resolved count) so the first real frame always rebuilds. The
// eye-height amplitude-fade scale is deliberately absent — it folds into the per-frame amplitude
// multiply, not a rebuild.
struct LowWaveTunables
{
	int64_t iCount = -1;
	float fAngle = 0.0f;
	float fWavelength = 0.0f;
	float fAmplitude = 0.0f;
	float fSpeed = 0.0f;
	float fAngleAdjust = 0.0f;
	float fWavelengthAdjust = 0.0f;
	float fAmplitudeAdjust = 0.0f;
	float fSpeedAdjust = 0.0f;

	bool operator==(const LowWaveTunables&) const = default;
};

struct MediumWaveTunables
{
	int64_t iCount = -1;
	float fWavelength = 0.0f;
	float fAmplitude = 0.0f;
	float fSpeed = 0.0f;
	float fAngleAdjust = 0.0f;
	float fWavelengthAdjust = 0.0f;
	float fAmplitudeAdjust = 0.0f;
	float fSpeedAdjust = 0.0f;

	bool operator==(const MediumWaveTunables&) const = default;
};

static GerstnerWaveBandStaging sLowWaveStaging {};
static GerstnerWaveBandStaging sMediumWaveStaging {};
static LowWaveTunables sLowWaveTunables {};
static MediumWaveTunables sMediumWaveTunables {};

// Cached wave terms require fixed per-wave float expression order and RandomEngine consumption.
static void RebuildLowWaveInvariants(int64_t iCount)
{
	// Wave 0: fixed primary direction, no RNG draw, no amplitude clamp.
	auto vecDirection = XMVector3TransformNormal(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), XMMatrixRotationZ(gWaterLowAngle.mfCurrent));
	sLowWaveStaging.pf4WavesOne[0].x = XMVectorGetX(vecDirection);
	sLowWaveStaging.pf4WavesOne[0].y = XMVectorGetY(vecDirection);

	vecDirection = XMVector3TransformNormal(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), XMMatrixRotationZ(0.0f));
	sLowWaveStaging.pf4WavesOne[0].z = XMVectorGetX(vecDirection);
	sLowWaveStaging.pf4WavesOne[0].w = XMVectorGetY(vecDirection);

	sLowWaveStaging.pf4WavesTwo[0].x = (2.0f * XM_PI) / (gWaterLowWavelength.mfCurrent); // Omega
	sLowWaveStaging.pfBaseAmplitude[0] = gWaterLowAmplitude.mfCurrent;
	sLowWaveStaging.pf4WavesTwo[0].z = gWaterLowSpeed.mfCurrent * sLowWaveStaging.pf4WavesTwo[0].x; // Phi

	common::RandomEngine randomEngine {};
	for (int64_t i = 1; i < iCount; ++i)
	{
		float fAngleAdjust = ((i % 2) == 0 ? 1.0f : -1.0f) * gWaterLowAngleAdjust.mfCurrent * common::Random(randomEngine);
		vecDirection = XMVector3TransformNormal(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), XMMatrixRotationZ(gWaterLowAngle.mfCurrent + fAngleAdjust));
		sLowWaveStaging.pf4WavesOne[i].x = XMVectorGetX(vecDirection);
		sLowWaveStaging.pf4WavesOne[i].y = XMVectorGetY(vecDirection);

		vecDirection = XMVector3TransformNormal(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), XMMatrixRotationZ(XM_2PI * static_cast<float>(i) / static_cast<float>(iCount)));
		sLowWaveStaging.pf4WavesOne[i].z = XMVectorGetX(vecDirection);
		sLowWaveStaging.pf4WavesOne[i].w = XMVectorGetY(vecDirection);

		float fAdjust = common::Random(randomEngine);
		float fWavelengthAdjust = fAdjust * gWaterLowWavelengthAdjust.mfCurrent;
		float fAmplitudeAdjust = (1.0f - fAdjust) * std::abs(gWaterLowAmplitudeAdjust.mfCurrent) * common::Random(randomEngine);
		float fSpeedAdjust = fAdjust * gWaterLowSpeedAdjust.mfCurrent;
		sLowWaveStaging.pf4WavesTwo[i].x = std::abs((2.0f * XM_PI) / (gWaterLowWavelength.mfCurrent + fWavelengthAdjust * gWaterLowWavelength.mfCurrent)); // Omega
		float fBaseAmplitude = std::abs(gWaterLowAmplitude.mfCurrent - fAmplitudeAdjust * gWaterLowAmplitude.mfCurrent);
		fBaseAmplitude = std::min(fBaseAmplitude, 0.1f * (1.0f / sLowWaveStaging.pf4WavesTwo[i].x));
		sLowWaveStaging.pf4WavesTwo[i].z = (gWaterLowSpeed.mfCurrent + gWaterLowSpeed.mfCurrent * fSpeedAdjust * common::Random(randomEngine)) * sLowWaveStaging.pf4WavesTwo[i].x; // Phi

		// Thin the low-frequency band: zero every kiWaveCullModulo-th wave's amplitude below kiWaveCullLimit (tuning to reduce low-wave repetition).
		static constexpr int64_t kiWaveCullLimit = 64;
		static constexpr int64_t kiWaveCullModulo = 3;
		if (i < kiWaveCullLimit && (i % kiWaveCullModulo) == 0)
		{
			fBaseAmplitude = 0.0f;
		}
		sLowWaveStaging.pfBaseAmplitude[i] = fBaseAmplitude;
	}
}

// Rebuild the frame-invariant medium-band terms into the staging cache. Only pf4WavesOne.xy is
// written (the displacement shader reads only the .xy direction); .zw stay zero-initialised.
static void RebuildMediumWaveInvariants(int64_t iCount)
{
	common::RandomEngine randomEngine {};
	for (int64_t i = 0; i < iCount; ++i)
	{
		float fAngleAdjust = gWaterMediumAngleAdjust.mfCurrent * common::Random(randomEngine);
		auto vecDirection = XMVector3TransformNormal(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), XMMatrixRotationZ(fAngleAdjust));
		sMediumWaveStaging.pf4WavesOne[i].x = XMVectorGetX(vecDirection);
		sMediumWaveStaging.pf4WavesOne[i].y = XMVectorGetY(vecDirection);

		float fWavelengthAdjust = -gWaterMediumWavelengthAdjust.mfCurrent + 2.0f * gWaterMediumWavelengthAdjust.mfCurrent * common::Random(randomEngine);
		float fAmplitudeAdjust = -gWaterMediumAmplitudeAdjust.mfCurrent + 2.0f * gWaterMediumAmplitudeAdjust.mfCurrent * common::Random(randomEngine);
		float fSpeedAdjust = -gWaterMediumSpeedAdjust.mfCurrent + 2.0f * gWaterMediumSpeedAdjust.mfCurrent * common::Random(randomEngine);
		sMediumWaveStaging.pf4WavesTwo[i].x = std::abs((2.0f * XM_PI) / (gWaterMediumWavelength.mfCurrent + fWavelengthAdjust * gWaterMediumWavelength.mfCurrent)); // Omega
		float fBaseAmplitude = std::abs(gWaterMediumAmplitude.mfCurrent + fAmplitudeAdjust * gWaterMediumAmplitude.mfCurrent);
		fBaseAmplitude = std::min(fBaseAmplitude, 0.1f * (1.0f / sMediumWaveStaging.pf4WavesTwo[i].x));
		sMediumWaveStaging.pfBaseAmplitude[i] = fBaseAmplitude;
		sMediumWaveStaging.pf4WavesTwo[i].z = (gWaterMediumSpeed.mfCurrent + fSpeedAdjust * gWaterMediumSpeed.mfCurrent) * sMediumWaveStaging.pf4WavesTwo[i].x; // Phi
	}
}

static void PopulateGerstnerLowWaves(shaders::MainLayout& rMainLayout, double fWaveTime, double fWaveCameraX, double fWaveCameraY, float fLowAmplitudeScale)
{
	if (fLowAmplitudeScale <= 0.0f)
	{
		rMainLayout.iWaterLowCount = 0;
		return;
	}

	int64_t iCount = std::min(gWaterLowCount.Get<int64_t>(), static_cast<int64_t>(gWaterLowMaximum.mfCurrent));

	LowWaveTunables tunables {};
	tunables.iCount = iCount;
	tunables.fAngle = gWaterLowAngle.mfCurrent;
	tunables.fWavelength = gWaterLowWavelength.mfCurrent;
	tunables.fAmplitude = gWaterLowAmplitude.mfCurrent;
	tunables.fSpeed = gWaterLowSpeed.mfCurrent;
	tunables.fAngleAdjust = gWaterLowAngleAdjust.mfCurrent;
	tunables.fWavelengthAdjust = gWaterLowWavelengthAdjust.mfCurrent;
	tunables.fAmplitudeAdjust = gWaterLowAmplitudeAdjust.mfCurrent;
	tunables.fSpeedAdjust = gWaterLowSpeedAdjust.mfCurrent;
	if (tunables != sLowWaveTunables)
	{
		sLowWaveTunables = tunables;
		RebuildLowWaveInvariants(iCount);
	}

	for (int64_t i = 0; i < iCount; ++i)
	{
		sLowWaveStaging.pf4WavesTwo[i].y = sLowWaveStaging.pfBaseAmplitude[i] * fLowAmplitudeScale;
		double fDirectionX = static_cast<double>(sLowWaveStaging.pf4WavesOne[i].x);
		double fDirectionY = static_cast<double>(sLowWaveStaging.pf4WavesOne[i].y);
		double fOmega = static_cast<double>(sLowWaveStaging.pf4WavesTwo[i].x);
		double fPhi = static_cast<double>(sLowWaveStaging.pf4WavesTwo[i].z);
		sLowWaveStaging.pf4WavesTwo[i].w = static_cast<float>(std::fmod((fDirectionX * fWaveCameraX + fDirectionY * fWaveCameraY) * fOmega + fPhi * (fWaveTime + static_cast<double>(i)), kfWaveTwoPi));
	}

	std::memcpy(rMainLayout.pf4LowWavesOne, sLowWaveStaging.pf4WavesOne, static_cast<size_t>(iCount) * sizeof(rMainLayout.pf4LowWavesOne[0]));
	std::memcpy(rMainLayout.pf4LowWavesTwo, sLowWaveStaging.pf4WavesTwo, static_cast<size_t>(iCount) * sizeof(rMainLayout.pf4LowWavesTwo[0]));
	rMainLayout.iWaterLowCount = static_cast<int32_t>(iCount);
}

static void PopulateGerstnerMediumWaves(shaders::MainLayout& rMainLayout, double fWaveTime, double fWaveCameraX, double fWaveCameraY, float fMediumAmplitudeScale)
{
	if (fMediumAmplitudeScale <= 0.0f)
	{
		rMainLayout.iWaterMediumCount = 0;
		return;
	}

	int64_t iCount = gWaterMediumCount.Get<int64_t>();

	MediumWaveTunables tunables {};
	tunables.iCount = iCount;
	tunables.fWavelength = gWaterMediumWavelength.mfCurrent;
	tunables.fAmplitude = gWaterMediumAmplitude.mfCurrent;
	tunables.fSpeed = gWaterMediumSpeed.mfCurrent;
	tunables.fAngleAdjust = gWaterMediumAngleAdjust.mfCurrent;
	tunables.fWavelengthAdjust = gWaterMediumWavelengthAdjust.mfCurrent;
	tunables.fAmplitudeAdjust = gWaterMediumAmplitudeAdjust.mfCurrent;
	tunables.fSpeedAdjust = gWaterMediumSpeedAdjust.mfCurrent;
	if (tunables != sMediumWaveTunables)
	{
		sMediumWaveTunables = tunables;
		RebuildMediumWaveInvariants(iCount);
	}

	for (int64_t i = 0; i < iCount; ++i)
	{
		sMediumWaveStaging.pf4WavesTwo[i].y = sMediumWaveStaging.pfBaseAmplitude[i] * fMediumAmplitudeScale;
		double fDirectionX = static_cast<double>(sMediumWaveStaging.pf4WavesOne[i].x);
		double fDirectionY = static_cast<double>(sMediumWaveStaging.pf4WavesOne[i].y);
		double fOmega = static_cast<double>(sMediumWaveStaging.pf4WavesTwo[i].x);
		double fPhi = static_cast<double>(sMediumWaveStaging.pf4WavesTwo[i].z);
		sMediumWaveStaging.pf4WavesTwo[i].w = static_cast<float>(std::fmod((fDirectionX * fWaveCameraX + fDirectionY * fWaveCameraY) * fOmega + fPhi * fWaveTime, kfWaveTwoPi));
	}

	std::memcpy(rMainLayout.pf4MediumWavesOne, sMediumWaveStaging.pf4WavesOne, static_cast<size_t>(iCount) * sizeof(rMainLayout.pf4MediumWavesOne[0]));
	std::memcpy(rMainLayout.pf4MediumWavesTwo, sMediumWaveStaging.pf4WavesTwo, static_cast<size_t>(iCount) * sizeof(rMainLayout.pf4MediumWavesTwo[0]));
	rMainLayout.iWaterMediumCount = static_cast<int32_t>(iCount);
}

// Wave phase reduction: fCurrentTime is the same elapsed time this frame's RenderFrameGlobal received.
// The band helpers publish each iWater*Count beside its MainLayout wave arrays; when a per-stack camera-eye-height
// fade clamps amplitude to zero they zero the count so the WaterDisplacement.comp Gerstner loop short-circuits to no work.
static void PopulateGerstnerWaves(shaders::MainLayout& rMainLayout, float fCurrentTime, GraphicsQualityLevel eWaterLevel)
{
	if (eWaterLevel == GraphicsQualityLevel::kLow)
	{
		rMainLayout.iWaterLowCount = 0;
		rMainLayout.iWaterMediumCount = 0;
		return;
	}

	double fWaveTime = static_cast<double>(fCurrentTime);
	XMFLOAT4A f4WaveCameraPosition {};
	XMStoreFloat4A(&f4WaveCameraPosition, engine::gpCamera->mVecPosition);
	// WaterDisplacement.comp adds dot(direction, worldPosition - waterOrigin) to this term, so the term must carry the
	// absolute camera position for the wave phase to be continuous: reconstruct it as a double from the camera cell
	// and the camera's local position, exactly as the reduced water origins do. CPU only — the uploaded phase is
	// already reduced modulo 2*pi.
	auto [fWaveCellX, fWaveCellY] = CellCenterOffsetDouble(kOriginCoordinate, engine::gpCamera->mBasisCoordinate);
	double fWaveCameraX = fWaveCellX + static_cast<double>(f4WaveCameraPosition.x);
	double fWaveCameraY = fWaveCellY + static_cast<double>(f4WaveCameraPosition.y);

	// Camera eye height scales geometric wave amplitudes using each stack's Start and End settings.
	float fCameraEyeHeight = engine::gpCamera->GetRenderEyeHeight();
	float fLowFadeStart = gWaterLowAmplitudeFadeStart.mfCurrent;
	float fLowFadeEnd = gWaterLowAmplitudeFadeEnd.mfCurrent;
	float fLowAmplitudeScale = std::clamp((fLowFadeEnd - fCameraEyeHeight) / std::max(fLowFadeEnd - fLowFadeStart, 1.0e-3f), 0.0f, 1.0f);
	float fMediumFadeStart = gWaterMediumAmplitudeFadeStart.mfCurrent;
	float fMediumFadeEnd = gWaterMediumAmplitudeFadeEnd.mfCurrent;
	float fMediumAmplitudeScale = std::clamp((fMediumFadeEnd - fCameraEyeHeight) / std::max(fMediumFadeEnd - fMediumFadeStart, 1.0e-3f), 0.0f, 1.0f);

	PopulateGerstnerLowWaves(rMainLayout, fWaveTime, fWaveCameraX, fWaveCameraY, fLowAmplitudeScale);

	if (eWaterLevel == GraphicsQualityLevel::kMedium)
	{
		rMainLayout.iWaterMediumCount = 0;
		return;
	}

	PopulateGerstnerMediumWaves(rMainLayout, fWaveTime, fWaveCameraX, fWaveCameraY, fMediumAmplitudeScale);
}

void RenderWaterMain(int64_t iCommandBuffer, std::chrono::duration<float> currentTime)
{
	shaders::MainLayout& rMainLayout = *reinterpret_cast<shaders::MainLayout*>(&gpBufferManager->mMainLayoutUniformBuffers.at(iCommandBuffer).mpMappedMemory[0]);
	GraphicsQualityLevel eWaterLevel = static_cast<GraphicsQualityLevel>(std::clamp(gWaterLevel.Get<int64_t>(), 0i64, static_cast<int64_t>(GraphicsQualityLevel::kCount) - 1));

	// Per-frame visible-area LOD draw params for water. The water pipeline binds a single concat
	// mesh buffer holding all LODs; per-frame we tell vkCmdDrawIndexedIndirect which LOD's index
	// range and vertex base to draw. engine::CameraBase computes miVisibleAreaLevelOfDetail from eye distance with 4×
	// hysteresis bands; mesh density and snap-grid are in lockstep. Terrain draws via one
	// vkCmdDrawIndexedIndirect per island template in CommandBufferRecordMain.cpp.
	int64_t iLevelOfDetail = engine::gpCamera->miVisibleAreaLevelOfDetail;
	const BufferManager::VisibleAreaMeshLod& rWaterLevelOfDetail = gpBufferManager->mpWaterMeshLods[iLevelOfDetail];
	gpPipelineManager->mpPipelines[kPipelineWater].WriteIndirectBuffer(iCommandBuffer, 1, rWaterLevelOfDetail.iIndexCount, rWaterLevelOfDetail.iIndexOffset, rWaterLevelOfDetail.iVertexOffset);

	// Active LOD's vertex-grid dims (iQuadCount* == iMeshX/Y - 1). Read by:
	//   1) WaterDisplacement.comp — bounds-checks each thread, only writes the top-left rectangle.
	//   2) Water.vert — scales f2InTexcoord to the matching texel index via texelFetch.
	//   3) WaterFoam.comp — clamps native compression samples to the active grid.
	// All consumers read the same values, including frames with no renderable camera cell.
	rMainLayout.iWaterActiveQuadX = static_cast<int32_t>(rWaterLevelOfDetail.iQuadCountX);
	rMainLayout.iWaterActiveQuadY = static_cast<int32_t>(rWaterLevelOfDetail.iQuadCountY);

	// Low writes a zero dispatch because Water.vert also bypasses the displacement textures. Medium and High use
	// one workgroup per kiComputeTileSize block over the active LOD sub-region, written per framebuffer to match
	// RecordComputeIndirect's slot indexing.
	if (eWaterLevel == GraphicsQualityLevel::kLow)
	{
		gpPipelineManager->mpPipelines[kPipelineWaterDisplacement].WriteIndirectComputeBuffer(iCommandBuffer, 0, 0, 0);
	}
	else
	{
		gpPipelineManager->mpPipelines[kPipelineWaterDisplacement].WriteIndirectComputeBuffer(iCommandBuffer, (rWaterLevelOfDetail.iQuadCountX + 1 + shaders::kiComputeTileSize - 1) / shaders::kiComputeTileSize, (rWaterLevelOfDetail.iQuadCountY + 1 + shaders::kiComputeTileSize - 1) / shaders::kiComputeTileSize, 1);
	}

	PopulateGerstnerWaves(rMainLayout, currentTime.count(), eWaterLevel);
}

} // namespace engine

#endif // defined(BT_CLIENT)
