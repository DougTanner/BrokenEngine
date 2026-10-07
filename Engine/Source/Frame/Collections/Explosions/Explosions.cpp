#include "Explosions.h"

#if defined(BT_CLIENT)
#include "Data/Texture.h"
#include "Profile/ProfileManager.h"
#include "Ui/WrapperBase.h"
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

using enum ExplosionFlags;

#if defined(BT_CLIENT)

constexpr float kfWindDepositDuration = 0.3f;

constexpr float kfPrimaryTime = 0.06f;

constexpr float kfPrimaryPuffStartTime = 0.0f;
constexpr float kfPrimaryPuffEndTime = 0.2f;
constexpr float kfSecondaryPuffTimes = 0.4f * (kfPrimaryPuffEndTime - kfPrimaryPuffStartTime);

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

void ExplosionsInterpolate::AllocateAndCopy(ExplosionsInterpolate& rCurrent, const ExplosionsInterpolate& rPrevious)
{
	AllocateAndCopyMembers(rCurrent, rPrevious);
}

void ExplosionsPostRender::AllocateAndCopy(ExplosionsPostRender& rCurrent, const ExplosionsPostRender& rPrevious)
{
	engine::AllocateAndCopyMembers(rCurrent, rPrevious);
}

void ExplosionsInterpolate::Register()
{
#if defined(BT_CLIENT)
	if (siExplosionPointLightTypeIndex != kiInvalidControllerType)
	{
		return;
	}

	// Most of these pointers end up in controller-scale arrays that treat nullptr as "multiplier 1.0", so an
	// unfilled field would silently disable that slider instead of failing. Catch it here while it is still cheap.
	static constexpr int64_t kiTuningFieldCount = static_cast<int64_t>(sizeof(ExplosionTuning) / sizeof(Wrapper*));
	static_assert(kiTuningFieldCount == 40);
	for (const Wrapper* pTuningField : std::bit_cast<std::array<Wrapper*, static_cast<size_t>(kiTuningFieldCount)>>(sTuning))
	{
		ASSERT(pTuningField != nullptr);
	}

	PointLightsInterpolate::RegisterType(siExplosionPointLightTypeIndex,
	{
		.uiCrc = data::kTexturesBC7ExplosionpngCrc,
		.uiColor = 0xFFFFFFFF,
		.fVisibleArea = sTuning.pPrimaryVisibleAreaOne->mfCurrent,
		.fVisibleIntensity = sTuning.pPrimaryVisibleIntensityOne->mfCurrent,
		.fLightingArea = sTuning.pPrimaryLightingAreaOne->mfCurrent,
		.fLightingIntensity = sTuning.pPrimaryLightingIntensityOne->mfCurrent,
	});

	// Keyframes are normalized; wrappers supply their magnitudes.
	int64_t iPrimaryLightControllerTypeIndex = suiPrimaryLightControllerTypeIndex;
	PointLightsInterpolate::RegisterControllerType(iPrimaryLightControllerTypeIndex,
	{
		.iBaseTypeIndex = siExplosionPointLightTypeIndex,
		.iKeyframeCount = 3,
		.bDestroysSelf = true,
		.times = {std::chrono::duration<float>(0.0f), std::chrono::duration<float>(0.4f * kfPrimaryTime), std::chrono::duration<float>(3.0f * kfPrimaryTime), std::chrono::duration<float>(0.0f)},
		.keyframes =
		{
			{.fVisibleArea = 0.3f, .fVisibleIntensity = 0.25f, .fLightingArea = 0.6f, .fLightingIntensity = 0.25f, .fRotation = 0.0f},
			{.fVisibleArea = 0.6f, .fVisibleIntensity = 1.0f, .fLightingArea = 1.5f, .fLightingIntensity = 1.0f, .fRotation = 0.0f},
			{.fVisibleArea = 0.6f, .fVisibleIntensity = 1.0f, .fLightingArea = 1.2f, .fLightingIntensity = 1.0f, .fRotation = 0.0f},
			{},
		},
		.ppVisibleAreaScales = {sTuning.pPrimaryVisibleAreaOne, sTuning.pPrimaryVisibleAreaTwo, sTuning.pPrimaryVisibleAreaThree, nullptr},
		.ppVisibleIntensityScales = {sTuning.pPrimaryVisibleIntensityOne, sTuning.pPrimaryVisibleIntensityTwo, sTuning.pPrimaryVisibleIntensityThree, nullptr},
		.ppLightingAreaScales = {sTuning.pPrimaryLightingAreaOne, sTuning.pPrimaryLightingAreaTwo, sTuning.pPrimaryLightingAreaThree, nullptr},
		.ppLightingIntensityScales = {sTuning.pPrimaryLightingIntensityOne, sTuning.pPrimaryLightingIntensityTwo, sTuning.pPrimaryLightingIntensityThree, nullptr},
	});
	suiPrimaryLightControllerTypeIndex = static_cast<uint8_t>(iPrimaryLightControllerTypeIndex);

	int64_t iSecondaryLightControllerTypeIndex = suiSecondaryLightControllerTypeIndex;
	PointLightsInterpolate::RegisterControllerType(iSecondaryLightControllerTypeIndex,
	{
		.iBaseTypeIndex = siExplosionPointLightTypeIndex,
		.iKeyframeCount = 3,
		.bDestroysSelf = true,
		.times = {std::chrono::duration<float>(0.0f), std::chrono::duration<float>(1.0f * kfPrimaryTime), std::chrono::duration<float>(3.0f * kfPrimaryTime), std::chrono::duration<float>(0.0f)},
		.keyframes =
		{
			{.fVisibleArea = 1.0f, .fVisibleIntensity = 1.0f, .fLightingArea = 1.0f, .fLightingIntensity = 1.0f, .fRotation = 0.0f},
			{.fVisibleArea = 1.0f, .fVisibleIntensity = 1.0f, .fLightingArea = 2.0f, .fLightingIntensity = 1.0f, .fRotation = 0.0f},
			{.fVisibleArea = 1.0f, .fVisibleIntensity = 1.0f, .fLightingArea = 1.0f, .fLightingIntensity = 1.0f, .fRotation = 0.0f},
			{},
		},
		.ppVisibleAreaScales = {sTuning.pSecondaryVisibleAreaOne, sTuning.pSecondaryVisibleAreaTwo, sTuning.pSecondaryVisibleAreaThree, nullptr},
		.ppVisibleIntensityScales = {sTuning.pSecondaryVisibleIntensityOne, sTuning.pSecondaryVisibleIntensityTwo, sTuning.pSecondaryVisibleIntensityThree, nullptr},
		.ppLightingAreaScales = {sTuning.pSecondaryLightingAreaOne, sTuning.pSecondaryLightingAreaTwo, sTuning.pSecondaryLightingAreaThree, nullptr},
		.ppLightingIntensityScales = {sTuning.pSecondaryLightingIntensityOne, sTuning.pSecondaryLightingIntensityTwo, sTuning.pSecondaryLightingIntensityThree, nullptr},
	});
	suiSecondaryLightControllerTypeIndex = static_cast<uint8_t>(iSecondaryLightControllerTypeIndex);

	PuffsInterpolate::RegisterType(siExplosionPuffTypeIndex,
	{
		.uiCrc = 0,
		.uiColor = 0xFFFFFFFF,
	});

	int64_t iPrimaryPuffControllerTypeIndex = suiPrimaryPuffControllerTypeIndex;
	PuffsInterpolate::RegisterControllerType(iPrimaryPuffControllerTypeIndex,
	{
		.iBaseTypeIndex = siExplosionPuffTypeIndex,
		.iKeyframeCount = 2,
		.bDestroysSelf = true,
		.times = {std::chrono::duration<float>(kfPrimaryPuffStartTime), std::chrono::duration<float>(kfPrimaryPuffEndTime), std::chrono::duration<float>(0.0f), std::chrono::duration<float>(0.0f)},
		.keyframes =
		{
			{.fArea = 1.0f, .fIntensity = 1.0f, .fRotation = 0.0f},
			{.fArea = 1.0f, .fIntensity = 1.0f, .fRotation = 0.0f},
			{},
			{},
		},
		.ppAreaScales = {sTuning.pPrimaryPuffAreaOne, sTuning.pPrimaryPuffAreaTwo, nullptr, nullptr},
		.ppIntensityScales = {sTuning.pPrimaryPuffIntensityOne, sTuning.pPrimaryPuffIntensityTwo, nullptr, nullptr},
	});
	suiPrimaryPuffControllerTypeIndex = static_cast<uint8_t>(iPrimaryPuffControllerTypeIndex);

	int64_t iSecondaryPuffControllerTypeIndex = suiSecondaryPuffControllerTypeIndex;
	PuffsInterpolate::RegisterControllerType(iSecondaryPuffControllerTypeIndex,
	{
		.iBaseTypeIndex = siExplosionPuffTypeIndex,
		.iKeyframeCount = 2,
		.bDestroysSelf = true,
		.times = {std::chrono::duration<float>(0.0f), std::chrono::duration<float>(kfSecondaryPuffTimes), std::chrono::duration<float>(0.0f), std::chrono::duration<float>(0.0f)},
		.keyframes =
		{
			{.fArea = 1.0f, .fIntensity = 1.0f, .fRotation = 0.0f},
			{.fArea = 1.0f, .fIntensity = 1.0f, .fRotation = 0.0f},
			{},
			{},
		},
		.ppAreaScales = {sTuning.pSecondaryPuffAreaOne, sTuning.pSecondaryPuffAreaTwo, nullptr, nullptr},
		.ppIntensityScales = {sTuning.pSecondaryPuffIntensityOne, sTuning.pSecondaryPuffIntensityTwo, nullptr, nullptr},
	});
	suiSecondaryPuffControllerTypeIndex = static_cast<uint8_t>(iSecondaryPuffControllerTypeIndex);

	SmokeTrailsInterpolate::RegisterType(siExplosionTrailTypeIndex,
	{
		.uiCrc = 0,
		.iColor = 0xFFFFFFFF,
		.fWidth = kfExplosionTrailWidth,
	});

	int64_t iWindRadialControllerTypeIndex = suiWindRadialControllerTypeIndex;
	WindRadialsInterpolate::RegisterControllerType(iWindRadialControllerTypeIndex,
	{
		.iKeyframeCount = 2,
		.bDestroysSelf = true,
		.times = {std::chrono::duration<float>(0.0f), std::chrono::duration<float>(kfWindDepositDuration), std::chrono::duration<float>(0.0f), std::chrono::duration<float>(0.0f)},
		.keyframes = {{.fIntensity = 1.0f, .fSize = 1.0f}, {.fIntensity = 0.0f, .fSize = 1.0f}, {}, {}},
	});
	suiWindRadialControllerTypeIndex = static_cast<uint8_t>(iWindRadialControllerTypeIndex);
#endif // BT_CLIENT
}

