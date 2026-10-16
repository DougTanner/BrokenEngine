#pragma once

#if defined(BT_SERVER)

#include "Fleet.h"

namespace game
{

// Game-owned half of a staged grid save. The engine grid reader carries it without inspecting it.
struct SaveStagedState
{
	std::unordered_map<engine::ClientGuid, std::vector<Fleet>> fleets;
	std::unordered_map<engine::ClientGuid, int64_t> guidToClientId;
	common::RandomEngine randomEngine;
};

void WriteSaveState(std::fstream& rFileStream);
void ReadSaveState(std::fstream& rFileStream, SaveStagedState& rStagedState);
void AdoptSaveState(SaveStagedState&& rStagedState);
void ResetSaveState();

} // namespace game

#endif // defined(BT_SERVER)
