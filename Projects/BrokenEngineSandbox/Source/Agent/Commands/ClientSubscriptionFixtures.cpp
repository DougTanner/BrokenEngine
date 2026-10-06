#include "Agent/Commands/ClientSubscriptionFixtures.h"

#if defined(BT_CLIENT)

#include "Agent/Commands/ClientNetworkFixtures.h"
#include "Agent/AgentCommandServer.h"
#include "Network/Client/Client.h"
#include "Network/Client/ClientSessionRuntime.h"
#include "Network/NetworkMessages.h"

#include "Agent/AgentCommands.h"
#include "Network/Client/ClientSession.h"
#include "Game.h"

namespace game
{

static engine::Client& RequireFixtureClient(std::string_view command)
{
	if (gpGame == nullptr)
	{
		throw std::runtime_error(std::format("{} requires an accepted connected client", command));
	}
	if (gpClientSession == nullptr)
	{
		throw std::runtime_error(std::format("{} requires an accepted connected client", command));
	}
	if (gpClientSession->mpRuntime->mpClient == nullptr)
	{
		throw std::runtime_error(std::format("{} requires an accepted connected client", command));
	}
	engine::Client& rClient = *gpClientSession->mpRuntime->mpClient;
	if (!(rClient.mStateFlags & engine::Client::ClientStateFlags::kConnected))
	{
		throw std::runtime_error(std::format("{} requires an accepted connected client", command));
	}
	if (!(rClient.mStateFlags & engine::Client::ClientStateFlags::kConnectionAccepted))
	{
		throw std::runtime_error(std::format("{} requires an accepted connected client", command));
	}
	return rClient;
}

void CommandClientSubscribeAcceptFixture(const nlohmann::json& rParams, nlohmann::json& rResult)
{
	if constexpr (!kbDebugInput)
	{
		throw std::runtime_error("client_subscribe_accept_fixture requires kbDebugInput build");
	}
	else
	{
		ScopedSuppressAllocationTracking suppress;
		if (!rParams.is_object())
		{
			throw std::runtime_error("client_subscribe_accept_fixture requires exactly {\"slot\":int}");
		}
		if (rParams.size() != 1)
		{
			throw std::runtime_error("client_subscribe_accept_fixture requires exactly {\"slot\":int}");
		}
		if (!rParams.contains("slot"))
		{
			throw std::runtime_error("client_subscribe_accept_fixture requires exactly {\"slot\":int}");
		}
		if (!rParams.at("slot").is_number_integer())
		{
			throw std::runtime_error("client_subscribe_accept_fixture requires exactly {\"slot\":int}");
		}
		engine::Client& rClient = RequireFixtureClient("client_subscribe_accept_fixture");
		int64_t iSlot = rParams.at("slot").get<int64_t>();
		if (iSlot < std::ssize(rClient.mSubscriptions.mCoordinateSlots))
		{
			throw std::runtime_error("client_subscribe_accept_fixture 'slot' must be outside the client pool and below the rejection sentinel");
		}
		if (iSlot >= engine::kiSubscribeRejectSlot)
		{
			throw std::runtime_error("client_subscribe_accept_fixture 'slot' must be outside the client pool and below the rejection sentinel");
		}

		engine::ClientNetworkFixtures::SubscribeAcceptResult result = engine::ClientNetworkFixtures::ReceiveSubscribeAccept(rClient, iSlot, 0, {});
		rResult["slot"] = iSlot;
		rResult["outOfRange"] = true;
		rResult["cleanupSerialized"] = result.iSerializedBytes == engine::NetworkMessages::ClientUnsubscribeMessage::kiFixedSize;
		rResult["cleanupSlot"] = result.iSerializedSlot;
		rResult["cleanupSize"] = result.iSerializedBytes;
		rResult["networkSendSuppressed"] = result.bSendSuppressed;
	}
}

void CommandClientStaleUpdateFixture(const nlohmann::json& rParams, [[maybe_unused]] nlohmann::json& rResult)
{
	if constexpr (!kbDebugInput)
	{
		throw std::runtime_error("client_stale_update_fixture requires kbDebugInput build");
	}
	else
	{
		ScopedSuppressAllocationTracking suppress;
		if (!rParams.is_object())
		{
			throw std::runtime_error("client_stale_update_fixture requires exactly {}");
		}
		if (!rParams.empty())
		{
			throw std::runtime_error("client_stale_update_fixture requires exactly {}");
		}
		engine::Client& rClient = RequireFixtureClient("client_stale_update_fixture");
		bool bHasActiveConfirmedCoordinate = std::ranges::any_of(rClient.mSubscriptions.mCoordinateSlots, [](const engine::ClientCoordSlot& rSlot)
		{
			auto it = gpGame->mCoordinateFrames.find(rSlot.coordinate);
			return rSlot.eState == engine::CoordSubscriptionState::kActive && it != gpGame->mCoordinateFrames.end() && it->second.iConfirmedTick >= 0;
		});
		if (!bHasActiveConfirmedCoordinate)
		{
			throw std::runtime_error("client_stale_update_fixture requires an active confirmed coord");
		}

		std::shared_ptr<engine::ClientNetworkFixtures::StaleUpdateState> pState = std::make_shared<engine::ClientNetworkFixtures::StaleUpdateState>();
		engine::Client* pClient = &rClient;
		engine::ClientNetworkFixtures::ArmStaleUpdate(rClient, pState);
		engine::gpAgentCommandServer->DeferResponse([pState, pClient]() -> std::optional<nlohmann::json>
		{
			if (gpClientSession == nullptr)
			{
				throw std::runtime_error("client_stale_update_fixture client was replaced or disconnected");
			}
			if (gpClientSession->mpRuntime->mpClient.get() != pClient)
			{
				throw std::runtime_error("client_stale_update_fixture client was replaced or disconnected");
			}
			if (pState->flags & engine::ClientNetworkFixtures::StaleUpdateFlags::kReset)
			{
				throw std::runtime_error("client_stale_update_fixture was cleared by a server load reset");
			}
			if (pState->flags & engine::ClientNetworkFixtures::StaleUpdateFlags::kBoundExpired)
			{
				throw std::runtime_error("client_stale_update_fixture capture/readiness bound expired");
			}
			if (!(pState->flags & engine::ClientNetworkFixtures::StaleUpdateFlags::kComplete))
			{
				return std::nullopt;
			}

			nlohmann::json result;
			result["coord"] = {pState->coord.iX, pState->coord.iY};
			result["slot"] = pState->iSlotIndex;
			result["epoch"] = pState->iEpoch;
			result["tick"] = pState->iTick;
			result["capturedBytes"] = pState->iCapturedBytes;
			result["ackFloorBefore"] = pState->iAckFloorBefore;
			result["ackFloorAfter"] = pState->iAckFloorAfter;
			result["confirmedBefore"] = pState->iConfirmedBefore;
			result["confirmedTick"] = pState->iConfirmedAfter;
			result["retainedUpdate"] = static_cast<bool>(pState->flags & engine::ClientNetworkFixtures::StaleUpdateFlags::kRetainedAfterDrain);
			result["connected"] = static_cast<bool>(pState->flags & engine::ClientNetworkFixtures::StaleUpdateFlags::kConnectedAfterDrain);
			return result;
		});
	}
}

void CommandClientCancelledSubscriptionFixture([[maybe_unused]] const nlohmann::json& rParams, [[maybe_unused]] nlohmann::json& rResult)
{
	if constexpr (!kbDebugInput)
	{
		throw std::runtime_error("client_cancelled_subscription_fixture requires kbDebugInput build");
	}
	else
	{
		ScopedSuppressAllocationTracking suppress;
		if (!rParams.is_object())
		{
			throw std::runtime_error("client_cancelled_subscription_fixture requires exactly {\"coord\":[x,y]}");
		}
		if (rParams.size() != 1)
		{
			throw std::runtime_error("client_cancelled_subscription_fixture requires exactly {\"coord\":[x,y]}");
		}
		if (!rParams.contains("coord"))
		{
			throw std::runtime_error("client_cancelled_subscription_fixture requires exactly {\"coord\":[x,y]}");
		}
		const nlohmann::json& rCoordinate = rParams.at("coord");
		if (!rCoordinate.is_array())
		{
			throw std::runtime_error("client_cancelled_subscription_fixture 'coord' must be an array of 2 integers");
		}
		if (rCoordinate.size() != 2)
		{
			throw std::runtime_error("client_cancelled_subscription_fixture 'coord' must be an array of 2 integers");
		}
		engine::GridCoord coordinate
		{
			.iX = static_cast<int32_t>(ClientGridCoordinateValue(rCoordinate.at(0), "client_cancelled_subscription_fixture")),
			.iY = static_cast<int32_t>(ClientGridCoordinateValue(rCoordinate.at(1), "client_cancelled_subscription_fixture")),
		};
		engine::Client& rClient = RequireFixtureClient("client_cancelled_subscription_fixture");
		if (!(gpGame->ClientPlayerIdentifier().iValue != 0))
		{
			throw std::runtime_error("client_cancelled_subscription_fixture requires an assigned player");
		}
		for (const engine::ClientCoordSlot& rSlot : rClient.mSubscriptions.mCoordinateSlots)
		{
			if (rSlot.eState != engine::CoordSubscriptionState::kUnsubscribed && rSlot.coordinate == coordinate)
			{
				throw std::runtime_error("client_cancelled_subscription_fixture coord is already active");
			}
		}
		// A real outstanding subscribe could be answered at the fixture's slot, which would make its accept a ghost
		if (!rClient.mSubscriptions.mSubscribeRequests.mRecords.empty())
		{
			throw std::runtime_error("client_cancelled_subscription_fixture requires no outstanding subscribe request");
		}
		engine::ClientSessionRuntime& rRuntime = *gpClientSession->mpRuntime;
		if (std::ranges::contains(rRuntime.mDesiredCoordinates, coordinate))
		{
			throw std::runtime_error("client_cancelled_subscription_fixture coord is owned by subscription policy");
		}
		if (std::ranges::contains(rRuntime.mSubscriptionQueue, coordinate))
		{
			throw std::runtime_error("client_cancelled_subscription_fixture coord is owned by subscription policy");
		}
		if (rRuntime.mUnwantedTimestamps.contains(coordinate))
		{
			throw std::runtime_error("client_cancelled_subscription_fixture coord is owned by subscription policy");
		}
		int64_t iSlot = -1;
		for (int64_t i = 0; i < std::ssize(rClient.mSubscriptions.mCoordinateSlots); ++i)
		{
			if (rClient.mSubscriptions.mCoordinateSlots.at(i).eState == engine::CoordSubscriptionState::kUnsubscribed
			 && rClient.mReceivedCoordinateUpdates.at(i).empty())
			{
				iSlot = i;
				break;
			}
		}
		if (iSlot < 0)
		{
			throw std::runtime_error("client_cancelled_subscription_fixture requires a clean unsubscribed slot");
		}
		std::shared_ptr<engine::ClientNetworkFixtures::CancelledSubscriptionState> pState = std::make_shared<engine::ClientNetworkFixtures::CancelledSubscriptionState>();
		pState->iSlot = iSlot;
		engine::ClientNetworkFixtures::ArmCancelledSubscription(rClient, pState);

		std::vector<engine::GridCoord> desiredBefore = rRuntime.mDesiredCoordinates;
		std::unordered_map<engine::GridCoord, std::chrono::steady_clock::time_point> stickyBefore = rRuntime.mUnwantedTimestamps;
		std::vector<engine::GridCoord> queueBefore = rRuntime.mSubscriptionQueue;
		engine::ClientCoordSlot& rSlot = rClient.mSubscriptions.mCoordinateSlots.at(iSlot);
		rClient.mSubscriptions.mSubscribeRequests.Add(coordinate);
		int64_t iRetainedEpoch = rSlot.acknowledgementState.uiEpoch;
		uint16_t uiEpoch = static_cast<uint16_t>(iRetainedEpoch + 1);
		if (uiEpoch == 0)
		{
			uiEpoch = 1;
		}
		rClient.mSubscriptions.mSubscribeRequests.Cancel(coordinate);
		bool bCancelledToUnsubscribed = !rClient.mSubscriptions.mSubscribeRequests.IsLive(coordinate) && rSlot.eState == engine::CoordSubscriptionState::kUnsubscribed;

		common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
		common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();
		engine::NetworkMessages::ServerSubscribeAcceptMessage accept
		{
			.uiLoadGeneration = static_cast<uint8_t>(rClient.miCommittedLoadGeneration),
			.uiSlotIndex = static_cast<uint8_t>(iSlot),
			.uiEpoch = uiEpoch,
			.coord = coordinate,
		};
		engine::NetworkMessages::Write(rWorkbuffer, accept);
		rClient.Receive(rWorkbuffer.Span<uint8_t>());
		bool bAcceptToUnsubscribing = rSlot.eState == engine::CoordSubscriptionState::kUnsubscribing;
		if (bAcceptToUnsubscribing)
		{
			// The server never issued the invented epoch; retaining it would make the stale-epoch check drop
			// the server's genuine accept when it next allocates this slot with that same epoch
			rSlot.acknowledgementState.uiEpoch = static_cast<uint16_t>(iRetainedEpoch);
		}
		bool bPolicyUnchanged = desiredBefore == rRuntime.mDesiredCoordinates && stickyBefore == rRuntime.mUnwantedTimestamps
		                     && queueBefore == rRuntime.mSubscriptionQueue;
		if (!bCancelledToUnsubscribed)
		{
			throw std::runtime_error("client_cancelled_subscription_fixture immediate transition or policy check failed");
		}
		if (!bAcceptToUnsubscribing)
		{
			throw std::runtime_error("client_cancelled_subscription_fixture immediate transition or policy check failed");
		}
		if (!bPolicyUnchanged)
		{
			throw std::runtime_error("client_cancelled_subscription_fixture immediate transition or policy check failed");
		}

		engine::Client* pClient = &rClient;
		std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + 4s;
		engine::gpAgentCommandServer->DeferResponse([pState, pClient, coordinate, iSlot, bPolicyUnchanged, bCancelledToUnsubscribed, bAcceptToUnsubscribing, deadline]() -> std::optional<nlohmann::json>
		{
			if (gpClientSession == nullptr)
			{
				throw std::runtime_error("client_cancelled_subscription_fixture client was replaced or disconnected");
			}
			if (gpClientSession->mpRuntime->mpClient.get() != pClient)
			{
				throw std::runtime_error("client_cancelled_subscription_fixture client was replaced or disconnected");
			}
			if (!(pClient->mStateFlags & engine::Client::ClientStateFlags::kConnected))
			{
				throw std::runtime_error("client_cancelled_subscription_fixture client was replaced or disconnected");
			}
			if (pState->eOutcome == engine::ClientNetworkFixtures::CancelledSubscriptionOutcome::kReset)
			{
				throw std::runtime_error("client_cancelled_subscription_fixture was cleared by a server load reset");
			}
			engine::CoordSubscriptionState eState = pClient->mSubscriptions.mCoordinateSlots.at(iSlot).eState;
			if (pState->eOutcome == engine::ClientNetworkFixtures::CancelledSubscriptionOutcome::kAcked
			 && eState == engine::CoordSubscriptionState::kUnsubscribed)
			{
				nlohmann::json result;
				result["coord"] = {coordinate.iX, coordinate.iY};
				result["slot"] = iSlot;
				result["initialState"] = "subscribing";
				result["afterCancelState"] = "unsubscribed";
				result["afterAcceptState"] = "unsubscribing";
				result["subscribingToUnsubscribed"] = bCancelledToUnsubscribed;
				result["acceptToUnsubscribing"] = bAcceptToUnsubscribing;
				result["policyUnchanged"] = bPolicyUnchanged;
				result["ackRetired"] = true;
				return result;
			}
			if (pState->eOutcome != engine::ClientNetworkFixtures::CancelledSubscriptionOutcome::kPending)
			{
				throw std::runtime_error("client_cancelled_subscription_fixture observed an unexpected slot state");
			}
			if (eState != engine::CoordSubscriptionState::kUnsubscribing)
			{
				throw std::runtime_error("client_cancelled_subscription_fixture observed an unexpected slot state");
			}
			if (std::chrono::steady_clock::now() >= deadline)
			{
				throw std::runtime_error("client_cancelled_subscription_fixture timed out before unsubscribe ACK");
			}
			return std::nullopt;
		});
	}
}

void DetachClientSubscriptionFixtures([[maybe_unused]] const ClientSession& rSession)
{
	engine::ClientNetworkFixtures::Detach();
}

} // namespace game

#endif // BT_CLIENT
