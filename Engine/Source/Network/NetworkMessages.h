#pragma once

#include "Network/NetworkCursor.h"

namespace engine::NetworkMessages
{

inline constexpr int64_t kiPacketTypeSize = sizeof(uint8_t);
inline constexpr int64_t kiGridCoordSize = sizeof(int32_t) * 2;
inline constexpr int64_t kiGuidSize = sizeof(uint64_t) * 2;

struct PacketPayload
{
	const uint8_t* puiData = nullptr;
	int32_t iSize = 0;
};

class MessageWriter
{
public:

	explicit MessageWriter(common::Workbuffer& rWorkbuffer)
	: mrWorkbuffer(rWorkbuffer)
	{
	}

	void Type(PacketType eType) const
	{
		mrWorkbuffer.PushBack<uint8_t>(static_cast<uint8_t>(eType));
	}

	void Field(const uint8_t& ruiValue) const
	{
		mrWorkbuffer.PushBack<uint8_t>(ruiValue);
	}
	void Field(const uint16_t& ruiValue) const
	{
		mrWorkbuffer.PushBack<uint16_t>(ruiValue);
	}
	void Field(const int32_t& riValue) const
	{
		mrWorkbuffer.PushBack<int32_t>(riValue);
	}
	void Field(const uint32_t& ruiValue) const
	{
		mrWorkbuffer.PushBack<uint32_t>(ruiValue);
	}
	void Field(const int64_t& riValue) const
	{
		mrWorkbuffer.PushBack<int64_t>(riValue);
	}
	void Field(const uint64_t& ruiValue) const
	{
		mrWorkbuffer.PushBack<uint64_t>(ruiValue);
	}
	void Field(const float& rfValue) const
	{
		mrWorkbuffer.PushBack<float>(rfValue);
	}

	void BoundedCount(const int64_t& riCount, int64_t, int64_t) const
	{
		Field(riCount);
	}

	void Field(const GridCoord& rCoord) const
	{
		WriteGridCoord(mrWorkbuffer, rCoord);
	}

	void Payload(const PacketPayload& rPayload) const
	{
		if (rPayload.iSize > 0)
		{
			mrWorkbuffer.Append(std::string_view(reinterpret_cast<const char*>(rPayload.puiData), rPayload.iSize));
		}
	}

	void NullTerminatedString(const std::string_view& rValue) const
	{
		mrWorkbuffer.Append(rValue);
		mrWorkbuffer.PushBack<uint8_t>(0);
	}

	void OptionalGuid(const ClientGuid& rGuid, const bool& rbHasGuid) const
	{
		if (rbHasGuid)
		{
			Field(rGuid.uiHigh);
			Field(rGuid.uiLow);
		}
	}

	void ConnectionResponseTail(const uint8_t& ruiAccepted, const uint8_t& ruiDebugInput, const ClientGuid& rGuid, const bool& rbHasGuid, const std::string_view& rRejectionMessage) const
	{
		if (ruiAccepted != 0)
		{
			Field(ruiDebugInput);
			OptionalGuid(rGuid, rbHasGuid);
		}
		else
		{
			mrWorkbuffer.Append(rRejectionMessage);
		}
	}

	void Invalidate() const
	{
	}

	common::Workbuffer& mrWorkbuffer;
};

class MessageReader
{
public:

	MessageReader(std::span<const uint8_t> packetData)
	: mCursor {.pCursor = packetData.data(), .pEnd = packetData.data() + packetData.size()}
	{
	}

	void Type(PacketType eExpected)
	{
		if (!mCursor.Has(kiPacketTypeSize) || ReadUint8(mCursor.pCursor) != static_cast<uint8_t>(eExpected))
		{
			mbValid = false;
		}
	}

