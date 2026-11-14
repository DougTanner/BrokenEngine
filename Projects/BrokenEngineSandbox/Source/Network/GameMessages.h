#pragma once

#include "Fleet.h"

namespace game::GameMessages
{

struct CreateFleetRequestMessage
{
	static constexpr int64_t kiSize = 0;
	static_assert(kiSize == 0);

	template <typename TVISITOR>
	static void Visit([[maybe_unused]] TVISITOR& rVisitor, [[maybe_unused]] CreateFleetRequestMessage& rMessage)
	{
	}
};

struct MapFleetMarkersRequestMessage
{
	static constexpr int64_t kiSize = 0;
	static_assert(kiSize == 0);

	template <typename TVISITOR>
	static void Visit([[maybe_unused]] TVISITOR& rVisitor, [[maybe_unused]] MapFleetMarkersRequestMessage& rMessage)
	{
	}
};

struct SpawnIntoFleetRequestMessage
{
	static constexpr int64_t kiSize = sizeof(uint64_t) + sizeof(uint64_t) + sizeof(uint8_t);
	static_assert(kiSize == 17);

	FleetGuid fleetGuid {};
	uint8_t uiShipType = 0;

	template <typename TVISITOR>
	static void Visit(TVISITOR& rVisitor, SpawnIntoFleetRequestMessage& rMessage)
	{
		rVisitor.Field(rMessage.fleetGuid.uiHigh);
		rVisitor.Field(rMessage.fleetGuid.uiLow);
		rVisitor.Field(rMessage.uiShipType);
	}
};

// Carries every fleet navigation setting; a sender changes one field and sends the fleet's current values for the rest.
struct FleetNavigationRequestMessage
{
	static constexpr int64_t kiSize = sizeof(uint64_t) + sizeof(uint64_t) + sizeof(float) + sizeof(uint8_t) + sizeof(int32_t) + sizeof(int32_t);
	static_assert(kiSize == 29);

	FleetGuid fleetGuid {};
	float fNavigationDelay = 0.0f;
	uint8_t uiBehaviour = 0;
	int32_t iHoldQ = 0;
	int32_t iHoldR = 0;

	template <typename TVISITOR>
	static void Visit(TVISITOR& rVisitor, FleetNavigationRequestMessage& rMessage)
	{
		rVisitor.Field(rMessage.fleetGuid.uiHigh);
		rVisitor.Field(rMessage.fleetGuid.uiLow);
		rVisitor.Field(rMessage.fNavigationDelay);
		rVisitor.Field(rMessage.uiBehaviour);
		rVisitor.Field(rMessage.iHoldQ);
		rVisitor.Field(rMessage.iHoldR);
	}
};

struct DestroyFleetMemberRequestMessage
{
	static constexpr int64_t kiSize = sizeof(int64_t);
	static_assert(kiSize == 8);

	engine::GlobalId globalId {};

	template <typename TVISITOR>
	static void Visit(TVISITOR& rVisitor, DestroyFleetMemberRequestMessage& rMessage)
	{
		rVisitor.Field(rMessage.globalId.iValue);
	}
};

// Sets the squadron setting flags (SquadronOrderFlags) of the squadron of the Fighter or Omni member globalId to uiFlags.
struct SquadronOrdersRequestMessage
{
	static constexpr int64_t kiSize = sizeof(int64_t) + sizeof(uint8_t);
	static_assert(kiSize == 9);

	engine::GlobalId globalId {};
	uint8_t uiFlags = 0;

	template <typename TVISITOR>
	static void Visit(TVISITOR& rVisitor, SquadronOrdersRequestMessage& rMessage)
	{
		rVisitor.Field(rMessage.globalId.iValue);
		rVisitor.Field(rMessage.uiFlags);
	}
};

struct FleetSyncMessage
{
	static constexpr int64_t kiFleetCountSize = sizeof(int64_t);
	static constexpr int64_t kiFleetHeaderSize = sizeof(uint64_t) + sizeof(uint64_t) + sizeof(int64_t) + sizeof(int64_t) + sizeof(float) + sizeof(uint8_t) + sizeof(int32_t) + sizeof(int32_t);
	static constexpr int64_t kiFleetMemberSize = sizeof(int64_t) + sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint8_t);

	static_assert(kiFleetCountSize == 8);
	static_assert(kiFleetHeaderSize == 45);
	static_assert(kiFleetMemberSize == 11);

	template <typename TVISITOR, typename TFLEET>
	static void VisitFleetHeader(TVISITOR& rVisitor, TFLEET& rFleet, int64_t& riMemberCount)
	{
		rVisitor.Field(rFleet.guid.uiHigh);
		rVisitor.Field(rFleet.guid.uiLow);
		rVisitor.BoundedCount(riMemberCount, kiFleetMemberSize, sizeof(int64_t) + sizeof(float) + sizeof(uint8_t) + sizeof(int32_t) + sizeof(int32_t));
		rVisitor.Field(rFleet.flagshipGlobalId.iValue);
		float fNavigationDelay = rFleet.navigationDelaySeconds.count();
		rVisitor.Field(fNavigationDelay);
		uint8_t uiBehaviour = std::to_underlying(rFleet.eBehaviour);
		rVisitor.Field(uiBehaviour);
		rVisitor.Field(rFleet.holdCoordinate.iQ);
		rVisitor.Field(rFleet.holdCoordinate.iR);
		if constexpr (!std::is_const_v<TFLEET>)
		{
			rFleet.navigationDelaySeconds = std::chrono::duration<float>(fNavigationDelay);
			rFleet.eBehaviour = static_cast<FleetBehaviour>(uiBehaviour);
		}
	}

