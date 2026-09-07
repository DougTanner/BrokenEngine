#pragma once

#include "Network/NetworkProtocol.h" // kiMaxStatusChangesPerCell for the shared batch-size bounds below

// The engine owns the batch codec body (NetworkSerialization.cpp): the tagged [type][count] group envelope,
// the bounded receive cursor, and the all-or-nothing malformed-input rejection. The game supplies the
// StatusChangeType enum, the concrete payload variants, and the per-type NetworkSessionContract operations
// that give those payloads their bytes. The game type is forward-declared rather than included so this
// engine header stays free of game dependencies; the codec .cpp includes the game header for the full type.
namespace game { struct StatusChange; }

namespace engine
{

// Upper bound on a single serialized StatusChange of any type. A game wire fact, but shared here (the codec is
// engine-owned) so the codec's SerializeGroup per-item ASSERT and the engine Server's compression-scratch sizing
// reference one source. Bump when any StatusChange payload grows past it.
inline constexpr int64_t kiMaxStatusChangeBytesPerItem = 120;

// Worst-case serialized bytes for a full kiMaxStatusChangesPerCell batch: every item at its max size plus a
// conservative 3-byte group header charged per item (over-bounds the real per-type group headers so this stays
// free of the game enum's type count).
inline constexpr int64_t kiMaxSerializedStatusChangeBatchBytes = kiMaxStatusChangesPerCell * (kiMaxStatusChangeBytesPerItem + 3);

// Maximum nested Workbuffer high-water while CompressStatusChangeBatch consumes its input: serialized bytes,
// worst-case alignment before the sorted-index reservation, and one int64_t index per status change.
inline constexpr int64_t kiMaxCompressStatusChangeWorkbufferBytes = kiMaxSerializedStatusChangeBatchBytes + 15 + kiMaxStatusChangesPerCell * static_cast<int64_t>(sizeof(int64_t));

// Worst-case bytes CompressStatusChangeBatch can emit for a valid capped batch (4-byte uncompressed-size prefix +
// LZ4 payload). Sizes the server's reused compression scratch so any valid batch always fits; the codec still checks
// the LZ4 return as a belt.
inline constexpr int64_t kiMaxCompressedStatusChangeBatchBytes = static_cast<int64_t>(sizeof(int32_t)) + LZ4_COMPRESSBOUND(kiMaxSerializedStatusChangeBatchBytes);

// Type-specific serialization (no compression)
// Returns bytes written to pDest
int64_t SerializeStatusChangeBatch(const game::StatusChange* pChanges, int64_t iCount, void* pDest);

// Returns number of StatusChanges written to pDest; throws common::CorruptStreamException on malformed input
int64_t DeserializeStatusChangeBatch(const void* pSource, int64_t iSourceSize, game::StatusChange* pDest, int64_t iMaxCount);

// Serialization + LZ4 compression
// Returns bytes written to pDest (4-byte uncompressed size prefix + compressed data)
int64_t CompressStatusChangeBatch(const game::StatusChange* pChanges, int64_t iCount, void* pDest, int64_t iDestCapacity);

// Returns number of StatusChanges written to pDest; throws common::CorruptStreamException on malformed input
int64_t DecompressStatusChangeBatch(const void* pSource, int64_t iSourceSize, game::StatusChange* pDest, int64_t iMaxCount);

} // namespace engine