	void Field(uint8_t& ruiValue)
	{
		Read(ruiValue, sizeof(ruiValue), ReadUint8);
	}
	void Field(uint16_t& ruiValue)
	{
		Read(ruiValue, sizeof(ruiValue), ReadUint16);
	}
	void Field(int32_t& riValue)
	{
		Read(riValue, sizeof(riValue), ReadInt32);
	}
	void Field(uint32_t& ruiValue)
	{
		Read(ruiValue, sizeof(ruiValue), ReadUint32);
	}
	void Field(int64_t& riValue)
	{
		Read(riValue, sizeof(riValue), ReadInt64);
	}
	void Field(uint64_t& ruiValue)
	{
		Read(ruiValue, sizeof(ruiValue), ReadUint64);
	}
	void Field(float& rfValue)
	{
		Read(rfValue, sizeof(rfValue), ReadFloat);
	}

	void BoundedCount(int64_t& riCount, int64_t iItemMinSize, int64_t iTrailingBytes)
	{
		Field(riCount);
		if (!mbValid || riCount < 0 || iTrailingBytes > mCursor.Remaining()
		 || riCount > (mCursor.Remaining() - iTrailingBytes) / iItemMinSize)
		{
			mbValid = false;
		}
	}

	bool AtEnd() const
	{
		return mCursor.Remaining() == 0;
	}

	void Field(GridCoord& rCoord)
	{
		if (!mCursor.Has(kiGridCoordSize))
		{
			mbValid = false;
			return;
		}
		rCoord = ReadGridCoord(mCursor.pCursor);
	}

	void Payload(PacketPayload& rPayload)
	{
		if (rPayload.iSize < 0 || !mCursor.Has(rPayload.iSize))
		{
			mbValid = false;
			return;
		}
		rPayload.puiData = mCursor.pCursor;
		mCursor.pCursor += rPayload.iSize;
	}

	void NullTerminatedString(std::string_view& rValue)
	{
		if (!mbValid)
		{
			return;
		}

		const uint8_t* puiStart = mCursor.pCursor;
		while (mCursor.pCursor < mCursor.pEnd && *mCursor.pCursor != 0)
		{
			++mCursor.pCursor;
		}
		rValue = std::string_view(reinterpret_cast<const char*>(puiStart), static_cast<size_t>(mCursor.pCursor - puiStart));
		if (mCursor.pCursor < mCursor.pEnd)
		{
			++mCursor.pCursor;
		}
	}

	void OptionalGuid(ClientGuid& rGuid, bool& rbHasGuid)
	{
		rbHasGuid = false;
		if (!mbValid || !mCursor.Has(kiGuidSize))
		{
			return;
		}
		Field(rGuid.uiHigh);
		Field(rGuid.uiLow);
		rbHasGuid = mbValid;
	}

	void ConnectionResponseTail(const uint8_t& ruiAccepted, uint8_t& ruiDebugInput, ClientGuid& rGuid, bool& rbHasGuid, std::string_view& rRejectionMessage)
	{
		rbHasGuid = false;
		rRejectionMessage = {};
		if (!mbValid)
		{
			return;
		}

		if (ruiAccepted != 0)
		{
			Field(ruiDebugInput);
			OptionalGuid(rGuid, rbHasGuid);
			return;
		}

		rRejectionMessage = std::string_view(reinterpret_cast<const char*>(mCursor.pCursor), static_cast<size_t>(mCursor.Remaining()));
		mCursor.pCursor = mCursor.pEnd;
	}

	void Invalidate()
	{
		mbValid = false;
	}

private:

	template <typename TVALUE, typename TREAD>
	void Read(TVALUE& rValue, int64_t iSize, TREAD pRead)
	{
		if (!mbValid || !mCursor.Has(iSize))
		{
			mbValid = false;
			return;
		}
		rValue = pRead(mCursor.pCursor);
	}

	BoundedCursor mCursor;

public:

	bool mbValid = true;
};

class CoordUpdateTickReader
{
public:

	CoordUpdateTickReader(std::span<const uint8_t> packetData)
	: mCursor {.pCursor = packetData.data(), .pEnd = packetData.data() + packetData.size()}
	{
	}

	bool HasTick() const
	{
		return mbValid && mbHasTick;
	}

