#include "Network/Client/ClientSession.h"

#include "Frame/Collections/Blasters/Blasters.h"
#include "Frame/Collections/Missiles/Missiles.h"
#include "Frame/Collections/Spaceships/Spaceships.h"
#include "Game.h"

namespace game
{

#if defined(BT_CLIENT)

void ClientSession::ApplyReceivedStaticData()
{
	// Heap: try_emplace may insert new CoordFrames, NavData vectors moved into staticData
	ScopedSuppressAllocationTracking suppress;

	std::vector<engine::ReceivedStaticData>& rStaticDataList = mpRuntime->mpClient->mReceivedStaticData;
	for (engine::ReceivedStaticData& rReceived : rStaticDataList)
	{
		engine::CoordFrames& rFrames = gpGame->mCoordFrames.try_emplace(rReceived.coord).first->second;
		rFrames.staticData = std::move(rReceived.staticData);
		rFrames.staticData.coord = rReceived.coord;

		// Subscription-driven island texture loading. AcquireTextureSlot is idempotent; duplicate
		// CRCs across placements short-circuit on the hot path. Slot mint + chunk-load request
		// happens here so the data is in-flight before UpdateActiveIslands references the slot.
		for (const engine::IslandPlacement& rPlacement : rFrames.staticData.islands)
		{
			engine::gpIslandTerrain->AcquireTextureSlot(rPlacement.islandCrc);
		}
	}
}

void ClientSession::HydrateReceivedFullState(Frame& rReceived, const Frame* pRingTail)
{
	// Initialize client-only objects
	BlastersInterpolate::ClientInitAll(rReceived);
	MissilesInterpolate::ClientInitAll(rReceived);
	SpaceshipsInterpolate::ClientInitAll(rReceived);

	// Copy smoke trail smoothed positions from the most recent ring frame to preserve
	// rendering continuity across reconciliation.
	if (pRingTail != nullptr)
	{
		const engine::SmokeTrailsInterpolate& rOldSmokeTrails = pRingTail->interpolate.smokeTrails;
		engine::SmokeTrailsInterpolate& rNewSmokeTrails = rReceived.interpolate.smokeTrails;
		int64_t iCopyCount = std::min(rOldSmokeTrails.iCount, rNewSmokeTrails.iCount);
		if (iCopyCount > 0)
		{
			std::memcpy(rNewSmokeTrails.pVecSmoothedPositions, rOldSmokeTrails.pVecSmoothedPositions, iCopyCount * sizeof(XMVECTOR));
		}
	}
}

void ClientSession::ResetCoordStatesForResync()
{
	for (auto& [rCoord, rSub] : gpGame->mCoordFrames)
	{
		rSub.ResetClientState();
	}

	mpReconciler->Reset();
	mpRuntime->mUnwantedTimestamps.clear();
}

void ClientSession::LogDesyncFrameDifferences(const Frame& rClientFrame, const Frame& rServerFrame)
{
	rClientFrame.LogDifferences(rServerFrame);
}

#endif // BT_CLIENT

} // namespace game
