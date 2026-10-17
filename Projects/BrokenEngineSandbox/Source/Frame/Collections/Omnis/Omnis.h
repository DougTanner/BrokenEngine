#pragma once

#include "Frame/Collections/Collection.h"
#include "Frame/Alignments.h"
#include "Frame/GridCoord.h"
#if defined(BT_CLIENT)
#include "Frame/Collections/HexShields/HexShields.h"
#include "Frame/Collections/WindTrails/WindTrails.h"
#endif

namespace engine
{
struct CellStaticData;
} // namespace engine


namespace game
{

// Shared constants (used across Omnis*.cpp files)
inline constexpr std::chrono::duration<float> kDestroyTime = std::chrono::duration<float>(0.7f);
inline constexpr std::chrono::duration<float> kDestroyExplosionInterval = std::chrono::duration<float>(0.005f);
inline constexpr float kfOmniBlastersSpeed = 150.0f;
inline constexpr std::chrono::duration<float> kBlasterFireInterval = std::chrono::duration<float>(0.05f);

inline constexpr float kfOmniRadius = 1.1f;

inline constexpr float kfPushMargin = kfOmniRadius * 0.6667f;

// Movement — three independent tuning axes
inline constexpr float kfOmniAcceleration = 75.0f;
inline constexpr float kfOmniCatchUpAcceleration = 100.0f;
inline constexpr float kfOmniDrag = 1.0f;
inline constexpr float kfOmniMaximumSpeed = 33.33f;
inline constexpr float kfOmniCatchUpMaximumSpeed = 33.33f;
inline constexpr float kfOmniJitterRange = 0.60f;

inline constexpr float kfOmniPusherRadius = kfOmniRadius * 3.3333f;
inline constexpr float kfOmniPusherIntensity = 50.0f;
inline constexpr float kfOmniPusherPower = 2.0f;
inline constexpr float kfOmniMaximumPusherPushVelocity = kfOmniMaximumSpeed * 0.5f;

#if defined(BT_CLIENT)
struct HexShieldDirections
{
	XMFLOAT4A data[shaders::kiHexShieldDirections] {};
};

struct HexShieldIntensities
{
	float data[shaders::kiHexShieldDirections] {};
};
#endif // BT_CLIENT

struct OmnisInterpolate : public engine::Collection<OmnisInterpolate, engine::CollectionFlags::kIdToIndex>
{
	static constexpr int64_t kiVersion = 13;
	static constexpr char kName[] = "Omni";
	static constexpr common::crc_t kCrc = common::CrcConsteval(kName);

	static void Register();

	static void GraphicsResources();

	static inline int64_t siAreaLightTypeIndex = 0xFF;
	static inline int64_t siBlasterTypeIndex = 0xFF;
	static inline int64_t siExplosionTypeIndex = 0xFF;
	static inline int64_t siImpactPuffControllerTypeIndex = engine::kiInvalidControllerType;
	static inline int64_t siImpactPointLightControllerTypeIndex = engine::kiInvalidControllerType;
	static inline int64_t siHexShieldTypeIndex = 0xFF;

	// Simulation keeps the destroyed time in [0, kDestroyTime]; reject anything else as a corrupt stream, since a huge or +inf value
	// puts the death-explosion particle count cast out of range. Written as one range test so NaN and both infinities fail it too.
	static void PostRead(const OmnisInterpolate& rCurrent)
	{
		for (int64_t i = 0; i < rCurrent.iCount; ++i)
		{
			std::chrono::duration<float> destroyedTime = rCurrent.pDestroyedTimes[i];
			if (!(destroyedTime.count() >= 0.0f && destroyedTime.count() <= kDestroyTime.count()))
			{
				throw std::ios_base::failure("OmnisInterpolate pDestroyedTimes");
			}
		}
	}

	static void Update(FrameInterpolate& __restrict rFrameInterpolate, const Frame& __restrict rPreviousFrame);

