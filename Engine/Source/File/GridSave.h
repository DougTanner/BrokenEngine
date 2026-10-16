#pragma once

#if defined(BT_SERVER)

#include "Save/GameSaveState.h"

namespace engine
{

// Isolated grid snapshot: staged by ReadGridSave, adopted only after the whole stream validates.
struct StagedGridSave
{
	GridCoord clientGridCoordinate {};
	int64_t iNextGlobalId = 0;
	int64_t iTick = 0;
	bool bHeaderValidated = false;
	std::unordered_map<GridCoord, Cell> cells;
	game::SaveStagedState saveState;
};

bool WriteGridSave(const FileFlags_t& rFlags, const std::filesystem::path& rFilename, GridCoord clientGridCoordinate);
bool ReadGridSave(const FileFlags_t& rFlags, const std::filesystem::path& rFilename, GridCoord& rClientGridCoordinate);
bool ReadGridSave(const FileFlags_t& rFlags, const std::filesystem::path& rFilename, StagedGridSave& rStagedGrid);
void AdoptGridSave(StagedGridSave&& rStagedGrid);

} // namespace engine

#endif // defined(BT_SERVER)
