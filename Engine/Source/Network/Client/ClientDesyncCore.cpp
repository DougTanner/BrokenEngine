#include "Pch.h"

#include "Network/Client/ClientDesyncCore.h"

#if defined(BT_CLIENT)

#include "Network/Client/ReconcileReplay.h"

#include "Game.h"
#include "Network/Client/ClientSession.h"

namespace engine
{

void ClientDesyncCore::OnDesyncDetected(ReconcileDesyncInfo&& rDesyncInfo)
{
	// Heap: Network sends for desync reporting
	ScopedSuppressAllocationTracking suppress;

	game::gpClientSession->mpRuntime->mpClient->SendDesyncReport(rDesyncInfo.iDesyncTick, rDesyncInfo.desyncCoord, rDesyncInfo.desyncExpectedCrc, rDesyncInfo.desyncActualCrc);
	if constexpr (kbDesyncDebugFrames)
	{
		game::gpClientSession->mpRuntime->mpClient->SendDebugFrameRequest(rDesyncInfo.iDesyncTick, rDesyncInfo.desyncCoord);
		game::gpClientSession->mpRuntime->mpClient->mStateFlags.Set(engine::Client::ClientStateFlags::kDesyncDebugMode);

		mDesyncDebugState.iTick = rDesyncInfo.iDesyncTick;
		mDesyncDebugState.coord = rDesyncInfo.desyncCoord;
		mDesyncDebugState.pClientFrame = std::move(rDesyncInfo.pDesyncClientFrame);
		mDesyncDebugState.entryTime = std::chrono::steady_clock::now();
	}
	else if constexpr (kbDesyncRecovery)
	{
		RecoverFromDesync();
	}
	else
	{
		std::snprintf(game::gpGame->mModalMessage, sizeof(game::gpGame->mModalMessage), "Desynced from server");
		game::gpClientSession->mpRuntime->mpClient->Disconnect();
	}
}

void ClientDesyncCore::PollDebugFrameResponse()
{
	std::unique_ptr<engine::ReceivedDebugFrame> pDebugFrame = std::move(game::gpClientSession->mpRuntime->mpClient->mpReceivedDebugFrame);
	if (pDebugFrame != nullptr && mDesyncDebugState.pClientFrame != nullptr)
	{
		if (pDebugFrame->iTick != mDesyncDebugState.iTick)
		{
			LOG(kNetwork, kDebug, "ClientDesyncCore::PollDebugFrameResponse Ignoring non-matching response Frame: {} Coord: ({},{}) Expected Frame: {} Coord: ({},{})", pDebugFrame->iTick, pDebugFrame->coord.x, pDebugFrame->coord.y, mDesyncDebugState.iTick, mDesyncDebugState.coord.x, mDesyncDebugState.coord.y);
			return;
		}

		if (pDebugFrame->coord != mDesyncDebugState.coord)
		{
			LOG(kNetwork, kDebug, "ClientDesyncCore::PollDebugFrameResponse Ignoring non-matching response Frame: {} Coord: ({},{}) Expected Frame: {} Coord: ({},{})", pDebugFrame->iTick, pDebugFrame->coord.x, pDebugFrame->coord.y, mDesyncDebugState.iTick, mDesyncDebugState.coord.x, mDesyncDebugState.coord.y);
			return;
		}

		LOG(kNetwork, kError, "ClientDesyncCore::PollDebugFrameResponse Frame: {} Coord: ({},{}) matched, dumping diff", mDesyncDebugState.iTick, mDesyncDebugState.coord.x, mDesyncDebugState.coord.y);
		game::gpClientSession->LogDesyncFrameDifferences(*mDesyncDebugState.pClientFrame, *pDebugFrame->pFrame);
		mDesyncDebugState = {};

		if constexpr (kbDesyncRecovery)
		{
			if constexpr (kbDebugBreak)
			{
				DEBUG_BREAK();
			}

			RecoverFromDesync();
		}
		else
		{
			std::snprintf(game::gpGame->mModalMessage, sizeof(game::gpGame->mModalMessage), "Desynced from server");
			game::gpClientSession->mpRuntime->mpClient->Disconnect();
		}
	}
}

bool ClientDesyncCore::PollDesyncTimeout()
{
	if (mDesyncDebugState.iTick < 0 || std::chrono::steady_clock::now() - mDesyncDebugState.entryTime <= kDesyncDebugTimeout)
	{
		return false;
	}

	mDesyncDebugState = {};
	if constexpr (kbDesyncRecovery)
	{
		LOG(kNetwork, kError, "ClientDesyncCore::PollDesyncTimeout Desync debug mode timed out, recovering without debug frame");
		RecoverFromDesync();
	}
	else
	{
		LOG(kNetwork, kError, "ClientDesyncCore::PollDesyncTimeout Desync debug mode timed out, disconnecting");
		std::snprintf(game::gpGame->mModalMessage, sizeof(game::gpGame->mModalMessage), "Desynced from server (debug frame timeout)");
		game::gpClientSession->mpRuntime->mpClient->Disconnect();
	}
	return true;
}

void ClientDesyncCore::RecoverFromDesync()
{
	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

	if (miDesyncCount > 0 && now - mFirstDesyncTime > kDesyncWindowDuration)
	{
		miDesyncCount = 0;
	}

	if (miDesyncCount == 0)
	{
		mFirstDesyncTime = now;
	}
	++miDesyncCount;

	LOG(kNetwork, kError, "ClientDesyncCore::RecoverFromDesync DesyncCount: {} / {}", miDesyncCount, kiMaxDesyncsBeforeDisconnect);

	if (miDesyncCount >= kiMaxDesyncsBeforeDisconnect)
	{
		LOG(kNetwork, kError, "ClientDesyncCore::RecoverFromDesync Escalating to disconnect");
		std::snprintf(game::gpGame->mModalMessage, sizeof(game::gpGame->mModalMessage), "Desynced from server");
		game::gpClientSession->mpRuntime->mpClient->Disconnect();
		return;
	}

	game::gpClientSession->mpRuntime->mpClient->SendResyncRequest();
	game::gpClientSession->mpRuntime->mpClient->mStateFlags.Clear(engine::Client::ClientStateFlags::kDesyncDebugMode);
	game::gpClientSession->ResetCoordStatesForResync();
}

void ClientDesyncCore::Reset()
{
	if (engine::gpClient != nullptr)
	{
		game::gpClientSession->mpRuntime->mpClient->mStateFlags.Clear(engine::Client::ClientStateFlags::kDesyncDebugMode);
	}
	mDesyncDebugState = {};
	mAgentFullStateFixtureState = {};
	miDesyncCount = 0;
}

void ClientDesyncCore::ArmAgentFullStateFixture(int64_t iTick, GridCoord coord)
{
	ASSERT(!IsStalled());
	mAgentFullStateFixtureState.bStalled = true;
	mAgentFullStateFixtureState.iTick = iTick;
	mAgentFullStateFixtureState.coord = coord;
	game::gpClientSession->mpRuntime->mpClient->mStateFlags.Set(engine::Client::ClientStateFlags::kDesyncDebugMode);
	game::gpClientSession->mpRuntime->mpClient->SendResyncRequest();
}

void ClientDesyncCore::ClearAgentFullStateFixture()
{
	mAgentFullStateFixtureState = {};
	if (engine::gpClient != nullptr && mDesyncDebugState.iTick < 0)
	{
		game::gpClientSession->mpRuntime->mpClient->mStateFlags.Clear(engine::Client::ClientStateFlags::kDesyncDebugMode);
	}
}

} // namespace engine

#endif // BT_CLIENT