	XMVECTOR* __restrict pVecPositions = nullptr;
	XMVECTOR* __restrict pVecDirections = nullptr;
	std::chrono::duration<float>* __restrict pDestroyedTimes = nullptr;
	std::chrono::duration<float>* __restrict pAnimationTimes = nullptr;
	engine::pusher_t* __restrict pPushers = nullptr;
#if defined(BT_CLIENT)
	float* __restrict pfRotationAccelerationXs = nullptr;
	float* __restrict pfRotationAccelerationYs = nullptr;
	engine::wind_trail_t* __restrict pWindTrails = nullptr;
	engine::hex_shields_t* __restrict pHexShields = nullptr;
	float* __restrict pfShieldRotations = nullptr;
	float* __restrict pfShieldShrinks = nullptr;
	HexShieldDirections* __restrict pHexShieldDirections = nullptr;
	HexShieldIntensities* __restrict pHexShieldVertexIntensities = nullptr;
	HexShieldIntensities* __restrict pHexShieldFragmentIntensities = nullptr;
#endif // BT_CLIENT

	auto SharedMembers(this auto&& rSelf)
	{
		return std::tie(rSelf.pVecPositions, rSelf.pVecDirections, rSelf.pDestroyedTimes, rSelf.pAnimationTimes, rSelf.pPushers);
	}
#if defined(BT_CLIENT)
	auto ClientMembers(this auto&& rSelf)
	{
		return std::tie(rSelf.pfRotationAccelerationXs, rSelf.pfRotationAccelerationYs, rSelf.pWindTrails, rSelf.pHexShields, rSelf.pfShieldRotations, rSelf.pfShieldShrinks, rSelf.pHexShieldDirections, rSelf.pHexShieldVertexIntensities, rSelf.pHexShieldFragmentIntensities);
	}
#endif // BT_CLIENT
	auto Members(this auto&& rSelf)
	{
#if defined(BT_CLIENT)
		return std::tuple_cat(rSelf.SharedMembers(), rSelf.ClientMembers());
#else
		return rSelf.SharedMembers();
#endif
	}
	auto PersistentMembers(this auto&& rSelf)
	{
#if defined(BT_CLIENT)
		return std::tie(rSelf.pPushers, rSelf.pWindTrails, rSelf.pHexShields);
#else
		return std::tie(rSelf.pPushers);
#endif
	}

	// CRC excludes client-only animation time and cell-local pusher handles.
	auto SharedCrcMembers(this auto&& rSelf)
	{
		return std::tie(rSelf.pVecPositions, rSelf.pVecDirections, rSelf.pDestroyedTimes);
	}

	static void BeginRender(int64_t iCommandBuffer, const std::unordered_map<engine::GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<engine::GridCoord>& rActiveCoordinates);
	static void Render(const FrameInterpolate& __restrict rFrameInterpolate, int64_t iCommandBuffer);
	static void EndRender(int64_t iCommandBuffer);
	static void DebugRender(const FrameInterpolate& __restrict rFrameInterpolate, engine::GridCoord coordinate);

	bool LogDifferences(const OmnisInterpolate& rOther) const;

#if defined(BT_CLIENT)
	// Helper shared by Omnis.cpp (Destroy, spawn-status-change cleanup) and OmnisNavigation.cpp (Transfer)
	static void RemoveOwnedVisuals(Frame& rFrame, OmnisInterpolate& rCurrentInterpolate, int64_t i);
#endif
};

enum class OmniFlags : uint16_t
{
	kExploding        = 0x0001,
	kFireBlaster      = 0x0002,
	kBlasterSpawnLeft = 0x0004,
	kFireMissile      = 0x0008,
	kMissileSpawnLeft = 0x0010,
	kTransfer         = 0x0020,
	kUseMissiles      = 0x0040,
	kIsFlagship       = 0x0080,

	// Nav direction in bits 8-10 (3 bits, stored as value+1: 0-6 representing -1 to 5)
	kNavigationDirectionBit0 = 0x0100,
	kNavigationDirectionBit1 = 0x0200,
	kNavigationDirectionBit2 = 0x0400,

	// Pending weapon mode (bit 11)
	kPendingUseMissiles = 0x0800,

