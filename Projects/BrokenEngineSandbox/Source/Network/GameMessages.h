#pragma once

#include "Fleet.h"
#include "Network/PlayerEvents.h"

namespace game::GameMessages
{

struct AssignPlayerMessage
{
	static constexpr int64_t kiSize = sizeof(int64_t) + engine::NetworkMessages::kiGridCoordSize;
	static_assert(kiSize == 16);

	int64_t iGlobalPlayerId = 0;
	engine::GridCoord coord {};

	template <typename TVisitor>
	static void Visit(TVisitor& rVisitor, AssignPlayerMessage& rMessage)
	{
		rVisitor.Field(rMessage.iGlobalPlayerId);
		rVisitor.Field(rMessage.coord);
	}
};

struct PlayerStateMessage
{
	static constexpr int64_t kiSize = sizeof(uint8_t) + sizeof(int64_t) + engine::NetworkMessages::kiGridCoordSize;
	static_assert(kiSize == 17);

	uint8_t uiWireType = 0;
	int64_t iGlobalPlayerId = 0;
	engine::GridCoord coord {};

	template <typename TVisitor>
	static void Visit(TVisitor& rVisitor, PlayerStateMessage& rMessage)
	{
		rVisitor.Field(rMessage.uiWireType);
		rVisitor.Field(rMessage.iGlobalPlayerId);
		rVisitor.Field(rMessage.coord);
	}
};

struct PlayerStateDescriptor
{
	PlayerStateWireType eWireType {};
	PlayerEventType eEventType {};
	const char* pcName = "";
};

inline constexpr PlayerStateDescriptor kpPlayerStateDescriptors[] =
{
	{.eWireType = PlayerStateWireType::kSpawned, .eEventType = PlayerEventType::kSpawned, .pcName = "Spawned"},
	{.eWireType = PlayerStateWireType::kChangedFrame, .eEventType = PlayerEventType::kChangedFrame, .pcName = "ChangedFrame"},
	{.eWireType = PlayerStateWireType::kDied, .eEventType = PlayerEventType::kDied, .pcName = "Died"},
};
static_assert(std::size(kpPlayerStateDescriptors) == static_cast<size_t>(PlayerStateWireType::kCount));

inline constexpr const PlayerStateDescriptor& GetPlayerStateDescriptor(PlayerStateWireType eWireType)
{
	return kpPlayerStateDescriptors[static_cast<size_t>(eWireType)];
}

inline constexpr const PlayerStateDescriptor* FindPlayerStateDescriptor(uint8_t uiWireType)
{
	for (const PlayerStateDescriptor& rDescriptor : kpPlayerStateDescriptors)
	{
		if (static_cast<uint8_t>(rDescriptor.eWireType) == uiWireType)
		{
			return &rDescriptor;
		}
	}
	return nullptr;
}

struct FleetSyncMessage
{
	static constexpr int64_t kiFleetCountSize = sizeof(int64_t);
	static constexpr int64_t kiFleetHeaderSize = sizeof(uint64_t) + sizeof(uint64_t) + sizeof(int64_t) + sizeof(int64_t) + sizeof(float);
	static constexpr int64_t kiFleetMemberSize = sizeof(int64_t) + sizeof(uint8_t);

	static_assert(kiFleetCountSize == 8);
	static_assert(kiFleetHeaderSize == 36);
	static_assert(kiFleetMemberSize == 9);

	template <typename TVisitor, typename TFleet>
	static void VisitFleetHeader(TVisitor& rVisitor, TFleet& rFleet, int64_t& riMemberCount)
	{
		rVisitor.Field(rFleet.guid.uiHigh);
		rVisitor.Field(rFleet.guid.uiLow);
		rVisitor.BoundedCount(riMemberCount, kiFleetMemberSize, sizeof(int64_t) + sizeof(float));
		rVisitor.Field(rFleet.iFlagshipIndex);
		rVisitor.Field(rFleet.fNavigationDelay);
	}

