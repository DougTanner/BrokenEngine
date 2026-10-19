#pragma once

#include "Network/NetworkProtocol.h" // kiMaximumStatusChangesPerCell for the shared batch-size bounds below

// The engine owns the batch codec (NetworkSerialization.cpp): the tagged [type][count] group envelope, ascending type
// order, and LZ4 framing. The game owns the StatusChangeType enum and each type's payload bytes, which the codec writes
// and reads through game::WriteStatusChangePayload and game::ReadStatusChangePayload. The game type is forward-declared
// rather than included so this engine header stays free of game dependencies; the codec .cpp includes the game headers.
namespace game
{
struct StatusChange;
} // namespace game

namespace engine
{

// Upper bound on a single serialized StatusChange payload of any type. A game wire fact, kept here so the engine
// codec's SerializeGroup per-item ASSERT and the engine Server's compression-scratch sizing reference one source.
inline constexpr int64_t kiMaxStatusChangeBytesPerItem = 128;

// Worst-case serialized bytes for a batch of that many status changes: every item at its max size plus a
// conservative 3-byte group header charged per item (over-bounds the real per-type group headers so this stays
// free of the game enum's type count).
constexpr int64_t MaxSerializedStatusChangeBatchBytes(int64_t iStatusChangeCount)
{
	return iStatusChangeCount * (kiMaxStatusChangeBytesPerItem + 3);
}

// Maximum nested Workbuffer high-water while CompressStatusChangeBatch consumes a kiMaximumStatusChangesPerCell batch:
// serialized bytes, worst-case alignment before the sorted-index reservation, and one int64_t index per status change.
inline constexpr int64_t kiMaxCompressStatusChangeWorkbufferBytes = MaxSerializedStatusChangeBatchBytes(kiMaximumStatusChangesPerCell) + 15 + kiMaximumStatusChangesPerCell * static_cast<int64_t>(sizeof(int64_t));

// Worst-case bytes CompressStatusChangeBatch can emit for a batch of that many status changes (4-byte
// uncompressed-size prefix + LZ4 payload). Sizes the server's reused compression scratch so the batch always fits;
// the codec asserts the LZ4 return.
constexpr int64_t MaxCompressedStatusChangeBatchBytes(int64_t iStatusChangeCount)
{
	return static_cast<int64_t>(sizeof(int32_t)) + LZ4_COMPRESSBOUND(MaxSerializedStatusChangeBatchBytes(iStatusChangeCount));
}

// Type-specific serialization (no compression)
// Returns bytes written to pDestination
int64_t SerializeStatusChangeBatch(std::span<const game::StatusChange> changes, void* pDestination);

// Returns number of StatusChanges written to rDestination, which grows when the batch exceeds its size
int64_t DeserializeStatusChangeBatch(std::span<const uint8_t> source, std::vector<game::StatusChange>& rDestination);

// Serialization + LZ4 compression
// Returns bytes written to destination (4-byte uncompressed size prefix + compressed data)
int64_t CompressStatusChangeBatch(std::span<const game::StatusChange> changes, std::span<uint8_t> destination);

// Returns number of StatusChanges written to rDestination, which grows when the batch exceeds its size
int64_t DecompressStatusChangeBatch(std::span<const uint8_t> source, std::vector<game::StatusChange>& rDestination);

} // namespace engine
