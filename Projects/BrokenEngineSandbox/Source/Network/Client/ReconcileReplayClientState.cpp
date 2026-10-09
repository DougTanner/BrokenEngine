#include "Network/Client/ReconcileReplay.h"

#include "Frame/Collections/Players/Players.h"
#include "Network/Client/ClientReconciler.h"
#include "Game.h"

namespace game
{

#if defined(BT_CLIENT)

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
				}
			}
		}
	}

	rInOutState = clientState;
}

#endif // BT_CLIENT

} // namespace game
