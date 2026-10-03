#include "Network/Client/ClientSession.h"
#include "Game.h"

namespace game
{

#if defined(BT_CLIENT)

void ClientSession::UpdateDesiredCoords(SubscriptionChangeReason eReason)
{
	std::optional<common::LogTickScope> optionalTickScope;
	if (common::gpThreadLocal->miLogTickCounter < 0)
	{
		optionalTickScope.emplace(gpGame->TickCounter());
	}

	static constexpr int64_t kiMaxDesiredCoords = 9;
	engine::GridCoord desiredCoords[kiMaxDesiredCoords] {};
	int64_t iDesiredCount = 0;
	auto pushCoord = [&](engine::GridCoord coord)
	{
		ASSERT(iDesiredCount < kiMaxDesiredCoords);
		desiredCoords[iDesiredCount++] = coord;
	};

	if ((gpGame->ClientPlayerId().iValue != 0))
	{
		pushCoord(gpGame->mClientGridCoord);
		for (int64_t i = 0; i < gpGame->miVisibleNeighborCount; ++i)
		{
			pushCoord(gpGame->mVisibleNeighbors[i]);
		}
	}
	else
	{
		pushCoord(engine::kOriginCoordinate);
	}

	mpRuntime->SetDesiredCoordinates(std::span<const engine::GridCoord>(desiredCoords, static_cast<size_t>(iDesiredCount)), ToString(eReason), gpGame->TickCounter());
}

void ClientSession::UpdateSubscriptions()
{
	mpRuntime->SynchronizeSubscriptions();
}

#endif // BT_CLIENT

} // namespace game
