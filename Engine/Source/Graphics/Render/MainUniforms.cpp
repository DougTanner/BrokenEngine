#if defined(BT_CLIENT)

#include "Graphics/Debug/DebugRender.h"
#include "Ui/GraphicsQualityWrappersBase.h"
#include "Ui/HexShieldWrappersBase.h"
#include "Ui/WaterWrappersBase.h"
#include "Ui/WrapperBase.h"
#include "Render.h"

#include "Frame/Collections/Players/Players.h"
#include "Game.h"

namespace engine
{

static void DebugRenderFrameEdges(const std::vector<GridCoord>& rActiveCoordinates, GridCoord cameraCoordinate)
{
	if constexpr (!kbDebugRender)
	{
		return;
	}

	float fZ = gBaseHeight.mfCurrent;
	static constexpr XMFLOAT4A kf4EdgeColor = {0.0f, 1.0f, 1.0f, 1.0f};

	for (const GridCoord& rCoordinate : rActiveCoordinates)
	{
		auto it = game::gpGame->mCoordinateFrames.find(rCoordinate);
		if (it == game::gpGame->mCoordinateFrames.end())
		{
			continue;
		}

		// Every cell has the same local edges; the basis offset is what places this one on screen.
		// vecArea packing: x=minX, y=maxY, z=maxX, w=minY
		XMFLOAT2 f2Offset = MakeRenderBasis(rCoordinate, cameraCoordinate).f2Offset;
		XMVECTOR vecArea = LocalFrameArea();
		float fMinX = XMVectorGetX(vecArea) + f2Offset.x;
		float fMaxY = XMVectorGetY(vecArea) + f2Offset.y;
		float fMaxX = XMVectorGetZ(vecArea) + f2Offset.x;
		float fMinY = XMVectorGetW(vecArea) + f2Offset.y;

		XMFLOAT3A f3MinMin = {fMinX, fMinY, fZ};
		XMFLOAT3A f3MaxMin = {fMaxX, fMinY, fZ};
		XMFLOAT3A f3MaxMax = {fMaxX, fMaxY, fZ};
		XMFLOAT3A f3MinMax = {fMinX, fMaxY, fZ};

		DebugRender::Line(f3MinMin, f3MaxMin, kf4EdgeColor);
		DebugRender::Line(f3MaxMin, f3MaxMax, kf4EdgeColor);
		DebugRender::Line(f3MaxMax, f3MinMax, kf4EdgeColor);
		DebugRender::Line(f3MinMax, f3MinMin, kf4EdgeColor);
	}
}

static void DebugRenderIslandBoundaries(const std::vector<GridCoord>& rActiveCoordinates, GridCoord cameraCoordinate)
{
	if constexpr (!kbDebugRender)
	{
		return;
	}

	float fZ = gBaseHeight.mfCurrent;
	static constexpr XMFLOAT4A kf4BoundaryColor = {1.0f, 0.0f, 1.0f, 1.0f};

	for (const GridCoord& rCoordinate : rActiveCoordinates)
	{
		auto it = game::gpGame->mCoordinateFrames.find(rCoordinate);
		if (it == game::gpGame->mCoordinateFrames.end())
		{
			continue;
		}

		// Placement positions are local to this cell; the Rotate helper is the one point that converts them.
		XMFLOAT2 f2Offset = MakeRenderBasis(rCoordinate, cameraCoordinate).f2Offset;

		for (const IslandPlacement& rPlacement : it->second.staticData.islands)
		{
			const IslandTemplate& rTemplate = gpIslandTerrain->mIslands.at(rPlacement.islandCrc);
			float fHalfX = 0.5f * rTemplate.fQuadFootprintX;
			float fHalfY = 0.5f * rTemplate.fQuadFootprintY;
			float fCosine = std::cos(rPlacement.fRotation);
			float fSine = std::sin(rPlacement.fRotation);

			auto Rotate = [&](float fLocalX, float fLocalY)
			{
				return XMFLOAT3A {rPlacement.f2WorldPosition.x + f2Offset.x + fLocalX * fCosine - fLocalY * fSine, rPlacement.f2WorldPosition.y + f2Offset.y + fLocalX * fSine + fLocalY * fCosine, fZ};
			};

			XMFLOAT3A f3Corner0 = Rotate(-fHalfX, -fHalfY);
			XMFLOAT3A f3Corner1 = Rotate( fHalfX, -fHalfY);
			XMFLOAT3A f3Corner2 = Rotate( fHalfX,  fHalfY);
			XMFLOAT3A f3Corner3 = Rotate(-fHalfX,  fHalfY);

			DebugRender::Line(f3Corner0, f3Corner1, kf4BoundaryColor);
			DebugRender::Line(f3Corner1, f3Corner2, kf4BoundaryColor);
			DebugRender::Line(f3Corner2, f3Corner3, kf4BoundaryColor);
			DebugRender::Line(f3Corner3, f3Corner0, kf4BoundaryColor);
		}
	}
}

static void DebugRenderIslandValidArea(const std::vector<GridCoord>& rActiveCoordinates, GridCoord cameraCoordinate)
{
	if constexpr (!kbDebugRender)
	{
		return;
	}

	// Drawn at the underwater mask threshold depth (the depth that defines the hull boundary), below
	// the magenta boundary rectangle / cyan frame edges at gBaseHeight. Debug lines are an overlay
	// (no depth test), so the underwater Z is never occluded by terrain or water.
	float fZ = common::kfUnderwaterMaskThresholdMeters;
	static constexpr XMFLOAT4A kf4ValidAreaColor = {0.0f, 1.0f, 0.0f, 1.0f};

	for (const GridCoord& rCoordinate : rActiveCoordinates)
	{
		auto it = game::gpGame->mCoordinateFrames.find(rCoordinate);
		if (it == game::gpGame->mCoordinateFrames.end())
		{
			continue;
		}

		// Placement positions are local to this cell; the Rotate helper is the one point that converts them.
		XMFLOAT2 f2Offset = MakeRenderBasis(rCoordinate, cameraCoordinate).f2Offset;

		for (const IslandPlacement& rPlacement : it->second.staticData.islands)
		{
			const IslandTemplate& rTemplate = gpIslandTerrain->mIslands.at(rPlacement.islandCrc);
			if (rTemplate.pf2ValidAreaVertices == nullptr || rTemplate.iValidAreaVertexCount < 3)
			{
				continue;
			}

			float fCosine = std::cos(rPlacement.fRotation);
			float fSine = std::sin(rPlacement.fRotation);

			auto Rotate = [&](const XMFLOAT2& rVertex)
			{
				return XMFLOAT3A {rPlacement.f2WorldPosition.x + f2Offset.x + rVertex.x * fCosine - rVertex.y * fSine, rPlacement.f2WorldPosition.y + f2Offset.y + rVertex.x * fSine + rVertex.y * fCosine, fZ};
			};

			int64_t iCount = rTemplate.iValidAreaVertexCount;
			for (int64_t i = 0; i < iCount; ++i)
			{
				const XMFLOAT2& rStartVertex = rTemplate.pf2ValidAreaVertices[i];
				const XMFLOAT2& rEndVertex = rTemplate.pf2ValidAreaVertices[(i + 1) % iCount];
				DebugRender::Line(Rotate(rStartVertex), Rotate(rEndVertex), kf4ValidAreaColor);
			}
		}
	}
}

static void DebugRenderNavigationData(const std::vector<GridCoord>& rActiveCoordinates, GridCoord cameraCoordinate)
{
	if constexpr (!kbDebugRender)
	{
		return;
	}

	float fZ = gBaseHeight.mfCurrent;
	static constexpr XMFLOAT4A kf4PolygonColor = {1.0f, 1.0f, 0.0f, 1.0f};
	static constexpr XMFLOAT4A kf4VertexColor = {1.0f, 0.5f, 0.0f, 1.0f};

	for (const GridCoord& rCoordinate : rActiveCoordinates)
	{
		auto it = game::gpGame->mCoordinateFrames.find(rCoordinate);
		if (it == game::gpGame->mCoordinateFrames.end())
		{
			continue;
		}

		const NavData& rNavigationData = it->second.staticData.navigationData;
		// Nav vertices are local to this cell; the two draw sites below are this helper's conversion point.
		XMFLOAT2 f2Offset = MakeRenderBasis(rCoordinate, cameraCoordinate).f2Offset;

		for (int64_t i = 0; i < std::ssize(rNavigationData.polygonOffsets); ++i)
		{
			int64_t iStart = rNavigationData.polygonOffsets.at(i);
			int64_t iEnd = (i + 1 < std::ssize(rNavigationData.polygonOffsets)) ? rNavigationData.polygonOffsets.at(i + 1) : std::ssize(rNavigationData.vertices);

			for (int64_t j = iStart; j < iEnd; ++j)
			{
				int64_t iNext = (j + 1 < iEnd) ? j + 1 : iStart;
				XMFLOAT3A f3StartVertex = {rNavigationData.vertices.at(j).x + f2Offset.x, rNavigationData.vertices.at(j).y + f2Offset.y, fZ};
				XMFLOAT3A f3EndVertex = {rNavigationData.vertices.at(iNext).x + f2Offset.x, rNavigationData.vertices.at(iNext).y + f2Offset.y, fZ};
				DebugRender::Line(f3StartVertex, f3EndVertex, kf4PolygonColor);
			}
		}

		for (const XMFLOAT2& rVertex : rNavigationData.vertices)
		{
			DebugRender::Circle({rVertex.x + f2Offset.x, rVertex.y + f2Offset.y, fZ}, 0.75f, kf4VertexColor);
		}
	}
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
	double fWaveCameraX = static_cast<double>(engine::gpCamera->mBasisCoordinate.iX) * static_cast<double>(kfCellWidth) + static_cast<double>(f4WaveCameraPosition.x);
	double fWaveCameraY = static_cast<double>(engine::gpCamera->mBasisCoordinate.iY) * static_cast<double>(kfCellHeight) + static_cast<double>(f4WaveCameraPosition.y);

	// Fade geometric wave amplitudes by camera eye height — per-stack Start/End sliders (1.0 at ≤ Start, 0.0 at ≥ End, linear between).
	float fCameraEyeHeight = engine::gpCamera->mfCameraEyeHeight;
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

static void PopulateHexShield(shaders::MainLayout& rMainLayout)
{
	rMainLayout.fHexShieldGrow = engine::gHexShieldGrow.mfCurrent;
	rMainLayout.fHexShieldEdgeDistance = engine::gHexShieldEdgeDistance.mfCurrent;
	rMainLayout.fHexShieldEdgePower = engine::gHexShieldEdgePower.mfCurrent;
	rMainLayout.fHexShieldEdgeMultiplier = engine::gHexShieldEdgeMultiplier.mfCurrent;

	rMainLayout.fHexShieldWaveMultiplier = engine::gHexShieldWaveMultiplier.mfCurrent;
	rMainLayout.fHexShieldWaveDotMultiplier = engine::gHexShieldWaveDotMultiplier.mfCurrent;
	rMainLayout.fHexShieldWaveIntensityMultiplier = engine::gHexShieldWaveIntensityMultiplier.mfCurrent;
	rMainLayout.fHexShieldWaveIntensityPower = engine::gHexShieldWaveIntensityPower.mfCurrent;
	rMainLayout.fHexShieldWaveFalloffPower = engine::gHexShieldWaveFalloffPower.mfCurrent;

	rMainLayout.fHexShieldDirectionFalloffPower = engine::gHexShieldDirectionFalloffPower.mfCurrent;
	rMainLayout.fHexShieldDirectionMultiplier = engine::gHexShieldDirectionMultiplier.mfCurrent;
}

void RenderFrameMain(int64_t iCommandBuffer, const std::unordered_map<GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<GridCoord>& rActiveCoordinates, GridCoord cameraCoordinate, std::chrono::duration<float> currentTime)
{
	if (rActiveCoordinates.empty() || rRenderInterpolates.find(cameraCoordinate) == rRenderInterpolates.end())
	{
		// Active-set computation and reset include the client cell, and boot prerender seeds the origin; the empty-set check is defensive.
		// A missing camera interpolate can coexist with other renderable cells. Begin/end publication without per-cell Render clears indirect counts and per-transaction profile counters; collection BeginRender skips missing interpolates.
		// Return before accessing the absent camera interpolate.
		game::FrameInterpolate::BeginRender(iCommandBuffer, rRenderInterpolates, rActiveCoordinates);
		game::FrameInterpolate::EndRender(iCommandBuffer);
		RenderLightingSpreadIndirect(iCommandBuffer);
		if constexpr (kbDebugRender)
		{
			DebugRender::BeginRender(iCommandBuffer);
			DebugRender::EndRender(iCommandBuffer);
		}
		return;
	}

	const game::FrameInterpolate& rCameraInterpolate = rRenderInterpolates.at(cameraCoordinate);
	shaders::MainLayout& rMainLayout = *reinterpret_cast<shaders::MainLayout*>(&gpBufferManager->mMainLayoutUniformBuffers.at(iCommandBuffer).mpMappedMemory[0]);
	GraphicsQualityLevel eWaterLevel = static_cast<GraphicsQualityLevel>(std::clamp(gWaterLevel.Get<int64_t>(), 0i64, static_cast<int64_t>(GraphicsQualityLevel::kCount) - 1));

	RenderLightingMain(iCommandBuffer);
	gpBufferManager->ResetSkinningAllocations(iCommandBuffer);

	// Per-frame visible-area LOD draw params for water. The water pipeline binds a single concat
	// mesh buffer holding all LODs; per-frame we tell vkCmdDrawIndexedIndirect which LOD's index
	// range and vertex base to draw. engine::Camera computes miVisibleAreaLevelOfDetail from eye distance with 4×
	// hysteresis bands; mesh density and snap-grid are in lockstep. Terrain draws via one
	// vkCmdDrawIndexedIndirect per island template in CommandBufferRecordMain.cpp.
	int64_t iLevelOfDetail = engine::gpCamera->miVisibleAreaLevelOfDetail;
	const BufferManager::VisibleAreaMeshLod& rWaterLevelOfDetail = gpBufferManager->mWaterMeshLods[iLevelOfDetail];
	gpPipelineManager->mpPipelines[kPipelineWater].WriteIndirectBuffer(iCommandBuffer, 1, rWaterLevelOfDetail.iIndexCount, rWaterLevelOfDetail.iIndexOffset, rWaterLevelOfDetail.iVertexOffset);

	// Active LOD's vertex-grid dims (iQuadCount* == iMeshX/Y - 1). Read by:
	//   1) WaterDisplacement.comp — bounds-checks each thread, only writes the top-left rectangle.
	//   2) Water.vert — scales f2InTexcoord to the matching texel index via texelFetch.
	// Both shaders must read the SAME values; populating once here keeps them in lockstep.
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

	game::FrameInterpolate::BeginRender(iCommandBuffer, rRenderInterpolates, rActiveCoordinates);

	// Render the camera cell first to keep its index at zero.
	auto RenderFrame = [&](const GridCoord& rCoordinate)
	{
		auto it = rRenderInterpolates.find(rCoordinate);
		if (it == rRenderInterpolates.end())
		{
			return;
		}
		const game::FrameInterpolate& rFrameInterpolate = it->second;
		// Automatically dispatched main collections
		game::FrameInterpolate::Render(rFrameInterpolate, iCommandBuffer);
		// Manually rendered trail collections
		SmokeTrailsInterpolate::Render(rFrameInterpolate, iCommandBuffer);
		WindTrailsInterpolate::Render(rFrameInterpolate, iCommandBuffer);
	};
	RenderFrame(cameraCoordinate);
	for (const GridCoord& rCoordinate : rActiveCoordinates)
	{
		if (rCoordinate == cameraCoordinate)
		{
			continue;
		}
		RenderFrame(rCoordinate);
	}

	game::FrameInterpolate::EndRender(iCommandBuffer);

	// EndRender publishes deposit indirect counts before spread indirect parameters are published.
	RenderLightingSpreadIndirect(iCommandBuffer);

	// Phase 4: Game-specific debug rendering (per-coord, positions from fully-interpolated frame)
	if constexpr (kbDebugRender)
	{
		for (const GridCoord& rCoordinate : rActiveCoordinates)
		{
			auto it = rRenderInterpolates.find(rCoordinate);
			if (it != rRenderInterpolates.end())
			{
				game::PlayersInterpolate::DebugRender(it->second, rCoordinate);
			}
		}
	}

	DebugRenderNavigationData(rActiveCoordinates, cameraCoordinate);
	DebugRenderFrameEdges(rActiveCoordinates, cameraCoordinate);
	DebugRenderIslandBoundaries(rActiveCoordinates, cameraCoordinate);
	DebugRenderIslandValidArea(rActiveCoordinates, cameraCoordinate);

	DebugRender::BeginRender(iCommandBuffer);
	DebugRender::EndRender(iCommandBuffer);

	const game::FrameInterpolate& rFrameInterpolate = rCameraInterpolate;

	rMainLayout.iFrameNumber = static_cast<int>(engine::gpCamera->miFrame);
	rMainLayout.iRenderNumber = static_cast<int32_t>(gpGraphics->miFrameCounter);

	float fCameraShake = engine::gpCamera->mfShake;
	static constexpr float kfMaxRoll = 0.005f;
	static constexpr float kfMaxPitch = 0.005f;
	static constexpr float kfMaxYaw = 0.01f;
	// Fixed-seed permutation tables are identical every frame — construct once (each ctor reshuffles a 256-entry table).
	static const siv::BasicPerlinNoise<float> sPerlinRoll(0);
	static const siv::BasicPerlinNoise<float> sPerlinPitch(1);
	static const siv::BasicPerlinNoise<float> sPerlinYaw(2);
	auto matCameraShake = XMMatrixRotationRollPitchYaw(kfMaxRoll * fCameraShake * (-1.0f + 2.0f * sPerlinRoll.octave1D_01(8.0f * rFrameInterpolate.fCurrentTime, 4)), kfMaxPitch * fCameraShake * (-1.0f + 2.0f * sPerlinPitch.octave1D_01(8.0f * rFrameInterpolate.fCurrentTime, 4)), kfMaxYaw * fCameraShake * (-1.0f + 2.0f * sPerlinYaw.octave1D_01(8.0f * rFrameInterpolate.fCurrentTime, 4)));

	XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(&rMainLayout.f4x4ViewProjection[0]), XMMatrixTranspose(XMMatrixMultiply(engine::gpCamera->mMatView, XMMatrixMultiply(matCameraShake, engine::gpCamera->mMatPerspective))));

	XMStoreFloat4(&rMainLayout.f4EyePosition, engine::gpCamera->mVecEyePosition);
	XMVECTOR vecToEyeNormal = engine::gpCamera->mVecToEyeNormal;
	XMStoreFloat4(&rMainLayout.f4ToEyeNormal, vecToEyeNormal);
	// DebugRenderBillboard.vert consumes this basis for every vertex; forward remains f4ToEyeNormal.
	// The reference axis switches from +Z to +X at |forward.z| = 0.999; right is normalized and up = cross(forward, right) is not.
	XMVECTOR vecBillboardWorldUpNormal = std::abs(XMVectorGetZ(vecToEyeNormal)) < 0.999f ? XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f) : XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
	XMVECTOR vecBillboardRightNormal = XMVector3Normalize(XMVector3Cross(vecBillboardWorldUpNormal, vecToEyeNormal));
	XMStoreFloat4(&rMainLayout.f4BillboardRight, vecBillboardRightNormal);
	XMStoreFloat4(&rMainLayout.f4BillboardUp, XMVector3Cross(vecToEyeNormal, vecBillboardRightNormal));

	PopulateGerstnerWaves(rMainLayout, currentTime.count(), eWaterLevel);

	PopulateHexShield(rMainLayout);
}

} // namespace engine

#endif // defined(BT_CLIENT)
