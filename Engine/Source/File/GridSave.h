#pragma once

#if defined(BT_SERVER)

#include "Network/Server/ServerFleetSerialization.h"

namespace engine
{

// Isolated grid snapshot: staged by ReadGridSave, adopted only after the whole stream validates.
struct StagedGridSave
{
	GridCoord clientGridCoord {};
	int64_t iNextGlobalId = 0;
	int64_t iTick = 0;
	float fCurrentTime = 0.0f;
	bool bHeaderValidated = false;
	std::unordered_map<GridCoord, CoordFrames> coordFrames;
	game::SaveStagedState saveState;
};

bool WriteGridSave(GameBase& rGameBase, const FileFlags_t& rFlags, const std::filesystem::path& rFilename, GridCoord clientGridCoord);
bool ReadGridSave(GameBase& rGameBase, const FileFlags_t& rFlags, const std::filesystem::path& rFilename, GridCoord& rClientGridCoord);
bool ReadGridSave(const FileFlags_t& rFlags, const std::filesystem::path& rFilename, StagedGridSave& rStagedGrid);
void AdoptGridSave(GameBase& rGameBase, StagedGridSave&& rStagedGrid);

} // namespace engine

#endif // defined(BT_SERVER)
