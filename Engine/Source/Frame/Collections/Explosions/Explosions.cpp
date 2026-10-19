#include "Explosions.h"

#if defined(BT_CLIENT)
#include "Data/Texture.h"
#include "Profile/ProfileRows.h"
#include "Ui/WrapperBase.h"
#include "Ui/LightingWrappers.h"
#include "Ui/SmokeWrappers.h"
#include "Graphics/Managers/ParticleManager.h"
#include "Frame/Collections/PointLights/PointLights.h"
#include "Frame/Collections/Puffs/Puffs.h"
#include "Frame/Collections/SmokeTrails/SmokeTrails.h"
#include "Frame/Collections/WindRadials/WindRadials.h"
#endif // BT_CLIENT

namespace engine
{

template struct Collection<ExplosionsInterpolate>;
template struct Collection<ExplosionsPostRender>;

#if defined(BT_CLIENT)

constexpr std::chrono::duration<float> kWindDepositDuration = std::chrono::duration<float>(0.3f);

constexpr std::chrono::duration<float> kPrimaryTime = std::chrono::duration<float>(0.06f);

constexpr std::chrono::duration<float> kPrimaryPuffStartTime = std::chrono::duration<float>::zero();
constexpr std::chrono::duration<float> kPrimaryPuffEndTime = std::chrono::duration<float>(0.2f);
constexpr std::chrono::duration<float> kSecondaryPuffTimes = 0.4f * (kPrimaryPuffEndTime - kPrimaryPuffStartTime);

constexpr float kfExplosionTrailWidth = 1.0f;

static int64_t siExplosionPointLightTypeIndex = kiInvalidControllerType;
static int64_t siExplosionPuffTypeIndex = kiInvalidControllerType;

void XM_CALLCONV SyncExplosionTrail(game::FrameInterpolate& rFrameInterpolate, smoke_trails_t trailId, FXMVECTOR vecPosition, float fIntensity)
{
	if (!(trailId.uuid.iValue != 0))
	{
		return;
	}

	SmokeTrailsInterpolate::Sync(rFrameInterpolate, trailId,
	{
		.vecPosition = vecPosition,
		.fIntensity = fIntensity,
	});
}

#endif // BT_CLIENT

void ExplosionsInterpolate::Register()
{
#if defined(BT_CLIENT)
	if (siExplosionPointLightTypeIndex != kiInvalidControllerType)
	{
		return;
	}

	PointLightsInterpolate::RegisterType(siExplosionPointLightTypeIndex,
	{
		.uiCrc = data::kTexturesBC7ExplosionpngCrc,
		.uiColor = 0xFFFFFFFF,
		.fVisibleArea = game::gExplosionPrimaryVisibleAreaOne.mfCurrent,
		.fVisibleIntensity = game::gExplosionPrimaryVisibleIntensityOne.mfCurrent,
		.fLightingArea = game::gExplosionPrimaryLightingAreaOne.mfCurrent,
		.fLightingIntensity = game::gExplosionPrimaryLightingIntensityOne.mfCurrent,
	});

	// Keyframes are normalized; wrappers supply their magnitudes.
	PointLightsInterpolate::RegisterControllerType(giExplosionPrimaryLightControllerTypeIndex,
	{
		.iBaseTypeIndex = siExplosionPointLightTypeIndex,
		.iKeyframeCount = 3,
		.bDestroysSelf = true,
		.pTimes = {std::chrono::duration<float>(0.0f), 0.4f * kPrimaryTime, 3.0f * kPrimaryTime, std::chrono::duration<float>(0.0f)},
		.pKeyframes =
		{
			{.fVisibleArea = 0.3f, .fVisibleIntensity = 0.25f, .fLightingArea = 0.6f, .fLightingIntensity = 0.25f, .fRotation = 0.0f},
			{.fVisibleArea = 0.6f, .fVisibleIntensity = 1.0f, .fLightingArea = 1.5f, .fLightingIntensity = 1.0f, .fRotation = 0.0f},
			{.fVisibleArea = 0.6f, .fVisibleIntensity = 1.0f, .fLightingArea = 1.2f, .fLightingIntensity = 1.0f, .fRotation = 0.0f},
			{},
		},
		.ppVisibleAreaScales = {&game::gExplosionPrimaryVisibleAreaOne, &game::gExplosionPrimaryVisibleAreaTwo, &game::gExplosionPrimaryVisibleAreaThree, nullptr},
		.ppVisibleIntensityScales = {&game::gExplosionPrimaryVisibleIntensityOne, &game::gExplosionPrimaryVisibleIntensityTwo, &game::gExplosionPrimaryVisibleIntensityThree, nullptr},
		.ppLightingAreaScales = {&game::gExplosionPrimaryLightingAreaOne, &game::gExplosionPrimaryLightingAreaTwo, &game::gExplosionPrimaryLightingAreaThree, nullptr},
		.ppLightingIntensityScales = {&game::gExplosionPrimaryLightingIntensityOne, &game::gExplosionPrimaryLightingIntensityTwo, &game::gExplosionPrimaryLightingIntensityThree, nullptr},
	});

	PointLightsInterpolate::RegisterControllerType(giExplosionSecondaryLightControllerTypeIndex,
	{
		.iBaseTypeIndex = siExplosionPointLightTypeIndex,
		.iKeyframeCount = 3,
		.bDestroysSelf = true,
		.pTimes = {std::chrono::duration<float>(0.0f), 1.0f * kPrimaryTime, 3.0f * kPrimaryTime, std::chrono::duration<float>(0.0f)},
		.pKeyframes =
		{
			{.fVisibleArea = 1.0f, .fVisibleIntensity = 1.0f, .fLightingArea = 1.0f, .fLightingIntensity = 1.0f, .fRotation = 0.0f},
			{.fVisibleArea = 1.0f, .fVisibleIntensity = 1.0f, .fLightingArea = 2.0f, .fLightingIntensity = 1.0f, .fRotation = 0.0f},
			{.fVisibleArea = 1.0f, .fVisibleIntensity = 1.0f, .fLightingArea = 1.0f, .fLightingIntensity = 1.0f, .fRotation = 0.0f},
			{},
		},
		.ppVisibleAreaScales = {&game::gExplosionSecondaryVisibleAreaOne, &game::gExplosionSecondaryVisibleAreaTwo, &game::gExplosionSecondaryVisibleAreaThree, nullptr},
		.ppVisibleIntensityScales = {&game::gExplosionSecondaryVisibleIntensityOne, &game::gExplosionSecondaryVisibleIntensityTwo, &game::gExplosionSecondaryVisibleIntensityThree, nullptr},
		.ppLightingAreaScales = {&game::gExplosionSecondaryLightingAreaOne, &game::gExplosionSecondaryLightingAreaTwo, &game::gExplosionSecondaryLightingAreaThree, nullptr},
		.ppLightingIntensityScales = {&game::gExplosionSecondaryLightingIntensityOne, &game::gExplosionSecondaryLightingIntensityTwo, &game::gExplosionSecondaryLightingIntensityThree, nullptr},
	});

	PuffsInterpolate::RegisterType(siExplosionPuffTypeIndex,
	{
		.uiCrc = 0,
		.uiColor = 0xFFFFFFFF,
	});

	PuffsInterpolate::RegisterControllerType(giExplosionPrimaryPuffControllerTypeIndex,
	{
		.iBaseTypeIndex = siExplosionPuffTypeIndex,
		.iKeyframeCount = 2,
		.bDestroysSelf = true,
		.pTimes = {kPrimaryPuffStartTime, kPrimaryPuffEndTime, std::chrono::duration<float>(0.0f), std::chrono::duration<float>(0.0f)},
		.pKeyframes =
		{
			{.fArea = 1.0f, .fIntensity = 1.0f, .fRotation = 0.0f},
			{.fArea = 1.0f, .fIntensity = 1.0f, .fRotation = 0.0f},
			{},
			{},
		},
		.ppAreaScales = {&game::gExplosionPrimaryPuffAreaOne, &game::gExplosionPrimaryPuffAreaTwo, nullptr, nullptr},
		.ppIntensityScales = {&game::gExplosionPrimaryPuffIntensityOne, &game::gExplosionPrimaryPuffIntensityTwo, nullptr, nullptr},
	});

	PuffsInterpolate::RegisterControllerType(giExplosionSecondaryPuffControllerTypeIndex,
	{
		.iBaseTypeIndex = siExplosionPuffTypeIndex,
		.iKeyframeCount = 2,
		.bDestroysSelf = true,
		.pTimes = {std::chrono::duration<float>(0.0f), kSecondaryPuffTimes, std::chrono::duration<float>(0.0f), std::chrono::duration<float>(0.0f)},
		.pKeyframes =
		{
			{.fArea = 1.0f, .fIntensity = 1.0f, .fRotation = 0.0f},
			{.fArea = 1.0f, .fIntensity = 1.0f, .fRotation = 0.0f},
			{},
			{},
		},
		.ppAreaScales = {&game::gExplosionSecondaryPuffAreaOne, &game::gExplosionSecondaryPuffAreaTwo, nullptr, nullptr},
		.ppIntensityScales = {&game::gExplosionSecondaryPuffIntensityOne, &game::gExplosionSecondaryPuffIntensityTwo, nullptr, nullptr},
	});

	SmokeTrailsInterpolate::RegisterType(siExplosionTrailTypeIndex,
	{
		.uiCrc = 0,
		.iColor = 0xFFFFFFFF,
		.fWidth = kfExplosionTrailWidth,
	});

	WindRadialsInterpolate::RegisterControllerType(giExplosionWindRadialControllerTypeIndex,
	{
		.iKeyframeCount = 2,
		.bDestroysSelf = true,
		.pTimes = {std::chrono::duration<float>(0.0f), kWindDepositDuration, std::chrono::duration<float>(0.0f), std::chrono::duration<float>(0.0f)},
		.pKeyframes = {{.fIntensity = 1.0f, .fSize = 1.0f}, {.fIntensity = 0.0f, .fSize = 1.0f}, {}, {}},
	});
#endif // BT_CLIENT
}

#if defined(BT_CLIENT)

#endif // BT_CLIENT

void ExplosionsPostRender::Destroy(game::Frame& __restrict rFrame, [[maybe_unused]] const CellStaticData& rStaticData)
{
	ExplosionsInterpolate& rInterpolate = rFrame.interpolate.explosions;
	ExplosionsPostRender& rPostRender = rFrame.postRender.explosions;

	SimulationTime currentTime = TickToSimulationTime(rFrame.interpolate.iTick);

	for (int64_t i = 0; i < rInterpolate.iCount; ++i)
	{
		int64_t iTypeIndex = rInterpolate.puiTypeIndices[i];
		const ExplosionType& rType = ExplosionsInterpolate::sTypes.at(static_cast<size_t>(iTypeIndex));

		SimulationTime startTime = rInterpolate.pStartTimes[i];
		std::chrono::nanoseconds explosionTime = currentTime - startTime;
		float fTimePercent = rInterpolate.pfTimePercents[i];

		int64_t iTrailCount = rInterpolate.piTrailCounts[i];

#if defined(BT_CLIENT)
		// Release expired trails (cleanup happens every frame, not just at explosion expiration).
		// Cleanup uses unmultiplied pTrailTimes so it fires in lockstep with the shared explosion-entry
		// destruction below — applying the client-only Duration multiplier here would let the entry be
		// destroyed first, and the compaction fold would then release its trails before they end.
		for (int64_t j = 0; j < iTrailCount; ++j)
		{
			std::chrono::duration<float> trailEndTime = fTimePercent * rType.trailDelayTime + rInterpolate.pTrailTimes[j][i];
			if (explosionTime >= common::FloatSecondsToNanoseconds(trailEndTime))
			{
				ReleaseIndexableElementAndClearHandle(rFrame, rInterpolate.pTrails[j][i]);
			}
		}
#endif // BT_CLIENT

		if (!(rPostRender.pLifecycleFlags[i] & LifecycleFlags::kDying))
		{
			continue;
		}

		std::chrono::nanoseconds endTime = 0ns;
		for (int64_t j = 0; j < iTrailCount; ++j)
		{
			std::chrono::duration<float> trailEndTime = fTimePercent * rType.trailDelayTime + rInterpolate.pTrailTimes[j][i];
			endTime = std::max(endTime, common::FloatSecondsToNanoseconds(trailEndTime));
		}

		if (explosionTime < endTime)
		{
			continue;
		}

		rPostRender.pLifecycleFlags[i].Set(LifecycleFlags::kDestroy);
	}
}

bool ExplosionsInterpolate::LogDifferences(const ExplosionsInterpolate& rOther) const
{
	common::ScopedLogDifferenceContext context("ExplosionsInterpolate");
	bool bEqual = true;
	bEqual &= Collection::LogDifferences(rOther);

	for (int64_t i = 0; i < std::min(iCount, rOther.iCount); ++i)
	{
		bEqual &= common::LogDifference<"puiTypeIndices">(i, puiTypeIndices[i], rOther.puiTypeIndices[i]);
		bEqual &= common::LogDifference<"pStartTimes">(i, pStartTimes[i].time_since_epoch().count(), rOther.pStartTimes[i].time_since_epoch().count());
		bEqual &= common::LogDifference<"pVecPositions">(i, pVecPositions[i], rOther.pVecPositions[i]);
		bEqual &= common::LogDifference<"pVecDirections">(i, pVecDirections[i], rOther.pVecDirections[i]);
		bEqual &= common::LogDifference<"pfTimePercents">(i, pfTimePercents[i], rOther.pfTimePercents[i]);
		bEqual &= common::LogDifference<"piTrailCounts">(i, piTrailCounts[i], rOther.piTrailCounts[i]);

		for (int64_t j = 0; j < piTrailCounts[i]; ++j)
		{
			bEqual &= common::LogDifference<"pTrailTimes">(i, pTrailTimes[j][i], rOther.pTrailTimes[j][i]);
		}
	}

	return bEqual;
}

bool ExplosionsPostRender::LogDifferences(const ExplosionsPostRender& rOther) const
{
	common::ScopedLogDifferenceContext context("ExplosionsPostRender");
	bool bEqual = true;
	bEqual &= Collection::LogDifferences(rOther);

	for (int64_t i = 0; i < std::min(iCount, rOther.iCount); ++i)
	{
		bEqual &= common::LogDifference<"pLifecycleFlags">(i, pLifecycleFlags[i], rOther.pLifecycleFlags[i]);
	}

	return bEqual;
}

#if defined(BT_CLIENT)

#endif // BT_CLIENT

} // namespace engine
