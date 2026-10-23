#pragma once

#include "Frame/Collections/Collection.h"
#include "Frame/Alignments.h"
#include "Frame/FlightPlanes.h"
#include "Frame/FrameRegistry.h"
#include "Frame/GridCoord.h"
#if defined(BT_CLIENT)
#include "Frame/Collections/HexShields/HexShields.h"
#include "Frame/Collections/WindTrails/WindTrails.h"
#endif

#include "Frame/Collections/Ships/ShipBehaviours.h"
#include "Frame/FlightPlaneTable.h"

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
inline constexpr std::chrono::duration<float> kBlasterFireInterval = std::chrono::duration<float>(0.1f);
inline constexpr float kfBlasterTargetRange = 120.0f;
inline constexpr float kfMissileTargetRange = 160.0f;

inline constexpr float kfOmniRadius = 1.1f;

inline constexpr float kfPushMargin = kfOmniRadius * 0.6667f;

// Movement — three independent tuning axes
inline constexpr float kfOmniAcceleration = 75.0f;
inline constexpr float kfOmniCatchUpAcceleration = 100.0f;
inline constexpr float kfOmniDrag = 1.0f;
inline constexpr float kfOmniMaximumSpeed = 33.33f;
inline constexpr float kfOmniCatchUpMaximumSpeed = 33.33f;
inline constexpr float kfOmniJitterRange = 0.60f;

inline constexpr float kfOmniClimbRate = 12.0f;
inline constexpr int64_t kiOmniHighestFlightPlane = 1;
static_assert(kiOmniHighestFlightPlane < std::ssize(kFlightPlanes));

inline constexpr AcceleratedMovementConstants kOmniMovement
{
	.fAcceleration = kfOmniAcceleration,
	.fCatchUpAcceleration = kfOmniCatchUpAcceleration,
	.fDrag = kfOmniDrag,
	.fMaximumSpeed = kfOmniMaximumSpeed,
	.fCatchUpMaximumSpeed = kfOmniCatchUpMaximumSpeed,
};

inline constexpr float kfOmniPusherRadius = kfOmniRadius * 3.3333f;
inline constexpr float kfOmniPusherIntensity = 50.0f;
inline constexpr float kfOmniPusherPower = 2.0f;
inline constexpr float kfOmniMaximumPusherPushVelocity = kfOmniMaximumSpeed * 0.5f;

#if defined(BT_CLIENT)
struct HexShieldDirections
{
	XMFLOAT4A pf4Data[shaders::kiHexShieldDirections] {};
};

struct HexShieldIntensities
{
	float pfData[shaders::kiHexShieldDirections] {};
};
#endif // BT_CLIENT

struct OmnisInterpolate : public engine::Collection<OmnisInterpolate, engine::CollectionFlags::kIdToIndex>
{
	static constexpr int64_t kiVersion = 16;

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
	// Only the climb moves height, so it stays within [Lowest, Highest] flight plane heights; reject any other the same way.
	// The stored flight plane is kuiNoFlightPlane or a plane the type flies.
	static void PostRead(const OmnisInterpolate& rCurrent)
	{
		for (int64_t i = 0; i < rCurrent.iCount; ++i)
		{
			std::chrono::duration<float> destroyedTime = rCurrent.pDestroyedTimes[i];
			if (!(destroyedTime.count() >= 0.0f && destroyedTime.count() <= kDestroyTime.count()))
			{
				throw std::ios_base::failure("OmnisInterpolate pDestroyedTimes");
			}
			float fHeight = XMVectorGetZ(rCurrent.pVecPositions[i]);
			if (!(fHeight >= engine::gpFlightPlanes->Lowest().fHeightMeters && fHeight <= engine::gpFlightPlanes->Highest().fHeightMeters))
			{
				throw std::ios_base::failure("OmnisInterpolate position height");
			}
			if (rCurrent.puiFlightPlanes[i] != kuiNoFlightPlane && !IsFlightPlaneAllowed(ShipType::kOmni, rCurrent.puiFlightPlanes[i]))
			{
				throw std::ios_base::failure("OmnisInterpolate puiFlightPlanes");
			}
		}
	}

	static void Update(FrameInterpolate& __restrict rFrameInterpolate, const Frame& __restrict rPreviousFrame);