	// Nav waypoint index in bits 12-13 (2 bits): 0 = largest island, 1 = smallest, 2 = random (saturates at 2)
	kNavigationWaypointBit0 = 0x1000,
	kNavigationWaypointBit1 = 0x2000,
};
using OmniFlags_t = common::Flags<OmniFlags>;

inline constexpr int64_t GetNavigationDirection(OmniFlags_t flags)
{
	return static_cast<int64_t>(((std::to_underlying(flags.meFlags) >> 8) & 0x7) - 1);
}

inline void SetNavigationDirection(OmniFlags_t& rFlags, int64_t iDirection)
{
	uint16_t uiRaw = std::to_underlying(rFlags.meFlags);
	uiRaw = static_cast<uint16_t>((uiRaw & ~0x0700) | (static_cast<uint16_t>(iDirection + 1) << 8));
	rFlags.meFlags = static_cast<OmniFlags>(uiRaw);
}

inline constexpr int64_t GetNavigationWaypointIndex(OmniFlags_t flags)
{
	return static_cast<int64_t>((std::to_underlying(flags.meFlags) >> 12) & 0x3);
}

inline void SetNavigationWaypointIndex(OmniFlags_t& rFlags, int64_t iIndex)
{
	uint16_t uiRaw = std::to_underlying(rFlags.meFlags);
	uiRaw = static_cast<uint16_t>((uiRaw & ~0x3000) | (static_cast<uint16_t>(iIndex) << 12));
	rFlags.meFlags = static_cast<OmniFlags>(uiRaw);
}

struct OmnisPostRender : public engine::Collection<OmnisPostRender>
{
	static constexpr int64_t kiVersion = 19;

	// Collision layer (set each frame in PreCollision)
	// thread_local: parallel per-Frame tick via Dispatch
	static inline thread_local int64_t siCollisionLayerIndex = 0;


	// Simulation keeps the blaster countdown in [0, kBlasterFireInterval], which bounds SpawnBlasters to one shot per tick.
	// Written as one range test so NaN and both infinities fail it too.
	static bool IsBlasterFireTimeInRange(float fTime)
	{
		return fTime >= 0.0f && fTime <= kBlasterFireInterval.count();
	}

	// Navigation delays in seconds match the HUD slider's [0, 60] range. Written as one range test so NaN and both infinities fail it too.
	static bool IsNavigationDelayInRange(float fDelay)
	{
		return fDelay >= 0.0f && fDelay <= 60.0f;
	}

	// Reject an out-of-range countdown as a corrupt stream: NaN or +inf never fires, and a large negative value never finishes the burst loop.
	// Reject an out-of-range navigation delay too, since the client echoes it back in its update-owned-entity request.
	static void PostRead(const OmnisPostRender& rCurrent)
	{
		for (int64_t i = 0; i < rCurrent.iCount; ++i)
		{
			if (!IsBlasterFireTimeInRange(rCurrent.pNextBlasterFireTimes[i].count()))
			{
				throw std::ios_base::failure("OmnisPostRender pNextBlasterFireTimes");
			}
			if (!IsNavigationDelayInRange(rCurrent.pNavigationDelays[i].count()))
			{
				throw std::ios_base::failure("OmnisPostRender pNavigationDelays");
			}
		}
	}

	// Update (orchestrator in Omnis.cpp; per-omni phase work split into helpers below)
	static void Update(Frame& __restrict rFrame, const Frame& __restrict rPreviousFrame, const engine::CellStaticData& rStaticData);
	static void ProcessUpdateStatusChanges(Frame& __restrict rFrame, const FrameInput& __restrict rFrameInput, const engine::CellStaticData& rStaticData);

	// Collision (PreCollision: Omnis.cpp; PostCollision: OmnisCombat.cpp)
	static void PreCollision(Frame& __restrict rFrame, const Frame& __restrict rPreviousFrame, const engine::CellStaticData& rStaticData);
	static void PostCollision(Frame& __restrict rFrame, const Frame& __restrict rPreviousFrame, const engine::CellStaticData& rStaticData);

	// Lifecycle (Transfer: OmnisNavigation.cpp; Destroy/Spawn: Omnis.cpp)
	static void Transfer(Frame& __restrict rFrame, const engine::CellStaticData& rStaticData);
	static void Destroy(Frame& __restrict rFrame, const engine::CellStaticData& rStaticData);
	static void Spawn(Frame& __restrict rFrame, const FrameInput& __restrict rFrameInput, const engine::CellStaticData& rStaticData);

private:
	// Per-omni Update helpers (called from OmnisPostRender::Update orchestrator)
	// Defined in OmnisNavigation.cpp:
	static void XM_CALLCONV ComputeNavigation(Frame& __restrict rFrame, const Frame& __restrict rPreviousFrame, const engine::CellStaticData& rStaticData, int64_t i, FXMVECTOR vecPosition, FXMVECTOR vecCellCenter, engine::GridCoord fleetWantedCoordinate, int64_t iPendingFleetWantedCoordinateTicks, OmniFlags_t flags, std::chrono::duration<float> deltaTime, int64_t& riNavigationDirection, int64_t& riNavigationWaypointIndex, XMVECTOR& rVecArtificialIntelligenceDirection, XMVECTOR& rVecIslandDestination, std::chrono::duration<float>& rCellChangeTimer);
	static void XM_CALLCONV ApplyMovement(int64_t iNavigationDirection, FXMVECTOR vecArtificialIntelligenceDirection, std::chrono::duration<float> deltaTime, float fAccelerationMultiplier, float fDecayMultiplier, XMVECTOR& rVecVelocity);
	static void XM_CALLCONV ApplyTerrainPush(const engine::CellStaticData& rStaticData, FXMVECTOR vecPosition, XMVECTOR& rVecVelocity);
	static void XM_CALLCONV ResolveTerrainContact(const engine::CellStaticData& rStaticData, int64_t i, FXMVECTOR vecPreviousPosition, XMVECTOR& rVecPosition, XMVECTOR& rVecVelocity);
	static void XM_CALLCONV ApplyPusherPush(const Frame& __restrict rFrame, const Frame& __restrict rPreviousFrame, int64_t i, FXMVECTOR vecPosition, XMVECTOR& rVecVelocity);

