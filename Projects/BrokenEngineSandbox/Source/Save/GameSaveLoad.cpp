#include "Pch.h"

#if defined(BT_SERVER)

#include "Save/GameSaveLoad.h"

#include "File/GridSave.h"
#include "File/Replay.h"
#include "GameBase.h"

#include "Agent/Commands/ServerSimulationFixtures.h"
#include "Network/Server/ServerFleetSerialization.h"
#include "Network/Server/ServerSession.h"
#include "Profile/ProfileManager.h"
#include "Game.h"

namespace game
{

bool WriteReplayMetadata(const engine::FileFlags_t& rFlags, const std::filesystem::path& rFilename)
{
	ReplayMeta metadata
	{
		.clientGridCoordinate = game::gpGame->mClientGridCoordinate,
		.iClientPlayerIdentifierValue = game::gpGame->ClientPlayerIdentifier().iValue,
		.fPreviousClientArmor = game::gpGame->mfPreviousClientArmor,
	};
	return engine::WriteVersionedFile(rFlags, rFilename, metadata);
}

bool ReadReplayMetadata(const engine::FileFlags_t& rFlags, const std::filesystem::path& rFilename, ReplayStagedMeta& rStagedMetadata)
{
	return engine::ReadVersionedFile(rFlags, rFilename, rStagedMetadata.metadata);
}

void AdoptReplayMetadata(const ReplayStagedMeta&& rStagedMetadata)
{
	game::gpGame->RestoreReplayMetadata(rStagedMetadata.metadata);
}

void OnReplayStreamsInvalidated()
{
	ResetReplayTransferFixtures(*game::gpServerSession);
}

void OnStateReplaced()
{
	game::gpServerSession->ResetClientsForLoad();
}

bool GameSaveLoad::ServerSave(const std::filesystem::path& rFilename)
{
	ScopedSuppressAllocationTracking suppress;
	return engine::WriteGridSave({engine::FileFlags::kAppDataDirectory, engine::FileFlags::kWrite}, rFilename, game::gpGame->mClientGridCoordinate);
}

bool GameSaveLoad::ServerLoad(const std::filesystem::path& rFilename)
{
	ScopedSuppressAllocationTracking suppress;
	gpProfileManager->LatchRawCpuTimers(false, game::gpGame->miTickCounter);

	engine::GridCoord loadedClientGridCoordinate {};
	if (!engine::ReadGridSave({engine::FileFlags::kAppDataDirectory, engine::FileFlags::kRead}, rFilename, loadedClientGridCoordinate))
	{
		return false;
	}

	int64_t iLoadedTick = game::gpGame->miTickCounter;
	float fLoadedTime = game::gpGame->mfCurrentTime;
	game::gpGame->Reset();
	// Reset clears the clock; restore the saved values before client resynchronization.
	int64_t iTickCounter = iLoadedTick;
	ASSERT(iTickCounter >= 0);
	game::gpGame->miTickCounter = iTickCounter;
	game::gpGame->mfCurrentTime = fLoadedTime;
	game::gpGame->SetClientGridCoordinate(loadedClientGridCoordinate);
	game::OnStateReplaced();
	game::gpServerSession->mpRuntime->ComputeActiveSet();

	return true;
}

void GameSaveLoad::ServerReset()
{
	ScopedSuppressAllocationTracking suppress;
	gpProfileManager->LatchRawCpuTimers(false, game::gpGame->miTickCounter);

	game::gpGame->CreateNewFrame(game::GameFlags::kGame);
	game::gpGame->miNextGlobalId = 1;
	game::gpGame->Reset();
	// Fresh-game wipe of fleet manager state. Load path leaves mFleets populated by ReadFleetData;
	// fresh-game has no save to restore from, so explicitly clear before ResetClientsForLoad runs.
	game::ResetSaveState();
	game::OnStateReplaced();
	game::gpServerSession->mpRuntime->ComputeActiveSet();
}

bool GameSaveLoad::Autosave()
{
	ScopedSuppressAllocationTracking suppress;
	return engine::WriteGridSave({engine::FileFlags::kAppDataDirectory, engine::FileFlags::kWrite, engine::FileFlags::kBackup}, std::filesystem::path("ServerAutosave.save"), game::gpGame->mClientGridCoordinate);
}

void GameSaveLoad::TickAutosave()
{
	if (game::gpGame->mbReplaying)
	{
		return;
	}

	if (!engine::gpReplay->mReplayWriters.empty())
	{
		return;
	}

	if (game::gpGame->mGameFlags & engine::GameFlags::kLoadReplay)
	{
		return;
	}

	if (mAutosaveTimer.GetDeltaNs(false) >= kAutosaveInterval)
	{
		bool bAutosaveSucceeded = Autosave();
		mAutosaveTimer.Reset();
		if (bAutosaveSucceeded)
		{
			LOG(kDefault, kInfo, "Periodic autosave succeeded (interval {}s)", kAutosaveInterval.count());
		}
		else
		{
			LOG(kDefault, kError, "Periodic autosave failed (interval {}s)", kAutosaveInterval.count());
		}
	}
}

bool GameSaveLoad::Autoload()
{
	ScopedSuppressAllocationTracking suppress;

	if (!std::filesystem::exists(engine::gpFileManager->GetFilePath({engine::FileFlags::kAppDataDirectory, engine::FileFlags::kRead}, std::filesystem::path("ServerAutosave.save"))))
	{
		return false;
	}

	engine::GridCoord loadedClientGridCoordinate {};
	if (!engine::ReadGridSave({engine::FileFlags::kAppDataDirectory, engine::FileFlags::kRead}, std::filesystem::path("ServerAutosave.save"), loadedClientGridCoordinate))
	{
		return false;
	}

	game::gpGame->SetClientGridCoordinate(loadedClientGridCoordinate);
	return true;
}

} // namespace game

#endif // BT_SERVER
