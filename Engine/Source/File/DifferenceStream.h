#pragma once

#include "File/FileManager.h"

namespace engine
{

// Streaming supports non-copyable types.
inline void TransferViaStream(const game::Frame& rFrom, game::Frame& rTo)
{
	std::stringstream buffer;
	buffer << rFrom;
	buffer >> rTo;
}

} // namespace engine
