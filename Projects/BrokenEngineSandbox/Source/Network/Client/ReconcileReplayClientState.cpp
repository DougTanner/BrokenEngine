#include "Network/Client/ReconcileReplay.h"

#include "Frame/Collections/Players/Players.h"
#include "Network/Client/ClientReconciler.h"
#include "Game.h"

namespace game
{

#if defined(BT_CLIENT)

static bool FindMatchingPlayerInCoordinate(std::span<const engine::CoordWork> works, engine::GridCoord destination, engine::GlobalId globalPlayerId)
{
	for (const engine::CoordWork& rDestinationWork : works)
	{
		if (rDestinationWork.coord != destination)
		{
			continue;
		}

		const engine::Cell& rDestinationCell = *rDestinationWork.pCell;
		const engine::CoordScratch& rDestinationScratch = rDestinationWork.scratch;

		const Frame* pDestinationFrame = nullptr;
		if (rDestinationScratch.iReplayStackCount > 0)
		{
			pDestinationFrame = rDestinationScratch.replayStack.at(static_cast<size_t>(rDestinationScratch.iReplayStackCount - 1));
		}
		else if ((rDestinationScratch.flags & engine::ReconcileScratchFlags::kCrcFastPath) && rDestinationScratch.outputLayout.iHead >= 0)
		{
			int64_t iConfirmedPhysical = SnapshotIndex(rDestinationScratch.outputLayout.iHead, rDestinationScratch.outputLayout.iConfirmedInner);
			pDestinationFrame = rDestinationCell.snapshots[iConfirmedPhysical].get();
		}
		if (pDestinationFrame == nullptr)
		{
			continue;
		}
		const Frame& rDestinationFrame = *pDestinationFrame;
		for (int64_t j = 0; j < rDestinationFrame.postRender.pPlayers->iCount; ++j)
		{
			if (rDestinationFrame.postRender.pPlayers->pGlobalPlayerIds[j] == globalPlayerId)
			{
				LOG(kNetwork, kVerbose, "ReconcileUpdateClientState Transfer matched GlobalPlayerId: {} Coord: ({},{})", globalPlayerId, destination.iX, destination.iY);
				return true;
			}
		}
		LOG(kNetwork, kVerbose, "ReconcileUpdateClientState Transfer global ID match failed Coord: ({},{}) PlayerCount: {}", destination.iX, destination.iY, rDestinationFrame.postRender.pPlayers->iCount);
		break;
	}
	return false;
}

void ReconcileUpdateClientState(std::span<const engine::CoordWork> works, bool bAnyFullReplay, ConfirmedClientState& rInOutState)
{
	ConfirmedClientState clientState = rInOutState;

	if (bAnyFullReplay)
	{
		for (const engine::CoordWork& rWork : works)
		{
			const engine::CoordScratch& rScratch = rWork.scratch;
			if (rScratch.flags & engine::ReconcileScratchFlags::kCrcFastPath)
			{
				continue;
			}

			// replayStack[0] is the confirmed frame; scan from index 1 onwards
			for (int64_t i = 1; i < rScratch.iReplayStackCount; ++i)
			{
				const Frame& rFrame = *rScratch.replayStack.at(static_cast<size_t>(i));
				for (const TransferRequest& rRequest : rFrame.postRender.transferRequests)
				{
					if (rRequest.eType != StatusChangeType::kTransferPlayer)
					{
						continue;
					}
					if (!(clientState.clientGlobalPlayerIdentifier.iValue != 0))
					{
						continue;
					}
					if (rRequest.data.globalPlayerId != clientState.clientGlobalPlayerIdentifier)
					{
						continue;
					}

					if (std::abs(rRequest.iDeltaX) > 1 || std::abs(rRequest.iDeltaY) > 1) [[unlikely]]
					{
						LOG(kDefault, kError, "ReconcileUpdateClientState Transfer delta spans more than one grid cell Tick: {} Source: ({},{}) Delta: ({},{}) GlobalPlayerId: {}", rFrame.interpolate.iTick, rWork.coord.iX, rWork.coord.iY, static_cast<int32_t>(rRequest.iDeltaX), static_cast<int32_t>(rRequest.iDeltaY), clientState.clientGlobalPlayerIdentifier);
						DEBUG_BREAK();
					}
					// Checked exactly like the server's transfer destination, so a cell at a numeric coordinate edge
					// follows the server in having no outward neighbour instead of wrapping to the far side.
					engine::GridCoord destination {};
					if (!engine::TryAddGridCoordinate(rWork.coord, rRequest.iDeltaX, rRequest.iDeltaY, destination)) [[unlikely]]
					{
						continue;
					}
					LOG(kNetwork, kVerbose, "ReconcileUpdateClientState TransferPlayer GlobalPlayerId: {} Source: ({},{}) Dest: ({},{})", clientState.clientGlobalPlayerIdentifier, rWork.coord.iX, rWork.coord.iY, destination.iX, destination.iY);
					clientState.fPreviousClientArmor = rRequest.data.fHealth;

					FindMatchingPlayerInCoordinate(works, destination, clientState.clientGlobalPlayerIdentifier);
				}
			}
		}
	}

	rInOutState = clientState;
}

#endif // BT_CLIENT

} // namespace game