	XMVECTOR* __restrict pVecPositions = nullptr;
	XMVECTOR* __restrict pVecDirections = nullptr;
	std::chrono::duration<float>* __restrict pDestroyedTimes = nullptr;
	std::chrono::duration<float>* __restrict pAnimationTimes = nullptr;
	engine::pusher_t* __restrict pPushers = nullptr;
	engine::registry_id_t* __restrict pRegistryIds = nullptr;
	// The plane the ship is on, kuiNoFlightPlane while it climbs between planes.
	uint8_t* __restrict puiFlightPlanes = nullptr;
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
		return std::tie(rSelf.pVecPositions, rSelf.pVecDirections, rSelf.pDestroyedTimes, rSelf.pAnimationTimes, rSelf.pPushers, rSelf.pRegistryIds, rSelf.puiFlightPlanes);
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
		return std::tie(rSelf.pPushers, rSelf.pRegistryIds, rSelf.pWindTrails, rSelf.pHexShields);
#else
		return std::tie(rSelf.pPushers, rSelf.pRegistryIds);
#endif
	}

	// CRC excludes client-only animation time and cell-local pusher handles.
	auto SharedCrcMembers(this auto&& rSelf)
	{
		return std::tie(rSelf.pVecPositions, rSelf.pVecDirections, rSelf.pDestroyedTimes, rSelf.pRegistryIds, rSelf.puiFlightPlanes);
	}
	auto OwnedHandles(this auto&& rSelf)
	{
#if defined(BT_CLIENT)
		return std::tie(rSelf.pPushers, rSelf.pWindTrails, rSelf.pHexShields);
#else
		return std::tie(rSelf.pPushers);
#endif
	}

	static void BeginRender(int64_t iCommandBuffer, const std::unordered_map<engine::GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<engine::GridCoord>& rActiveCoordinates);
	static void Render(const FrameInterpolate& __restrict rFrameInterpolate, int64_t iCommandBuffer);
	static void EndRender(int64_t iCommandBuffer);
	static void DebugRender(const FrameInterpolate& __restrict rFrameInterpolate, engine::GridCoord coordinate);

	bool LogDifferences(const OmnisInterpolate& rOther) const;
};

using omni_t = engine::Id<OmnisInterpolate>;

struct OmnisPostRender : public engine::Collection<OmnisPostRender>
{
	static constexpr int64_t kiVersion = 22;

	// Collision layer (set each frame in PreCollision)
	// thread_local: parallel per-Frame tick via Dispatch
	static inline thread_local int64_t siCollisionLayerIndex = 0;


	// Simulation keeps the blaster countdown in [0, kBlasterFireInterval], which bounds SpawnBlasters to one shot per tick.
	// Written as one range test so NaN and both infinities fail it too.
	static bool IsBlasterFireTimeInRange(float fTime)
	{
		return fTime >= 0.0f && fTime <= kBlasterFireInterval.count();
	}

	// Reject an out-of-range countdown as a corrupt stream: NaN or +inf never fires, and a large negative value never finishes the burst loop.
	// Reject an out-of-range navigation delay too, since the client echoes it back in its update-owned-entity request, an
	// unknown controller, which selects no script, and a target flight plane the type disallows, which no controller writes.
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
			if (rCurrent.pController[i] >= ShipController::kCount)
			{
				throw std::ios_base::failure("OmnisPostRender pController");
			}
			if (!IsFlightPlaneAllowed(ShipType::kOmni, GetTargetFlightPlane(rCurrent.pOrders[i])))
			{
				throw std::ios_base::failure("OmnisPostRender target flight plane");
			}
		}
	}

	// Update (orchestrator in Omnis.cpp; per-omni phase work split into helpers below)
	static void Update(Frame& __restrict rFrame, const Frame& __restrict rPreviousFrame, const engine::CellStaticData& rStaticData);

	// Collision (PreCollision: Omnis.cpp; PostCollision and AreaDamage: OmnisCombat.cpp)
	static void PreCollision(Frame& __restrict rFrame, const Frame& __restrict rPreviousFrame, const engine::CellStaticData& rStaticData);
	static void PostCollision(Frame& __restrict rFrame, const Frame& __restrict rPreviousFrame, const engine::CellStaticData& rStaticData);
	static void AreaDamage(Frame& __restrict rFrame, const Frame& __restrict rPreviousFrame, const engine::CellStaticData& rStaticData);

	// Lifecycle (Transfer: OmnisNavigation.cpp; Destroy/Spawn: Omnis.cpp)
	static void Transfer(Frame& __restrict rFrame, const engine::CellStaticData& rStaticData);
	static void Destroy(Frame& __restrict rFrame, const engine::CellStaticData& rStaticData);
	static void Spawn(Frame& __restrict rFrame, const engine::CellStaticData& rStaticData);

