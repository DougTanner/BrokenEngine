#include "Pch.h"

#include "Network/NetworkSerialization.h"

#include "Network/NetworkCursor.h"

#include "Frame/StatusChange.h"

namespace engine
{

static void SerializeBlasterTransfer(uint8_t*& rpCursor, const game::TransferData& rData)
{
	WriteVector4(rpCursor, rData.vecPosition);
	WriteVector4(rpCursor, rData.vecVelocity);
	WriteUint8(rpCursor, rData.uiTypeIndex);
	WriteUint32(rpCursor, rData.alignment.uiValue);
}

static void SerializeSpaceshipTransfer(uint8_t*& rpCursor, const game::TransferData& rData)
{
	WriteVector4(rpCursor, rData.vecPosition);
	WriteVector4(rpCursor, rData.vecDirection);
	WriteVector4(rpCursor, rData.vecVelocity);
	WriteUint32(rpCursor, rData.alignment.uiValue);
	WriteFloat(rpCursor, rData.fHealth);
	WriteFloat(rpCursor, rData.nextBlasterSpawnTimeSeconds.count());
	WriteFloat(rpCursor, rData.fDeltaRotation);
}

static void SerializeMissileTransfer(uint8_t*& rpCursor, const game::TransferData& rData)
{
	WriteVector4(rpCursor, rData.vecPosition);
	WriteVector4(rpCursor, rData.vecDirection);
	WriteVector4(rpCursor, rData.vecVelocity);
	WriteUint32(rpCursor, rData.alignment.uiValue);
	WriteFloat(rpCursor, rData.fAcceleration);
	WriteFloat(rpCursor, rData.deltaRotationDelaySeconds.count());
	WriteFloat(rpCursor, rData.timeSeconds.count());
	WriteFloat(rpCursor, rData.nextJitterSeconds.count());
	WriteFloat(rpCursor, rData.fDeltaRotation);
	WriteFloat(rpCursor, rData.fDeltaRotationMaximum);
	WriteFloat(rpCursor, rData.fPitch);
}

static void SerializePlayerTransfer(uint8_t*& rpCursor, const game::TransferData& rData)
{
	WriteVector4(rpCursor, rData.vecPosition);
	WriteVector4(rpCursor, rData.vecDirection);
	WriteVector4(rpCursor, rData.vecVelocity);
	WriteUint32(rpCursor, rData.alignment.uiValue);
	WriteFloat(rpCursor, rData.fHealth);
	WriteFloat(rpCursor, rData.fShield);
	WriteFloat(rpCursor, rData.nextBlasterFireTimeSeconds.count());
	WriteFloat(rpCursor, rData.nextSecondarySpawnTimeSeconds.count());
	WriteFloat(rpCursor, rData.shieldCooldownSeconds.count());
	WriteFloat(rpCursor, rData.shieldDownSoundCooldownSeconds.count());
	WriteFloat(rpCursor, rData.animationTimeSeconds.count());
	WriteUint16(rpCursor, rData.uiPlayerFlags);
	WriteFloat(rpCursor, rData.navigationDelaySeconds.count());
	WriteInt64(rpCursor, rData.globalPlayerId.iValue);
	WriteGridCoord(rpCursor, rData.fleetWantedCoordinate);
	WriteUint8(rpCursor, rData.uiPendingFleetWantedCoordinateTicks);
	WriteUint8(rpCursor, rData.uiPendingWeaponModeTicks);
}

static void DeserializeBlasterTransfer(const uint8_t*& rpCursor, game::TransferData& rData)
{
	rData.vecPosition = ReadVector4(rpCursor);
	rData.vecVelocity = ReadVector4(rpCursor);
	rData.uiTypeIndex = ReadUint8(rpCursor);
	rData.alignment = AlignmentIdentifier(ReadUint32(rpCursor));
}

static void DeserializeSpaceshipTransfer(const uint8_t*& rpCursor, game::TransferData& rData)
{
	rData.vecPosition = ReadVector4(rpCursor);
	rData.vecDirection = ReadVector4(rpCursor);
	rData.vecVelocity = ReadVector4(rpCursor);
	rData.alignment = AlignmentIdentifier(ReadUint32(rpCursor));
	rData.fHealth = ReadFloat(rpCursor);
	rData.nextBlasterSpawnTimeSeconds = std::chrono::duration<float>(ReadFloat(rpCursor));
	rData.fDeltaRotation = ReadFloat(rpCursor);
}

static void DeserializeMissileTransfer(const uint8_t*& rpCursor, game::TransferData& rData)
{
	rData.vecPosition = ReadVector4(rpCursor);
	rData.vecDirection = ReadVector4(rpCursor);
	rData.vecVelocity = ReadVector4(rpCursor);
	rData.alignment = AlignmentIdentifier(ReadUint32(rpCursor));
	rData.fAcceleration = ReadFloat(rpCursor);
	rData.deltaRotationDelaySeconds = std::chrono::duration<float>(ReadFloat(rpCursor));
	rData.timeSeconds = std::chrono::duration<float>(ReadFloat(rpCursor));
	rData.nextJitterSeconds = std::chrono::duration<float>(ReadFloat(rpCursor));
	rData.fDeltaRotation = ReadFloat(rpCursor);
	rData.fDeltaRotationMaximum = ReadFloat(rpCursor);
	rData.fPitch = ReadFloat(rpCursor);
}

static void DeserializePlayerTransfer(const uint8_t*& rpCursor, game::TransferData& rData)
{
	rData.vecPosition = ReadVector4(rpCursor);
	rData.vecDirection = ReadVector4(rpCursor);
	rData.vecVelocity = ReadVector4(rpCursor);
	rData.alignment = AlignmentIdentifier(ReadUint32(rpCursor));
	rData.fHealth = ReadFloat(rpCursor);
	rData.fShield = ReadFloat(rpCursor);
	rData.nextBlasterFireTimeSeconds = std::chrono::duration<float>(ReadFloat(rpCursor));
	rData.nextSecondarySpawnTimeSeconds = std::chrono::duration<float>(ReadFloat(rpCursor));
	rData.shieldCooldownSeconds = std::chrono::duration<float>(ReadFloat(rpCursor));
	rData.shieldDownSoundCooldownSeconds = std::chrono::duration<float>(ReadFloat(rpCursor));
	rData.animationTimeSeconds = std::chrono::duration<float>(ReadFloat(rpCursor));
	rData.uiPlayerFlags = ReadUint16(rpCursor);
	rData.navigationDelaySeconds = std::chrono::duration<float>(ReadFloat(rpCursor));
	rData.globalPlayerId.iValue = ReadInt64(rpCursor);
	rData.fleetWantedCoordinate = ReadGridCoord(rpCursor);
	rData.uiPendingFleetWantedCoordinateTicks = ReadUint8(rpCursor);
	rData.uiPendingWeaponModeTicks = ReadUint8(rpCursor);
}

// Serialize a group of StatusChanges that share the same type
static void SerializeGroup(uint8_t*& rpCursor, game::StatusChangeType eType, const game::StatusChange* pChanges, std::span<const int64_t> indices)
{
	int64_t iGroupCount = static_cast<int64_t>(indices.size());

	if (iGroupCount == 0)
	{
		return;
	}

	WriteUint8(rpCursor, static_cast<uint8_t>(eType));
	WriteUint16(rpCursor, static_cast<uint16_t>(iGroupCount));

	for (int64_t i = 0; i < iGroupCount; ++i)
	{
		const game::StatusChangeData& rData = pChanges[indices[i]].data;
		const uint8_t* pItemStart = rpCursor;

		switch (eType)
		{
			case game::StatusChangeType::kSpawnPlayer:
			{
				const game::SpawnPlayerData& rSpawn = std::get<game::SpawnPlayerData>(rData);
				WriteInt64(rpCursor, rSpawn.iGlobalId);
				WriteUint8(rpCursor, rSpawn.bIsFlagship ? 1 : 0);
				WriteGridCoord(rpCursor, rSpawn.fleetWantedCoordinate);
				WriteUint8(rpCursor, rSpawn.uiPendingFleetWantedCoordinateTicks);
				WriteFloat(rpCursor, rSpawn.fSpawnOffsetX);
				WriteFloat(rpCursor, rSpawn.fSpawnOffsetY);
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
			case game::StatusChangeType::kTransferPlayer:
				SerializePlayerTransfer(rpCursor, std::get<game::TransferData>(rData));
				break;
			case game::StatusChangeType::kDestroyPlayer:
				WriteInt64(rpCursor, std::get<game::DestroyPlayerData>(rData).iPlayerUuid);
				break;
			case game::StatusChangeType::kUpdatePlayer:
			{
				const game::UpdatePlayerData& rUpdate = std::get<game::UpdatePlayerData>(rData);
				WriteInt64(rpCursor, rUpdate.iPlayerUuid);
				WriteUint8(rpCursor, rUpdate.bUseMissiles ? 1 : 0);
				WriteFloat(rpCursor, rUpdate.navigationDelaySeconds.count());
				WriteUint8(rpCursor, rUpdate.uiPendingWeaponModeTicks);
				break;
			}
			case game::StatusChangeType::kUpdateFleet:
			{
				const game::UpdateFleetData& rUpdate = std::get<game::UpdateFleetData>(rData);
				WriteInt64(rpCursor, rUpdate.iPlayerUuid);
				WriteUint8(rpCursor, rUpdate.bIsFlagship ? 1 : 0);
				WriteGridCoord(rpCursor, rUpdate.fleetWantedCoordinate);
				WriteUint8(rpCursor, rUpdate.uiPendingFleetWantedCoordinateTicks);
				break;
			}
		}

		ASSERT(rpCursor - pItemStart <= kiMaxStatusChangeBytesPerItem);
	}
}

constexpr int64_t kiTypeCount = static_cast<int64_t>(game::StatusChangeType::kCount);

static void GroupIndicesByType(std::span<const game::StatusChange> changes, int64_t piOffsets[kiTypeCount], int64_t piCounts[kiTypeCount], std::span<int64_t> sortedIndices)
{
	int64_t iCount = static_cast<int64_t>(changes.size());

	for (int64_t i = 0; i < iCount; ++i)
	{
		++piCounts[static_cast<int64_t>(changes[i].eType)];
	}

	for (int64_t i = 1; i < kiTypeCount; ++i)
	{
		piOffsets[i] = piOffsets[i - 1] + piCounts[i - 1];
	}

	int64_t piWriteOffsets[kiTypeCount] = {};
	std::memcpy(piWriteOffsets, piOffsets, sizeof(int64_t) * kiTypeCount);
	for (int64_t i = 0; i < iCount; ++i)
	{
		int64_t iType = static_cast<int64_t>(changes[i].eType);
		sortedIndices[piWriteOffsets[iType]++] = i;
	}
}

int64_t SerializeStatusChangeBatch(std::span<const game::StatusChange> changes, void* pDestination)
{
	int64_t iCount = static_cast<int64_t>(changes.size());

	if (iCount == 0)
	{
		return 0;
	}

	uint8_t* pCursor = static_cast<uint8_t*>(pDestination);

	// Group indices by type in a workbuffer reservation (GroupIndicesByType writes every slot, so no zero-fill)
	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
	common::ScopedWorkbufferAllocation<int64_t*> sortedAllocation = rWorkbuffer.PushBuffer<int64_t*>(iCount * static_cast<int64_t>(sizeof(int64_t)));
	int64_t* pSorted = sortedAllocation.mpData;

	int64_t piGroupCounts[kiTypeCount] = {};
	int64_t piOffsets[kiTypeCount] = {};
	GroupIndicesByType(changes, piOffsets, piGroupCounts, std::span<int64_t>(pSorted, static_cast<size_t>(iCount)));

	for (int64_t i = 0; i < kiTypeCount; ++i)
	{
		SerializeGroup(pCursor, static_cast<game::StatusChangeType>(i), changes.data(), std::span<const int64_t>(pSorted + piOffsets[i], static_cast<size_t>(piGroupCounts[i])));
	}

	return pCursor - static_cast<uint8_t*>(pDestination);
}

int64_t DeserializeStatusChangeBatch(std::span<const uint8_t> source, game::StatusChange* pDestination)
{
	int64_t iSourceSize = static_cast<int64_t>(source.size());

	if (iSourceSize == 0)
	{
		return 0;
	}

	// Only the client decodes a batch, and it trusts its server's bytes, so the cursor only drives the loop to the end of
	// the batch; nothing is checked per group or item.
	BoundedCursor cursor {.pCursor = source.data(), .pEnd = source.data() + iSourceSize};
	int64_t iOutputCount = 0;

	while (cursor.Remaining() > 0)
	{
		uint8_t uiType = ReadUint8(cursor.pCursor);
		uint16_t uiGroupCount = ReadUint16(cursor.pCursor);

		game::StatusChangeType eType = static_cast<game::StatusChangeType>(uiType);

		for (int64_t i = 0; i < uiGroupCount; ++i)
		{
			game::StatusChange& rChange = pDestination[iOutputCount++];
			rChange = {};
			rChange.eType = eType;

			rChange.data = game::DefaultDataForType(eType);

			switch (eType)
			{
				case game::StatusChangeType::kSpawnPlayer:
				{
					game::SpawnPlayerData& rSpawn = std::get<game::SpawnPlayerData>(rChange.data);
					rSpawn.iGlobalId = ReadInt64(cursor.pCursor);
					rSpawn.bIsFlagship = ReadUint8(cursor.pCursor) != 0;
					rSpawn.fleetWantedCoordinate = ReadGridCoord(cursor.pCursor);
					rSpawn.uiPendingFleetWantedCoordinateTicks = ReadUint8(cursor.pCursor);
					rSpawn.fSpawnOffsetX = ReadFloat(cursor.pCursor);
					rSpawn.fSpawnOffsetY = ReadFloat(cursor.pCursor);
					break;
				}
				case game::StatusChangeType::kTransferBlaster:
					DeserializeBlasterTransfer(cursor.pCursor, std::get<game::TransferData>(rChange.data));
					break;
				case game::StatusChangeType::kTransferSpaceship:
					DeserializeSpaceshipTransfer(cursor.pCursor, std::get<game::TransferData>(rChange.data));
					break;
				case game::StatusChangeType::kTransferMissile:
					DeserializeMissileTransfer(cursor.pCursor, std::get<game::TransferData>(rChange.data));
					break;
				case game::StatusChangeType::kTransferPlayer:
					DeserializePlayerTransfer(cursor.pCursor, std::get<game::TransferData>(rChange.data));
					break;
				case game::StatusChangeType::kDestroyPlayer:
					std::get<game::DestroyPlayerData>(rChange.data).iPlayerUuid = ReadInt64(cursor.pCursor);
					break;
				case game::StatusChangeType::kUpdatePlayer:
				{
					game::UpdatePlayerData& rUpdate = std::get<game::UpdatePlayerData>(rChange.data);
					rUpdate.iPlayerUuid = ReadInt64(cursor.pCursor);
					rUpdate.bUseMissiles = ReadUint8(cursor.pCursor) != 0;
					rUpdate.navigationDelaySeconds = std::chrono::duration<float>(ReadFloat(cursor.pCursor));
					rUpdate.uiPendingWeaponModeTicks = ReadUint8(cursor.pCursor);
					break;
				}
				case game::StatusChangeType::kUpdateFleet:
				{
					game::UpdateFleetData& rUpdate = std::get<game::UpdateFleetData>(rChange.data);
					rUpdate.iPlayerUuid = ReadInt64(cursor.pCursor);
					rUpdate.bIsFlagship = ReadUint8(cursor.pCursor) != 0;
					rUpdate.fleetWantedCoordinate = ReadGridCoord(cursor.pCursor);
					rUpdate.uiPendingFleetWantedCoordinateTicks = ReadUint8(cursor.pCursor);
					break;
				}
			}
		}
	}

	return iOutputCount;
}

int64_t CompressStatusChangeBatch(std::span<const game::StatusChange> changes, std::span<uint8_t> destination)
{
	int64_t iCount = std::ssize(changes);
	int64_t iDestinationCapacity = std::ssize(destination);
	if (iCount == 0)
	{
		return 0;
	}

	// Serialize into a workbuffer reservation (SerializeStatusChangeBatch writes only iSerializedSize bytes and only
	// those are compressed, so no zero-fill), then LZ4 compress into destination
	static constexpr int64_t kiMaxGroupHeaders = kiTypeCount * 3;
	int64_t iMaxSerializedSize = kiMaxGroupHeaders + iCount * kiMaxStatusChangeBytesPerItem;

	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
	common::ScopedWorkbufferAllocation<uint8_t*> serializedAllocation = rWorkbuffer.PushBuffer<uint8_t*>(iMaxSerializedSize);
	uint8_t* pSerialized = serializedAllocation.mpData;

	int64_t iSerializedSize = SerializeStatusChangeBatch(changes, pSerialized);

	// Write 4-byte uncompressed size prefix, then LZ4 compressed data
	uint8_t* pOutput = destination.data();
	int32_t iUncompressedSize = static_cast<int32_t>(iSerializedSize);
	std::memcpy(pOutput, &iUncompressedSize, sizeof(int32_t));

	int64_t iCompressedSize = LZ4_compress_default(reinterpret_cast<const char*>(pSerialized), reinterpret_cast<char*>(pOutput + sizeof(int32_t)), static_cast<int>(iSerializedSize), static_cast<int>(iDestinationCapacity - sizeof(int32_t)));
	if (iCompressedSize <= 0)
	{
		// Belt (the caller sizes destination to fit any valid capped batch): a 0 return means the batch did not fit. Drop it
		// rather than ship the 4-byte prefix alone, which carries no decodable batch.
		LOG(kNetwork, kError, "CompressStatusChangeBatch: LZ4 compression failed (items {}, serialized {}, dest capacity {})", iCount, iSerializedSize, iDestinationCapacity);
		return 0;
	}

	return static_cast<int64_t>(sizeof(int32_t)) + iCompressedSize;
}

int64_t DecompressStatusChangeBatch(std::span<const uint8_t> source, game::StatusChange* pDestination)
{
	int64_t iSourceSize = std::ssize(source);
	const uint8_t* pInput = source.data();
	int32_t iUncompressedSize = 0;
	std::memcpy(&iUncompressedSize, pInput, sizeof(int32_t));

	// LZ4 writes iResult bytes and only those bytes are deserialized, so no zero-fill or extra slack is needed.
	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
	common::ScopedWorkbufferAllocation<uint8_t*> decompressedAllocation = rWorkbuffer.PushBuffer<uint8_t*>(iUncompressedSize);
	uint8_t* pDecompressed = decompressedAllocation.mpData;

	int64_t iResult = LZ4_decompress_safe(reinterpret_cast<const char*>(pInput + sizeof(int32_t)), reinterpret_cast<char*>(pDecompressed), static_cast<int>(iSourceSize - sizeof(int32_t)), iUncompressedSize);

	int64_t iCount = DeserializeStatusChangeBatch(std::span<const uint8_t>(pDecompressed, static_cast<size_t>(iResult)), pDestination);

	return iCount;
}

} // namespace engine
