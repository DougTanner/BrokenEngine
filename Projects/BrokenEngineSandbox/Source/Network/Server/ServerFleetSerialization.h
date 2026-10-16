#pragma once

#if defined(BT_SERVER)

#include "Save/GameSaveState.h"
#include "Fleet.h"

namespace game
{

void SendFleetSync(int64_t iClientId, const std::vector<Fleet>& rFleets);

void WriteFleetData(std::fstream& rFileStream, const std::unordered_map<engine::ClientGuid, std::vector<Fleet>>& rFleets, const common::RandomEngine& rRandom);

void ReadFleetData(std::fstream& rFileStream, std::unordered_map<engine::ClientGuid, std::vector<Fleet>>& rFleets, std::unordered_map<engine::ClientGuid, int64_t>& rGuidToClientId, common::RandomEngine& rRandom);

} // namespace game

#endif // BT_SERVER