	void Type(PacketType eExpected)
	{
		if (!mCursor.Has(kiPacketTypeSize) || ReadUint8(mCursor.pCursor) != static_cast<uint8_t>(eExpected))
		{
			mbValid = false;
		}
	}

	void Field(const uint8_t&)
	{
		Skip(sizeof(uint8_t));
	}
	void Field(const uint16_t&)
	{
		Skip(sizeof(uint16_t));
	}
	void Field(const int32_t&)
	{
		Skip(sizeof(int32_t));
	}
	void Field(const uint32_t&)
	{
		Skip(sizeof(uint32_t));
	}
	void Field(const uint64_t&)
	{
		Skip(sizeof(uint64_t));
	}
	void Field(const GridCoord&)
	{
		Skip(kiGridCoordSize);
	}

	void Field(int64_t& riValue)
	{
		if (mbHasTick)
		{
			return;
		}
		if (!mCursor.Has(sizeof(int64_t)))
		{
			mbValid = false;
			return;
		}
		riValue = ReadInt64(mCursor.pCursor);
		miTick = riValue;
		mbHasTick = true;
	}

	void Payload(const PacketPayload&)
	{
	}
	void NullTerminatedString(const std::string_view&)
	{
	}
	void OptionalGuid(const ClientGuid&, const bool&)
	{
	}
	void ConnectionResponseTail(const uint8_t&, const uint8_t&, const ClientGuid&, const bool&, const std::string_view&)
	{
	}
	void Invalidate()
	{
		mbValid = false;
	}

private:

	void Skip(int64_t iSize)
	{
		if (mbHasTick)
		{
			return;
		}
		if (!mbValid || !mCursor.Has(iSize))
		{
			mbValid = false;
			return;
		}
		mCursor.pCursor += iSize;
	}

	BoundedCursor mCursor;

public:

	int64_t miTick = 0;

private:

	bool mbValid = true;
	bool mbHasTick = false;
};

// The one corruption signal on the receive path: a reader that decided the peer's bytes are impossible
// raises this, and the dispatch catch owning that direction decides the response (client-fatal, server
// drop-and-count). pcReader identifies the reader; the suppress scope covers std::ios_base::failure's
// string copy for reader sites that have no ambient one.
[[noreturn]] inline void ThrowCorruptStream(const char* pcReader)
{
	ScopedSuppressAllocationTracking suppress;
	throw std::ios_base::failure(pcReader);
}

template <typename TMESSAGE>
inline void Write(common::Workbuffer& rWorkbuffer, TMESSAGE& rMessage)
{
	MessageWriter writer(rWorkbuffer);
	TMESSAGE::Visit(writer, rMessage);
}

template <typename TMESSAGE>
inline void Read(std::span<const uint8_t> packetData, TMESSAGE& rMessage)
{
	MessageReader reader(packetData);
	TMESSAGE::Visit(reader, rMessage);
	if (!reader.mbValid)
	{
		ThrowCorruptStream("NetworkMessages::Read");
	}
}

struct AckStreamEntry
{
	static constexpr int64_t kiSize = sizeof(uint8_t) + sizeof(uint16_t) + sizeof(int64_t) + sizeof(uint64_t) + sizeof(uint64_t);

	uint8_t uiSlotIndex = 0;
	uint16_t uiEpoch = 0;
	int64_t iAckFloor = -1;
	uint64_t uiReceivedBitfieldLow = 0;
	uint64_t uiReceivedBitfieldHigh = 0;

	template <typename TVISITOR>
	static void Visit(TVISITOR& rVisitor, AckStreamEntry& rMessage)
	{
		rVisitor.Field(rMessage.uiSlotIndex);
		rVisitor.Field(rMessage.uiEpoch);
		rVisitor.Field(rMessage.iAckFloor);
		rVisitor.Field(rMessage.uiReceivedBitfieldLow);
		rVisitor.Field(rMessage.uiReceivedBitfieldHigh);
	}
};

struct ClientAckStreamMessage
{
	static constexpr PacketType keType = PacketType::kClientAcknowledgmentStream;
	static constexpr int64_t kiMaxSlotCount = 64;
	static constexpr int64_t kiFixedSize = kiPacketTypeSize + sizeof(uint8_t) + sizeof(int64_t);

