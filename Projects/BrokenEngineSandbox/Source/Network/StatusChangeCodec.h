#pragma once

namespace game
{

struct StatusChange;

// Writes one item's payload bytes; the engine codec writes the group envelope around it.
void WriteStatusChangePayload(uint8_t*& rpCursor, const StatusChange& rChange);

// Requires rChange.eType set; seats DefaultDataForType(eType) and reads that type's payload.
void ReadStatusChangePayload(const uint8_t*& rpCursor, StatusChange& rChange);

} // namespace game
