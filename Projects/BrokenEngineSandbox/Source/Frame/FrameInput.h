#pragma once

#include "Frame/StatusChange.h"

namespace game
{

struct FrameInput
{
	static constexpr int64_t kiVersion = 29;

	std::vector<StatusChange> statusChanges;

	common::crc_t Crc() const;

	friend std::ostream& operator<<(std::ostream& rStream, const FrameInput& rInput);
	friend std::istream& operator>>(std::istream& rStream, FrameInput& rInput);
};

// engine::ReadFrameInput calls this after each StatusChange payload read; it throws std::ios_base::failure for a
// change the game cannot adopt.
void AdmitFrameInputStatusChange(const StatusChange& rChange);

} // namespace game
