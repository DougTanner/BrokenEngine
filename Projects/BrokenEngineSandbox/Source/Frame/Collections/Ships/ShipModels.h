#pragma once

#if defined(BT_CLIENT)

#include "Data/Scene.h"

namespace game
{

// One model per ShipController, also used for animation lookup.
inline constexpr common::crc_t kuiOmniFleetModel = data::kModelsSpaceGliderGltfscenegltfCrc;
inline constexpr common::crc_t kuiOmniRandomModel = data::kModelsSpaceFighterscenegltfCrc;
inline constexpr common::crc_t kuiFighterFleetModel = data::kModelsRavenStarConflictV2scenegltfCrc;
inline constexpr common::crc_t kuiFighterRandomModel = data::kModelsSpaceshipscenegltfCrc;
inline constexpr common::crc_t kuiBattleshipFleetModel = data::kModelsBattleshipJesterz86scenegltfCrc;
inline constexpr common::crc_t kuiBattleshipRandomModel = data::kModelsSpaceBattleshipAquamarinescenegltfCrc;

} // namespace game

#endif // BT_CLIENT
