#pragma once

#include "Network/NetworkProtocol.h"
#include "Network/NetworkSerialization.h"

#include "Network/GamePacketType.h"

namespace game
{

inline constexpr int64_t kiDesiredCoordinateSlots = 16; // 9 visible subscriptions + 6 sticky + 1 spare

// Engine session runtimes consume this game-specific payload contract.
struct NetworkSessionContract
{
	using Frame = game::Frame;
	using StatusChange = game::StatusChange;
	using GamePacket = game::GamePacketType;

	static constexpr std::chrono::nanoseconds kTickDuration = engine::kTickNanoseconds;
	static constexpr int64_t kiCoordinateSlots = kiDesiredCoordinateSlots;
	static constexpr bool kbDebugFrames = kbDesynchronizationDebugFrames;

	static void WriteFrame(std::ostream& rStream, const Frame& rFrame)
	{
		rStream << rFrame;
	}
	static void ReadFrame(std::istream& rStream, Frame& rFrame)
	{
		rFrame.ServerRead(rStream);
	}
	static int64_t CompressStatusChanges(std::span<const StatusChange> changes, std::span<uint8_t> destination)
	{
		return engine::CompressStatusChangeBatch(changes, destination);
	}
	static int64_t DecompressStatusChanges(std::span<const uint8_t> source, StatusChange* pDestination)
	{
		return engine::DecompressStatusChangeBatch(source, pDestination);
	}
};

} // namespace game
