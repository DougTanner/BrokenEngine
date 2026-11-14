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

bool WaterFoamActive()
{
	return gWaterFoamSystemEnabled.Get<bool>() && gWaterLevel.Get<int64_t>() > static_cast<int64_t>(GraphicsQualityLevel::kLow);
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

	// RenderLightingMain publishes the same float size quotients; a different value would make the pattern drift with camera motion.
	float fWaterScale = gWaterScale.mfCurrent;
	double fSizeBaseOne = static_cast<double>(gLightingSampledNormalsOneSize.mfCurrent / fWaterScale);
	double fSizeBaseTwo = static_cast<double>(gLightingSampledNormalsTwoSize.mfCurrent / fWaterScale);
	double fSizeBaseThree = static_cast<double>(gLightingSampledNormalsThreeSize.mfCurrent / fWaterScale);
	// Camera-height-driven speed lerp — single-sourced fade endpoint shared with LightingUniforms.cpp.
	float fCameraHeightZoomFactor = engine::LerpAtHeight(engine::gpCamera->RenderFraming(), engine::CameraBase::kfCameraFramingDefault, engine::CameraBase::kfWaveFadeEndFraming, 0.0f, 1.0f);
	// Scroll speed follows the Gerstner phase speed, which grows with the square root of wavelength.
	float fSpeedScale = std::sqrt(fWaterScale);
	double fSpeedOne = static_cast<double>(std::lerp(gLightingSampledNormalsSpeedOneMinimum.mfCurrent, gLightingSampledNormalsSpeedOneMaximum.mfCurrent, fCameraHeightZoomFactor) * fSpeedScale);
	double fSpeedTwo = static_cast<double>(std::lerp(gLightingSampledNormalsSpeedTwoMinimum.mfCurrent, gLightingSampledNormalsSpeedTwoMaximum.mfCurrent, fCameraHeightZoomFactor) * fSpeedScale);
	double fSpeedThree = static_cast<double>(std::lerp(gLightingSampledNormalsSpeedThreeMinimum.mfCurrent, gLightingSampledNormalsSpeedThreeMaximum.mfCurrent, fCameraHeightZoomFactor) * fSpeedScale);
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
	double fNoiseFrequency = static_cast<double>(gWaterColorNoiseFrequency.mfCurrent / fWaterScale);
	XMFLOAT2 f2ReducedNoiseOrigin {static_cast<float>(std::fmod(fNoiseFrequency * fCameraX, 10.0)), static_cast<float>(std::fmod(fNoiseFrequency * fCameraY, 10.0))};
	rGlobalLayout.fWaterReducedNoiseOriginX = f2ReducedNoiseOrigin.x;
	rGlobalLayout.fWaterReducedNoiseOriginY = f2ReducedNoiseOrigin.y;
	gPresentationContinuity.f2ReducedNoiseOrigin = f2ReducedNoiseOrigin;
	gPresentationContinuity.fNoiseFrequency = static_cast<float>(fNoiseFrequency);

	// One patch lattice per blended texture read, at that read's own frequency; each read's seed is its position in this
	// order. Only each normal band's octave One read is blended.
	bool bBreakRepetition = gBreakRepetition.Get<bool>();
	rGlobalLayout.fWaterNormalPatchBlendOne = bBreakRepetition && gWaterNormalPatchBlendOneEnabled.Get<bool>() ? gWaterNormalPatchBlendOne.mfCurrent : 0.0f;
	rGlobalLayout.fWaterNormalPatchBlendTwo = bBreakRepetition && gWaterNormalPatchBlendTwoEnabled.Get<bool>() ? gWaterNormalPatchBlendTwo.mfCurrent : 0.0f;
	rGlobalLayout.fWaterNormalPatchBlendThree = bBreakRepetition && gWaterNormalPatchBlendThreeEnabled.Get<bool>() ? gWaterNormalPatchBlendThree.mfCurrent : 0.0f;
	rGlobalLayout.fWaterColorNoisePatchBlend = bBreakRepetition && gWaterColorNoisePatchBlendEnabled.Get<bool>() ? gWaterColorNoisePatchBlend.mfCurrent : 0.0f;
	rGlobalLayout.iWaterNormalExtraSamplePatchBlend = bBreakRepetition && gWaterNormalExtraSamplePatchBlendEnabled.Get<bool>() ? 1 : 0;
	static constexpr float kpfNormalSizeMultipliers[] = {shaders::kfWaterNormalOneOctaveOneSizeMultiplier, shaders::kfWaterNormalTwoOctaveOneSizeMultiplier, shaders::kfWaterNormalThreeOctaveOneSizeMultiplier};
	static_assert(std::size(kpfNormalSizeMultipliers) == std::extent_v<decltype(shaders::GlobalLayout::pWaterNormalPatchLattices)>);
	double pfSizeBases[] = {fSizeBaseOne, fSizeBaseTwo, fSizeBaseThree};
	uint32_t uiSeed = 0;
	for (int64_t i = 0; i < std::ssize(kpfNormalSizeMultipliers); ++i)
	{
		gPresentationContinuity.pWaterNormalLattices[i] = PopulatePatchLattice(rGlobalLayout.pWaterNormalPatchLattices[i], static_cast<double>(kpfNormalSizeMultipliers[i]) * pfSizeBases[i], uiSeed++);
	}
	gPresentationContinuity.pWaterColorNoiseLattices[0] = PopulatePatchLattice(rGlobalLayout.pWaterColorNoisePatchLattices[0], static_cast<double>(gWaterColorNoiseMultiplierOne.mfCurrent) * fNoiseFrequency, uiSeed++);
	gPresentationContinuity.pWaterColorNoiseLattices[1] = PopulatePatchLattice(rGlobalLayout.pWaterColorNoisePatchLattices[1], static_cast<double>(gWaterColorNoiseMultiplierTwo.mfCurrent) * fNoiseFrequency, uiSeed++);
	if (WaterFoamActive())
	{
		auto ReduceFoamOrigin = [&](float fTileSize, shaders::vec2& rf2ReducedOrigin)
		{
			double fInverseTileSize = 1.0 / static_cast<double>(fTileSize * fWaterScale);
			rf2ReducedOrigin.x = static_cast<float>(std::fmod(fInverseTileSize * fCameraX, 2.0));
			rf2ReducedOrigin.y = static_cast<float>(std::fmod(fInverseTileSize * fCameraY, 2.0));
			return static_cast<float>(fInverseTileSize);
		};
		float fTextureInverseTileSize = ReduceFoamOrigin(gWaterFoamTextureTileSize.mfCurrent, rGlobalLayout.f2WaterFoamReducedTextureOrigin);
		rGlobalLayout.fWaterFoamTextureInverseTileSize = fTextureInverseTileSize;
		rGlobalLayout.fWaterFoamTextureStrength = gWaterFoamEnabled.Get<bool>() ? gWaterFoamTextureStrength.mfCurrent : 0.0f;
		float fBreakupInverseTileSize = ReduceFoamOrigin(gWaterFoamBreakupTileSize.mfCurrent, rGlobalLayout.f2WaterFoamBreakupReducedTextureOrigin);
		rGlobalLayout.fWaterFoamBreakupInverseTileSize = fBreakupInverseTileSize;
		rGlobalLayout.fWaterFoamBreakupAmount = gWaterFoamBreakupAmount.mfCurrent;
		rGlobalLayout.uiWaterFoamTextureIndex = static_cast<uint32_t>(gWaterFoamTextureIndex.GetIndex());
		rGlobalLayout.uiWaterFoamBreakupTextureIndex = static_cast<uint32_t>(gWaterFoamBreakupTextureIndex.GetIndex());
		float fSpawnNoiseInverseTileSize = ReduceFoamOrigin(gWaterFoamSpawnNoiseTileSize.mfCurrent, rGlobalLayout.f2WaterFoamSpawnNoiseReducedOrigin);
		rGlobalLayout.fWaterFoamSpawnNoiseInverseTileSize = fSpawnNoiseInverseTileSize;

		rGlobalLayout.fWaterFoamTexturePatchBlend = bBreakRepetition && gWaterFoamTexturePatchBlendEnabled.Get<bool>() ? gWaterFoamTexturePatchBlend.mfCurrent : 0.0f;
		rGlobalLayout.fWaterFoamBreakupPatchBlend = bBreakRepetition && gWaterFoamBreakupPatchBlendEnabled.Get<bool>() ? gWaterFoamBreakupPatchBlend.mfCurrent : 0.0f;
		rGlobalLayout.fWaterFoamSpawnNoisePatchBlend = bBreakRepetition && gWaterFoamSpawnNoisePatchBlendEnabled.Get<bool>() ? gWaterFoamSpawnNoisePatchBlend.mfCurrent : 0.0f;
		gPresentationContinuity.waterFoamTextureLattice = PopulatePatchLattice(rGlobalLayout.waterFoamTexturePatchLattice, static_cast<double>(fTextureInverseTileSize), uiSeed++);
		gPresentationContinuity.waterFoamBreakupLattice = PopulatePatchLattice(rGlobalLayout.waterFoamBreakupPatchLattice, static_cast<double>(fBreakupInverseTileSize), uiSeed++);
		gPresentationContinuity.waterFoamSpawnNoiseLattice = PopulatePatchLattice(rGlobalLayout.waterFoamSpawnNoisePatchLattice, static_cast<double>(fSpawnNoiseInverseTileSize), uiSeed++);
	}
	++gPresentationContinuity.iPublishedFrames;
}

