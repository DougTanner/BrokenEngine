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
		engine::PointLightsInterpolate::Sync(rFrameInterpolate, pointLight,
		{
			.vecPosition = vecPosition,
			.fVisibleArea = fSize,
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
	BlastersInterpolate& rCurrentInterpolate = *rFrame.interpolate.pBlasters;
	BlastersPostRender& rCurrentPostRender = *rFrame.postRender.pBlasters;

	if (rCurrentInterpolate.iCount == 0)
	{
		return;
	}

	siCollisionLayerIndex = engine::Collision::AddLayer(
	{
		.pVecStartPositions = rPreviousFrame.interpolate.pBlasters->pVecPositions,
		.pVecEndPositions = rCurrentInterpolate.pVecPositions,
		.pVecVelocities = rCurrentPostRender.pVecVelocities,
		.iCount = rCurrentInterpolate.iCount,
		.fRadius = kfBlasterCollisionRadius,
		.fDamage = kfBlasterDamage,
		.bSweptTest = true,
		.uiCategory = CollisionCategory::kuiBlaster,
		.uiCollidesWith = CollidesWith::kuiBlaster,
		.pAlignments = rCurrentPostRender.pAlignments,
		.pTerrainStaticData = &rStaticData,
	});

	std::span<engine::CollisionFlags_t> flags = engine::Collision::GetLayerFlags(siCollisionLayerIndex);
	for (int64_t i = 0; i < rCurrentInterpolate.iCount; ++i)
	{
		flags[static_cast<size_t>(i)] = engine::CollisionFlags::kDestroyOnCollide;
	}
}

void BlastersPostRender::PostCollision([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const Frame& __restrict rPreviousFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	BlastersInterpolate& rCurrentInterpolate = *rFrame.interpolate.pBlasters;
	BlastersPostRender& rCurrentPostRender = *rFrame.postRender.pBlasters;

	if (rCurrentInterpolate.iCount == 0)
	{
		return;
	}

	for (int64_t i = 0; i < rCurrentInterpolate.iCount; ++i)
	{
		// Entity results are pre-filtered against terrain and cell-exit cutoffs.
		engine::CollisionOutcome outcome = engine::Collision::GetOutcome(siCollisionLayerIndex, i);
		if (outcome.eType == engine::CollisionOutcomeType::kEntity)
		{
			rCurrentPostRender.pFlags[i].Set(kDestroy);
			continue;
		}

		if (outcome.eType == engine::CollisionOutcomeType::kTerrain) [[unlikely]]
		{
			rCurrentPostRender.pFlags[i].Set(kDestroy);
			XMVECTOR vecCollisionPosition = outcome.vecTerrainPosition;

			// Add jitter for visual variety
			vecCollisionPosition = common::RandomPositionJitter<kfTerrainImpactJitter>(vecCollisionPosition, rFrame.postRender.randomEngine);

			[[maybe_unused]] float fRotation = common::Random<XM_2PI>(rFrame.postRender.randomEngine);
#if defined(BT_CLIENT)
			engine::PointLightsPostRender::AddControlled(rFrame, engine::TickToSimulationTime(rFrame.interpolate.iTick), siTerrainCraterControllerIndex, vecCollisionPosition, fRotation);
			engine::PuffsPostRender::AddControlled(rFrame, engine::TickToSimulationTime(rFrame.interpolate.iTick), siTerrainPuffControllerIndex, vecCollisionPosition);
#endif

#if defined(BT_CLIENT)
			engine::gpAudioManager->PlayOneShot3d(rFrame, data::kAudioBlaster16793__pushtobreak__earth1wavCrc, rStaticData.coordinate, vecCollisionPosition, gTerrainImpactVolume.mfCurrent);
#endif
		}
		else if (outcome.eType == engine::CollisionOutcomeType::kCellExit) [[unlikely]]
		{
			rCurrentPostRender.pFlags[i].Set(kTransfer);
		}
	}
}

} // namespace game
