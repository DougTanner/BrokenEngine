// Generated from Frame/StatusChange.h; do not edit. Regenerate with `pwsh -NoProfile -File .agents/scripts/Write-AgentFieldNames.ps1`.
#pragma once

#if defined(BT_SERVER)

namespace game
{

template <auto pMember> inline constexpr std::string_view kStatusChangeFieldName {};

// SpawnPlayerData
template <> inline constexpr std::string_view kStatusChangeFieldName<&SpawnPlayerData::iGlobalId> = "iGlobalId";
template <> inline constexpr std::string_view kStatusChangeFieldName<&SpawnPlayerData::bIsFlagship> = "bIsFlagship";
template <> inline constexpr std::string_view kStatusChangeFieldName<&SpawnPlayerData::fleetWantedCoordinate> = "fleetWantedCoordinate";
template <> inline constexpr std::string_view kStatusChangeFieldName<&SpawnPlayerData::uiPendingFleetWantedCoordinateTicks> = "uiPendingFleetWantedCoordinateTicks";
template <> inline constexpr std::string_view kStatusChangeFieldName<&SpawnPlayerData::fSpawnOffsetX> = "fSpawnOffsetX";
template <> inline constexpr std::string_view kStatusChangeFieldName<&SpawnPlayerData::fSpawnOffsetY> = "fSpawnOffsetY";
template <> inline constexpr std::string_view kStatusChangeFieldName<&SpawnPlayerData::clientGuid> = "clientGuid";

// TransferData
template <> inline constexpr std::string_view kStatusChangeFieldName<&TransferData::vecPosition> = "vecPosition";
template <> inline constexpr std::string_view kStatusChangeFieldName<&TransferData::vecDirection> = "vecDirection";
template <> inline constexpr std::string_view kStatusChangeFieldName<&TransferData::vecVelocity> = "vecVelocity";
template <> inline constexpr std::string_view kStatusChangeFieldName<&TransferData::alignment> = "alignment";
template <> inline constexpr std::string_view kStatusChangeFieldName<&TransferData::fHealth> = "fHealth";
template <> inline constexpr std::string_view kStatusChangeFieldName<&TransferData::fShield> = "fShield";
template <> inline constexpr std::string_view kStatusChangeFieldName<&TransferData::uiTypeIndex> = "uiTypeIndex";
template <> inline constexpr std::string_view kStatusChangeFieldName<&TransferData::fAcceleration> = "fAcceleration";
template <> inline constexpr std::string_view kStatusChangeFieldName<&TransferData::nextBlasterFireTimeSeconds> = "nextBlasterFireTimeSeconds";
template <> inline constexpr std::string_view kStatusChangeFieldName<&TransferData::nextSecondarySpawnTimeSeconds> = "nextSecondarySpawnTimeSeconds";
template <> inline constexpr std::string_view kStatusChangeFieldName<&TransferData::shieldCooldownSeconds> = "shieldCooldownSeconds";
template <> inline constexpr std::string_view kStatusChangeFieldName<&TransferData::shieldDownSoundCooldownSeconds> = "shieldDownSoundCooldownSeconds";
template <> inline constexpr std::string_view kStatusChangeFieldName<&TransferData::animationTimeSeconds> = "animationTimeSeconds";
template <> inline constexpr std::string_view kStatusChangeFieldName<&TransferData::uiPlayerFlags> = "uiPlayerFlags";
template <> inline constexpr std::string_view kStatusChangeFieldName<&TransferData::nextBlasterSpawnTimeSeconds> = "nextBlasterSpawnTimeSeconds";
template <> inline constexpr std::string_view kStatusChangeFieldName<&TransferData::navigationDelaySeconds> = "navigationDelaySeconds";
template <> inline constexpr std::string_view kStatusChangeFieldName<&TransferData::deltaRotationDelaySeconds> = "deltaRotationDelaySeconds";
template <> inline constexpr std::string_view kStatusChangeFieldName<&TransferData::timeSeconds> = "timeSeconds";
template <> inline constexpr std::string_view kStatusChangeFieldName<&TransferData::nextJitterSeconds> = "nextJitterSeconds";
template <> inline constexpr std::string_view kStatusChangeFieldName<&TransferData::fDeltaRotation> = "fDeltaRotation";
template <> inline constexpr std::string_view kStatusChangeFieldName<&TransferData::fDeltaRotationMaximum> = "fDeltaRotationMaximum";
template <> inline constexpr std::string_view kStatusChangeFieldName<&TransferData::fPitch> = "fPitch";
template <> inline constexpr std::string_view kStatusChangeFieldName<&TransferData::globalPlayerId> = "globalPlayerId";
template <> inline constexpr std::string_view kStatusChangeFieldName<&TransferData::fleetWantedCoordinate> = "fleetWantedCoordinate";
template <> inline constexpr std::string_view kStatusChangeFieldName<&TransferData::uiPendingFleetWantedCoordinateTicks> = "uiPendingFleetWantedCoordinateTicks";
template <> inline constexpr std::string_view kStatusChangeFieldName<&TransferData::uiPendingWeaponModeTicks> = "uiPendingWeaponModeTicks";
template <> inline constexpr std::string_view kStatusChangeFieldName<&TransferData::uiClientGuidHigh> = "uiClientGuidHigh";
template <> inline constexpr std::string_view kStatusChangeFieldName<&TransferData::uiClientGuidLow> = "uiClientGuidLow";

// DestroyPlayerData
template <> inline constexpr std::string_view kStatusChangeFieldName<&DestroyPlayerData::iPlayerUuid> = "iPlayerUuid";

// UpdatePlayerData
template <> inline constexpr std::string_view kStatusChangeFieldName<&UpdatePlayerData::iPlayerUuid> = "iPlayerUuid";
template <> inline constexpr std::string_view kStatusChangeFieldName<&UpdatePlayerData::bUseMissiles> = "bUseMissiles";
template <> inline constexpr std::string_view kStatusChangeFieldName<&UpdatePlayerData::navigationDelaySeconds> = "navigationDelaySeconds";
template <> inline constexpr std::string_view kStatusChangeFieldName<&UpdatePlayerData::uiPendingWeaponModeTicks> = "uiPendingWeaponModeTicks";

// UpdateFleetData
template <> inline constexpr std::string_view kStatusChangeFieldName<&UpdateFleetData::iPlayerUuid> = "iPlayerUuid";
template <> inline constexpr std::string_view kStatusChangeFieldName<&UpdateFleetData::bIsFlagship> = "bIsFlagship";
template <> inline constexpr std::string_view kStatusChangeFieldName<&UpdateFleetData::fleetWantedCoordinate> = "fleetWantedCoordinate";
template <> inline constexpr std::string_view kStatusChangeFieldName<&UpdateFleetData::uiPendingFleetWantedCoordinateTicks> = "uiPendingFleetWantedCoordinateTicks";

} // namespace game

#endif // BT_SERVER
