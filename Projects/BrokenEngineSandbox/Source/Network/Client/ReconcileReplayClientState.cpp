#include "Network/Client/ReconcileReplay.h"

#include "Frame/Collections/Battleships/Battleships.h"
#include "Frame/Collections/Fighters/Fighters.h"
#include "Frame/Collections/Omnis/Omnis.h"
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
					if (!IsOwnedEntityTransfer(rRequest.change))
					{
						continue;
					}
					if (!(clientState.clientGlobalIdentifier.iValue != 0))
					{
						continue;
					}
					if (OwnedSpawnGlobalId(rRequest.change) != clientState.clientGlobalIdentifier)
					{
						continue;
					}

					// Checked exactly like the server's transfer destination, so a cell at a numeric coordinate edge
					// follows the server in having no outward neighbour instead of wrapping to the far side.
					const common::HexDirection& rDirection = engine::kCellDirections.pEntries[rRequest.iDirection];
					engine::GridCoord destination {};
					if (!engine::TryAddGridCoordinate(rWork.coord, rDirection.iDeltaQ, rDirection.iDeltaR, destination)) [[unlikely]]
					{
						continue;
					}
					LOG(kNetwork, kVerbose, "ReconcileUpdateClientState {} GlobalId: {} Source: ({},{}) Direction: {} Dest: ({},{})", StatusChangeTypeName(rRequest.change.eType), clientState.clientGlobalIdentifier, rWork.coord.iQ, rWork.coord.iR, static_cast<int64_t>(rRequest.iDirection), destination.iQ, destination.iR);
					// The carried hull: armor for Omnis, health for Fighters and Battleships.
					clientState.fPreviousClientArmor = std::visit([](const auto& rData) -> float
					{
						using Data = std::remove_cvref_t<decltype(rData)>;
						if constexpr (std::is_same_v<Data, OmnisPostRender::SpawnInfo>)
						{
							return rData.fArmor;
						}
						else if constexpr (std::is_same_v<Data, FightersPostRender::SpawnInfo> || std::is_same_v<Data, BattleshipsPostRender::SpawnInfo>)
						{
							return rData.fHealth;
						}
						else
						{
							ASSERT(false);
							std::unreachable();
						}
					}, rRequest.change.data);
				}
			}
		}
	}

	rInOutState = clientState;
}

#endif // BT_CLIENT

} // namespace game