	// Defined in OmnisCombat.cpp:
	static void XM_CALLCONV AcquireTarget(const Frame& __restrict rPreviousFrame, FXMVECTOR vecPosition, OmniFlags_t& rFlags, bool& rbLookTargetFound, XMVECTOR& rVecLookPosition);
	static void XM_CALLCONV UpdateFacing(FXMVECTOR vecPosition, FXMVECTOR vecLookPosition, XMVECTOR& rVecWantedDirection);
	static void RegenerateShield(std::chrono::duration<float> deltaTime, std::chrono::duration<float> shieldCooldown, float& rfShield);

	// Weapon and death-explosion spawn helpers (called from OmnisPostRender::Spawn). Defined in OmnisCombat.cpp.
	static void SpawnBlasters(Frame& __restrict rFrame, engine::GridCoord emitterCoordinate);
	static void SpawnMissiles(Frame& __restrict rFrame, engine::GridCoord emitterCoordinate);
	static void SpawnDeathExplosions(Frame& __restrict rFrame, engine::GridCoord emitterCoordinate);

public:

	omni_t* __restrict pIds = nullptr;
	OmniFlags_t* __restrict pFlags = nullptr;
	engine::AlignmentIdentifier* __restrict pAlignments = nullptr;
	std::chrono::duration<float>* __restrict pNextBlasterFireTimes = nullptr;
	std::chrono::duration<float>* __restrict pNextSecondarySpawnTimes = nullptr;
	XMVECTOR* __restrict pVecVelocities = nullptr;
	XMVECTOR* __restrict pVecWantedDirections = nullptr;
	float* __restrict pfArmors = nullptr;
	float* __restrict pfShields = nullptr;
	std::chrono::duration<float>* __restrict pShieldCooldowns = nullptr;
	std::chrono::duration<float>* __restrict pDestroyedExplosionTimes = nullptr;
	std::chrono::duration<float>* __restrict pShieldDownSoundCooldowns = nullptr;
	XMVECTOR* __restrict pVecAiDirections = nullptr;
	std::chrono::duration<float>* __restrict pTransferLockTimers = nullptr;
	std::chrono::duration<float>* __restrict pArrivalGracePeriods = nullptr;
	std::chrono::duration<float>* __restrict pCellChangeTimers = nullptr;
	XMVECTOR* __restrict pVecIslandDestinations = nullptr;
	engine::ClientGuid* __restrict pClientGuids = nullptr;
	engine::GlobalId* __restrict pGlobalIds = nullptr;
	std::chrono::duration<float>* __restrict pNavigationDelays = nullptr;
	engine::GridCoord* __restrict pFleetWantedCoordinates = nullptr;
	uint8_t* __restrict puiPendingFleetWantedCoordinateTicks = nullptr;
	uint8_t* __restrict puiPendingWeaponModeTicks = nullptr;
#if defined(BT_CLIENT)
	XMVECTOR* __restrict pVecDebugNavigationWaypoints = nullptr;
#endif // BT_CLIENT

