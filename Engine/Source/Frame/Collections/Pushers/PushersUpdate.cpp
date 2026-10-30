#include "Pushers.h"

#include "Frame/FlightPlaneTable.h"

namespace engine
{

// One zone grid per flight-plane layer. engine::gpFlightPlanes holds this same table, so its Count() equals this.
constexpr int64_t kiPusherLayers = std::ssize(game::kFlightPlanes);
constexpr int64_t kiPusherZones = 50;
constexpr int64_t kiMaxPushersPerZone = 512;
constexpr int64_t kiPusherZoneSlots = kiPusherLayers * kiPusherZones * kiPusherZones * kiMaxPushersPerZone;

// Zone acceleration structure: thread_local so each Dispatch worker and reconcile thread gets its own copy. The
// slots, indexed layer, X zone, Y zone, slot, are sized on the thread's first SetupZones call, so only simulation
// threads commit them; the StableVector constructor makes no OS call, so it is safe before the allocator is ready.
alignas(64) static thread_local int64_t spppiPushersPerZone[kiPusherLayers][kiPusherZones][kiPusherZones] {};
static thread_local common::StableVector<int16_t> sPusherZones(kiPusherZoneSlots);

// The zone grid spans the cell's bounding rectangle.
constexpr float kfPusherAreaMinimumX = -kfCellHalfWidth;
constexpr float kfPusherAreaMinimumY = -kfCellHalfHeight;
constexpr float kfPusherZoneWidth = 2.0f * kfCellHalfWidth / static_cast<float>(kiPusherZones);
constexpr float kfPusherZoneHeight = 2.0f * kfCellHalfHeight / static_cast<float>(kiPusherZones);

// A peer within this XY distance gets no force, only the X correction. The correction must exceed the distance so a
// coincident pair, corrected in opposite directions, leaves it on the next tick.
constexpr float kfPusherCoincidentDistance = 0.0001f;
constexpr float kfPusherCoincidentCorrection = 0.001f;

static int16_t* PusherZone(int64_t iLayer, int64_t iZoneX, int64_t iZoneY)
{
	return sPusherZones.Data() + ((iLayer * kiPusherZones + iZoneX) * kiPusherZones + iZoneY) * kiMaxPushersPerZone;
}

void PushersInterpolate::Update([[maybe_unused]] game::FrameInterpolate& __restrict rFrameInterpolate, [[maybe_unused]] const game::Frame& __restrict rPreviousFrame)
{
	PushersInterpolate& __restrict rCurrent = rFrameInterpolate.pushers;
	const PushersInterpolate& rPrevious = rPreviousFrame.interpolate.pushers;
	CopyMemberRows(rCurrent.iCount, rCurrent.Members(), rPrevious.Members());
}

void XM_CALLCONV PushersInterpolate::Sync(game::FrameInterpolate& rFrameInterpolate, id_t id, const SyncData& rData)
{
	if (!(id.uuid.iValue != 0))
	{
		return;
	}

	PushersInterpolate& rPushers = rFrameInterpolate.pushers;
	int64_t iIndex = rPushers.idToIndexMap.at(id);

	rPushers.pVecPositions[iIndex] = XMVectorSetW(rData.vecPosition, 1.0f);
	rPushers.pfRadii[iIndex] = rData.fRadius;
	rPushers.pfIntensities[iIndex] = rData.fIntensity;
	rPushers.pfPowers[iIndex] = rData.fPower;
	rPushers.pFlags[iIndex] = rData.flags;
}

void PushersInterpolate::SetupZones([[maybe_unused]] const game::Frame& __restrict rFrame)
{
	const PushersInterpolate& rCurrent = rFrame.interpolate.pushers;

	if (sPusherZones.Size() == 0)
	{
		sPusherZones.Resize(kiPusherZoneSlots);
	}

	ZeroMemory(spppiPushersPerZone, sizeof(spppiPushersPerZone));

	int64_t iDroppedRegistrations = 0;
	for (int64_t i = 0; i < rCurrent.iCount; ++i)
	{
		XMFLOAT4A f4Position {};
		XMStoreFloat4A(&f4Position, rCurrent.pVecPositions[i]);
		float fRadius = rCurrent.pfRadii[i];

		int64_t iZoneStartX = static_cast<int64_t>((f4Position.x - fRadius - kfPusherAreaMinimumX) / kfPusherZoneWidth);
		int64_t iZoneEndX = static_cast<int64_t>((f4Position.x + fRadius - kfPusherAreaMinimumX) / kfPusherZoneWidth);
		int64_t iZoneStartY = static_cast<int64_t>((f4Position.y - fRadius - kfPusherAreaMinimumY) / kfPusherZoneHeight);
		int64_t iZoneEndY = static_cast<int64_t>((f4Position.y + fRadius - kfPusherAreaMinimumY) / kfPusherZoneHeight);

		if (iZoneStartX >= kiPusherZones || iZoneEndX < 0 || iZoneStartY >= kiPusherZones || iZoneEndY < 0)
		{
			continue;
		}

		iZoneStartX = std::clamp(iZoneStartX, 0i64, kiPusherZones - 1);
		iZoneEndX = std::clamp(iZoneEndX, 0i64, kiPusherZones - 1);
		iZoneStartY = std::clamp(iZoneStartY, 0i64, kiPusherZones - 1);
		iZoneEndY = std::clamp(iZoneEndY, 0i64, kiPusherZones - 1);

		// A row's layer is the flight plane nearest its registered height. SetupZones and ApplyPush must both derive it
		// from the registered position, so a query's zone holds only rows of the querying row's layer.
		int64_t iLayer = gpFlightPlanes->NearestPlaneIndex(f4Position.z);
		for (int64_t j = iZoneStartY; j <= iZoneEndY; ++j)
		{
			for (int64_t k = iZoneStartX; k <= iZoneEndX; ++k)
			{
				int64_t iPushersPerZone = spppiPushersPerZone[iLayer][k][j];
				if (iPushersPerZone >= kiMaxPushersPerZone) [[unlikely]]
				{
					++iDroppedRegistrations;
					continue;
				}

				PusherZone(iLayer, k, j)[iPushersPerZone] = static_cast<int16_t>(i);
				++spppiPushersPerZone[iLayer][k][j];
			}
		}
	}

	if (iDroppedRegistrations > 0) [[unlikely]]
	{
		LOG(kDefault, kWarning, "PushersInterpolate::SetupZones dropped {} pusher zone registrations (cap {} per zone)", iDroppedRegistrations, kiMaxPushersPerZone);
		DEBUG_BREAK();
	}
}

XMVECTOR XM_CALLCONV PushersInterpolate::ApplyPush(const game::FrameInterpolate& rFrameInterpolate, id_t selfPusher, float& rfPositionCorrectionX, PusherFlags_t includeFlags, PusherFlags_t excludeFlags)
{
	const PushersInterpolate& rCurrent = rFrameInterpolate.pushers;
	int64_t iSelfIndex = rCurrent.idToIndexMap.at(selfPusher);
	auto vecPosition = rCurrent.pVecPositions[iSelfIndex];

	XMFLOAT2A f2Position {};
	XMStoreFloat2A(&f2Position, vecPosition);

	int64_t iZoneX = std::clamp(static_cast<int64_t>((f2Position.x - kfPusherAreaMinimumX) / kfPusherZoneWidth), 0i64, kiPusherZones - 1);
	int64_t iZoneY = std::clamp(static_cast<int64_t>((f2Position.y - kfPusherAreaMinimumY) / kfPusherZoneHeight), 0i64, kiPusherZones - 1);
	int64_t iLayer = gpFlightPlanes->NearestPlaneIndex(XMVectorGetZ(vecPosition));
	const int16_t* piZone = PusherZone(iLayer, iZoneX, iZoneY);
	int64_t iPushersInZone = spppiPushersPerZone[iLayer][iZoneX][iZoneY];

	auto vecPush = XMVectorZero();
	for (int64_t j = 0; j < iPushersInZone; ++j)
	{
		int64_t i = piZone[j];

		if (i == iSelfIndex) [[unlikely]]
		{
			continue;
		}

		PusherFlags_t flags = rCurrent.pFlags[i];

		if (flags.Mask(excludeFlags) != 0ui32) [[unlikely]]
		{
			continue;
		}

		if (flags.Mask(includeFlags) == 0ui32) [[unlikely]]
		{
			continue;
		}

		auto vecPusherPosition = rCurrent.pVecPositions[i];
		auto vecFromPusher = XMVectorSubtract(vecPosition, vecPusherPosition);
		auto vecDistanceSquared = XMVector3LengthSq(vecFromPusher);
		float fRadius = rCurrent.pfRadii[i];
		if (XMVectorGetX(vecDistanceSquared) > fRadius * fRadius) [[likely]]
		{
			continue;
		}

		if (XMVectorGetX(XMVector2LengthSq(vecFromPusher)) <= kfPusherCoincidentDistance * kfPusherCoincidentDistance) [[unlikely]]
		{
			rfPositionCorrectionX += iSelfIndex < i ? -kfPusherCoincidentCorrection : kfPusherCoincidentCorrection;
			continue;
		}

		auto vecIntensity = XMVectorSubtract(XMVectorReplicate(1.0f), XMVectorDivide(vecDistanceSquared, XMVectorReplicate(fRadius * fRadius)));
		vecIntensity = XMVectorPow(vecIntensity, XMVectorReplicate(rCurrent.pfPowers[i]));
		vecIntensity = XMVectorMultiply(XMVectorReplicate(rCurrent.pfIntensities[i]), vecIntensity);

		auto vecFromPusherNormal = XMVector3Normalize(XMVectorSetZ(vecFromPusher, 0.0f));
		vecPush = XMVectorMultiplyAdd(vecIntensity, vecFromPusherNormal, vecPush);
	}

	return vecPush;
}

void PushersPostRender::Update([[maybe_unused]] game::Frame& __restrict rFrame, [[maybe_unused]] const game::Frame& __restrict rPreviousFrame, [[maybe_unused]] const CellStaticData& rStaticData)
{
}

void PushersPostRender::Add(game::Frame& __restrict rFrame, pusher_t& rId)
{
	ASSERT(!(rId.uuid.iValue != 0));

	PushersInterpolate& rInterpolate = rFrame.interpolate.pushers;
	PushersPostRender& rPostRender = rFrame.postRender.pushers;

	GrowPairedCollections(rInterpolate, rPostRender, rInterpolate.Members(), rPostRender.Members());
	auto [iSpawnIndex, newId] = AddIndexableElement(rInterpolate, rPostRender, rFrame.postRender);
	rId = newId;
	rPostRender.pIds[iSpawnIndex] = newId;

	ZeroMemberRow(iSpawnIndex, rInterpolate.Members());
}

} // namespace engine