	uint8_t uiSlotCount = 0;
	AckStreamEntry* pEntries = nullptr;
	int64_t iEntryCapacity = 0;
	int64_t iTimestampNanoseconds = 0;

	static constexpr int64_t GetSize(int64_t iSlotCount)
	{
		return kiFixedSize + iSlotCount * AckStreamEntry::kiSize;
	}

	template <typename TVISITOR>
	static void Visit(TVISITOR& rVisitor, ClientAckStreamMessage& rMessage)
	{
		rVisitor.Type(keType);
		rVisitor.Field(rMessage.uiSlotCount);
		if (rMessage.uiSlotCount > rMessage.iEntryCapacity)
		{
			rVisitor.Invalidate();
			return;
		}
		for (int64_t i = 0; i < rMessage.uiSlotCount; ++i)
		{
			AckStreamEntry::Visit(rVisitor, rMessage.pEntries[i]);
		}
		rVisitor.Field(rMessage.iTimestampNanoseconds);
	}
};

struct ClientDesyncReportMessage
{
	static constexpr PacketType keType = PacketType::kClientDesynchronizationReport;
	static constexpr int64_t kiFixedSize = kiPacketTypeSize + sizeof(int64_t) + kiGridCoordSize + sizeof(uint64_t) + sizeof(uint64_t);

	int64_t iTick = 0;
	GridCoord coord {};
	uint64_t uiExpectedCrc = 0;
	uint64_t uiActualCrc = 0;

	template <typename TVISITOR>
	static void Visit(TVISITOR& rVisitor, ClientDesyncReportMessage& rMessage)
	{
		rVisitor.Type(keType);
		rVisitor.Field(rMessage.iTick);
		rVisitor.Field(rMessage.coord);
		rVisitor.Field(rMessage.uiExpectedCrc);
		rVisitor.Field(rMessage.uiActualCrc);
	}
};

struct ClientDebugFrameRequestMessage
{
	static constexpr PacketType keType = PacketType::kClientDebugFrameRequest;
	static constexpr int64_t kiFixedSize = kiPacketTypeSize + sizeof(int64_t) + kiGridCoordSize;

	int64_t iTick = 0;
	GridCoord coord {};

	template <typename TVISITOR>
	static void Visit(TVISITOR& rVisitor, ClientDebugFrameRequestMessage& rMessage)
	{
		rVisitor.Type(keType);
		rVisitor.Field(rMessage.iTick);
		rVisitor.Field(rMessage.coord);
	}
};

struct ClientHelloMessage
{
	static constexpr PacketType keType = PacketType::kClientHello;
	static constexpr int64_t kiFixedSize = kiPacketTypeSize + sizeof(uint32_t) + sizeof(int64_t) + sizeof(common::crc_t);
	static constexpr int64_t kiMinSize = kiFixedSize;
	static constexpr int64_t kiMaxBuildConfigurationBytes = 64;
	static constexpr int64_t kiMaxSize = kiFixedSize + kiMaxBuildConfigurationBytes + kiGuidSize;

	uint32_t uiProtocolVersion = 0;
	int64_t iFrameVersion = 0;
	common::crc_t uiPackIntegrityToken = 0;
	std::string_view buildConfiguration;
	ClientGuid guid {};
	bool bHasGuid = false;

	template <typename TVISITOR>
	static void Visit(TVISITOR& rVisitor, ClientHelloMessage& rMessage)
	{
		rVisitor.Type(keType);
		rVisitor.Field(rMessage.uiProtocolVersion);
		rVisitor.Field(rMessage.iFrameVersion);
		rVisitor.Field(rMessage.uiPackIntegrityToken);
		rVisitor.NullTerminatedString(rMessage.buildConfiguration);
		rVisitor.OptionalGuid(rMessage.guid, rMessage.bHasGuid);
	}
};

struct ServerConnectionResponseMessage
{
	static constexpr PacketType keType = PacketType::kServerConnectionResponse;
	static constexpr int64_t kiMinSize = kiPacketTypeSize + sizeof(uint8_t) + sizeof(uint8_t);
	static constexpr int64_t kiAcceptedGuidSize = kiMinSize + sizeof(uint8_t) + kiGuidSize;

