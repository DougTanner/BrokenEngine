#include "FrameInput.h"

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
	// Every legitimate fleet ship spawn carries a real global ID; a zero ID would create an unaddressable fleet row.
	if (IsSpawnType(rChange.eType) && std::visit([](const auto& rData)
	{
		if constexpr (requires { rData.eController; })
		{
			return rData.eController == ShipController::kFleet && rData.globalId.iValue == 0;
		}
		else
		{
			return false;
		}
	}, rChange.data))
	{
		throw std::ios_base::failure("FrameInput ship spawn global ID");
	}
}

} // namespace game
