#include "Pch.h"

#if defined(BT_SERVER)

#include "File/ServerSaveLoad.h"

#include "File/GridSave.h"
#include "File/Replay.h"
#include "GameBase.h"

#include "Network/Server/ServerSession.h"
#include "Game.h"

namespace engine
{

constexpr std::string_view kAutosaveFilename = "ServerAutosave.save";

bool ServerSaveLoad::ServerSave(const std::filesystem::path& rFilename)
{
	// Heap: the sorted coord key list, file paths, and grid save file I/O
	ScopedSuppressAllocationTracking suppress;
	return engine::WriteGridSave({engine::FileFlags::kWrite}, rFilename, game::gpGame->mClientGridCoordinate);
}

bool ServerSaveLoad::ServerLoad(const std::filesystem::path& rFilename)
{
	// Heap: grid save file I/O, the staged snapshot's frames, and the session reset that precedes adoption
	ScopedSuppressAllocationTracking suppress;
	gpProfileManager->LatchRawCpuTimers(false);

	engine::StagedGridSave stagedGrid;
	if (!engine::ReadGridSave(rFilename, stagedGrid))
	{
		return false;
	}

	engine::GridCoord loadedClientGridCoordinate = stagedGrid.clientGridCoordinate;
	game::gpGame->ResetSession();
	engine::AdoptGridSave(std::move(stagedGrid));
	game::gpGame->SetClientGridCoordinate(loadedClientGridCoordinate);
	game::gpServerSession->ResetForLoad();
	game::gpServerSession->mpRuntime->ComputeActiveSet();

	return true;
}

void ServerSaveLoad::ServerReset()
{
	// Heap: new frame creation and the session reset
	ScopedSuppressAllocationTracking suppress;
	gpProfileManager->LatchRawCpuTimers(false);

	game::gpGame->CreateNewFrame();
	game::gpGame->miNextGlobalId = 1;
	game::gpGame->ResetSession();
	// Fresh-game wipe of the game save state. A load leaves it populated by AdoptGridSave;
	// a fresh game has no save to restore from, so explicitly clear it before ResetForLoad runs.
	game::ResetSaveState();
	game::gpServerSession->ResetForLoad();
	game::gpServerSession->mpRuntime->ComputeActiveSet();
}

bool ServerSaveLoad::Autosave()
{
	// Heap: the sorted coord key list, file paths, the backup copy, and grid save file I/O
	ScopedSuppressAllocationTracking suppress;
	return engine::WriteGridSave({engine::FileFlags::kWrite, engine::FileFlags::kBackup}, std::filesystem::path(kAutosaveFilename), game::gpGame->mClientGridCoordinate);
}

void ServerSaveLoad::TickAutosave()
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

void ServerSaveLoad::FinalAutosave()
{
	if (mbFinalAutosaveDone)
	{
		return;
	}

	mbFinalAutosaveDone = true;
	if (!Autosave())
	{
		LOG(kDefault, kError, "Final autosave failed during shutdown");
	}
}

bool ServerSaveLoad::Autoload()
{
	// Heap: the autosave file paths
	ScopedSuppressAllocationTracking suppress;

	if (!std::filesystem::exists(engine::gpFileManager->GetFilePath(std::filesystem::path(kAutosaveFilename))))
	{
		return false;
	}

	return ServerLoad(std::filesystem::path(kAutosaveFilename));
}

} // namespace engine

#endif // BT_SERVER