	uint8_t uiLoadGeneration = 0;
	uint8_t uiAccepted = 0;
	// Server kbDebugInput (0 or 1): whether it accepts debug-control requests. Accepted tail only, so a
	// protocol-mismatch rejection keeps its message where an older peer reads it.
	uint8_t uiDebugInput = 0;
	ClientGuid guid {};
	bool bHasGuid = false;
	std::string_view rejectionMessage;

	template <typename TVISITOR>
	static void Visit(TVISITOR& rVisitor, ServerConnectionResponseMessage& rMessage)
	{
		rVisitor.Type(keType);
		rVisitor.Field(rMessage.uiLoadGeneration);
		rVisitor.Field(rMessage.uiAccepted);
		rVisitor.ConnectionResponseTail(rMessage.uiAccepted, rMessage.uiDebugInput, rMessage.guid, rMessage.bHasGuid, rMessage.rejectionMessage);
	}
};

struct ClientSubscribeMessage
{
	static constexpr PacketType keType = PacketType::kClientSubscribe;
	static constexpr int64_t kiFixedSize = kiPacketTypeSize + sizeof(uint8_t) + kiGridCoordSize;

	uint8_t uiLoadGeneration = 0;
	GridCoord coord {};

	template <typename TVISITOR>
	static void Visit(TVISITOR& rVisitor, ClientSubscribeMessage& rMessage)
	{
		rVisitor.Type(keType);
		rVisitor.Field(rMessage.uiLoadGeneration);
		rVisitor.Field(rMessage.coord);
	}
};

struct ClientUnsubscribeMessage
{
	static constexpr PacketType keType = PacketType::kClientUnsubscribe;
	static constexpr int64_t kiFixedSize = kiPacketTypeSize + sizeof(uint8_t) + sizeof(uint16_t);

	uint8_t uiSlotIndex = 0;
	uint16_t uiEpoch = 0;

	template <typename TVISITOR>
	static void Visit(TVISITOR& rVisitor, ClientUnsubscribeMessage& rMessage)
	{
		rVisitor.Type(keType);
		rVisitor.Field(rMessage.uiSlotIndex);
		rVisitor.Field(rMessage.uiEpoch);
	}
};

struct ClientResyncRequestMessage
{
	static constexpr PacketType keType = PacketType::kClientResynchronizationRequest;
	static constexpr int64_t kiFixedSize = kiPacketTypeSize;

	template <typename TVISITOR>
	static void Visit(TVISITOR& rVisitor, ClientResyncRequestMessage&)
	{
		rVisitor.Type(keType);
	}
};

struct ServerSubscribeAcceptMessage
{
	static constexpr PacketType keType = PacketType::kServerSubscribeAccept;
	static constexpr int64_t kiFixedSize = kiPacketTypeSize + sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint16_t) + kiGridCoordSize;

	uint8_t uiLoadGeneration = 0;
	uint8_t uiSlotIndex = 0;
	uint16_t uiEpoch = 0;
	GridCoord coord {};

	template <typename TVISITOR>
	static void Visit(TVISITOR& rVisitor, ServerSubscribeAcceptMessage& rMessage)
	{
		rVisitor.Type(keType);
		rVisitor.Field(rMessage.uiLoadGeneration);
		rVisitor.Field(rMessage.uiSlotIndex);
		rVisitor.Field(rMessage.uiEpoch);
		rVisitor.Field(rMessage.coord);
	}
};

struct ServerUnsubscribeAckMessage
{
	static constexpr PacketType keType = PacketType::kServerUnsubscribeAck;
	static constexpr int64_t kiFixedSize = kiPacketTypeSize + sizeof(uint8_t);

