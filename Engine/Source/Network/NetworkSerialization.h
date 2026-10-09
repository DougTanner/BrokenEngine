#pragma once

#include "Network/NetworkProtocol.h" // kiMaximumStatusChangesPerCell for the shared batch-size bounds below

// The engine owns the batch codec body (NetworkSerialization.cpp): the tagged [type][count] group envelope
// and each payload type's read/write. The game supplies the StatusChangeType enum and the concrete payload variants. The game type is forward-declared rather than included so this
// engine header stays free of game dependencies; the codec .cpp includes the game header for the full type.
namespace game
{
struct StatusChange;
} // namespace game

namespace engine
{

// Upper bound on a single serialized StatusChange of any type. A game wire fact, but shared here (the codec is
// engine-owned) so the codec's SerializeGroup per-item ASSERT and the engine Server's compression-scratch sizing
// reference one source.
inline constexpr int64_t kiMaxStatusChangeBytesPerItem = 120;

// Worst-case serialized bytes for a full kiMaximumStatusChangesPerCell batch: every item at its max size plus a
// conservative 3-byte group header charged per item (over-bounds the real per-type group headers so this stays
// free of the game enum's type count).
inline constexpr int64_t kiMaxSerializedStatusChangeBatchBytes = kiMaximumStatusChangesPerCell * (kiMaxStatusChangeBytesPerItem + 3);

// Maximum nested Workbuffer high-water while CompressStatusChangeBatch consumes its input: serialized bytes,
// worst-case alignment before the sorted-index reservation, and one int64_t index per status change.
inline constexpr int64_t kiMaxCompressStatusChangeWorkbufferBytes = kiMaxSerializedStatusChangeBatchBytes + 15 + kiMaximumStatusChangesPerCell * static_cast<int64_t>(sizeof(int64_t));

// Worst-case bytes CompressStatusChangeBatch can emit for a valid capped batch (4-byte uncompressed-size prefix +
// LZ4 payload). Sizes the server's reused compression scratch so any valid batch always fits; the codec asserts the
// LZ4 return.
inline constexpr int64_t kiMaxCompressedStatusChangeBatchBytes = static_cast<int64_t>(sizeof(int32_t)) + LZ4_COMPRESSBOUND(kiMaxSerializedStatusChangeBatchBytes);

// Type-specific serialization (no compression)
// Returns bytes written to pDestination
int64_t SerializeStatusChangeBatch(std::span<const game::StatusChange> changes, void* pDestination);

// Returns number of StatusChanges written to pDestination
int64_t DeserializeStatusChangeBatch(std::span<const uint8_t> source, game::StatusChange* pDestination);

// Serialization + LZ4 compression
// Returns bytes written to destination (4-byte uncompressed size prefix + compressed data)
int64_t CompressStatusChangeBatch(std::span<const game::StatusChange> changes, std::span<uint8_t> destination);

// Returns number of StatusChanges written to pDestination
int64_t DecompressStatusChangeBatch(std::span<const uint8_t> source, game::StatusChange* pDestination);

} // namespace engine
