#include "Pch.h"

#include "Blasters.h"

#include "Data/Audio.h"
#include "Frame/CellStaticData.h"

#include "Frame/HealthDamage.h"
#include "Frame/TerrainUtils.h"
#if defined(BT_CLIENT)
#include "Data/Texture.h"
#include "Frame/Collections/PointLights/PointLights.h"
#include "Frame/Collections/Puffs/Puffs.h"
#include "Ui/LightingWrappers.h"
#include "Ui/SmokeWrappers.h"
#include "Ui/SoundWrappers.h"
#endif

namespace game
{

using enum BlasterFlags;

// Collision layer index (set each frame in PreCollision)
// thread_local: parallel per-Frame tick via Dispatch
static thread_local int64_t siCollisionLayerIndex = 0;
static thread_local std::vector<engine::CollisionFlags_t> sCollisionFlags;
static thread_local std::vector<float> sCollisionRadii;
static thread_local std::vector<float> sCollisionDamages;

struct BlasterCollisionIntervalScratch
{
	std::vector<float> startTimes;
	std::vector<float> endTimes;
	std::vector<float> maxTimes;
	std::vector<engine::SegmentHit> terrainHits;
	std::vector<engine::SegmentHit> boundaryHits;
};

static BlasterCollisionIntervalScratch& GetBlasterCollisionIntervalScratch()
{
	// Function-local TLS is constructed allocation-free on first use, including before allocator startup; growth sites suppress allocation tracking.
	static thread_local BlasterCollisionIntervalScratch sScratch;
	return sScratch;
}

constexpr float kfBlasterCollisionRadius = 0.5f;

constexpr float kfTerrainImpactJitter = 0.25f;

#if defined(BT_CLIENT)
static int64_t siTerrainCraterTypeIndex = 0xFF;
static int64_t siTerrainCraterControllerIndex = 0xFF;
static int64_t siTerrainPuffTypeIndex = 0xFF;
static int64_t siTerrainPuffControllerIndex = 0xFF;

constexpr std::chrono::duration<float> kTerrainCraterTimeOne = 0.1s;
constexpr std::chrono::duration<float> kTerrainCraterTimeTwo = 3s;
constexpr std::chrono::duration<float> kTerrainCraterTimeThree = 5.1s;

constexpr std::chrono::duration<float> kTerrainPuffTime = 0.15s;
constexpr float kfTerrainPuffRotationEnd = 10.0f;

void BlastersInterpolate::Register()
{
	if (siTerrainCraterTypeIndex != 0xFF)
	{
		return;
	}

	engine::PointLightsInterpolate::RegisterType(siTerrainCraterTypeIndex,
	{
		.uiCrc = data::kTexturesBlasterBC7TerrainImpactpngCrc,
		.uiColor = 0xFFFFFFFF,
	});

	engine::PointLightsInterpolate::RegisterControllerType(siTerrainCraterControllerIndex,
	{
		.iBaseTypeIndex = siTerrainCraterTypeIndex,
		.iKeyframeCount = 4,
		.bDestroysSelf = true,
		.times = {0s, kTerrainCraterTimeOne, kTerrainCraterTimeTwo, kTerrainCraterTimeThree},
		.keyframes =
		{
			{.fVisibleArea = 1.0f, .fVisibleIntensity = 1.0f, .fLightingArea = 1.0f, .fLightingIntensity = 1.0f, .fRotation = 0.0f},
			{.fVisibleArea = 1.0f, .fVisibleIntensity = 1.0f, .fLightingArea = 1.0f, .fLightingIntensity = 1.0f, .fRotation = 0.0f},
			{.fVisibleArea = 1.0f, .fVisibleIntensity = 1.0f, .fLightingArea = 1.0f, .fLightingIntensity = 1.0f, .fRotation = 0.0f},
			{.fVisibleArea = 1.0f, .fVisibleIntensity = 1.0f, .fLightingArea = 1.0f, .fLightingIntensity = 1.0f, .fRotation = 0.0f},
		},
		.ppVisibleAreaScales = {&gCraterVisibleAreaOne, &gCraterVisibleAreaTwo, &gCraterVisibleAreaThree, &gCraterVisibleAreaFour},
		.ppVisibleIntensityScales = {&gCraterVisibleIntensityOne, &gCraterVisibleIntensityTwo, &gCraterVisibleIntensityThree, &gCraterVisibleIntensityFour},
		.ppLightingAreaScales = {&gCraterLightingAreaOne, &gCraterLightingAreaTwo, &gCraterLightingAreaThree, &gCraterLightingAreaFour},
		.ppLightingIntensityScales = {&gCraterLightingIntensityOne, &gCraterLightingIntensityTwo, &gCraterLightingIntensityThree, &gCraterLightingIntensityFour},
	});

	engine::PuffsInterpolate::RegisterType(siTerrainPuffTypeIndex,
	{
		.uiCrc = data::kTexturesSmokeBC44jpgCrc,
		.uiColor = 0xFFFFFFFF,
	});

	engine::PuffsInterpolate::RegisterControllerType(siTerrainPuffControllerIndex,
	{
		.iBaseTypeIndex = siTerrainPuffTypeIndex,
		.iKeyframeCount = 2,
		.bDestroysSelf = true,
		.times = {0s, kTerrainPuffTime, 0s, 0s},
		.keyframes =
		{
			{.fArea = 1.0f, .fIntensity = 1.0f, .fRotation = 0.0f},
			{.fArea = 1.0f, .fIntensity = 1.0f, .fRotation = kfTerrainPuffRotationEnd},
			{},
			{},
		},
		.ppAreaScales = {&gBlasterPuffAreaStart, &gBlasterPuffAreaEnd, nullptr, nullptr},
		.ppIntensityScales = {&gBlasterPuffIntensityStart, &gBlasterPuffIntensityEnd, nullptr, nullptr},
	});
}

static void XM_CALLCONV SynchronizeBlaster(FrameInterpolate& rFrameInterpolate, engine::area_lights_t areaLight, engine::point_lights_t pointLight, FXMVECTOR vecPosition, FXMVECTOR vecVelocity, int64_t iTypeIndex)
{
	const BlastersType& rType = BlastersInterpolate::sTypes.at(static_cast<size_t>(iTypeIndex));

	if ((pointLight.uuid.iValue != 0))
	{
		float fSize = rType.f2Size.x;
		const engine::PointLightsType& rPointLightType = engine::PointLightsInterpolate::sTypes.at(static_cast<size_t>(rType.iPointLightTypeIndex));
		engine::PointLightsInterpolate::Sync(rFrameInterpolate, pointLight,
		{
			.vecPosition = vecPosition,
			.fVisibleArea = fSize,
			.fVisibleIntensity = rPointLightType.pVisibleIntensityWrapper != nullptr ? rPointLightType.pVisibleIntensityWrapper->mfCurrent : rPointLightType.fVisibleIntensity,
			.fLightingArea = rPointLightType.pLightingAreaWrapper != nullptr ? rPointLightType.pLightingAreaWrapper->mfCurrent : rPointLightType.fLightingArea,
			.fLightingIntensity = rPointLightType.pLightingIntensityWrapper != nullptr ? rPointLightType.pLightingIntensityWrapper->mfCurrent : rPointLightType.fLightingIntensity,
			.fRotation = 0.0f,
		});
	}
	else
	{
		float fWidth = rType.f2Size.x;
		float fLength = rType.f2Size.y;

		XMVECTOR vecDirection = XMVector3Normalize(vecVelocity);
		auto [vecTopLeft, vecTopRight, vecBottomLeft, vecBottomRight] = common::CalculateArea(vecPosition, vecDirection, fLength, fLength, fWidth);

		engine::AreaLightsInterpolate::Sync(rFrameInterpolate, areaLight,
		{
			.iTypeIndex = rType.iAreaLightTypeIndex,
			.vecVisiblePositions = {vecTopLeft, vecTopRight, vecBottomLeft, vecBottomRight},
		});
	}
}
#endif // BT_CLIENT

void BlastersInterpolate::Update([[maybe_unused]] FrameInterpolate& __restrict rCurrentFrameInterpolate, [[maybe_unused]] const Frame& __restrict rPreviousFrame)
{
	BlastersInterpolate& rCurrent = *rCurrentFrameInterpolate.pBlasters;
	const BlastersInterpolate& rPrevious = *rPreviousFrame.interpolate.pBlasters;
	const BlastersPostRender& rPreviousPostRender = *rPreviousFrame.postRender.pBlasters;
	std::chrono::duration<float> deltaTime(rCurrentFrameInterpolate.fDeltaTime);

	for (int64_t i = 0; i < rCurrent.iCount; ++i)
	{
		// Load (type index copied in AllocateAndCopy)
		[[maybe_unused]] int64_t iTypeIndex = rCurrent.puiTypeIndices[i];
		XMVECTOR vecVelocity = rPreviousPostRender.pVecVelocities[i];

		XMVECTOR vecPosition = XMVectorMultiplyAdd(XMVectorReplicate(deltaTime.count()), vecVelocity, rPrevious.pVecPositions[i]);
		// Positions must always have W=1.0 — prevents W-lane drift via MultiplyAdd.
		vecPosition = XMVectorSetW(vecPosition, 1.0f);

		XMVECTOR vecDirection = XMVector3Normalize(vecVelocity);

		rCurrent.pVecPositions[i] = vecPosition;
		rCurrent.pVecDirections[i] = vecDirection;

#if defined(BT_CLIENT)
		SynchronizeBlaster(rCurrentFrameInterpolate, rCurrent.pAreaLights[i], rCurrent.pPointLights[i], vecPosition, vecVelocity, iTypeIndex);

		if ((rCurrent.pWindTrails[i].uuid.iValue != 0))
		{
			engine::WindTrailsInterpolate::Sync(rCurrentFrameInterpolate, rCurrent.pWindTrails[i],
			{
				.vecPosition = vecPosition,
				.fIntensity = rCurrent.pfWindTrailIntensities[i],
				.fWidth = rCurrent.pfWindTrailWidths[i],
				.fLengthMultiplier = rCurrent.pfWindTrailLengthMultipliers[i],
			});
		}
#endif // BT_CLIENT
	}
}

void BlastersPostRender::Update([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const Frame& __restrict rPreviousFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
}

void BlastersPostRender::PreCollision([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const Frame& __restrict rPreviousFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	BlasterCollisionIntervalScratch& rCollisionScratch = GetBlasterCollisionIntervalScratch();
	// Heap: static vectors resized each frame, only allocates on first call or when count grows (capacity retained).
	// .data() pointers are passed to AddLayer and must survive until PostCollision, so workbuffer can't be used
	ScopedSuppressAllocationTracking suppress;

	BlastersInterpolate& rCurrentInterpolate = *rFrame.interpolate.pBlasters;
	BlastersPostRender& rCurrentPostRender = *rFrame.postRender.pBlasters;

	if (rCurrentInterpolate.iCount == 0)
	{
		return;
	}

	int64_t iCount = rCurrentInterpolate.iCount;
	sCollisionFlags.resize(static_cast<size_t>(iCount));
	sCollisionRadii.resize(static_cast<size_t>(iCount));
	sCollisionDamages.resize(static_cast<size_t>(iCount));
	rCollisionScratch.startTimes.resize(static_cast<size_t>(iCount));
	rCollisionScratch.endTimes.resize(static_cast<size_t>(iCount));
	rCollisionScratch.maxTimes.resize(static_cast<size_t>(iCount));
	rCollisionScratch.terrainHits.resize(static_cast<size_t>(iCount));
	rCollisionScratch.boundaryHits.resize(static_cast<size_t>(iCount));
	const BlastersInterpolate& rPreviousInterpolate = *rPreviousFrame.interpolate.pBlasters;
	for (int64_t i = 0; i < rCurrentInterpolate.iCount; ++i)
	{
		int64_t iIndex = i;
		sCollisionFlags.at(static_cast<size_t>(iIndex)) = engine::CollisionFlags::kDestroyOnCollide;
		sCollisionRadii.at(static_cast<size_t>(iIndex)) = kfBlasterCollisionRadius;
		sCollisionDamages.at(static_cast<size_t>(iIndex)) = kfBlasterDamage;
		rCollisionScratch.startTimes.at(static_cast<size_t>(iIndex)) = 0.0f;
		rCollisionScratch.endTimes.at(static_cast<size_t>(iIndex)) = 1.0f;
		rCollisionScratch.terrainHits.at(static_cast<size_t>(iIndex)) = engine::TracePointAgainstTerrain(rStaticData, rPreviousInterpolate.pVecPositions[i], rCurrentInterpolate.pVecPositions[i], 0.0f, 1.0f);
		rCollisionScratch.boundaryHits.at(static_cast<size_t>(iIndex)) = engine::TracePointToCellExit(engine::LocalCellArea(), rPreviousInterpolate.pVecPositions[i], rCurrentInterpolate.pVecPositions[i], 0.0f, 1.0f);
		float fMaxTime = std::numeric_limits<float>::max();
		if (rCollisionScratch.terrainHits.at(static_cast<size_t>(iIndex)).bHit)
		{
			fMaxTime = rCollisionScratch.terrainHits.at(static_cast<size_t>(iIndex)).fTime;
		}
		if (rCollisionScratch.boundaryHits.at(static_cast<size_t>(iIndex)).bHit)
		{
			fMaxTime = std::min(fMaxTime, rCollisionScratch.boundaryHits.at(static_cast<size_t>(iIndex)).fTime);
		}
		rCollisionScratch.maxTimes.at(static_cast<size_t>(iIndex)) = fMaxTime;
	}

	siCollisionLayerIndex = engine::Collision::AddLayer(
	{
		.pVecStartPositions = rPreviousInterpolate.pVecPositions,
		.pVecEndPositions = rCurrentInterpolate.pVecPositions,
		.pfStartTimes = rCollisionScratch.startTimes.data(),
		.pfEndTimes = rCollisionScratch.endTimes.data(),
		.pfMaxTimes = rCollisionScratch.maxTimes.data(),
		.pfRadii = sCollisionRadii.data(),
		.pfDamages = sCollisionDamages.data(),
		.pFlags = sCollisionFlags.data(),
		.pVecVelocities = rCurrentPostRender.pVecVelocities,
		.iCount = rCurrentInterpolate.iCount,
		.bSweptTest = true,
		.uiCategory = CollisionCategory::kuiBlaster,
		.uiCollidesWith = CollidesWith::kuiBlaster,
		.pAlignments = rCurrentPostRender.pAlignments,
	});
}

void BlastersPostRender::PostCollision([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const Frame& __restrict rPreviousFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	BlasterCollisionIntervalScratch& rCollisionScratch = GetBlasterCollisionIntervalScratch();
	BlastersInterpolate& rCurrentInterpolate = *rFrame.interpolate.pBlasters;
	BlastersPostRender& rCurrentPostRender = *rFrame.postRender.pBlasters;

	if (rCurrentInterpolate.iCount == 0)
	{
		return;
	}

	for (int64_t i = 0; i < rCurrentInterpolate.iCount; ++i)
	{
		int64_t iIndex = i;
		// Entity results are pre-filtered against terrain and cell-exit cutoffs.
		if ((engine::Collision::sResultSpans[engine::Collision::sLayerBaseOffsets[siCollisionLayerIndex] + i].iCount > 0))
		{
			rCurrentPostRender.pFlags[i].Set(kDestroy);
			continue;
		}

		const engine::SegmentHit& rTerrainHit = rCollisionScratch.terrainHits.at(static_cast<size_t>(iIndex));
		const engine::SegmentHit& rBoundaryHit = rCollisionScratch.boundaryHits.at(static_cast<size_t>(iIndex));
		if (rTerrainHit.bHit && (!rBoundaryHit.bHit || rTerrainHit.fTime <= rBoundaryHit.fTime)) [[unlikely]]
		{
			rCurrentPostRender.pFlags[i].Set(kDestroy);
			XMVECTOR vecCollisionPosition = rTerrainHit.vecPosition;

			// Add jitter for visual variety
			vecCollisionPosition = common::RandomPositionJitter<kfTerrainImpactJitter>(vecCollisionPosition, rFrame.postRender.randomEngine);

			[[maybe_unused]] float fRotation = common::Random<XM_2PI>(rFrame.postRender.randomEngine);
#if defined(BT_CLIENT)
			engine::PointLightsPostRender::AddControlled(rFrame, std::chrono::duration<float>(rFrame.interpolate.fCurrentTime), siTerrainCraterControllerIndex, vecCollisionPosition, fRotation);
			engine::PuffsPostRender::AddControlled(rFrame, std::chrono::duration<float>(rFrame.interpolate.fCurrentTime), siTerrainPuffControllerIndex, vecCollisionPosition);
#endif

#if defined(BT_CLIENT)
			engine::gpAudioManager->PlayOneShot3d(rFrame, data::kAudioBlaster16793__pushtobreak__earth1wavCrc, rStaticData.coordinate, vecCollisionPosition, gTerrainImpactVolume.mfCurrent);
#endif
		}
		else if (rBoundaryHit.bHit) [[unlikely]]
		{
			rCurrentPostRender.pFlags[i].Set(kTransfer);
		}
	}
}

} // namespace game