	uint8_t uiSlotIndex = 0;

	template <typename TVISITOR>
	static void Visit(TVISITOR& rVisitor, ServerUnsubscribeAckMessage& rMessage)
	{
		rVisitor.Type(keType);
		rVisitor.Field(rMessage.uiSlotIndex);
	}
};

struct ServerLoadNotificationMessage
{
	static constexpr PacketType keType = PacketType::kServerLoadNotification;
	static constexpr int64_t kiFixedSize = kiPacketTypeSize + sizeof(uint8_t);

	uint8_t uiLoadGeneration = 0;

	template <typename TVISITOR>
	static void Visit(TVISITOR& rVisitor, ServerLoadNotificationMessage& rMessage)
	{
		rVisitor.Type(keType);
		rVisitor.Field(rMessage.uiLoadGeneration);
	}
};

struct ServerTimespeedUpdateMessage
{
	static constexpr PacketType keType = PacketType::kServerTimespeedUpdate;
	static constexpr int64_t kiFixedSize = kiPacketTypeSize + sizeof(int64_t) + sizeof(int64_t);

	int64_t iMultiply = 1;
	int64_t iDivide = 1;

	template <typename TVISITOR>
	static void Visit(TVISITOR& rVisitor, ServerTimespeedUpdateMessage& rMessage)
	{
		rVisitor.Type(keType);
		rVisitor.Field(rMessage.iMultiply);
		rVisitor.Field(rMessage.iDivide);
	}
};

struct ServerCoordFullStateMessage
{
	static constexpr PacketType keType = PacketType::kServerCoordinateFullState;
	static constexpr int64_t kiEnvelopeSize = kiPacketTypeSize + sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint16_t) + sizeof(int64_t) + kiGridCoordSize;
	static constexpr int64_t kiFixedSize = kiEnvelopeSize + sizeof(int32_t) + sizeof(int32_t);

	uint8_t uiLoadGeneration = 0;
	uint8_t uiSlotIndex = 0;
	uint16_t uiEpoch = 0;
	int64_t iTick = 0;
	GridCoord coord {};
	int32_t iUncompressedSize = 0;
	PacketPayload compressedPayload {};

	template <typename TVISITOR>
	static void Visit(TVISITOR& rVisitor, ServerCoordFullStateMessage& rMessage)
	{
		rVisitor.Type(keType);
		rVisitor.Field(rMessage.uiLoadGeneration);
		rVisitor.Field(rMessage.uiSlotIndex);
		rVisitor.Field(rMessage.uiEpoch);
		rVisitor.Field(rMessage.iTick);
		rVisitor.Field(rMessage.coord);
		rVisitor.Field(rMessage.iUncompressedSize);
		rVisitor.Field(rMessage.compressedPayload.iSize);
		rVisitor.Payload(rMessage.compressedPayload);
	}
};

struct ServerCoordStaticDataMessage
{
	static constexpr PacketType keType = PacketType::kServerCoordinateStaticData;
	static constexpr int64_t kiEnvelopeSize = kiPacketTypeSize + sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint16_t) + kiGridCoordSize;
	static constexpr int64_t kiFixedSize = kiEnvelopeSize + sizeof(int32_t);

	uint8_t uiLoadGeneration = 0;
	uint8_t uiSlotIndex = 0;
	uint16_t uiEpoch = 0;
	GridCoord coord {};
	PacketPayload staticData {};

	template <typename TVISITOR>
	static void Visit(TVISITOR& rVisitor, ServerCoordStaticDataMessage& rMessage)
	{
		rVisitor.Type(keType);
		rVisitor.Field(rMessage.uiLoadGeneration);
		rVisitor.Field(rMessage.uiSlotIndex);
		rVisitor.Field(rMessage.uiEpoch);
		rVisitor.Field(rMessage.coord);
		rVisitor.Field(rMessage.staticData.iSize);
		rVisitor.Payload(rMessage.staticData);
	}
};

