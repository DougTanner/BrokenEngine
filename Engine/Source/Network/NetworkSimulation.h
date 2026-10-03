#pragma once

namespace engine
{

// Network simulation levels for testing different real-world latency scenarios (East Coast server)
enum class NetworkSimulationLevel : uint8_t
{
	kDisabled,
	kEastCoast,    // East Coast to East Coast
	kWestCoast,    // West Coast to East Coast
	kEurope,       // Europe to East Coast
	kSouthAmerica, // South America to East Coast
	kChina,        // China (behind firewall) to East Coast
};

struct NetworkSimulationConfig
{
	float fPacketLossPercent = 0.0f;
	int64_t iPingMinimumMilliseconds = 0;
	int64_t iPingMaximumMilliseconds = 0;
};

// Applied per-direction, so half-ping delay on each side
inline constexpr NetworkSimulationConfig GetNetworkSimulationConfiguration(NetworkSimulationLevel eLevel)
{
	switch (eLevel)
	{
		case NetworkSimulationLevel::kEastCoast:    return {.fPacketLossPercent = 0.5f,  .iPingMinimumMilliseconds = 20,  .iPingMaximumMilliseconds = 40};
		case NetworkSimulationLevel::kWestCoast:    return {.fPacketLossPercent = 1.0f,  .iPingMinimumMilliseconds = 60,  .iPingMaximumMilliseconds = 90};
		case NetworkSimulationLevel::kEurope:       return {.fPacketLossPercent = 1.5f,  .iPingMinimumMilliseconds = 80, .iPingMaximumMilliseconds = 130};
		case NetworkSimulationLevel::kSouthAmerica: return {.fPacketLossPercent = 2.0f, .iPingMinimumMilliseconds = 120, .iPingMaximumMilliseconds = 200};
		case NetworkSimulationLevel::kChina:        return {.fPacketLossPercent = 2.5f, .iPingMinimumMilliseconds = 300, .iPingMaximumMilliseconds = 500};
		default:                                    return {.fPacketLossPercent = 0.0f,   .iPingMinimumMilliseconds = 0,   .iPingMaximumMilliseconds = 0};
	}
}

inline constexpr std::string_view GetNetworkSimulationName(NetworkSimulationLevel eLevel)
{
	switch (eLevel)
	{
		case NetworkSimulationLevel::kEastCoast:    return "EastCoast";
		case NetworkSimulationLevel::kWestCoast:    return "WestCoast";
		case NetworkSimulationLevel::kEurope:       return "Europe";
		case NetworkSimulationLevel::kSouthAmerica: return "SouthAmerica";
		case NetworkSimulationLevel::kChina:        return "China";
		default:                                    return "Off";
	}
}

struct NetworkSimulationBounds
{
	int64_t iCrcMinimum = 0;
	int64_t iAssumedMaximum = 0;
	int64_t iFastReplayMaximum = 0;
	int64_t iStatusReplayMaximum = 0;
	int64_t iKnockOnReplayMaximum = 0;
};

inline constexpr NetworkSimulationBounds GetNetworkSimulationBounds(NetworkSimulationLevel eLevel)
{
	switch (eLevel)
	{
		case NetworkSimulationLevel::kDisabled:     return {.iCrcMinimum = 62,  .iAssumedMaximum = 2,  .iFastReplayMaximum = 2,  .iStatusReplayMaximum = 2,  .iKnockOnReplayMaximum = 2};
		case NetworkSimulationLevel::kEastCoast:    return {.iCrcMinimum = 50,  .iAssumedMaximum = 8,  .iFastReplayMaximum = 6, .iStatusReplayMaximum = 10, .iKnockOnReplayMaximum = 10};
		case NetworkSimulationLevel::kWestCoast:    return {.iCrcMinimum = 40, .iAssumedMaximum = 20, .iFastReplayMaximum = 20, .iStatusReplayMaximum = 20, .iKnockOnReplayMaximum = 24};
		case NetworkSimulationLevel::kEurope:       return {.iCrcMinimum = 32, .iAssumedMaximum = 32, .iFastReplayMaximum = 32, .iStatusReplayMaximum = 30, .iKnockOnReplayMaximum = 36};
		case NetworkSimulationLevel::kSouthAmerica: return {.iCrcMinimum = 24, .iAssumedMaximum = 50, .iFastReplayMaximum = 50, .iStatusReplayMaximum = 44, .iKnockOnReplayMaximum = 50};
		case NetworkSimulationLevel::kChina:        return {.iCrcMinimum = 12, .iAssumedMaximum = 90, .iFastReplayMaximum = 600, .iStatusReplayMaximum = 70, .iKnockOnReplayMaximum = 80};
		default:                                    return {.iCrcMinimum = 62,  .iAssumedMaximum = 2,  .iFastReplayMaximum = 2,  .iStatusReplayMaximum = 2,  .iKnockOnReplayMaximum = 2};
	}
}

struct DelayedPacket
{
	std::chrono::steady_clock::time_point releaseTime;
	std::vector<uint8_t> data;
	ENetPeer* pPeer = nullptr;
	uint8_t uiChannelIdentifier = 0;
};

// Each Client or Server owns one simulation state, shared across its connections.
struct NetworkSimulationState
{
	static constexpr uint32_t kuiSeed = 0x9E3779B9u; // Constant seed keeps simulated latency and loss reproducible across runs.
	uint32_t uiRandomState = kuiSeed;
	int64_t iConsecutiveDrops[NetworkManager::kuiChannelCount] {};
	std::chrono::steady_clock::time_point channelReleaseTimes[NetworkManager::kuiChannelCount] {};
	int64_t iCoordinateDropCounts[NetworkManager::kiMaximumEnetCoordinateSlots] {};
	int64_t iControlDropCount = 0;
};

namespace NetworkSimulation
{

inline float Random01(NetworkSimulationState& rState)
{
	rState.uiRandomState = rState.uiRandomState * 1'103'515'245 + 12'345;
	return static_cast<float>(rState.uiRandomState >> 16) / 65'536.0f;
}

inline std::chrono::steady_clock::duration RandomOneWayDelay(NetworkSimulationState& rState, const NetworkSimulationConfig& rConfiguration)
{
	int64_t iHalfMinimum = rConfiguration.iPingMinimumMilliseconds / 2;
	int64_t iHalfMaximum = rConfiguration.iPingMaximumMilliseconds / 2;
	int64_t iDelayMilliseconds = iHalfMinimum + static_cast<int64_t>(Random01(rState) * static_cast<float>(iHalfMaximum - iHalfMinimum));
	return std::chrono::milliseconds(iDelayMilliseconds);
}

struct DropResult
{
	bool bDrop = false;
	int64_t iConsecutive = 0;
};

inline DropResult ShouldDrop(NetworkSimulationState& rState, const NetworkSimulationConfig& rConfiguration, uint8_t uiChannel)
{
	static constexpr int64_t kiMaxConsecutiveDrops = kiNetworkBufferSize / 2;

	int64_t& riDrops = rState.iConsecutiveDrops[uiChannel];
	bool bDrop = false;
	if (riDrops > 0)
	{
		bDrop = riDrops < kiMaxConsecutiveDrops && Random01(rState) < 0.5f;
	}
	else
	{
		bDrop = Random01(rState) * 100.0f < rConfiguration.fPacketLossPercent;
	}

	riDrops = bDrop ? riDrops + 1 : 0;
	return {.bDrop = bDrop, .iConsecutive = riDrops};
}

// upper_bound (not lower_bound) inserts after equal-key elements, keeping same-channel FIFO stable
// when the reliable monotonic-release clamp produces identical release times.
inline void EnqueueDelayed(std::deque<DelayedPacket>& rDelayedPackets, const ENetEvent& rEvent, std::chrono::steady_clock::time_point releaseTime)
{
	ScopedSuppressAllocationTracking suppress;
	// Heap: delay queue copies packet data for deferred processing
	DelayedPacket delayed {};
	delayed.releaseTime = releaseTime;
	delayed.data.assign(rEvent.packet->data, rEvent.packet->data + rEvent.packet->dataLength);
	delayed.pPeer = rEvent.peer;
	delayed.uiChannelIdentifier = rEvent.channelID;
	auto it = std::upper_bound(rDelayedPackets.begin(), rDelayedPackets.end(), delayed, [](const DelayedPacket& rLeft, const DelayedPacket& rRight)
	{
		return rLeft.releaseTime < rRight.releaseTime;
	});
	rDelayedPackets.insert(it, std::move(delayed));
	enet_packet_destroy(rEvent.packet);
}

// Unreliable packets may be dropped per the simulation configuration; reliable packets are never dropped and retain per-channel FIFO release order.
inline void EnqueueOrDrop(std::deque<DelayedPacket>& rDelayedPackets, NetworkSimulationState& rState, const NetworkSimulationConfig& rSimulationConfiguration, const ENetEvent& rEvent)
{
	bool bUnreliable = NetworkManager::IsUnreliableChannel(rEvent.channelID);
	if (bUnreliable)
	{
		DropResult dropResult = ShouldDrop(rState, rSimulationConfiguration, rEvent.channelID);
		if (dropResult.bDrop)
		{
			if (NetworkManager::IsCoordinateChannel(rEvent.channelID))
			{
				int64_t iSlot = NetworkManager::ChannelToSlot(rEvent.channelID);
				uint8_t uiPacketType = (rEvent.packet->dataLength > 0) ? rEvent.packet->data[0] : 0;
				int64_t iTick = NetworkMessages::GetCoordUpdateTickOrZero(std::span<const uint8_t>(rEvent.packet->data, rEvent.packet->dataLength));
				++rState.iCoordinateDropCounts[iSlot];
				LOG(kNetwork, kVerbose, "NetworkSimulation::Dropped Coord Slot: {} Tick: {} Type: {} Size: {} TotalDrops: {} Consecutive: {}", iSlot, iTick, PacketTypeName(static_cast<PacketType>(uiPacketType)), rEvent.packet->dataLength, rState.iCoordinateDropCounts[iSlot], dropResult.iConsecutive);
			}
			else
			{
				++rState.iControlDropCount;
				LOG(kNetwork, kVerbose, "NetworkSimulation::Dropped Control Channel: {} Size: {} TotalDrops: {} Consecutive: {}", rEvent.channelID, rEvent.packet->dataLength, rState.iControlDropCount, dropResult.iConsecutive);
			}
			enet_packet_destroy(rEvent.packet);
			return;
		}
		EnqueueDelayed(rDelayedPackets, rEvent, std::chrono::steady_clock::now() + RandomOneWayDelay(rState, rSimulationConfiguration));
	}
	else
	{
		std::chrono::steady_clock::time_point releaseTime = std::max(std::chrono::steady_clock::now() + RandomOneWayDelay(rState, rSimulationConfiguration), rState.channelReleaseTimes[rEvent.channelID]);
		rState.channelReleaseTimes[rEvent.channelID] = releaseTime;
		EnqueueDelayed(rDelayedPackets, rEvent, releaseTime);
	}
}

// Fast-forward (time multiply > 1) skips the delay, but queues behind still-pending delayed packets so each
// channel stays FIFO until the caller's flush; otherwise enqueue-or-drop per the sim config.
template <typename FNRECEIVE>
inline void DispatchOrEnqueue(std::deque<DelayedPacket>& rDelayedPackets, NetworkSimulationState& rState, const NetworkSimulationConfig& rSimulationConfiguration, bool bFastForward, ENetEvent& rEvent, FNRECEIVE Receive)
{
	if (bFastForward)
	{
		if (rDelayedPackets.empty())
		{
			Receive(rEvent);
			enet_packet_destroy(rEvent.packet);
		}
		else
		{
			// The queue is sorted by release time, so the last release time appends in arrival order.
			EnqueueDelayed(rDelayedPackets, rEvent, rDelayedPackets.back().releaseTime);
		}
	}
	else
	{
		EnqueueOrDrop(rDelayedPackets, rState, rSimulationConfiguration, rEvent);
	}
}

template <typename FNHANDLEPACKET>
inline void ProcessDelayed(std::deque<DelayedPacket>& rDelayedPackets, FNHANDLEPACKET HandlePacket)
{
	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	while (!rDelayedPackets.empty() && rDelayedPackets.front().releaseTime <= now)
	{
		HandlePacket(rDelayedPackets.front());
		rDelayedPackets.pop_front();
	}
}

template <typename FNHANDLEPACKET>
inline void FlushDelayed(std::deque<DelayedPacket>& rDelayedPackets, FNHANDLEPACKET HandlePacket)
{
	while (!rDelayedPackets.empty())
	{
		HandlePacket(rDelayedPackets.front());
		rDelayedPackets.pop_front();
	}
}

template <typename FNHANDLEPACKET>
inline void ProcessOrFlush(std::deque<DelayedPacket>& rDelayedPackets, bool bFastForward, FNHANDLEPACKET HandlePacket)
{
	if (bFastForward)
	{
		FlushDelayed(rDelayedPackets, HandlePacket);
	}
	else
	{
		ProcessDelayed(rDelayedPackets, HandlePacket);
	}
}

// Drop delayed packets queued on a coord slot's channels (called on load reset).
inline void PurgeDelayedForSlot(std::deque<DelayedPacket>& rDelayedPackets, int64_t iSlot)
{
	std::erase_if(rDelayedPackets, [iSlot](const DelayedPacket& rPacket)
	{
		return NetworkManager::IsCoordinateChannel(rPacket.uiChannelIdentifier) && NetworkManager::ChannelToSlot(rPacket.uiChannelIdentifier) == iSlot;
	});
}

} // namespace NetworkSimulation

} // namespace engine
