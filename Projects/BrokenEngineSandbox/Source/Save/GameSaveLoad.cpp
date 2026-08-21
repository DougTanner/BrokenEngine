#include "Pch.h"

#if defined(BT_SERVER)

#include "Save/GameSaveLoad.h"

#include "File/GridSave.h"
#include "File/Replay.h"
#include "GameBase.h"
#include "Game.h"
#include "Network/Server/ServerFleetSerialization.h"
#include "Network/Server/ServerSession.h"
#include "Profile/ProfileManager.h"

namespace game
{

bool WriteReplayMeta(const engine::FileFlags_t& rFlags, const std::filesystem::path& rFilename)
{
	ReplayMeta meta {
		.clientGridCoord = game::gpGame->mClientGridCoord,
		.iClientPlayerIdValue = game::gpGame->ClientPlayerId().iValue,
		.fPreviousClientArmor = game::gpGame->PreviousClientArmor(),
	};
	return engine::WriteVersionedFile(rFlags, rFilename, meta);
}

bool ReadReplayMeta(const engine::FileFlags_t& rFlags, const std::filesystem::path& rFilename, ReplayStagedMeta& rStagedMeta)
{
	return engine::ReadVersionedFile(rFlags, rFilename, rStagedMeta.meta);
}

void AdoptReplayMeta(ReplayStagedMeta&& rStagedMeta)
{
	game::gpGame->RestoreReplayMeta(rStagedMeta.meta);
}

void OnReplayStreamsInvalidated()
{
	game::gpServerSession->mReplayTransferFixtures.clear();
}

void OnStateReplaced()
{
	game::gpServerSession->ResetClientsForLoad();
}

void CountCapturedReplayTransfers(std::span<const StatusChange> transfers, ReplayTransferCaptureCounts& rCounts)
{
	for (const StatusChange& rTransfer : transfers)
	{
		switch (rTransfer.eType)
		{
		case StatusChangeType::kTransferPlayer: ++rCounts.iPlayerCount; break;
		case StatusChangeType::kTransferSpaceship: ++rCounts.iSpaceshipCount; break;
		case StatusChangeType::kTransferBlaster: ++rCounts.iBlasterCount; break;
		case StatusChangeType::kTransferMissile: ++rCounts.iMissileCount; break;
		default: DEBUG_BREAK(); break;
		}
	}
}

GameSaveLoad::GameSaveLoad(engine::GameBase& rGameBase)
	: mrGameBase(rGameBase)
{
}

bool GameSaveLoad::ServerSave()
{
	return ServerSave(game::gpGame->QuicksaveFile());
}

bool GameSaveLoad::ServerSave(const std::filesystem::path& rFilename)
{
	ScopedSuppressAllocationTracking suppress;
	return engine::WriteGridSave(mrGameBase, {engine::FileFlags::kAppDataDirectory, engine::FileFlags::kWrite}, rFilename, game::gpGame->mClientGridCoord);
}

bool GameSaveLoad::ServerLoad()
{
	return ServerLoad(game::gpGame->QuicksaveFile());
}

bool GameSaveLoad::ServerLoad(const std::filesystem::path& rFilename)
{
	ScopedSuppressAllocationTracking suppress;
	gpProfileManager->LatchRawCpuTimers(false, mrGameBase.TickCounter());

	engine::GridCoord loadedClientGridCoord {};
	if (!engine::ReadGridSave(mrGameBase, {engine::FileFlags::kAppDataDirectory, engine::FileFlags::kRead}, rFilename, loadedClientGridCoord))
	{
		return false;
	}

	const int64_t iLoadedTick = mrGameBase.TickCounter();
	const float fLoadedTime = mrGameBase.CurrentTime();
	game::gpGame->Reset();
	// Reset clears the clock; restore the saved values before client resynchronization.
	mrGameBase.SetTickCounter(iLoadedTick);
	mrGameBase.SetCurrentTime(fLoadedTime);
	game::gpGame->SetClientGridCoord(loadedClientGridCoord);
	game::OnStateReplaced();
	game::gpServerSession->mpRuntime->ComputeActiveSet();

	return true;
}

void GameSaveLoad::ServerReset()
{
	ScopedSuppressAllocationTracking suppress;
	gpProfileManager->LatchRawCpuTimers(false, mrGameBase.TickCounter());

	game::gpGame->CreateNewFrame(game::GameFlags::kGame);
	mrGameBase.SetNextGlobalId(1);
	game::gpGame->Reset();
	// Fresh-game wipe of fleet manager state. Load path leaves mFleets populated by ReadFleetData;
	// fresh-game has no save to restore from, so explicitly clear before ResetClientsForLoad runs.
	game::ResetSaveState();
	game::OnStateReplaced();
	game::gpServerSession->mpRuntime->ComputeActiveSet();
}

void GameSaveLoad::Autosave()
{
	ScopedSuppressAllocationTracking suppress;
	engine::WriteGridSave(mrGameBase, {engine::FileFlags::kAppDataDirectory, engine::FileFlags::kWrite, engine::FileFlags::kBackup}, std::filesystem::path("ServerAutosave.save"), game::gpGame->mClientGridCoord);
}

void GameSaveLoad::TickAutosave()
{
	if (mrGameBase.mbReplaying || engine::gpReplay->IsRecording() || (mrGameBase.mGameFlags & engine::GameFlags::kLoadReplay))
	{
		return;
	}

	if (mAutosaveTimer.GetDeltaNs(false) >= kAutosaveInterval)
	{
		Autosave();
		mAutosaveTimer.Reset();
		LOG(kDefault, kInfo, "Autosave fired (interval {}s)", kAutosaveInterval.count());
	}
}

bool GameSaveLoad::Autoload()
{
	ScopedSuppressAllocationTracking suppress;

	engine::GridCoord loadedClientGridCoord {};
	if (!engine::ReadGridSave(mrGameBase, {engine::FileFlags::kAppDataDirectory, engine::FileFlags::kRead}, std::filesystem::path("ServerAutosave.save"), loadedClientGridCoord))
	{
		return false;
	}

	game::gpGame->SetClientGridCoord(loadedClientGridCoord);
	return true;
}

} // namespace game

#endif // BT_SERVER
