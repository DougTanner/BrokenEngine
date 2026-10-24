#include "Pch.h"

#if defined(BT_SERVER)

#include "Save/GameSaveLoad.h"

#include "Network/Server/ServerClientManager.h"
#include "Network/Server/ServerFleetManager.h"
#include "Network/Server/ServerSession.h"
#include "Game.h"

namespace game
{

// Layout after the ReplayMeta version header and value: the replay fleet record count, each record's tick and bytes,
// the first fleet checksum tick, the fleet checksum count, and the fleet checksums.
bool WriteReplayMetadata(const engine::FileFlags_t& rFlags, const std::filesystem::path& rFilename)
{
	ReplayMeta metadata
	{
		.clientGridCoordinate = game::gpGame->mClientGridCoordinate,
		.iClientOwnedEntityIdentifierValue = game::gpGame->ClientOwnedEntityIdentifier().iValue,
		.fPreviousClientArmor = game::gpGame->mfPreviousClientArmor,
	};
	const ReplayFleetLog& rReplayLog = gpServerSession->mpFleetManager->mReplayLog;
	return engine::gpFileManager->WriteFileAtomically(rFlags, rFilename, [&](std::fstream& rFileStream)
	{
		engine::WriteVersionHeader<ReplayMeta>(rFileStream);
		common::Write(rFileStream, metadata);

		int64_t iRecordCount = std::ssize(rReplayLog.recordedRecords);
		common::Write(rFileStream, iRecordCount);
		for (const RecordedReplayFleetRecord& rRecord : rReplayLog.recordedRecords)
		{
			common::Write(rFileStream, rRecord.iTick);
			rFileStream.write(rRecord.bytes.data(), std::ssize(rRecord.bytes));
		}

		common::Write(rFileStream, rReplayLog.iFirstChecksumTick);
		int64_t iChecksumCount = std::ssize(rReplayLog.checksums);
		common::Write(rFileStream, iChecksumCount);
		common::Write(rFileStream, rReplayLog.checksums);
	});
}

bool ReadReplayMetadata(const engine::FileFlags_t& rFlags, const std::filesystem::path& rFilename, int64_t iNextGlobalId, ReplayStagedMeta& rStagedMetadata)
{
	std::fstream fileStream = engine::gpFileManager->OpenFile(rFlags, rFilename);

	int64_t iVersion = 0;
	int64_t iSize = 0;
	if (!engine::ReadAndValidateVersionHeader<ReplayMeta>(fileStream, iVersion, iSize))
	{
		return false;
	}
	common::Read(fileStream, rStagedMetadata.metadata);

	int64_t iRecordCount = 0;
	common::Read(fileStream, iRecordCount);
	rStagedMetadata.fleetRecords.resize(static_cast<size_t>(iRecordCount));
	for (ReplayFleetRecord& rRecord : rStagedMetadata.fleetRecords)
	{
		common::Read(fileStream, rRecord.iTick);
		ReadReplayFleetRecord(fileStream, iNextGlobalId, rRecord);
	}

	common::Read(fileStream, rStagedMetadata.iFirstFleetChecksumTick);
	int64_t iChecksumCount = 0;
	common::Read(fileStream, iChecksumCount);
	rStagedMetadata.fleetChecksums.resize(static_cast<size_t>(iChecksumCount));
	common::Read(fileStream, rStagedMetadata.fleetChecksums);
	return fileStream.good();
}

void AdoptReplayMetadata(ReplayStagedMeta&& rStagedMetadata)
{
	game::gpGame->RestoreReplayMetadata(rStagedMetadata.metadata);

	ReplayFleetLog& rReplayLog = gpServerSession->mpFleetManager->mReplayLog;
	rReplayLog.playbackRecords = std::move(rStagedMetadata.fleetRecords);
	rReplayLog.iNextPlaybackRecord = 0;
	rReplayLog.iFirstChecksumTick = rStagedMetadata.iFirstFleetChecksumTick;
	rReplayLog.checksums = std::move(rStagedMetadata.fleetChecksums);
}

void OnStateReplaced()
{
	game::gpServerSession->ResetForLoad();
}

void StartReplayRecording()
{
	ReplayFleetLog& rReplayLog = gpServerSession->mpFleetManager->mReplayLog;
	rReplayLog.recordedRecords.clear();
	rReplayLog.checksums.clear();
	rReplayLog.iFirstChecksumTick = gpGame->miTickCounter;
	rReplayLog.recordedRecords.push_back({.iTick = gpGame->miTickCounter, .bytes = ReplayFleetRecordBytes()});
}

void PrepareReplayPlaybackTick()
{
	gpServerSession->TickGameTimers(false);

	ReplayFleetLog& rReplayLog = gpServerSession->mpFleetManager->mReplayLog;
	if (rReplayLog.iNextPlaybackRecord == std::ssize(rReplayLog.playbackRecords))
	{
		return;
	}
	ReplayFleetRecord& rRecord = rReplayLog.playbackRecords.at(static_cast<size_t>(rReplayLog.iNextPlaybackRecord));
	if (rRecord.iTick != gpGame->miTickCounter)
	{
		return;
	}

	// mGuidToClientId stays: it maps the live connections, which no record carries.
	ServerFleetManager& rFleetManager = *gpServerSession->mpFleetManager;
	rFleetManager.mFleets = std::move(rRecord.fleets);
	rFleetManager.mRandomEngine = rRecord.randomEngine;
	gpServerSession->mpClientManager->mNextFighterLaunchTicks = std::move(rRecord.nextFighterLaunchTicks);
	++rReplayLog.iNextPlaybackRecord;
}

// The fleet checksum is replay-only: it never enters Frame::Crc or the wire.
void FinalizeReplayTick(bool bRecording)
{
	ReplayFleetLog& rReplayLog = gpServerSession->mpFleetManager->mReplayLog;
	int64_t iTick = gpGame->miTickCounter;
	if (bRecording)
	{
		rReplayLog.uiLastChecksum = common::Crc(ReplayFleetRecordBytes());
		rReplayLog.checksums.push_back(rReplayLog.uiLastChecksum);
		LOG(kReplay, kVerbose, "Fleet checksum recorded {}: {}", iTick, rReplayLog.uiLastChecksum);
		return;
	}
	if (!gpGame->mbReplaying)
	{
		return;
	}

	int64_t iChecksumIndex = iTick - rReplayLog.iFirstChecksumTick;
	if (iChecksumIndex < 0 || iChecksumIndex >= std::ssize(rReplayLog.checksums))
	{
		return;
	}
	common::crc_t uiReplayedChecksum = common::Crc(ReplayFleetRecordBytes());
	LOG(kReplay, kVerbose, "Fleet checksum replayed {}: {}", iTick, uiReplayedChecksum);
	common::crc_t uiRecordedChecksum = rReplayLog.checksums.at(static_cast<size_t>(iChecksumIndex));
	if (uiReplayedChecksum != uiRecordedChecksum)
	{
		LOG(kReplay, kError, "Replay fleet checksum mismatch Tick: {} Recorded: {} Replayed: {}", iTick, uiRecordedChecksum, uiReplayedChecksum);
	}
}

} // namespace game

#endif // BT_SERVER
