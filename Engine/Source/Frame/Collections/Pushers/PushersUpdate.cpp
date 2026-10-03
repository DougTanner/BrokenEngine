#include "Pushers.h"

namespace engine
{

constexpr int64_t kiPusherZones = 50;
constexpr int64_t kiMaxPushersPerZone = 512;

// Zone acceleration structure: thread_local so each Dispatch worker and reconcile thread gets its own copy
alignas(64) static thread_local uint16_t sppuiPushersPerZone[kiPusherZones][kiPusherZones] {};
alignas(64) static thread_local int16_t spppiPusherZones[kiPusherZones][kiPusherZones][kiMaxPushersPerZone] {};

// Cell bounds the zone grid spans, refreshed by SetupZones from the cell's own area
static thread_local float sfPusherAreaMinX = 0.0f;
static thread_local float sfPusherAreaMinY = 0.0f;
static thread_local float sfPusherZoneWidth = 0.0f;
static thread_local float sfPusherZoneHeight = 0.0f;

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

void XM_CALLCONV PushersInterpolate::SetupZones([[maybe_unused]] const game::Frame& __restrict rFrame, FXMVECTOR vecArea)
{
	const PushersInterpolate& rCurrent = rFrame.interpolate.pushers;

	ZeroMemory(sppuiPushersPerZone, sizeof(sppuiPushersPerZone));

	// Span the whole cell with the fixed zone grid
	FrameBounds bounds = ComputeFrameBounds(vecArea);
	sfPusherAreaMinX = bounds.fMinX;
	sfPusherAreaMinY = bounds.fMinY;
	sfPusherZoneWidth = (bounds.fMaxX - bounds.fMinX) / static_cast<float>(kiPusherZones);
	sfPusherZoneHeight = (bounds.fMaxY - bounds.fMinY) / static_cast<float>(kiPusherZones);

	int64_t iDroppedRegistrations = 0;
	for (int64_t i = 0; i < rCurrent.iCount; ++i)
	{
		XMFLOAT4A f4Position {};
		XMStoreFloat4A(&f4Position, rCurrent.pVecPositions[i]);
		float fRadius = rCurrent.pfRadii[i];

		int64_t iZoneStartX = static_cast<int64_t>((f4Position.x - fRadius - sfPusherAreaMinX) / sfPusherZoneWidth);
		int64_t iZoneEndX = static_cast<int64_t>((f4Position.x + fRadius - sfPusherAreaMinX) / sfPusherZoneWidth);
		int64_t iZoneStartY = static_cast<int64_t>((f4Position.y - fRadius - sfPusherAreaMinY) / sfPusherZoneHeight);
		int64_t iZoneEndY = static_cast<int64_t>((f4Position.y + fRadius - sfPusherAreaMinY) / sfPusherZoneHeight);

		if (iZoneStartX >= kiPusherZones || iZoneEndX < 0 || iZoneStartY >= kiPusherZones || iZoneEndY < 0)
		{
			continue;
		}

		iZoneStartX = std::clamp(iZoneStartX, 0ll, kiPusherZones - 1);
		iZoneEndX = std::clamp(iZoneEndX, 0ll, kiPusherZones - 1);
		iZoneStartY = std::clamp(iZoneStartY, 0ll, kiPusherZones - 1);
		iZoneEndY = std::clamp(iZoneEndY, 0ll, kiPusherZones - 1);

		for (int64_t j = iZoneStartY; j <= iZoneEndY; ++j)
		{
			for (int64_t k = iZoneStartX; k <= iZoneEndX; ++k)
			{
				int64_t iPushersPerZone = sppuiPushersPerZone[k][j];
				if (iPushersPerZone >= kiMaxPushersPerZone) [[unlikely]]
				{
					++iDroppedRegistrations;
					continue;
				}

				spppiPusherZones[k][j][iPushersPerZone] = static_cast<int16_t>(i);
				++sppuiPushersPerZone[k][j];
			}
		}
	}

	if (iDroppedRegistrations > 0) [[unlikely]]
	{
		LOG(kDefault, kWarning, "PushersInterpolate::SetupZones dropped {} pusher zone registrations (cap {} per zone)", iDroppedRegistrations, kiMaxPushersPerZone);
		DEBUG_BREAK();
	}
}

XMVECTOR XM_CALLCONV PushersInterpolate::ApplyPush(const game::FrameInterpolate& rFrameInterpolate, FXMVECTOR vecPosition, id_t ignorePusher, PusherFlags_t includeFlags, PusherFlags_t excludeFlags)
{
	const PushersInterpolate& rCurrent = rFrameInterpolate.pushers;

	XMFLOAT2A f2Position {};
	XMStoreFloat2A(&f2Position, vecPosition);

	int64_t iZoneX = std::clamp(static_cast<int64_t>((f2Position.x - sfPusherAreaMinX) / sfPusherZoneWidth), 0ll, kiPusherZones - 1);
	int64_t iZoneY = std::clamp(static_cast<int64_t>((f2Position.y - sfPusherAreaMinY) / sfPusherZoneHeight), 0ll, kiPusherZones - 1);
	int16_t* piZone = spppiPusherZones[iZoneX][iZoneY];
	int64_t iPushersInZone = sppuiPushersPerZone[iZoneX][iZoneY];

	int64_t iIgnoreIndex = -1;
	if ((ignorePusher.uuid.iValue != 0))
	{
		auto it = rCurrent.idToIndexMap.find(ignorePusher);
		if (it != rCurrent.idToIndexMap.end())
		{
			iIgnoreIndex = static_cast<int64_t>(it->second);
		}
	}

	auto vecPush = XMVectorZero();
	for (int64_t j = 0; j < iPushersInZone; ++j)
	{
		int64_t i = piZone[j];

		if (i == iIgnoreIndex) [[unlikely]]
		{
			continue;
		}

		PusherFlags_t flags = rCurrent.pFlags[i];

		if (flags.Mask(excludeFlags) != 0u) [[unlikely]]
		{
			continue;
		}

		if (flags.Mask(includeFlags) == 0u) [[unlikely]]
		{
			continue;
		}

		auto vecPusherPosition = rCurrent.pVecPositions[i];
		if (XMVector3NearEqual(vecPosition, vecPusherPosition, XMVectorReplicate(kfEpsilon))) [[unlikely]]
		{
			continue;
		}

		auto vecFromPusher = XMVectorSubtract(vecPosition, vecPusherPosition);
		auto vecDistanceSquared = XMVector3LengthSq(vecFromPusher);
		float fRadius = rCurrent.pfRadii[i];
		if (XMVectorGetX(vecDistanceSquared) > fRadius * fRadius) [[likely]]
		{
			continue;
		}

		auto vecIntensity = XMVectorSubtract(XMVectorReplicate(1.0f), XMVectorDivide(vecDistanceSquared, XMVectorReplicate(fRadius * fRadius)));
		vecIntensity = XMVectorPow(vecIntensity, XMVectorReplicate(rCurrent.pfPowers[i]));
		vecIntensity = XMVectorMultiply(XMVectorReplicate(rCurrent.pfIntensities[i]), vecIntensity);

		auto vecFromPusherNormal = XMVector3Normalize(vecFromPusher);
		vecPush = XMVectorMultiplyAdd(vecIntensity, vecFromPusherNormal, vecPush);
	}

	return vecPush;
}

void PushersPostRender::Update([[maybe_unused]] game::Frame& __restrict rFrame, [[maybe_unused]] const game::Frame& __restrict rPreviousFrame, [[maybe_unused]] const FrameStaticData& rStaticData)
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

void PushersPostRender::Remove(game::Frame& __restrict rFrame, pusher_t& rId)
{
	PushersInterpolate& rInterpolate = rFrame.interpolate.pushers;
	PushersPostRender& rPostRender = rFrame.postRender.pushers;

	RemoveIndexableElementAndClearHandle(rInterpolate, rPostRender, rId, rInterpolate.Members(), rPostRender.Members());
}

} // namespace engine
