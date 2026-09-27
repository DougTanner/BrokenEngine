#include "FrameInput.h"

#include "SpawnTransfer.h"

namespace game
{

// Same-session-only CRC: feeds the replay DifferenceStream writer-side dedup and a verbose reader log,
// never persisted or compared cross-build. Unlike the frame CRC it needs no game::Frame::kiVersion bump
// when the mixing algorithm changes.
common::crc_t FrameInput::Crc() const
{
	common::crc_t checksum = 0;
	for (const StatusChange& rStatusChange : statusChanges)
	{
		checksum = (checksum ^ common::Crc(rStatusChange.eType)) * common::kCrcMultiplier;
		std::visit([&](const auto& payload) { checksum = (checksum ^ common::Crc(payload)) * common::kCrcMultiplier; }, rStatusChange.data);
	}
	return checksum;
}

std::ostream& operator<<(std::ostream& rStream, const FrameInput& rInput)
{
	int64_t iStatusCount = static_cast<int64_t>(rInput.statusChanges.size());
	common::Write(rStream, iStatusCount);
	for (const StatusChange& rChange : rInput.statusChanges)
	{
		common::Write(rStream, rChange.eType);
		std::visit([&](const auto& payload) { common::Write(rStream, payload); }, rChange.data);
	}

	return rStream;
}

std::istream& operator>>(std::istream& rStream, FrameInput& rInput)
{
	int64_t iStatusCount = 0;
	common::Read(rStream, iStatusCount);
	// Bound the count against the replay stream before the resize — each
	// StatusChange serializes at least a type byte plus one payload byte.
	common::ValidateDeserializedCount(iStatusCount, sizeof(uint8_t) + 1, rStream, "FrameInput statusChanges");
	rInput.statusChanges.resize(iStatusCount);
	for (StatusChange& rChange : rInput.statusChanges)
	{
		common::Read(rStream, rChange.eType);
		// An unknown type tag seats the default variant alternative and reads
		// the wrong payload byte count, silently desyncing the rest of the stream — reject it.
		if (!IsKnownStatusChangeType(rChange.eType))
		{
			throw std::ios_base::failure("FrameInput StatusChange type");
		}
		rChange.data = DefaultDataForType(rChange.eType);
		std::visit([&](auto& payload) { common::Read(rStream, payload); }, rChange.data);
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

	return rStream;
}

} // namespace game