	auto SharedMembers(this auto&& rSelf)
	{
		return std::tie(rSelf.pIds, rSelf.pFlags, rSelf.pAlignments, rSelf.pNextBlasterFireTimes, rSelf.pNextSecondarySpawnTimes, rSelf.pVecVelocities, rSelf.pVecWantedDirections, rSelf.pfArmors, rSelf.pfShields, rSelf.pShieldCooldowns, rSelf.pDestroyedExplosionTimes, rSelf.pShieldDownSoundCooldowns, rSelf.pVecAiDirections, rSelf.pTransferLockTimers, rSelf.pArrivalGracePeriods, rSelf.pCellChangeTimers, rSelf.pVecIslandDestinations, rSelf.pClientGuids, rSelf.pGlobalIds, rSelf.pNavigationDelays, rSelf.pFleetWantedCoordinates, rSelf.puiPendingFleetWantedCoordinateTicks, rSelf.puiPendingWeaponModeTicks);
	}
#if defined(BT_CLIENT)
	auto ClientMembers(this auto&& rSelf)
	{
		return std::tie(rSelf.pVecDebugNavigationWaypoints);
	}
#endif // BT_CLIENT
	auto Members(this auto&& rSelf)
	{
#if defined(BT_CLIENT)
		return std::tuple_cat(rSelf.SharedMembers(), rSelf.ClientMembers());
#else
		return rSelf.SharedMembers();
#endif
	}
	auto PersistentMembers(this auto&& rSelf)
	{
		return std::tie(rSelf.pIds, rSelf.pAlignments, rSelf.pClientGuids, rSelf.pGlobalIds);
	}

	// CRC-only subset: excludes pClientGuids and pGlobalIds which are server-side bookkeeping
	auto SharedCrcMembers(this auto&& rSelf)
	{
		return std::tie(rSelf.pIds, rSelf.pFlags, rSelf.pAlignments, rSelf.pNextBlasterFireTimes, rSelf.pNextSecondarySpawnTimes, rSelf.pVecVelocities, rSelf.pVecWantedDirections, rSelf.pfArmors, rSelf.pfShields, rSelf.pShieldCooldowns, rSelf.pDestroyedExplosionTimes, rSelf.pShieldDownSoundCooldowns, rSelf.pVecAiDirections, rSelf.pTransferLockTimers, rSelf.pArrivalGracePeriods, rSelf.pCellChangeTimers, rSelf.pVecIslandDestinations, rSelf.pNavigationDelays, rSelf.pFleetWantedCoordinates, rSelf.puiPendingFleetWantedCoordinateTicks, rSelf.puiPendingWeaponModeTicks);
	}

	bool LogDifferences(const OmnisPostRender& rOther) const;

	struct SpawnInfo
	{
		XMVECTOR vecPosition = DirectX::XMVectorZero();
		XMVECTOR vecDirection = DirectX::XMVectorZero();
		XMVECTOR vecVelocity = DirectX::XMVectorZero();
		engine::AlignmentIdentifier alignment {};
		float fArmor = 0.0f;
		float fShield = 0.0f;
		std::chrono::duration<float> nextBlasterFireTime = std::chrono::duration<float>::zero();
		std::chrono::duration<float> nextSecondarySpawnTime = std::chrono::duration<float>::zero();
		std::chrono::duration<float> shieldCooldown = std::chrono::duration<float>::zero();
		std::chrono::duration<float> shieldDownSoundCooldown = std::chrono::duration<float>::zero();
		std::chrono::duration<float> animationTime = std::chrono::duration<float>::zero();
		float fShieldRotation = 0.0f;
		float fShieldShrink = 1.0f;
		OmniFlags_t flags = {OmniFlags::kBlasterSpawnLeft};
		std::chrono::duration<float> transferLockTimer = std::chrono::duration<float>::zero();
		std::chrono::duration<float> arrivalGracePeriod = std::chrono::duration<float>::zero();
		std::chrono::duration<float> navigationDelay = std::chrono::duration<float>(60.0f);
		engine::GlobalId globalId {};
		engine::ClientGuid clientGuid {};
		engine::GridCoord fleetWantedCoordinate {};
		int64_t iPendingFleetWantedCoordinateTicks = 0;
		int64_t iPendingWeaponModeTicks = 0;
		// Transfers preserve supplied combat fields and animation time; navigation resets at spawn, and shield rotation and shrink use defaults.
		bool bTransfer = false;
	};

	static bool Spawn(Frame& __restrict rFrame, const SpawnInfo& rInfo);
};

} // namespace game

namespace engine
{
extern template struct Collection<game::OmnisInterpolate, CollectionFlags::kIdToIndex>;
extern template struct Collection<game::OmnisPostRender>;
} // namespace engine
