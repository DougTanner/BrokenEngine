#pragma once

#include "Frame/StatusChange.h"

namespace game
{

// Frame
struct FrameInput
{
	static constexpr int64_t kiVersion = 17;

	std::vector<StatusChange> statusChanges;

	common::crc_t Crc() const;

	friend std::ostream& operator<<(std::ostream& rStream, const FrameInput& rInput);
	friend std::istream& operator>>(std::istream& rStream, FrameInput& rInput);
};

} // namespace game
