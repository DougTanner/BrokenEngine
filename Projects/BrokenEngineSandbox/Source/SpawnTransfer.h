#pragma once

#include "Frame/StatusChange.h"

namespace game
{

struct Frame;

void SpawnTransfer(Frame& rFrame, StatusChangeType eType, const TransferData& rData);

// Adoptability check the replay stream reader applies, because client hydration looks the Blaster type up with a checked index.
bool IsAdoptableStatusChange(const StatusChange& rChange);

} // namespace game