	template <typename TVisitor, typename TFleetMember, typename TAlive>
	static void VisitFleetMember(TVisitor& rVisitor, TFleetMember& rMember, TAlive& rAlive)
	{
		rVisitor.Field(rMember.globalPlayerId.iValue);
		rVisitor.Field(rAlive);
	}

	static void WritePayload(common::Workbuffer& rWorkbuffer, const std::vector<Fleet>& rFleets)
	{
		int64_t iExpectedSize = rWorkbuffer.Count<uint8_t>() + kiFleetCountSize;
		const int64_t iFleetCount = std::ssize(rFleets);
		engine::NetworkMessages::MessageWriter writer {rWorkbuffer};
		writer.BoundedCount(iFleetCount, kiFleetHeaderSize, 0);
		for (const Fleet& rFleet : rFleets)
		{
			int64_t iHeaderStart = rWorkbuffer.Count<uint8_t>();
			int64_t iMemberCount = std::ssize(rFleet.members);
			VisitFleetHeader(writer, rFleet, iMemberCount);
			ASSERT(rWorkbuffer.Count<uint8_t>() == iHeaderStart + kiFleetHeaderSize);
			iExpectedSize += kiFleetHeaderSize;
			for (const FleetMember& rMember : rFleet.members)
			{
				int64_t iMemberStart = rWorkbuffer.Count<uint8_t>();
				uint8_t uiAlive = static_cast<uint8_t>(rMember.bAlive ? 1 : 0);
				VisitFleetMember(writer, rMember, uiAlive);
				ASSERT(rWorkbuffer.Count<uint8_t>() == iMemberStart + kiFleetMemberSize);
				iExpectedSize += kiFleetMemberSize;
			}
		}
		ASSERT(rWorkbuffer.Count<uint8_t>() == iExpectedSize);
	}

	// Reads the payload with MessageReader directly rather than through NetworkMessages::Read, so it raises the
	// shared corruption signal itself, under its own reader-name literal. rOutFleets is written as the payload is
	// decoded, so a throw can leave it partially filled; callers read into a scratch vector and commit only after
	// this returns.
	static void ReadPayload(const std::vector<uint8_t>& rPayload, std::vector<Fleet>& rOutFleets)
	{
		engine::NetworkMessages::MessageReader reader {std::span<const uint8_t>(rPayload.data(), rPayload.size())};
		int64_t iFleetCount = 0;
		reader.BoundedCount(iFleetCount, kiFleetHeaderSize, 0);
		if (!reader.IsValid())
		{
			engine::NetworkMessages::ThrowCorruptStream("FleetSyncMessage::ReadPayload");
		}

		rOutFleets.resize(static_cast<size_t>(iFleetCount));
		for (int64_t i = 0; i < iFleetCount; ++i)
		{
			Fleet& rFleet = rOutFleets[static_cast<size_t>(i)];
			int64_t iMemberCount = 0;
			VisitFleetHeader(reader, rFleet, iMemberCount);
			if (!reader.IsValid())
			{
				engine::NetworkMessages::ThrowCorruptStream("FleetSyncMessage::ReadPayload");
			}
			if (rFleet.iFlagshipIndex < 0 ||
				(iMemberCount == 0 && rFleet.iFlagshipIndex != 0) ||
				(iMemberCount > 0 && rFleet.iFlagshipIndex >= iMemberCount))
			{
				engine::NetworkMessages::ThrowCorruptStream("FleetSyncMessage::ReadPayload");
			}

			rFleet.members.resize(static_cast<size_t>(iMemberCount));
			for (int64_t j = 0; j < iMemberCount; ++j)
			{
				FleetMember& rMember = rFleet.members[static_cast<size_t>(j)];
				uint8_t uiAlive = 0;
				VisitFleetMember(reader, rMember, uiAlive);
				rMember.bAlive = uiAlive != 0;
				if (!reader.IsValid())
				{
					engine::NetworkMessages::ThrowCorruptStream("FleetSyncMessage::ReadPayload");
				}
			}
		}

		if (!reader.AtEnd())
		{
			engine::NetworkMessages::ThrowCorruptStream("FleetSyncMessage::ReadPayload");
		}
	}
};

} // namespace game::GameMessages
