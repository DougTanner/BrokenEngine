#include "Pch.h"

#if defined(BT_SERVER)

#include "Save/GameSaveLoad.h"

#include "Agent/Commands/ServerSimulationHarnessRigs.h"
#include "Network/Server/ServerSession.h"
#include "Game.h"

namespace game
{

bool WriteReplayMetadata(const engine::FileFlags_t& rFlags, const std::filesystem::path& rFilename)
{
	ReplayMeta metadata
	{
		.clientGridCoordinate = game::gpGame->mClientGridCoordinate,
		.iClientOwnedEntityIdentifierValue = game::gpGame->ClientOwnedEntityIdentifier().iValue,
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
	ResetReplayTransferHarnessRigs(*game::gpServerSession);
}

void OnStateReplaced()
{
	game::gpServerSession->ResetForLoad();
}

} // namespace game

#endif // BT_SERVER
