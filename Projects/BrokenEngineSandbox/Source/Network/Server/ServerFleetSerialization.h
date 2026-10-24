#pragma once

#if defined(BT_SERVER)

#include "Save/GameSaveState.h"
#include "Fleet.h"

namespace game
{

// The fleet state of one replay fleet record, staged for playback. Its members carry their respawn countdowns.
struct ReplayFleetRecord
{
	int64_t iTick = 0;
	std::unordered_map<engine::ClientGuid, std::vector<Fleet>> fleets;
	common::RandomEngine randomEngine;
	std::unordered_map<FleetGuid, int64_t> nextFighterLaunchTicks;
};

// The replay fleet record bytes of the fleet state at the end of the fleet step of tick iTick.
struct RecordedReplayFleetRecord
{
	int64_t iTick = 0;
	std::string bytes;
};

// The fleet half of a replay. Recording fills recordedRecords and the checksums; playback adoption fills
// playbackRecords and the checksums. The checksums hold one fleet checksum per tick from iFirstChecksumTick.
struct ReplayFleetLog
{
	std::vector<RecordedReplayFleetRecord> recordedRecords;
	std::vector<ReplayFleetRecord> playbackRecords;
	int64_t iNextPlaybackRecord = 0;
	int64_t iFirstChecksumTick = 0;
	std::vector<common::crc_t> checksums;
	common::crc_t uiLastChecksum = 0;
};

// rMap's GUID keys sorted by uiHigh, then uiLow, held in rArena's frame, which must be empty; valid until it pops.
template <typename KEY, typename VALUE>
std::span<const KEY> SortedGuidKeys(const std::unordered_map<KEY, VALUE>& rMap, common::ScopedWorkbufferArena& rArena)
{
	for ([[maybe_unused]] const auto& [rKey, rValue] : rMap)
	{
		rArena.mBuffer.PushBack(rKey);
	}
	std::span<KEY> keys = rArena.mBuffer.Span<KEY>();
	std::ranges::sort(keys, [](const KEY& rLeft, const KEY& rRight)
	{
		return rLeft.uiHigh != rRight.uiHigh ? rLeft.uiHigh < rRight.uiHigh : rLeft.uiLow < rRight.uiLow;
	});
	return keys;
}

void SendFleetSync(int64_t iClientId, const std::vector<Fleet>& rFleets);

void WriteFleetData(std::ostream& rStream, const std::unordered_map<engine::ClientGuid, std::vector<Fleet>>& rFleets, const common::RandomEngine& rRandom);

void ReadFleetData(std::fstream& rFileStream, int64_t iNextGlobalId, std::unordered_map<engine::ClientGuid, std::vector<Fleet>>& rFleets, std::unordered_map<engine::ClientGuid, int64_t>& rGuidToClientId, common::RandomEngine& rRandom);

// The replay fleet record bytes of the live fleet state: the WriteFleetData bytes, each member's respawn countdown in
// the same walk order, then the Fighter launch ticks in fleet GUID order. Owner client IDs and queued fleet requests
// are left out.
std::string ReplayFleetRecordBytes();

// Reads the bytes ReplayFleetRecordBytes writes; the caller reads the record's tick first.
void ReadReplayFleetRecord(std::fstream& rFileStream, int64_t iNextGlobalId, ReplayFleetRecord& rRecord);

} // namespace game

#endif // BT_SERVER
