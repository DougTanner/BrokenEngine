#include "Pch.h"

#include "Network/NetworkSerialization.h"

#include "Network/NetworkCursor.h"

#include "Frame/StatusChange.h"
#include "Network/StatusChangeCodec.h"

namespace engine
{

// Serialize a group of StatusChanges that share the same type
static void SerializeGroup(uint8_t*& rpCursor, game::StatusChangeType eType, const game::StatusChange* pChanges, std::span<const int64_t> indices)
{
	int64_t iGroupCount = static_cast<int64_t>(indices.size());

	if (iGroupCount == 0)
	{
		return;
	}

	WriteUint8(rpCursor, static_cast<uint8_t>(eType));
	WriteUint16(rpCursor, static_cast<uint16_t>(iGroupCount));

	for (int64_t i : indices)
	{
		const uint8_t* pItemStart = rpCursor;

		game::WriteStatusChangePayload(rpCursor, pChanges[i]);

		ASSERT(rpCursor - pItemStart <= kiMaxStatusChangeBytesPerItem);
	}
}

constexpr int64_t kiTypeCount = static_cast<int64_t>(game::StatusChangeType::kCount);

static void GroupIndicesByType(std::span<const game::StatusChange> changes, int64_t piOffsets[kiTypeCount], int64_t piCounts[kiTypeCount], std::span<int64_t> sortedIndices)
{
	int64_t iCount = static_cast<int64_t>(changes.size());

	for (const game::StatusChange& rChange : changes)
	{
		++piCounts[static_cast<int64_t>(rChange.eType)];
	}

	for (int64_t i = 1; i < kiTypeCount; ++i)
	{
		piOffsets[i] = piOffsets[i - 1] + piCounts[i - 1];
	}

	int64_t piWriteOffsets[kiTypeCount] = {};
	std::memcpy(piWriteOffsets, piOffsets, sizeof(int64_t) * kiTypeCount);
	for (int64_t i = 0; i < iCount; ++i)
	{
		int64_t iType = static_cast<int64_t>(changes[i].eType);
		sortedIndices[piWriteOffsets[iType]++] = i;
	}
}

int64_t SerializeStatusChangeBatch(std::span<const game::StatusChange> changes, void* pDestination)
{
	int64_t iCount = static_cast<int64_t>(changes.size());

	if (iCount == 0)
	{
		return 0;
	}

	uint8_t* pCursor = static_cast<uint8_t*>(pDestination);

	// Group indices by type in a workbuffer reservation (GroupIndicesByType writes every slot, so no zero-fill)
	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
	common::ScopedWorkbufferAllocation<int64_t*> sortedAllocation = rWorkbuffer.PushBuffer<int64_t*>(iCount * static_cast<int64_t>(sizeof(int64_t)));
	int64_t* pSorted = sortedAllocation.mpData;

	int64_t piGroupCounts[kiTypeCount] = {};
	int64_t piOffsets[kiTypeCount] = {};
	GroupIndicesByType(changes, piOffsets, piGroupCounts, std::span<int64_t>(pSorted, static_cast<size_t>(iCount)));

	for (int64_t i = 0; i < kiTypeCount; ++i)
	{
		SerializeGroup(pCursor, static_cast<game::StatusChangeType>(i), changes.data(), std::span<const int64_t>(pSorted + piOffsets[i], static_cast<size_t>(piGroupCounts[i])));
	}

	return pCursor - static_cast<uint8_t*>(pDestination);
}

int64_t DeserializeStatusChangeBatch(std::span<const uint8_t> source, game::StatusChange* pDestination)
{
	int64_t iSourceSize = static_cast<int64_t>(source.size());

	// Only the client decodes a batch, and it trusts its server's bytes, so the cursor only drives the loop to the end of
	// the batch; nothing is checked per group or item.
	BoundedCursor cursor {.pCursor = source.data(), .pEnd = source.data() + iSourceSize};
	int64_t iOutputCount = 0;

	while (cursor.Remaining() > 0)
	{
		uint8_t uiType = ReadUint8(cursor.pCursor);
		uint16_t uiGroupCount = ReadUint16(cursor.pCursor);

		game::StatusChangeType eType = static_cast<game::StatusChangeType>(uiType);

		for (int64_t i = 0; i < uiGroupCount; ++i)
		{
			game::StatusChange& rChange = pDestination[iOutputCount++];
			rChange = {};
			rChange.eType = eType;

			game::ReadStatusChangePayload(cursor.pCursor, rChange);
		}
	}

	return iOutputCount;
}

int64_t CompressStatusChangeBatch(std::span<const game::StatusChange> changes, std::span<uint8_t> destination)
{
	int64_t iCount = std::ssize(changes);
	int64_t iDestinationCapacity = std::ssize(destination);
	if (iCount == 0)
	{
		return 0;
	}

	// Serialize into a workbuffer reservation (SerializeStatusChangeBatch writes only iSerializedSize bytes and only
	// those are compressed, so no zero-fill), then LZ4 compress into destination
	static constexpr int64_t kiMaxGroupHeaders = kiTypeCount * 3;
	int64_t iMaxSerializedSize = kiMaxGroupHeaders + iCount * kiMaxStatusChangeBytesPerItem;

	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
	common::ScopedWorkbufferAllocation<uint8_t*> serializedAllocation = rWorkbuffer.PushBuffer<uint8_t*>(iMaxSerializedSize);
	uint8_t* pSerialized = serializedAllocation.mpData;

	int64_t iSerializedSize = SerializeStatusChangeBatch(changes, pSerialized);

	// Write 4-byte uncompressed size prefix, then LZ4 compressed data
	uint8_t* pOutput = destination.data();
	int32_t iUncompressedSize = static_cast<int32_t>(iSerializedSize);
	std::memcpy(pOutput, &iUncompressedSize, sizeof(int32_t));

	int64_t iCompressedSize = LZ4_compress_default(reinterpret_cast<const char*>(pSerialized), reinterpret_cast<char*>(pOutput + sizeof(int32_t)), static_cast<int>(iSerializedSize), static_cast<int>(iDestinationCapacity - sizeof(int32_t)));
	ASSERT(iCompressedSize > 0);

	return static_cast<int64_t>(sizeof(int32_t)) + iCompressedSize;
}

int64_t DecompressStatusChangeBatch(std::span<const uint8_t> source, game::StatusChange* pDestination)
{
	int64_t iSourceSize = std::ssize(source);
	const uint8_t* pInput = source.data();
	int32_t iUncompressedSize = 0;
	std::memcpy(&iUncompressedSize, pInput, sizeof(int32_t));

	// LZ4 writes iResult bytes and only those bytes are deserialized, so no zero-fill or extra slack is needed.
	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
	common::ScopedWorkbufferAllocation<uint8_t*> decompressedAllocation = rWorkbuffer.PushBuffer<uint8_t*>(iUncompressedSize);
	uint8_t* pDecompressed = decompressedAllocation.mpData;

	int64_t iResult = LZ4_decompress_safe(reinterpret_cast<const char*>(pInput + sizeof(int32_t)), reinterpret_cast<char*>(pDecompressed), static_cast<int>(iSourceSize - sizeof(int32_t)), iUncompressedSize);

	int64_t iCount = DeserializeStatusChangeBatch(std::span<const uint8_t>(pDecompressed, static_cast<size_t>(iResult)), pDestination);

	return iCount;
}

} // namespace engine
