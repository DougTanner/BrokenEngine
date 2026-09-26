#pragma once

#include "Frame/StatusChange.h"

namespace game
{

struct Frame;

void SpawnTransfer(Frame& rFrame, StatusChangeType eType, const TransferData& rData, engine::alignment_t playerAlignment);

// Trust-boundary adoptability check both stream readers apply, because client hydration looks the Blaster type up with a checked index.
bool IsAdoptableStatusChange(const StatusChange& rChange);

} // namespace game