private:
	// Defined in OmnisCombat.cpp:
	static void XM_CALLCONV UpdateFacing(FXMVECTOR vecPosition, FXMVECTOR vecLookPosition, XMVECTOR& rVecWantedDirection);

	// Weapon and death-explosion spawn helpers (called from OmnisPostRender::Spawn). Defined in OmnisCombat.cpp.
	static void SpawnBlasters(Frame& __restrict rFrame, engine::GridCoord emitterCoordinate);
	static void SpawnMissiles(Frame& __restrict rFrame, engine::GridCoord emitterCoordinate);
	static void SpawnDeathExplosions(Frame& __restrict rFrame, engine::GridCoord emitterCoordinate);

public:

	omni_t* __restrict pIds = nullptr;
	engine::LifecycleFlags_t* __restrict pLifecycleFlags = nullptr;
	ShipController* __restrict pController = nullptr;
	ShipOrders_t* __restrict pOrders = nullptr;
	ShipFlags_t* __restrict pFlags = nullptr;
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
	engine::GlobalId* __restrict pFlagshipGlobalIds = nullptr;
	std::chrono::duration<float>* __restrict pNavigationDelays = nullptr;
	engine::GridCoord* __restrict pFleetWantedCoordinates = nullptr;
	uint8_t* __restrict puiPendingFleetWantedCoordinateTicks = nullptr;
	uint8_t* __restrict puiPendingWeaponModeTicks = nullptr;
#if defined(BT_CLIENT)
	XMVECTOR* __restrict pVecDebugNavigationWaypoints = nullptr;