struct CoordUpdateFields
{
	static constexpr int64_t kiEnvelopeSize = kiPacketTypeSize + sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint16_t) + sizeof(int64_t) + sizeof(int64_t) + sizeof(uint64_t);
	static constexpr int64_t kiFixedSize = kiEnvelopeSize + sizeof(int32_t);

	uint8_t uiLoadGeneration = 0;
	uint8_t uiSlotIndex = 0;
	uint16_t uiEpoch = 0;
	int64_t iTick = 0;
	int64_t iEchoedTimestampNanoseconds = 0;
	uint64_t uiSharedCrc = 0;
	PacketPayload compressedPayload {};

	template <typename TVISITOR>
	static void VisitFields(TVISITOR& rVisitor, CoordUpdateFields& rMessage)
	{
		rVisitor.Field(rMessage.uiLoadGeneration);
		rVisitor.Field(rMessage.uiSlotIndex);
		rVisitor.Field(rMessage.uiEpoch);
		rVisitor.Field(rMessage.iTick);
		rVisitor.Field(rMessage.iEchoedTimestampNanoseconds);
		rVisitor.Field(rMessage.uiSharedCrc);
		rVisitor.Field(rMessage.compressedPayload.iSize);
		rVisitor.Payload(rMessage.compressedPayload);
	}
};

struct ServerCoordUpdateMessage : CoordUpdateFields
{
	static constexpr PacketType keType = PacketType::kServerCoordinateUpdate;
	static constexpr int64_t kiEnvelopeSize = CoordUpdateFields::kiEnvelopeSize;
	static constexpr int64_t kiFixedSize = CoordUpdateFields::kiFixedSize;

	template <typename TVISITOR>
	static void Visit(TVISITOR& rVisitor, ServerCoordUpdateMessage& rMessage)
	{
		rVisitor.Type(keType);
		VisitFields(rVisitor, rMessage);
	}
};

struct ServerCoordResendMessage : CoordUpdateFields
{
	static constexpr PacketType keType = PacketType::kServerCoordinateResend;
	static constexpr int64_t kiEnvelopeSize = CoordUpdateFields::kiEnvelopeSize;
	static constexpr int64_t kiFixedSize = CoordUpdateFields::kiFixedSize;

	template <typename TVISITOR>
	static void Visit(TVISITOR& rVisitor, ServerCoordResendMessage& rMessage)
	{
		rVisitor.Type(keType);
		VisitFields(rVisitor, rMessage);
	}
};

struct ServerDebugFrameMessage
{
	static constexpr PacketType keType = PacketType::kServerDebugFrame;
	static constexpr int64_t kiEnvelopeSize = kiPacketTypeSize + sizeof(int64_t) + kiGridCoordSize;
	static constexpr int64_t kiFixedSize = kiEnvelopeSize + sizeof(int32_t) + sizeof(int32_t);

	int64_t iTick = 0;
	GridCoord coord {};
	int32_t iUncompressedSize = 0;
	PacketPayload compressedPayload {};

	template <typename TVISITOR>
	static void Visit(TVISITOR& rVisitor, ServerDebugFrameMessage& rMessage)
	{
		rVisitor.Type(keType);
		rVisitor.Field(rMessage.iTick);
		rVisitor.Field(rMessage.coord);
		rVisitor.Field(rMessage.iUncompressedSize);
		rVisitor.Field(rMessage.compressedPayload.iSize);
		rVisitor.Payload(rMessage.compressedPayload);
	}
};

inline int64_t GetCoordUpdateTickOrZero(std::span<const uint8_t> packetData)
{
	ServerCoordUpdateMessage update {};
	CoordUpdateTickReader updateReader(packetData);
	ServerCoordUpdateMessage::Visit(updateReader, update);
	if (updateReader.HasTick())
	{
		return updateReader.miTick;
	}

	ServerCoordResendMessage resend {};
	CoordUpdateTickReader resendReader(packetData);
	ServerCoordResendMessage::Visit(resendReader, resend);
	return resendReader.HasTick() ? resendReader.miTick : 0;
}

} // namespace engine::NetworkMessages
