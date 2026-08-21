#pragma once

#if defined(BT_SERVER)

#include "Fleet.h"

namespace game
{

// Game-owned half of a staged grid save. The engine grid reader carries it without inspecting it.
struct SaveStagedState
{
	std::unordered_map<engine::ClientGuid, std::vector<Fleet>, engine::ClientGuidHash> fleets;
	std::unordered_map<engine::ClientGuid, int64_t, engine::ClientGuidHash> guidToClientId;
	common::RandomEngine randomEngine;
};

void WriteSaveState(std::fstream& rFileStream);
void ReadSaveState(std::fstream& rFileStream, SaveStagedState& rStagedState);
void AdoptSaveState(SaveStagedState&& rStagedState);
void ResetSaveState();

void SendFleetSync(int64_t iClientId, const std::vector<Fleet>& rFleets);

void WriteFleetData(std::fstream& rFileStream, const std::unordered_map<engine::ClientGuid, std::vector<Fleet>, engine::ClientGuidHash>& rFleets, const common::RandomEngine& rRandom);

void ReadFleetData(std::fstream& rFileStream, std::unordered_map<engine::ClientGuid, std::vector<Fleet>, engine::ClientGuidHash>& rFleets, std::unordered_map<engine::ClientGuid, int64_t, engine::ClientGuidHash>& rGuidToClientId, common::RandomEngine& rRandom);

} // namespace game

#endif // BT_SERVER
