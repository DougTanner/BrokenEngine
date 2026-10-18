#include "Pch.h"

#include "Network/StatusChangeCodec.h"

#include "Network/StatusChangeFields.h"

#include "Frame/StatusChange.h"

namespace game
{

void WriteStatusChangePayload(uint8_t*& rpCursor, const StatusChange& rChange)
{
	std::visit([&](const auto& rData)
	{
		engine::WriteStatusChangeFields(rpCursor, rData);
	}, rChange.data);
}

void ReadStatusChangePayload(const uint8_t*& rpCursor, StatusChange& rChange)
{
	rChange.data = DefaultDataForType(rChange.eType);
	std::visit([&](auto& rData)
	{
		engine::ReadStatusChangeFields(rpCursor, rData);
	}, rChange.data);
}

} // namespace game
