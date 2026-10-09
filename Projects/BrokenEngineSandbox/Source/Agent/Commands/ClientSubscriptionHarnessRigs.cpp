#include "Agent/Commands/ClientSubscriptionHarnessRigs.h"

#if defined(BT_CLIENT)

#include "Agent/Commands/ClientNetworkHarnessRigs.h"
#include "Agent/AgentCommandServer.h"
#include "Network/Client/Client.h"
#include "Network/Client/ClientSessionRuntime.h"
#include "Network/NetworkMessages.h"

#include "Agent/AgentCommands.h"
#include "Network/Client/ClientSession.h"
#include "Game.h"

namespace game
{

void CommandClientSubscribeAcceptHarnessRig(const nlohmann::json& rParams, nlohmann::json& rResult)
{
	if constexpr (!kbDebugInput)
	{
		throw std::runtime_error("client_subscribe_accept_harness_rig requires kbDebugInput build");
	}
	else
	{
		ScopedSuppressAllocationTracking suppress;
		if (!rParams.is_object())
		{
			throw std::runtime_error("client_subscribe_accept_harness_rig requires exactly {\"slot\":int}");
		}
		if (rParams.size() != 1)
		{
			throw std::runtime_error("client_subscribe_accept_harness_rig requires exactly {\"slot\":int}");
		}
		if (!rParams.contains("slot"))
		{
			throw std::runtime_error("client_subscribe_accept_harness_rig requires exactly {\"slot\":int}");
		}
		if (!rParams.at("slot").is_number_integer())
		{
			throw std::runtime_error("client_subscribe_accept_harness_rig requires exactly {\"slot\":int}");
		}
		engine::Client& rClient = RequireHarnessRigClient("client_subscribe_accept_harness_rig");
		int64_t iSlot = rParams.at("slot").get<int64_t>();
		if (iSlot < std::ssize(rClient.mSubscriptions.mCoordinateSlots))
		{
			throw std::runtime_error("client_subscribe_accept_harness_rig 'slot' must be outside the client pool and below the rejection sentinel");
		}
		if (iSlot >= engine::kiSubscribeRejectSlot)
		{
			throw std::runtime_error("client_subscribe_accept_harness_rig 'slot' must be outside the client pool and below the rejection sentinel");
		}

		engine::ClientNetworkHarnessRigs::SubscribeAcceptResult result = engine::ClientNetworkHarnessRigs::ReceiveSubscribeAccept(rClient, iSlot, 0, {});
		rResult["slot"] = iSlot;
		rResult["outOfRange"] = true;
		rResult["cleanupSerialized"] = result.iSerializedBytes == engine::NetworkMessages::ClientUnsubscribeMessage::kiFixedSize;
		rResult["cleanupSlot"] = result.iSerializedSlot;
		rResult["cleanupSize"] = result.iSerializedBytes;
		rResult["networkSendSuppressed"] = result.bSendSuppressed;
	}
}

void CommandClientStaleUpdateHarnessRig(const nlohmann::json& rParams, [[maybe_unused]] nlohmann::json& rResult)
{
	if constexpr (!kbDebugInput)
	{
		throw std::runtime_error("client_stale_update_harness_rig requires kbDebugInput build");
	}
	else
	{
		ScopedSuppressAllocationTracking suppress;
		if (!rParams.is_object())
		{
			throw std::runtime_error("client_stale_update_harness_rig requires exactly {}");
		}
		if (!rParams.empty())
		{
			throw std::runtime_error("client_stale_update_harness_rig requires exactly {}");
		}
		engine::Client& rClient = RequireHarnessRigClient("client_stale_update_harness_rig");
		bool bHasActiveConfirmedCoordinate = std::ranges::any_of(rClient.mSubscriptions.mCoordinateSlots, [](const engine::ClientCoordSlot& rSlot)
		{
			auto it = gpGame->mCells.find(rSlot.coordinate);
			return rSlot.eState == engine::CoordSubscriptionState::kActive && it != gpGame->mCells.end() && it->second.iConfirmedTick >= 0;
		});
		if (!bHasActiveConfirmedCoordinate)
		{
			throw std::runtime_error("client_stale_update_harness_rig requires an active confirmed coord");
		}

		std::shared_ptr<engine::ClientNetworkHarnessRigs::StaleUpdateState> pState = std::make_shared<engine::ClientNetworkHarnessRigs::StaleUpdateState>();
		engine::Client* pClient = &rClient;
		engine::ClientNetworkHarnessRigs::ArmStaleUpdate(rClient, pState);
		engine::gpAgentCommandServer->DeferResponse([pState, pClient]() -> std::optional<nlohmann::json>
		{
			if (gpClientSession->mpRuntime->mpClient.get() != pClient)
			{
				throw std::runtime_error("client_stale_update_harness_rig client was replaced or disconnected");
			}
			if (pState->flags & engine::ClientNetworkHarnessRigs::StaleUpdateFlags::kReset)
			{
				throw std::runtime_error("client_stale_update_harness_rig was cleared by a server load reset");
			}
			if (pState->flags & engine::ClientNetworkHarnessRigs::StaleUpdateFlags::kBoundExpired)
			{
				throw std::runtime_error("client_stale_update_harness_rig capture/readiness bound expired");
			}
			if (!(pState->flags & engine::ClientNetworkHarnessRigs::StaleUpdateFlags::kComplete))
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
			result["retainedUpdate"] = static_cast<bool>(pState->flags & engine::ClientNetworkHarnessRigs::StaleUpdateFlags::kRetainedAfterDrain);
			result["connected"] = static_cast<bool>(pState->flags & engine::ClientNetworkHarnessRigs::StaleUpdateFlags::kConnectedAfterDrain);
			return result;
		});
	}
}

void CommandClientCancelledSubscriptionHarnessRig([[maybe_unused]] const nlohmann::json& rParams, [[maybe_unused]] nlohmann::json& rResult)
{
	if constexpr (!kbDebugInput)
	{
		throw std::runtime_error("client_cancelled_subscription_harness_rig requires kbDebugInput build");
	}
	else
	{
		ScopedSuppressAllocationTracking suppress;
		if (!rParams.is_object())
		{
			throw std::runtime_error("client_cancelled_subscription_harness_rig requires exactly {\"coord\":[x,y]}");
		}
		if (rParams.size() != 1)
		{
			throw std::runtime_error("client_cancelled_subscription_harness_rig requires exactly {\"coord\":[x,y]}");
		}
		if (!rParams.contains("coord"))
		{
			throw std::runtime_error("client_cancelled_subscription_harness_rig requires exactly {\"coord\":[x,y]}");
		}
		const nlohmann::json& rCoordinate = rParams.at("coord");
		if (!rCoordinate.is_array())
		{
			throw std::runtime_error("client_cancelled_subscription_harness_rig 'coord' must be an array of 2 integers");
		}
		if (rCoordinate.size() != 2)
		{
			throw std::runtime_error("client_cancelled_subscription_harness_rig 'coord' must be an array of 2 integers");
		}
		engine::GridCoord coordinate
		{
			.iX = static_cast<int32_t>(ClientGridCoordinateValue(rCoordinate.at(0), "client_cancelled_subscription_harness_rig")),
			.iY = static_cast<int32_t>(ClientGridCoordinateValue(rCoordinate.at(1), "client_cancelled_subscription_harness_rig")),
		};
		engine::Client& rClient = RequireHarnessRigClient("client_cancelled_subscription_harness_rig");
		if (!(gpGame->ClientPlayerIdentifier().iValue != 0))
		{
			throw std::runtime_error("client_cancelled_subscription_harness_rig requires an assigned player");
		}
		for (const engine::ClientCoordSlot& rSlot : rClient.mSubscriptions.mCoordinateSlots)
		{
			if (rSlot.eState != engine::CoordSubscriptionState::kUnsubscribed && rSlot.coordinate == coordinate)
			{
				throw std::runtime_error("client_cancelled_subscription_harness_rig coord is already active");
			}
		}
		// A real outstanding subscribe could be answered at the harness rig's slot, which would make its accept a ghost
		if (!rClient.mSubscriptions.mSubscribeRequests.mRecords.empty())
		{
			throw std::runtime_error("client_cancelled_subscription_harness_rig requires no outstanding subscribe request");
		}
		engine::ClientSessionRuntime& rRuntime = *gpClientSession->mpRuntime;
		if (std::ranges::contains(rRuntime.mDesiredCoordinates, coordinate))
		{
			throw std::runtime_error("client_cancelled_subscription_harness_rig coord is owned by subscription policy");
		}
		if (std::ranges::contains(rRuntime.mSubscriptionQueue, coordinate))
		{
			throw std::runtime_error("client_cancelled_subscription_harness_rig coord is owned by subscription policy");
		}
		if (rRuntime.mUnwantedTimestamps.contains(coordinate))
		{
			throw std::runtime_error("client_cancelled_subscription_harness_rig coord is owned by subscription policy");
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
			throw std::runtime_error("client_cancelled_subscription_harness_rig requires a clean unsubscribed slot");
		}
		std::shared_ptr<engine::ClientNetworkHarnessRigs::CancelledSubscriptionState> pState = std::make_shared<engine::ClientNetworkHarnessRigs::CancelledSubscriptionState>();
		pState->iSlot = iSlot;
		engine::ClientNetworkHarnessRigs::ArmCancelledSubscription(rClient, pState);

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
			throw std::runtime_error("client_cancelled_subscription_harness_rig immediate transition or policy check failed");
		}
		if (!bAcceptToUnsubscribing)
		{
			throw std::runtime_error("client_cancelled_subscription_harness_rig immediate transition or policy check failed");
		}
		if (!bPolicyUnchanged)
		{
			throw std::runtime_error("client_cancelled_subscription_harness_rig immediate transition or policy check failed");
		}

		engine::Client* pClient = &rClient;
		std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + 4s;
		engine::gpAgentCommandServer->DeferResponse([pState, pClient, coordinate, iSlot, bPolicyUnchanged, bCancelledToUnsubscribed, bAcceptToUnsubscribing, deadline]() -> std::optional<nlohmann::json>
		{
			if (gpClientSession->mpRuntime->mpClient.get() != pClient)
			{
				throw std::runtime_error("client_cancelled_subscription_harness_rig client was replaced or disconnected");
			}
			if (!(pClient->mStateFlags & engine::Client::ClientStateFlags::kConnected))
			{
				throw std::runtime_error("client_cancelled_subscription_harness_rig client was replaced or disconnected");
			}
			if (pState->eOutcome == engine::ClientNetworkHarnessRigs::CancelledSubscriptionOutcome::kReset)
			{
				throw std::runtime_error("client_cancelled_subscription_harness_rig was cleared by a server load reset");
			}
			engine::CoordSubscriptionState eState = pClient->mSubscriptions.mCoordinateSlots.at(iSlot).eState;
			if (pState->eOutcome == engine::ClientNetworkHarnessRigs::CancelledSubscriptionOutcome::kAcked
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
			if (pState->eOutcome != engine::ClientNetworkHarnessRigs::CancelledSubscriptionOutcome::kPending)
			{
				throw std::runtime_error("client_cancelled_subscription_harness_rig observed an unexpected slot state");
			}
			if (eState != engine::CoordSubscriptionState::kUnsubscribing)
			{
				throw std::runtime_error("client_cancelled_subscription_harness_rig observed an unexpected slot state");
			}
			if (std::chrono::steady_clock::now() >= deadline)
			{
				throw std::runtime_error("client_cancelled_subscription_harness_rig timed out before unsubscribe ACK");
			}
			return std::nullopt;
		});
	}
}

void DetachClientSubscriptionHarnessRigs([[maybe_unused]] const ClientSession& rSession)
{
	engine::ClientNetworkHarnessRigs::Detach();
}

} // namespace game

#endif // BT_CLIENT
