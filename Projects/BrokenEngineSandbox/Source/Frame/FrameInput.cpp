#include "FrameInput.h"

#include "SpawnTransfer.h"

namespace game
{

common::crc_t FrameInput::Crc() const
{
	return engine::FrameInputCrc(*this);
}

std::ostream& operator<<(std::ostream& rStream, const FrameInput& rInput)
{
	engine::WriteFrameInput(rStream, rInput);
	return rStream;
}

std::istream& operator>>(std::istream& rStream, FrameInput& rInput)
{
	engine::ReadFrameInput(rStream, rInput);
	return rStream;
}

void AdmitFrameInputStatusChange(const StatusChange& rChange)
{
	// Reject a payload the game cannot adopt before the replay reaches the simulation.
	if (!IsAdoptableStatusChange(rChange))
	{
		throw std::ios_base::failure("FrameInput StatusChange payload");
	}
	// Every legitimate spawn carries a real global ID; a zero ID would create an
	// unaddressable Player row.
	if (rChange.eType == StatusChangeType::kSpawnPlayer && std::get<SpawnPlayerData>(rChange.data).iGlobalId == 0)
	{
		throw std::ios_base::failure("FrameInput SpawnPlayer global ID");
	}
}

} // namespace game