static void PopulateWaterFoam(shaders::GlobalLayout& rGlobalLayout, std::chrono::duration<float> elapsedTime)
{
	static bool sbInitialized = false;
	static RetainedAreaBasis sRetainedBasis {};
	static XMFLOAT4 sf4PreviousArea {};
	static std::chrono::duration<float> sPreviousElapsedTime(0.0f);
	static std::chrono::steady_clock::time_point sPreviousUpdate {};

	if (!WaterFoamActive())
	{
		rGlobalLayout.iWaterFoamEnabled = 0;
		sbInitialized = false;
		return;
	}

	std::chrono::steady_clock::time_point updateTime = std::chrono::steady_clock::now();
	XMFLOAT2 f2Offset = sRetainedBasis.Advance(engine::gpCamera->mBasisCoordinate);
	bool bClockReset = elapsedTime < sPreviousElapsedTime;
	bool bReset = !sbInitialized || gbWaterFoamReset || bClockReset;
	XMFLOAT4 f4Area = engine::gpCamera->mf4RenderVisibleArea;
	XMFLOAT4 f4PreviousArea = sf4PreviousArea;
	if (bReset)
	{
		f4PreviousArea = f4Area;
	}
	else
	{
		ShiftArea(f4PreviousArea, f2Offset);
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
	rGlobalLayout.iWaterFoamEnabled = 1;
	sf4PreviousArea = f4Area;
	sPreviousElapsedTime = elapsedTime;
	sPreviousUpdate = updateTime;
	sbInitialized = true;
	gbWaterFoamReset = false;
}

void PopulateWaterParameters(shaders::GlobalLayout& rGlobalLayout, float fSunAngle, float fDayPercent, std::chrono::duration<float> elapsedTime)
{
	rGlobalLayout.fWaterTerrainHeight = gWaterTerrainHeight.mfCurrent;
	rGlobalLayout.fWaterTerrainFadeInverse = 1.0f / gWaterTerrainFade.mfCurrent;

	rGlobalLayout.fWaterDepthLookupTableFeather = gWaterDepthLookupTableFeather.mfCurrent;
	rGlobalLayout.fWaterDepthColorFeather = gWaterDepthColorFeather.mfCurrent;
	rGlobalLayout.fWaterDepthColorFloor = gWaterDepthColorFloor.mfCurrent;
	rGlobalLayout.fWaterUnderseaCompressionInverse = 1.0f / gWaterUnderseaCompression.mfCurrent; // TerrainElevation.frag undersea depth-curve exponent
	rGlobalLayout.fWaterDepthReflectionFeatherInverse = 1.0f / (fDayPercent * gWaterDepthReflectionFeather.mfCurrent); // unguarded: night dayPercent=0 -> +inf absorbed by the shader clamp
	rGlobalLayout.fWaterColorNoiseFrequency = gWaterColorNoiseFrequency.mfCurrent / gWaterScale.mfCurrent; // same quotient PopulateWaterReducedUv reduces

	PopulateWaterSunsetFade(rGlobalLayout, fSunAngle);

	rGlobalLayout.fWaterFresnel = std::pow(fDayPercent, 0.5f) * gWaterFresnel.mfCurrent;
	rGlobalLayout.fWaterColorBottom = gWaterColorBottom.mfCurrent;
	rGlobalLayout.fWaterColorHeightInverse = 1.0f / (gWaterColorHeight.mfCurrent * gWaterScale.mfCurrent);
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
	// Each wave's k*A is Slope / sqrt(count), so Choppiness 1 sets the band's RMS of the summed Q*k*A terms to 0.4, below
	// the fold limit of 1. Amplitude publishes Slope only as the shader's band gate.
	static constexpr float kfSteepnessPerChoppiness = std::numbers::sqrt2_v<float> / 2.5f;
	float fLowSlope = gWaterLowSlope.mfCurrent;
	float fMediumSlope = gWaterMediumSlope.mfCurrent;
	rGlobalLayout.fWaterLowSteepness = fLowSlope > 0.0f ? gWaterLowChoppiness.mfCurrent * kfSteepnessPerChoppiness / fLowSlope : 0.0f;
	rGlobalLayout.fWaterMediumSteepness = fMediumSlope > 0.0f ? gWaterMediumChoppiness.mfCurrent * kfSteepnessPerChoppiness / fMediumSlope : 0.0f;
	rGlobalLayout.fWaterWaveNormalBlend = gWaterWaveNormalBlend.mfCurrent;
	rGlobalLayout.fWaterLowAmplitude = fLowSlope;
	rGlobalLayout.fWaterMediumAmplitude = fMediumSlope;

	PopulateWaterFoam(rGlobalLayout, elapsedTime);
	PopulateWaterReducedUv(rGlobalLayout, elapsedTime);
}

// Gerstner wave phase-reduction modulus (shared by both bands).
constexpr double kfWaveTwoPi = 2.0 * 3.14159265358979323846;

// CPU staging for one Gerstner wave band. Wave math reads and writes this cacheable copy because
// each readback from the write-combined mapped uniform buffer stalls on memory latency; the populate
// ends with one memcpy per array region into rMainLayout, which `vec4` == XMFLOAT4 makes a straight
// copy. Rebuilt only when the consumed tunables change: direction (pf4WavesOne.xy), wavenumber k
// (pf4WavesTwo.x), angular rate (pf4WavesTwo.z, CPU-only), pfBaseAmplitude (before the framing fade
// scale), pfPhaseOffset. Rewritten every frame: pf4WavesTwo.y (pfBaseAmplitude x fade scale x
// anti-aliasing weight) and pf4WavesTwo.w (the fmod phase term). Sized to the 256-entry shader maxima.
struct GerstnerWaveBandStaging
{
	XMFLOAT4 pf4WavesOne[256];
	XMFLOAT4 pf4WavesTwo[256];
	float pfBaseAmplitude[256] {};
	float pfPhaseOffset[256] {};
};

// Consumed band tunables snapshot; inequality vs last frame triggers an invariant rebuild. iCount
// defaults to -1 (never a resolved count) so the first real frame always rebuilds. Choppiness and
// Wave Time Scale are deliberately absent: Choppiness publishes in Global and the time scale is read
// per frame, as is the framing amplitude-fade scale.
struct WaveBandTunables
{
	int64_t iCount = -1;
	float fDirectionDegrees = 0.0f;
	float fSpreadDegrees = 0.0f;
	float fWavelengthShortest = 0.0f;
	float fWavelengthLongest = 0.0f;
	float fSlope = 0.0f;
	float fScale = 0.0f;

	bool operator==(const WaveBandTunables&) const = default;
};

static GerstnerWaveBandStaging sLowWaveStaging {};
static GerstnerWaveBandStaging sMediumWaveStaging {};
static WaveBandTunables sLowWaveTunables {};
static WaveBandTunables sMediumWaveTunables {};

// Cached wave terms require fixed per-wave float expression order and RandomEngine consumption: two draws per wave, both
// always taken.
static void PopulateGerstnerBand(const WaveBandTunables& rTunables, WaveBandTunables& rCachedTunables, GerstnerWaveBandStaging& rStaging, shaders::vec4* pf4LayoutWavesOne, shaders::vec4* pf4LayoutWavesTwo, int32_t& riLayoutCount, uint32_t uiSeed, double fWaveTime, double fWaveCameraX, double fWaveCameraY, float fAmplitudeScale, float fQuadSize)
{
	if (fAmplitudeScale <= 0.0f)
	{
		riLayoutCount = 0;
		return;
	}

	int64_t iCount = rTunables.iCount;
	if (rTunables != rCachedTunables)
	{
		rCachedTunables = rTunables;

		// Water Scale multiplies wavelength and so amplitude, preserving wave shape; deep-water dispersion then scales
		// phase speed by its square root. Wavelengths are geometric between Shortest and Longest, and dividing amplitude
		// by sqrt(count) keeps the band's RMS slope independent of the count.
		float fWavelengthShortest = rTunables.fWavelengthShortest * rTunables.fScale;
		float fWavelengthLongest = rTunables.fWavelengthLongest * rTunables.fScale;
		float fWavelengthRatio = fWavelengthLongest / fWavelengthShortest;
		float fAmplitudeNumerator = rTunables.fSlope / std::sqrt(static_cast<float>(iCount));
		common::RandomEngine randomEngine(uiSeed);
		for (int64_t i = 0; i < iCount; ++i)
		{
			float fDirectionRandom = common::Random(randomEngine);
			float fPhaseRandom = common::Random(randomEngine);

			float fWavelength = fWavelengthShortest * std::pow(fWavelengthRatio, static_cast<float>(i) / static_cast<float>(iCount - 1));
			float fWavenumber = XM_2PI / fWavelength;
			rStaging.pf4WavesTwo[i].x = fWavenumber;

			// Counter-clockwise from +x.
			float fAngle = XMConvertToRadians(rTunables.fDirectionDegrees + rTunables.fSpreadDegrees * (2.0f * fDirectionRandom - 1.0f));
			rStaging.pf4WavesOne[i].x = std::cos(fAngle);
			rStaging.pf4WavesOne[i].y = std::sin(fAngle);

			rStaging.pfBaseAmplitude[i] = fAmplitudeNumerator / fWavenumber;
			rStaging.pf4WavesTwo[i].z = std::sqrt(9.81f * fWavenumber);
			rStaging.pfPhaseOffset[i] = XM_2PI * fPhaseRandom;
		}
	}

	// The shader phase is k*dot(D, x) + w, so subtracting the time term makes waves travel along +D.
	double fTimeScale = static_cast<double>(gWaterWaveTimeScale.mfCurrent);
	for (int64_t i = 0; i < iCount; ++i)
	{
		// A wave shorter than two grid quads aliases on the vertex grid and flickers as the quad size re-latches.
		float fQuads = XM_2PI / (rStaging.pf4WavesTwo[i].x * fQuadSize);
		float fAlias = std::clamp((fQuads - 2.0f) * 0.5f, 0.0f, 1.0f);
		float fAliasWeight = fAlias * fAlias * (3.0f - 2.0f * fAlias);
		rStaging.pf4WavesTwo[i].y = rStaging.pfBaseAmplitude[i] * fAmplitudeScale * fAliasWeight;
		double fDirectionX = static_cast<double>(rStaging.pf4WavesOne[i].x);
		double fDirectionY = static_cast<double>(rStaging.pf4WavesOne[i].y);
		double fWavenumber = static_cast<double>(rStaging.pf4WavesTwo[i].x);
		double fRate = static_cast<double>(rStaging.pf4WavesTwo[i].z);
		double fPhaseOffset = static_cast<double>(rStaging.pfPhaseOffset[i]);
		rStaging.pf4WavesTwo[i].w = static_cast<float>(std::fmod((fDirectionX * fWaveCameraX + fDirectionY * fWaveCameraY) * fWavenumber + fPhaseOffset - fRate * fTimeScale * fWaveTime, kfWaveTwoPi));
	}

	std::memcpy(pf4LayoutWavesOne, rStaging.pf4WavesOne, static_cast<size_t>(iCount) * sizeof(pf4LayoutWavesOne[0]));
	std::memcpy(pf4LayoutWavesTwo, rStaging.pf4WavesTwo, static_cast<size_t>(iCount) * sizeof(pf4LayoutWavesTwo[0]));
	riLayoutCount = static_cast<int32_t>(iCount);
}

// Wave phase reduction: fCurrentTime is the same elapsed time this frame's RenderFrameGlobal received.
// PopulateGerstnerBand publishes each iWater*Count beside its MainLayout wave arrays; when a per-stack camera-framing
// fade clamps amplitude to zero it zeroes the count so the WaterDisplacement.comp Gerstner loop short-circuits to no work.
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

	// Camera framing scales geometric wave amplitudes using each stack's Start and End settings.
	float fCameraFraming = engine::gpCamera->RenderFraming();
	float fQuadSize = std::max(engine::gpCamera->mf2VisibleAreaQuadSize.x, engine::gpCamera->mf2VisibleAreaQuadSize.y);
	float fLowFadeStart = gWaterLowAmplitudeFadeStart.mfCurrent;
	float fLowFadeEnd = gWaterLowAmplitudeFadeEnd.mfCurrent;
	float fLowAmplitudeScale = std::clamp((fLowFadeEnd - fCameraFraming) / std::max(fLowFadeEnd - fLowFadeStart, 1.0e-3f), 0.0f, 1.0f);
	float fMediumFadeStart = gWaterMediumAmplitudeFadeStart.mfCurrent;
	float fMediumFadeEnd = gWaterMediumAmplitudeFadeEnd.mfCurrent;
	float fMediumAmplitudeScale = std::clamp((fMediumFadeEnd - fCameraFraming) / std::max(fMediumFadeEnd - fMediumFadeStart, 1.0e-3f), 0.0f, 1.0f);

	WaveBandTunables lowTunables {.iCount = gWaterLowCount.Get<int64_t>(), .fDirectionDegrees = gWaterLowDirectionDegrees.mfCurrent, .fSpreadDegrees = gWaterLowSpreadDegrees.mfCurrent, .fWavelengthShortest = gWaterLowWavelengthShortest.mfCurrent, .fWavelengthLongest = gWaterLowWavelengthLongest.mfCurrent, .fSlope = gWaterLowSlope.mfCurrent, .fScale = gWaterScale.mfCurrent};
	PopulateGerstnerBand(lowTunables, sLowWaveTunables, sLowWaveStaging, rMainLayout.pf4LowWavesOne, rMainLayout.pf4LowWavesTwo, rMainLayout.iWaterLowCount, 0, fWaveTime, fWaveCameraX, fWaveCameraY, fLowAmplitudeScale, fQuadSize);

	if (eWaterLevel == GraphicsQualityLevel::kMedium)
	{
		rMainLayout.iWaterMediumCount = 0;
		return;
	}

	WaveBandTunables mediumTunables {.iCount = gWaterMediumCount.Get<int64_t>(), .fDirectionDegrees = gWaterMediumDirectionDegrees.mfCurrent, .fSpreadDegrees = gWaterMediumSpreadDegrees.mfCurrent, .fWavelengthShortest = gWaterMediumWavelengthShortest.mfCurrent, .fWavelengthLongest = gWaterMediumWavelengthLongest.mfCurrent, .fSlope = gWaterMediumSlope.mfCurrent, .fScale = gWaterScale.mfCurrent};
	PopulateGerstnerBand(mediumTunables, sMediumWaveTunables, sMediumWaveStaging, rMainLayout.pf4MediumWavesOne, rMainLayout.pf4MediumWavesTwo, rMainLayout.iWaterMediumCount, 1, fWaveTime, fWaveCameraX, fWaveCameraY, fMediumAmplitudeScale, fQuadSize);
}

void RenderWaterMain(int64_t iCommandBuffer, std::chrono::duration<float> currentTime)
{
	shaders::MainLayout& rMainLayout = *reinterpret_cast<shaders::MainLayout*>(&gpBufferManager->mMainLayoutUniformBuffers.at(iCommandBuffer).mpMappedMemory[0]);
	GraphicsQualityLevel eWaterLevel = static_cast<GraphicsQualityLevel>(std::clamp(gWaterLevel.Get<int64_t>(), 0i64, static_cast<int64_t>(GraphicsQualityLevel::kCount) - 1));

	// Per-frame visible-area LOD draw params for water. The water pipeline binds a single concat
	// mesh buffer holding all LODs; per-frame we tell vkCmdDrawIndexedIndirect which LOD's index
	// range and vertex base to draw. engine::CameraBase computes miVisibleAreaLevelOfDetail from framing with 4×
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
