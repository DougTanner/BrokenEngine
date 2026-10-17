#include "Pch.h"

#include "Network/StatusChangeCodec.h"

#include "Network/NetworkCursor.h"

#include "Frame/StatusChange.h"

namespace game
{

static void SerializeBlasterTransfer(uint8_t*& rpCursor, const game::TransferData& rData)
{
	engine::WriteVector4(rpCursor, rData.vecPosition);
	engine::WriteVector4(rpCursor, rData.vecVelocity);
	engine::WriteUint8(rpCursor, rData.uiTypeIndex);
	engine::WriteUint32(rpCursor, rData.alignment.uiValue);
}

static void SerializeSpaceshipTransfer(uint8_t*& rpCursor, const game::TransferData& rData)
{
	engine::WriteVector4(rpCursor, rData.vecPosition);
	engine::WriteVector4(rpCursor, rData.vecDirection);
	engine::WriteVector4(rpCursor, rData.vecVelocity);
	engine::WriteUint32(rpCursor, rData.alignment.uiValue);
	engine::WriteFloat(rpCursor, rData.fHealth);
	engine::WriteFloat(rpCursor, rData.nextBlasterSpawnTimeSeconds.count());
	engine::WriteFloat(rpCursor, rData.fDeltaRotation);
}

static void SerializeMissileTransfer(uint8_t*& rpCursor, const game::TransferData& rData)
{
	engine::WriteVector4(rpCursor, rData.vecPosition);
	engine::WriteVector4(rpCursor, rData.vecDirection);
	engine::WriteVector4(rpCursor, rData.vecVelocity);
	engine::WriteUint32(rpCursor, rData.alignment.uiValue);
	engine::WriteFloat(rpCursor, rData.fAcceleration);
	engine::WriteFloat(rpCursor, rData.deltaRotationDelaySeconds.count());
	engine::WriteFloat(rpCursor, rData.timeSeconds.count());
	engine::WriteFloat(rpCursor, rData.nextJitterSeconds.count());
	engine::WriteFloat(rpCursor, rData.fDeltaRotation);
	engine::WriteFloat(rpCursor, rData.fDeltaRotationMaximum);
	engine::WriteFloat(rpCursor, rData.fPitch);
}

static void SerializeOmniTransfer(uint8_t*& rpCursor, const game::TransferData& rData)
{
	engine::WriteVector4(rpCursor, rData.vecPosition);
	engine::WriteVector4(rpCursor, rData.vecDirection);
	engine::WriteVector4(rpCursor, rData.vecVelocity);
	engine::WriteUint32(rpCursor, rData.alignment.uiValue);
	engine::WriteFloat(rpCursor, rData.fHealth);
	engine::WriteFloat(rpCursor, rData.fShield);
	engine::WriteFloat(rpCursor, rData.nextBlasterFireTimeSeconds.count());
	engine::WriteFloat(rpCursor, rData.nextSecondarySpawnTimeSeconds.count());
	engine::WriteFloat(rpCursor, rData.shieldCooldownSeconds.count());
	engine::WriteFloat(rpCursor, rData.shieldDownSoundCooldownSeconds.count());
	engine::WriteFloat(rpCursor, rData.animationTimeSeconds.count());
	engine::WriteUint16(rpCursor, rData.uiOmniFlags);
	engine::WriteFloat(rpCursor, rData.navigationDelaySeconds.count());
	engine::WriteInt64(rpCursor, rData.globalId.iValue);
	engine::WriteGridCoord(rpCursor, rData.fleetWantedCoordinate);
	engine::WriteUint8(rpCursor, rData.uiPendingFleetWantedCoordinateTicks);
	engine::WriteUint8(rpCursor, rData.uiPendingWeaponModeTicks);
}

static void DeserializeBlasterTransfer(const uint8_t*& rpCursor, game::TransferData& rData)
{
	rData.vecPosition = engine::ReadVector4(rpCursor);
	rData.vecVelocity = engine::ReadVector4(rpCursor);
	rData.uiTypeIndex = engine::ReadUint8(rpCursor);
	rData.alignment = engine::AlignmentIdentifier(engine::ReadUint32(rpCursor));
}

static void DeserializeSpaceshipTransfer(const uint8_t*& rpCursor, game::TransferData& rData)
{
	rData.vecPosition = engine::ReadVector4(rpCursor);
	rData.vecDirection = engine::ReadVector4(rpCursor);
	rData.vecVelocity = engine::ReadVector4(rpCursor);
	rData.alignment = engine::AlignmentIdentifier(engine::ReadUint32(rpCursor));
	rData.fHealth = engine::ReadFloat(rpCursor);
	rData.nextBlasterSpawnTimeSeconds = std::chrono::duration<float>(engine::ReadFloat(rpCursor));
	rData.fDeltaRotation = engine::ReadFloat(rpCursor);
}

static void DeserializeMissileTransfer(const uint8_t*& rpCursor, game::TransferData& rData)
{
	rData.vecPosition = engine::ReadVector4(rpCursor);
	rData.vecDirection = engine::ReadVector4(rpCursor);
	rData.vecVelocity = engine::ReadVector4(rpCursor);
	rData.alignment = engine::AlignmentIdentifier(engine::ReadUint32(rpCursor));
	rData.fAcceleration = engine::ReadFloat(rpCursor);
	rData.deltaRotationDelaySeconds = std::chrono::duration<float>(engine::ReadFloat(rpCursor));
	rData.timeSeconds = std::chrono::duration<float>(engine::ReadFloat(rpCursor));
	rData.nextJitterSeconds = std::chrono::duration<float>(engine::ReadFloat(rpCursor));
	rData.fDeltaRotation = engine::ReadFloat(rpCursor);
	rData.fDeltaRotationMaximum = engine::ReadFloat(rpCursor);
	rData.fPitch = engine::ReadFloat(rpCursor);
}

static void DeserializeOmniTransfer(const uint8_t*& rpCursor, game::TransferData& rData)
{
	rData.vecPosition = engine::ReadVector4(rpCursor);
	rData.vecDirection = engine::ReadVector4(rpCursor);
	rData.vecVelocity = engine::ReadVector4(rpCursor);
	rData.alignment = engine::AlignmentIdentifier(engine::ReadUint32(rpCursor));
	rData.fHealth = engine::ReadFloat(rpCursor);
	rData.fShield = engine::ReadFloat(rpCursor);
	rData.nextBlasterFireTimeSeconds = std::chrono::duration<float>(engine::ReadFloat(rpCursor));
	rData.nextSecondarySpawnTimeSeconds = std::chrono::duration<float>(engine::ReadFloat(rpCursor));
	rData.shieldCooldownSeconds = std::chrono::duration<float>(engine::ReadFloat(rpCursor));
	rData.shieldDownSoundCooldownSeconds = std::chrono::duration<float>(engine::ReadFloat(rpCursor));
	rData.animationTimeSeconds = std::chrono::duration<float>(engine::ReadFloat(rpCursor));
	rData.uiOmniFlags = engine::ReadUint16(rpCursor);
	rData.navigationDelaySeconds = std::chrono::duration<float>(engine::ReadFloat(rpCursor));
	rData.globalId.iValue = engine::ReadInt64(rpCursor);
	rData.fleetWantedCoordinate = engine::ReadGridCoord(rpCursor);
	rData.uiPendingFleetWantedCoordinateTicks = engine::ReadUint8(rpCursor);
	rData.uiPendingWeaponModeTicks = engine::ReadUint8(rpCursor);
}

void WriteStatusChangePayload(uint8_t*& rpCursor, const StatusChange& rChange)
{
	const game::StatusChangeData& rData = rChange.data;

	switch (rChange.eType)
	{
		case game::StatusChangeType::kSpawnOmni:
		{
			const game::SpawnOmniData& rSpawn = std::get<game::SpawnOmniData>(rData);
			engine::WriteInt64(rpCursor, rSpawn.iGlobalId);
			engine::WriteUint8(rpCursor, rSpawn.bIsFlagship ? 1 : 0);
			engine::WriteGridCoord(rpCursor, rSpawn.fleetWantedCoordinate);
			engine::WriteUint8(rpCursor, rSpawn.uiPendingFleetWantedCoordinateTicks);
			engine::WriteFloat(rpCursor, rSpawn.fSpawnOffsetX);
			engine::WriteFloat(rpCursor, rSpawn.fSpawnOffsetY);
			break;
		}
		case game::StatusChangeType::kTransferBlaster:
			SerializeBlasterTransfer(rpCursor, std::get<game::TransferData>(rData));
			break;
		case game::StatusChangeType::kTransferSpaceship:
			SerializeSpaceshipTransfer(rpCursor, std::get<game::TransferData>(rData));
			break;
		case game::StatusChangeType::kTransferMissile:
			SerializeMissileTransfer(rpCursor, std::get<game::TransferData>(rData));
			break;
		case game::StatusChangeType::kTransferOmni:
			SerializeOmniTransfer(rpCursor, std::get<game::TransferData>(rData));
			break;
		case game::StatusChangeType::kDestroyOmni:
			engine::WriteInt64(rpCursor, std::get<game::DestroyOmniData>(rData).iOmniUuid);
			break;
		case game::StatusChangeType::kUpdateOmni:
		{
			const game::UpdateOmniData& rUpdate = std::get<game::UpdateOmniData>(rData);
			engine::WriteInt64(rpCursor, rUpdate.iOmniUuid);
			engine::WriteUint8(rpCursor, rUpdate.bUseMissiles ? 1 : 0);
			engine::WriteFloat(rpCursor, rUpdate.navigationDelaySeconds.count());
			engine::WriteUint8(rpCursor, rUpdate.uiPendingWeaponModeTicks);
			break;
		}
		case game::StatusChangeType::kUpdateFleet:
		{
			const game::UpdateFleetData& rUpdate = std::get<game::UpdateFleetData>(rData);
			engine::WriteInt64(rpCursor, rUpdate.iOmniUuid);
			engine::WriteUint8(rpCursor, rUpdate.bIsFlagship ? 1 : 0);
			engine::WriteGridCoord(rpCursor, rUpdate.fleetWantedCoordinate);
			engine::WriteUint8(rpCursor, rUpdate.uiPendingFleetWantedCoordinateTicks);
			break;
		}
	}
}

void ReadStatusChangePayload(const uint8_t*& rpCursor, StatusChange& rChange)
{
	rChange.data = game::DefaultDataForType(rChange.eType);

	switch (rChange.eType)
	{
		case game::StatusChangeType::kSpawnOmni:
		{
			game::SpawnOmniData& rSpawn = std::get<game::SpawnOmniData>(rChange.data);
			rSpawn.iGlobalId = engine::ReadInt64(rpCursor);
			rSpawn.bIsFlagship = engine::ReadUint8(rpCursor) != 0;
			rSpawn.fleetWantedCoordinate = engine::ReadGridCoord(rpCursor);
			rSpawn.uiPendingFleetWantedCoordinateTicks = engine::ReadUint8(rpCursor);
			rSpawn.fSpawnOffsetX = engine::ReadFloat(rpCursor);
			rSpawn.fSpawnOffsetY = engine::ReadFloat(rpCursor);
			break;
		}
		case game::StatusChangeType::kTransferBlaster:
			DeserializeBlasterTransfer(rpCursor, std::get<game::TransferData>(rChange.data));
			break;
		case game::StatusChangeType::kTransferSpaceship:
			DeserializeSpaceshipTransfer(rpCursor, std::get<game::TransferData>(rChange.data));
			break;
		case game::StatusChangeType::kTransferMissile:
			DeserializeMissileTransfer(rpCursor, std::get<game::TransferData>(rChange.data));
			break;
		case game::StatusChangeType::kTransferOmni:
			DeserializeOmniTransfer(rpCursor, std::get<game::TransferData>(rChange.data));
			break;
		case game::StatusChangeType::kDestroyOmni:
			std::get<game::DestroyOmniData>(rChange.data).iOmniUuid = engine::ReadInt64(rpCursor);
			break;
		case game::StatusChangeType::kUpdateOmni:
		{
			game::UpdateOmniData& rUpdate = std::get<game::UpdateOmniData>(rChange.data);
			rUpdate.iOmniUuid = engine::ReadInt64(rpCursor);
			rUpdate.bUseMissiles = engine::ReadUint8(rpCursor) != 0;
			rUpdate.navigationDelaySeconds = std::chrono::duration<float>(engine::ReadFloat(rpCursor));
			rUpdate.uiPendingWeaponModeTicks = engine::ReadUint8(rpCursor);
			break;
		}
		case game::StatusChangeType::kUpdateFleet:
		{
			game::UpdateFleetData& rUpdate = std::get<game::UpdateFleetData>(rChange.data);
			rUpdate.iOmniUuid = engine::ReadInt64(rpCursor);
			rUpdate.bIsFlagship = engine::ReadUint8(rpCursor) != 0;
			rUpdate.fleetWantedCoordinate = engine::ReadGridCoord(rpCursor);
			rUpdate.uiPendingFleetWantedCoordinateTicks = engine::ReadUint8(rpCursor);
			break;
		}
	}
}

} // namespace game
