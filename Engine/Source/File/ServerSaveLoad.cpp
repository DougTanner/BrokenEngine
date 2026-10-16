#include "Pch.h"

#if defined(BT_SERVER)

#include "File/ServerSaveLoad.h"

#include "File/GridSave.h"
#include "File/Replay.h"
#include "GameBase.h"

#include "Network/Server/ServerSession.h"
#include "Profile/ProfileManager.h"
#include "Game.h"

namespace engine
{

bool ServerSaveLoad::ServerSave(const std::filesystem::path& rFilename)
{
	ScopedSuppressAllocationTracking suppress;
	return engine::WriteGridSave({engine::FileFlags::kAppDataDirectory, engine::FileFlags::kWrite}, rFilename, game::gpGame->mClientGridCoordinate);
}

bool ServerSaveLoad::ServerLoad(const std::filesystem::path& rFilename)
{
	ScopedSuppressAllocationTracking suppress;
	gpProfileManager->LatchRawCpuTimers(false);

	engine::GridCoord loadedClientGridCoordinate {};
	if (!engine::ReadGridSave({engine::FileFlags::kAppDataDirectory, engine::FileFlags::kRead}, rFilename, loadedClientGridCoordinate))
	{
		return false;
	}

	int64_t iLoadedTick = game::gpGame->miTickCounter;
	game::gpGame->ResetSession();
	// ResetSession clears the tick counter; restore the saved tick before client resynchronization.
	game::gpGame->miTickCounter = iLoadedTick;
	game::gpGame->SetClientGridCoordinate(loadedClientGridCoordinate);
	game::OnStateReplaced();
	game::gpServerSession->mpRuntime->ComputeActiveSet();

	return true;
}

void ServerSaveLoad::ServerReset()
{
	ScopedSuppressAllocationTracking suppress;
	gpProfileManager->LatchRawCpuTimers(false);

	game::gpGame->CreateNewFrame();
	game::gpGame->miNextGlobalId = 1;
	game::gpGame->ResetSession();
	// Fresh-game wipe of the game save state. A load leaves it populated by ReadGridSave;
	// a fresh game has no save to restore from, so explicitly clear it before ResetClientsForLoad runs.
	game::ResetSaveState();
	game::OnStateReplaced();
	game::gpServerSession->mpRuntime->ComputeActiveSet();
}

bool ServerSaveLoad::Autosave()
{
	ScopedSuppressAllocationTracking suppress;
	return engine::WriteGridSave({engine::FileFlags::kAppDataDirectory, engine::FileFlags::kWrite, engine::FileFlags::kBackup}, std::filesystem::path("ServerAutosave.save"), game::gpGame->mClientGridCoordinate);
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

bool ServerSaveLoad::Autoload()
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

} // namespace engine

#endif // BT_SERVER