#endif // BT_CLIENT

	auto SharedMembers(this auto&& rSelf)
	{
		return std::tie(rSelf.pIds, rSelf.pLifecycleFlags, rSelf.pController, rSelf.pOrders, rSelf.pFlags, rSelf.pAlignments, rSelf.pNextBlasterFireTimes, rSelf.pNextSecondarySpawnTimes, rSelf.pVecVelocities, rSelf.pVecWantedDirections, rSelf.pfArmors, rSelf.pfShields, rSelf.pShieldCooldowns, rSelf.pDestroyedExplosionTimes, rSelf.pShieldDownSoundCooldowns, rSelf.pVecAiDirections, rSelf.pTransferLockTimers, rSelf.pArrivalGracePeriods, rSelf.pCellChangeTimers, rSelf.pVecIslandDestinations, rSelf.pClientGuids, rSelf.pGlobalIds, rSelf.pFlagshipGlobalIds, rSelf.pNavigationDelays, rSelf.pFleetWantedCoordinates, rSelf.puiPendingFleetWantedCoordinateTicks, rSelf.puiPendingWeaponModeTicks);
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
		return std::tie(rSelf.pIds, rSelf.pLifecycleFlags, rSelf.pController, rSelf.pAlignments, rSelf.pClientGuids, rSelf.pGlobalIds, rSelf.pFlagshipGlobalIds);
	}

	// CRC-only subset: excludes pClientGuids and pGlobalIds which are server-side bookkeeping
	auto SharedCrcMembers(this auto&& rSelf)
	{
		return std::tie(rSelf.pIds, rSelf.pLifecycleFlags, rSelf.pController, rSelf.pOrders, rSelf.pFlags, rSelf.pAlignments, rSelf.pNextBlasterFireTimes, rSelf.pNextSecondarySpawnTimes, rSelf.pVecVelocities, rSelf.pVecWantedDirections, rSelf.pfArmors, rSelf.pfShields, rSelf.pShieldCooldowns, rSelf.pDestroyedExplosionTimes, rSelf.pShieldDownSoundCooldowns, rSelf.pVecAiDirections, rSelf.pTransferLockTimers, rSelf.pArrivalGracePeriods, rSelf.pCellChangeTimers, rSelf.pVecIslandDestinations, rSelf.pFlagshipGlobalIds, rSelf.pNavigationDelays, rSelf.pFleetWantedCoordinates, rSelf.puiPendingFleetWantedCoordinateTicks, rSelf.puiPendingWeaponModeTicks);
	}

	bool LogDifferences(const OmnisPostRender& rOther) const;

	struct SpawnInfo
	{
		XMVECTOR vecPosition = DirectX::XMVectorZero();
		XMVECTOR vecDirection = DirectX::XMVectorZero();
		XMVECTOR vecVelocity = DirectX::XMVectorZero();
		engine::AlignmentIdentifier alignment {};
		ShipController eController {};
		ShipOrders_t orders {};
		float fArmor = 0.0f;
		float fShield = 0.0f;
		std::chrono::duration<float> nextBlasterFireTime = std::chrono::duration<float>::zero();
		std::chrono::duration<float> nextSecondarySpawnTime = std::chrono::duration<float>::zero();
		std::chrono::duration<float> shieldCooldown = std::chrono::duration<float>::zero();
		std::chrono::duration<float> shieldDownSoundCooldown = std::chrono::duration<float>::zero();
		std::chrono::duration<float> animationTime = std::chrono::duration<float>::zero();
		float fShieldRotation = 0.0f;
		float fShieldShrink = 1.0f;
		ShipFlags_t flags = {ShipFlags::kBlasterSpawnLeft};
		std::chrono::duration<float> transferLockTimer = std::chrono::duration<float>::zero();
		std::chrono::duration<float> arrivalGracePeriod = std::chrono::duration<float>::zero();
		std::chrono::duration<float> navigationDelay = std::chrono::duration<float>(60.0f);
		engine::GlobalId globalId {};
		engine::ClientGuid clientGuid {};
		engine::GlobalId flagshipGlobalId {};
		engine::GridCoord fleetWantedCoordinate {};
		uint8_t uiPendingFleetWantedCoordinateTicks = 0;
		uint8_t uiPendingWeaponModeTicks = 0;
		uint8_t uiFlightPlane = kuiNoFlightPlane;
		// Transfers preserve supplied combat fields and animation time; navigation resets at spawn, and shield rotation and shrink use defaults.
		bool bTransfer = false;

		// Status-change payload members. clientGuid is server-local and the shield rotation and shrink are client visuals,
		// so none of them is listed.
		static constexpr auto Fields()
		{
			return engine::FrameColumnList<FRAME_COLUMN(SpawnInfo, vecPosition), FRAME_COLUMN(SpawnInfo, vecDirection), FRAME_COLUMN(SpawnInfo, vecVelocity), FRAME_COLUMN(SpawnInfo, alignment), FRAME_COLUMN(SpawnInfo, eController), FRAME_COLUMN(SpawnInfo, orders), FRAME_COLUMN(SpawnInfo, fArmor), FRAME_COLUMN(SpawnInfo, fShield), FRAME_COLUMN(SpawnInfo, nextBlasterFireTime), FRAME_COLUMN(SpawnInfo, nextSecondarySpawnTime), FRAME_COLUMN(SpawnInfo, shieldCooldown), FRAME_COLUMN(SpawnInfo, shieldDownSoundCooldown), FRAME_COLUMN(SpawnInfo, animationTime), FRAME_COLUMN(SpawnInfo, flags), FRAME_COLUMN(SpawnInfo, transferLockTimer), FRAME_COLUMN(SpawnInfo, arrivalGracePeriod), FRAME_COLUMN(SpawnInfo, navigationDelay), FRAME_COLUMN(SpawnInfo, globalId), FRAME_COLUMN(SpawnInfo, flagshipGlobalId), FRAME_COLUMN(SpawnInfo, fleetWantedCoordinate), FRAME_COLUMN(SpawnInfo, uiPendingFleetWantedCoordinateTicks), FRAME_COLUMN(SpawnInfo, uiPendingWeaponModeTicks), FRAME_COLUMN(SpawnInfo, uiFlightPlane), FRAME_COLUMN(SpawnInfo, bTransfer)> {};
		}
	};

	static bool Spawn(Frame& __restrict rFrame, const SpawnInfo& rInfo);
};

} // namespace game

namespace engine
{
extern template struct Collection<game::OmnisInterpolate, CollectionFlags::kIdToIndex>;
extern template struct Collection<game::OmnisPostRender>;
} // namespace engine