	template <typename TVISITOR, typename TFLEETMEMBER, typename TBYTE>
	static void VisitFleetMember(TVISITOR& rVisitor, TFLEETMEMBER& rMember, TBYTE& ruiType, TBYTE& ruiFlags)
	{
		rVisitor.Field(rMember.globalId.iValue);
		rVisitor.Field(ruiType);
		rVisitor.Field(ruiFlags);
		rVisitor.Field(rMember.uiSquadron);
	}

	static void WritePayload(common::Workbuffer& rWorkbuffer, const std::vector<Fleet>& rFleets)
	{
		int64_t iExpectedSize = rWorkbuffer.Count<uint8_t>() + kiFleetCountSize;
		int64_t iFleetCount = std::ssize(rFleets);
		engine::NetworkMessages::MessageWriter writer(rWorkbuffer);
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
				uint8_t uiType = std::to_underlying(rMember.eType);
				uint8_t uiFlags = std::to_underlying(rMember.flags.meFlags);
				VisitFleetMember(writer, rMember, uiType, uiFlags);
				ASSERT(rWorkbuffer.Count<uint8_t>() == iMemberStart + kiFleetMemberSize);
				iExpectedSize += kiFleetMemberSize;
			}
		}
		ASSERT(rWorkbuffer.Count<uint8_t>() == iExpectedSize);
	}

	// Only the client reads this server-to-client payload, and it trusts its server, so the decode checks neither
	// reader validity nor the flagship relation; the server's fleet invariant provides that relation.
	static void ReadPayload(const std::vector<uint8_t>& rPayload, std::vector<Fleet>& rOutFleets)
	{
		engine::NetworkMessages::MessageReader reader(std::span<const uint8_t>(rPayload.data(), static_cast<size_t>(std::ssize(rPayload))));
		int64_t iFleetCount = 0;
		reader.BoundedCount(iFleetCount, kiFleetHeaderSize, 0);

		rOutFleets.resize(static_cast<size_t>(iFleetCount));
		for (Fleet& rFleet : rOutFleets)
		{
			int64_t iMemberCount = 0;
			VisitFleetHeader(reader, rFleet, iMemberCount);
			rFleet.members.resize(static_cast<size_t>(iMemberCount));
			for (FleetMember& rMember : rFleet.members)
			{
				uint8_t uiType = 0;
				uint8_t uiFlags = 0;
				VisitFleetMember(reader, rMember, uiType, uiFlags);
				rMember.eType = static_cast<ShipType>(uiType);
				rMember.flags.meFlags = static_cast<FleetMemberFlags>(uiFlags);
			}
		}
	}
};

// One live fleet flagship on the hex map: its cell, its position in that cell's frame, and its flags.
struct MapFleetMarker
{
	engine::GridCoord coordinate {};
	XMFLOAT2 f2Position {};
	uint8_t uiFlags = 0;
};

struct MapFleetMarkersMessage
{
	static constexpr int64_t kiMarkerCountSize = sizeof(int64_t);
	static constexpr int64_t kiMarkerSize = sizeof(int32_t) + sizeof(int32_t) + sizeof(float) + sizeof(float) + sizeof(uint8_t);

	static_assert(kiMarkerCountSize == 8);
	static_assert(kiMarkerSize == 17);

	// MapFleetMarker::uiFlags bit: the requesting client owns the marker's fleet.
	static constexpr uint8_t kuiOwnFlag = 1 << 0;

	template <typename TVISITOR, typename TMARKER>
	static void VisitMarker(TVISITOR& rVisitor, TMARKER& rMarker)
	{
		rVisitor.Field(rMarker.coordinate.iQ);
		rVisitor.Field(rMarker.coordinate.iR);
		rVisitor.Field(rMarker.f2Position.x);
		rVisitor.Field(rMarker.f2Position.y);
		rVisitor.Field(rMarker.uiFlags);
	}

	static void WritePayload(common::Workbuffer& rWorkbuffer, std::span<const MapFleetMarker> markers)
	{
		int64_t iMarkerCount = std::ssize(markers);
		int64_t iExpectedSize = rWorkbuffer.Count<uint8_t>() + kiMarkerCountSize + iMarkerCount * kiMarkerSize;
		engine::NetworkMessages::MessageWriter writer(rWorkbuffer);
		writer.BoundedCount(iMarkerCount, kiMarkerSize, 0);
		for (const MapFleetMarker& rMarker : markers)
		{
			VisitMarker(writer, rMarker);
		}
		ASSERT(rWorkbuffer.Count<uint8_t>() == iExpectedSize);
	}

	// Only the client reads this server-to-client payload, and it trusts its server, so the decode does not check
	// reader validity.
	static void ReadPayload(const std::vector<uint8_t>& rPayload, std::vector<MapFleetMarker>& rOutMarkers)
	{
		engine::NetworkMessages::MessageReader reader(std::span<const uint8_t>(rPayload.data(), static_cast<size_t>(std::ssize(rPayload))));
		int64_t iMarkerCount = 0;
		reader.BoundedCount(iMarkerCount, kiMarkerSize, 0);

		rOutMarkers.resize(static_cast<size_t>(iMarkerCount));
		for (MapFleetMarker& rMarker : rOutMarkers)
		{
			VisitMarker(reader, rMarker);
		}
	}
};

} // namespace game::GameMessages