#if defined(BT_CLIENT)

#endif // BT_CLIENT

void ExplosionsPostRender::Destroy(game::Frame& __restrict rFrame, [[maybe_unused]] const CellStaticData& rStaticData)
{
	ExplosionsInterpolate& rInterpolate = rFrame.interpolate.explosions;
	ExplosionsPostRender& rPostRender = rFrame.postRender.explosions;

	float fCurrentTime = rFrame.interpolate.fCurrentTime;

	for (int64_t i = 0; i < rInterpolate.iCount; ++i)
	{
		int64_t iTypeIndex = rInterpolate.puiTypeIndices[i];
		const ExplosionType& rType = ExplosionsInterpolate::sTypes.at(static_cast<size_t>(iTypeIndex));

		float fStartTime = rInterpolate.pfStartTimes[i];
		float fExplosionTime = fCurrentTime - fStartTime;
		float fTimePercent = rInterpolate.pfTimePercents[i];

		int64_t iTrailCount = rInterpolate.piTrailCounts[i];

#if defined(BT_CLIENT)
		// Remove expired trails (cleanup happens every frame, not just at explosion expiration).
		// Cleanup uses unmultiplied pfTrailTimes so it fires in lockstep with the shared explosion-entry
		// destruction below — applying the client-only Duration multiplier here would let the entry be
		// destroyed before the cleanup fires, orphaning the SmokeTrail and leaking it indefinitely.
		for (int64_t j = 0; j < iTrailCount; ++j)
		{
			smoke_trails_t& rTrailId = rInterpolate.pTrails[j][i];
			if (!(rTrailId.uuid.iValue != 0))
			{
				continue;
			}

			float fTrailEndTime = fTimePercent * rType.fTrailDelayTime + rInterpolate.pfTrailTimes[j][i];
			if (fExplosionTime >= fTrailEndTime)
			{
				RemoveIndexableElementAndClearHandle(rFrame.interpolate.smokeTrails, rFrame.postRender.smokeTrails, rTrailId, rFrame.interpolate.smokeTrails.Members(), rFrame.postRender.smokeTrails.Members());
			}
		}
#endif // BT_CLIENT

		ExplosionFlags_t flags = rInterpolate.pFlags[i];

		if (!(flags & kDestroysSelf))
		{
			continue;
		}

		float fEndTime = 0.0f;
		for (int64_t j = 0; j < iTrailCount; ++j)
		{
			float fTrailEndTime = fTimePercent * rType.fTrailDelayTime + rInterpolate.pfTrailTimes[j][i];
			fEndTime = std::max(fEndTime, fTrailEndTime);
		}

		if (fExplosionTime < fEndTime)
		{
			continue;
		}

		// Remove the explosion using swap-and-pop
		DestroyElement(rInterpolate, rPostRender, i, rInterpolate.Members(), rPostRender.Members());
		--i;
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
		bEqual &= common::LogDifference<"pFlags">(i, pFlags[i], rOther.pFlags[i]);
		bEqual &= common::LogDifference<"pfStartTimes">(i, pfStartTimes[i], rOther.pfStartTimes[i]);
		bEqual &= common::LogDifference<"pVecPositions">(i, pVecPositions[i], rOther.pVecPositions[i]);
		bEqual &= common::LogDifference<"pVecDirections">(i, pVecDirections[i], rOther.pVecDirections[i]);
		bEqual &= common::LogDifference<"pfTimePercents">(i, pfTimePercents[i], rOther.pfTimePercents[i]);
		bEqual &= common::LogDifference<"piTrailCounts">(i, piTrailCounts[i], rOther.piTrailCounts[i]);

		for (int64_t j = 0; j < piTrailCounts[i]; ++j)
		{
			bEqual &= common::LogDifference<"pfTrailTimes">(i, pfTrailTimes[j][i], rOther.pfTrailTimes[j][i]);
		}
	}

	return bEqual;
}

bool ExplosionsPostRender::LogDifferences(const ExplosionsPostRender& rOther) const
{
	common::ScopedLogDifferenceContext context("ExplosionsPostRender");
	return Collection::LogDifferences(rOther);
}

#if defined(BT_CLIENT)

#endif // BT_CLIENT

} // namespace engine
