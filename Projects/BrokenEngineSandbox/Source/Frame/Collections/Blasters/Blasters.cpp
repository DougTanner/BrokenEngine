#include "Pch.h"

#include "Blasters.h"

#include "Frame/CellStaticData.h"

#include "Ui/WindDepositsWrappers.h"

namespace engine
{
template struct Collection<game::BlastersInterpolate>;
template struct Collection<game::BlastersPostRender>;
} // namespace engine

namespace game
{

#if !defined(BT_CLIENT)
void BlastersInterpolate::Register()
{
}
#endif

#if defined(BT_CLIENT)
void BlastersInterpolate::ClientInitialize(Frame& rFrame, int64_t iIndex)
{
	BlastersInterpolate& rBlasters = *rFrame.interpolate.pBlasters;

	// Add client-only owned objects
	const BlastersType& rType = sTypes.at(rBlasters.puiTypeIndices[iIndex]);

	rBlasters.pAreaLights[iIndex] = {};
	rBlasters.pPointLights[iIndex] = {};

	if (rType.iPointLightTypeIndex != 0xFF)
	{
		engine::PointLightsPostRender::Add(rFrame, rBlasters.pPointLights[iIndex], rType.iPointLightTypeIndex);
	}
	else
	{
		engine::AreaLightsPostRender::Add(rFrame, rBlasters.pAreaLights[iIndex], rType.iAreaLightTypeIndex);
	}

	rBlasters.pWindTrails[iIndex] = {};
	if (rBlasters.pfWindTrailIntensities[iIndex] > 0.0f)
	{
		engine::WindTrailsPostRender::Add(rFrame, rBlasters.pWindTrails[iIndex],
		{
			.vecPosition = rBlasters.pVecPositions[iIndex],
			.fIntensity = rBlasters.pfWindTrailIntensities[iIndex],
			.fWidth = rBlasters.pfWindTrailWidths[iIndex],
			.fLengthMultiplier = rBlasters.pfWindTrailLengthMultipliers[iIndex],
		});
	}

	if ((rBlasters.pPointLights[iIndex].uuid.iValue != 0))
	{
		float fSize = rType.f2Size.x;
		engine::PointLightsInterpolate::Sync(rFrame.interpolate, rBlasters.pPointLights[iIndex],
		{
			.vecPosition = rBlasters.pVecPositions[iIndex],
			.fVisibleArea = fSize,
			.fRotation = 0.0f,
		});
	}
	else
	{
		float fWidth = rType.f2Size.x;
		float fLength = rType.f2Size.y;

		XMVECTOR vecDirection = rBlasters.pVecDirections[iIndex];
		auto [vecTopLeft, vecTopRight, vecBottomLeft, vecBottomRight] = common::CalculateArea(rBlasters.pVecPositions[iIndex], vecDirection, fLength, fLength, fWidth);

		engine::AreaLightsInterpolate::Sync(rFrame.interpolate, rBlasters.pAreaLights[iIndex],
		{
			.iTypeIndex = rType.iAreaLightTypeIndex,
			.pVecVisiblePositions = {vecTopLeft, vecTopRight, vecBottomLeft, vecBottomRight},
		});
	}
}

void BlastersInterpolate::ClientInitializeAll(Frame& rFrame)
{
	for (int64_t i = 0; i < rFrame.interpolate.pBlasters->iCount; ++i)
	{
		ClientInitialize(rFrame, i);
	}
}
#endif // BT_CLIENT

bool BlastersPostRender::Spawn(Frame& __restrict rFrame, const SpawnInfo& rSpawnInformation)
{
	if (!common::InsideArea(rSpawnInformation.vecPosition, engine::LocalCellArea()))
	{
		return false;
	}

	BlastersInterpolate& rCurrentInterpolate = *rFrame.interpolate.pBlasters;
	BlastersPostRender& rCurrentPostRender = *rFrame.postRender.pBlasters;

	common::ValidateVector<true >(rSpawnInformation.vecPosition);
	common::ValidateVector<false>(rSpawnInformation.vecVelocity);

	engine::GrowPairedCollections(rCurrentInterpolate, rCurrentPostRender, rCurrentInterpolate.Members(), rCurrentPostRender.Members());
	int64_t iIndex = engine::AddElement(rCurrentInterpolate, rCurrentPostRender);

	rCurrentInterpolate.pVecPositions[iIndex] = rSpawnInformation.vecPosition;
	rCurrentInterpolate.pVecDirections[iIndex] = XMVector3Normalize(rSpawnInformation.vecVelocity);
	rCurrentInterpolate.puiTypeIndices[iIndex] = static_cast<uint8_t>(rSpawnInformation.iTypeIndex);
#if defined(BT_CLIENT)
	rCurrentInterpolate.pfWindTrailIntensities[iIndex] = rSpawnInformation.fWindTrailIntensity;
	rCurrentInterpolate.pfWindTrailWidths[iIndex] = rSpawnInformation.fWindTrailWidth;
	rCurrentInterpolate.pfWindTrailLengthMultipliers[iIndex] = rSpawnInformation.fWindTrailLengthMultiplier;
#endif

	rCurrentPostRender.pVecVelocities[iIndex] = rSpawnInformation.vecVelocity;
	rCurrentPostRender.pAlignments[iIndex] = rSpawnInformation.alignment;

#if defined(BT_CLIENT)
	BlastersInterpolate::ClientInitialize(rFrame, iIndex);
#endif

	return true;
}

void BlastersPostRender::Transfer([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	BlastersInterpolate& rCurrentInterpolate = *rFrame.interpolate.pBlasters;
	BlastersPostRender& rCurrentPostRender = *rFrame.postRender.pBlasters;

	engine::CellBounds bounds = engine::ComputeCellBounds(engine::LocalCellArea());

	engine::DestroySweep(rCurrentInterpolate, rCurrentPostRender, [&](int64_t i)
	{
		return rCurrentPostRender.pLifecycleFlags[i] & engine::LifecycleFlags::kTransfer;
	}, [&](int64_t i)
	{
		XMVECTOR vecPosition = rCurrentInterpolate.pVecPositions[i];
		engine::AlignmentIdentifier alignment = rCurrentPostRender.pAlignments[i];
		bool bOmniAlignment = alignment == rFrame.postRender.omniAlignment;
		TransferRequest request
		{
			.change = MakeTransfer(SpawnInfo
			{
				.vecPosition = vecPosition,
				.vecVelocity = rCurrentPostRender.pVecVelocities[i],
				.iTypeIndex = rCurrentInterpolate.puiTypeIndices[i],
				.alignment = alignment,
				// Wind-trail tuning is client-only visual debug state; reset to canonical defaults on transfer.
				.fWindTrailIntensity = bOmniAlignment ? gWindDepositOmniBlastersIntensity.mfDefault : gWindDepositSpaceshipsBlastersIntensity.mfDefault,
				.fWindTrailWidth = bOmniAlignment ? gWindDepositOmniBlastersWidth.mfDefault : gWindDepositSpaceshipsBlastersWidth.mfDefault,
				.fWindTrailLengthMultiplier = bOmniAlignment ? gWindDepositOmniBlastersLengthMultiplier.mfDefault : gWindDepositSpaceshipsBlastersLengthMultiplier.mfDefault,
			}),
		};
		if (rFrame.postRender.PrepareTransferRequest(bounds, request)) [[unlikely]]
		{
			LOG(kDefault, kError, "Blaster Transfer capacity hit Tick: {} Source: ({},{}) Index: {} Position: {} Velocity: {} Delta: ({},{}) TypeIndex: {} Alignment: {} SourceCount: {} Pushed: {} Capacity: {}", rFrame.interpolate.iTick, rStaticData.coordinate.iX, rStaticData.coordinate.iY, i, common::WbV2(vecPosition, 1), common::WbV2(rCurrentPostRender.pVecVelocities[i], 1), static_cast<int64_t>(request.iDeltaX), static_cast<int64_t>(request.iDeltaY), static_cast<int64_t>(rCurrentInterpolate.puiTypeIndices[i]), rCurrentPostRender.pAlignments[i], rCurrentInterpolate.iCount, std::ssize(rFrame.postRender.transferRequests), rFrame.postRender.transferRequests.capacity());
			DEBUG_BREAK();
		}
		rFrame.postRender.PushTransferRequest(request);
	});
}

bool BlastersInterpolate::LogDifferences(const BlastersInterpolate& rOther) const
{
	common::ScopedLogDifferenceContext context("BlastersInterpolate");
	bool bEqual = true;
	bEqual &= Collection::LogDifferences(rOther);

	for (int64_t i = 0; i < std::min(iCount, rOther.iCount); ++i)
	{
		bEqual &= common::LogDifference<"puiTypeIndices">(i, puiTypeIndices[i], rOther.puiTypeIndices[i]);
		bEqual &= common::LogDifference<"pVecPositions">(i, pVecPositions[i], rOther.pVecPositions[i]);
		bEqual &= common::LogDifference<"pVecDirections">(i, pVecDirections[i], rOther.pVecDirections[i]);
	}

	return bEqual;
}

bool BlastersPostRender::LogDifferences(const BlastersPostRender& rOther) const
{
	common::ScopedLogDifferenceContext context("BlastersPostRender");
	bool bEqual = true;
	bEqual &= Collection::LogDifferences(rOther);

	for (int64_t i = 0; i < std::min(iCount, rOther.iCount); ++i)
	{
		bEqual &= common::LogDifference<"pLifecycleFlags">(i, pLifecycleFlags[i], rOther.pLifecycleFlags[i]);
		bEqual &= common::LogDifference<"pVecVelocities">(i, pVecVelocities[i], rOther.pVecVelocities[i]);
		bEqual &= common::LogDifference<"pAlignments">(i, pAlignments[i], rOther.pAlignments[i]);
	}

	return bEqual;
}

} // namespace game
