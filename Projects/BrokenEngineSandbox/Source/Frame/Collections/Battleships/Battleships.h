#pragma once

#include "Frame/Collections/Pushers/Pushers.h"
#include "Frame/Collections/Collection.h"
#include "Frame/Alignments.h"
#include "Frame/FrameRegistry.h"
#include "Frame/GridCoord.h"
#if defined(BT_CLIENT)
#include "Frame/Collections/WindTrails/WindTrails.h"
#endif

#include "Frame/Collections/Ships/ShipBehaviours.h"

namespace engine
{
struct CellStaticData;
} // namespace engine

namespace game
{

inline constexpr std::chrono::duration<float> kBattleshipDestroyTime = std::chrono::duration<float>(0.25f);
inline constexpr std::chrono::duration<float> kBattleshipDestroyExplosionInterval = std::chrono::duration<float>(0.024f);

inline constexpr float kfBattleshipRadius = 4.5f;

inline constexpr float kfBattleshipAcceleration = 4.0f;
inline constexpr float kfBattleshipDrag = 0.25f;
inline constexpr float kfBattleshipMaxSpeed = 15.0f;
inline constexpr float kfBattleshipMaxTurnRate = 1.0f;

inline constexpr float kfBattleshipSteeringSmoothing = 0.064f;
inline constexpr float kfBattleshipSteeringDecay = 6.0f;
inline constexpr float kfBattleshipChaseTurnRate = 8.0f;
inline constexpr float kfBattleshipFleeTurnRate = 8.0f;

inline constexpr HeadingMovementConstants kBattleshipMovement
{
	.fChaseAcceleration = 4.0f,
	.fFleeAcceleration = 6.0f,
	.fReturnAcceleration = kfBattleshipAcceleration,
	.fDrag = kfBattleshipDrag,
	.fMaximumSpeed = kfBattleshipMaxSpeed,
	.fVelocityToDirection = 4.0f,
};

inline constexpr float kfBattleshipPusherRadius = kfBattleshipRadius * 1.5f;
inline constexpr float kfBattleshipPusherIntensity = 36.0f;
inline constexpr float kfBattleshipPusherPower = 3.0f;
inline constexpr float kfBattleshipMaxPusherPushVelocity = kfBattleshipMaxSpeed * 0.5f;

// Each of the front and rear guns fires only at an enemy within this angle of its axis, so each arc spans 90 degrees.
inline constexpr float kfBattleshipBlasterArcHalfAngle = XM_PIDIV4;

struct BattleshipsInterpolate : public engine::Collection<BattleshipsInterpolate, engine::CollectionFlags::kIdToIndex>
{
	static constexpr int64_t kiVersion = 1;

	static void Register();

	static void AllocateAndCopy(BattleshipsInterpolate& rCurrent, const BattleshipsInterpolate& rPrevious);

	// Simulation keeps the destroyed time at the -1 sentinel or in [0, kBattleshipDestroyTime]; reject anything else as a corrupt stream, since a
	// huge or +inf value puts the death-explosion particle count cast out of range. Written as one range test so NaN and both infinities fail it too.
	static void PostRead(const BattleshipsInterpolate& rCurrent)
	{
		for (int64_t i = 0; i < rCurrent.iCount; ++i)
		{
			std::chrono::duration<float> destroyedTime = rCurrent.pDestroyedTimes[i];
			if (!(destroyedTime == std::chrono::duration<float>(-1.0f) || (destroyedTime.count() >= 0.0f && destroyedTime.count() <= kBattleshipDestroyTime.count())))
			{
				throw std::ios_base::failure("BattleshipsInterpolate pDestroyedTimes");
			}
		}
	}

	static void Update(FrameInterpolate& __restrict rCurrentFrameInterpolate, const Frame& __restrict rPreviousFrame);

#if defined(BT_CLIENT)
	static void ClientInitialize(Frame& rFrame, int64_t iIndex);
	static void ClientInitializeAll(Frame& rFrame);
#endif

	XMVECTOR* __restrict pVecPositions = nullptr;
	XMVECTOR* __restrict pVecDirections = nullptr;
	std::chrono::duration<float>* __restrict pDestroyedTimes = nullptr;
	engine::pusher_t* __restrict pPushers = nullptr;
	engine::registry_id_t* __restrict pRegistryIds = nullptr;
#if defined(BT_CLIENT)
	engine::wind_trail_t* __restrict pWindTrails = nullptr;
#endif
	float* __restrict pfDeltaRotations = nullptr;
#if defined(BT_CLIENT)
	std::chrono::duration<float>* __restrict pAnimationTimes = nullptr;
#endif
	auto SharedMembers(this auto&& rSelf)
	{
		return std::tie(rSelf.pVecPositions, rSelf.pVecDirections, rSelf.pDestroyedTimes, rSelf.pPushers, rSelf.pRegistryIds, rSelf.pfDeltaRotations);
	}
#if defined(BT_CLIENT)
	auto ClientMembers(this auto&& rSelf)
	{
		return std::tie(rSelf.pAnimationTimes, rSelf.pWindTrails);
	}
#endif
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
		return std::tie(rSelf.pPushers, rSelf.pRegistryIds, rSelf.pWindTrails);
#else
		return std::tie(rSelf.pPushers, rSelf.pRegistryIds);
#endif
	}
	auto OwnedHandles(this auto&& rSelf)
	{
#if defined(BT_CLIENT)
		return std::tie(rSelf.pPushers, rSelf.pWindTrails);
#else
		return std::tie(rSelf.pPushers);
#endif
	}

	bool LogDifferences(const BattleshipsInterpolate& rOther) const;

	static void GraphicsResources();

	static void BeginRender(int64_t iCommandBuffer, const std::unordered_map<engine::GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<engine::GridCoord>& rActiveCoordinates);
	static void Render(const FrameInterpolate& __restrict rFrameInterpolate, int64_t iCommandBuffer);
	static void EndRender(int64_t iCommandBuffer);
	static void DebugRender(const FrameInterpolate& __restrict rFrameInterpolate, engine::GridCoord coordinate);
};

using battleship_t = engine::Id<BattleshipsInterpolate>;

struct BattleshipsPostRender : public engine::Collection<BattleshipsPostRender>
{
	static constexpr int64_t kiVersion = 2;

	// Reject a value the simulation never produces as a corrupt stream: NaN or +inf health never dies, an unknown
	// controller selects no script, and the client echoes the navigation delay back in its update-owned-entity request.
	static void PostRead(const BattleshipsPostRender& rCurrent)
	{
		for (int64_t i = 0; i < rCurrent.iCount; ++i)
		{
			if (!std::isfinite(rCurrent.pfHealths[i]))
			{
				throw std::ios_base::failure("BattleshipsPostRender pfHealths");
			}
			if (rCurrent.pController[i] >= ShipController::kCount)
			{
				throw std::ios_base::failure("BattleshipsPostRender pController");
			}
			if (!IsNavigationDelayInRange(rCurrent.pNavigationDelays[i].count()))
			{
				throw std::ios_base::failure("BattleshipsPostRender pNavigationDelays");
			}
		}
	}

	static void Update(Frame& __restrict rFrame, const Frame& __restrict rPreviousFrame, const engine::CellStaticData& rStaticData);
	static void PreCollision(Frame& __restrict rFrame, const Frame& __restrict rPreviousFrame, const engine::CellStaticData& rStaticData);
	static void PostCollision(Frame& __restrict rFrame, const Frame& __restrict rPreviousFrame, const engine::CellStaticData& rStaticData);
	static void AreaDamage(Frame& __restrict rFrame, const Frame& __restrict rPreviousFrame, const engine::CellStaticData& rStaticData);
	static void Transfer(Frame& __restrict rFrame, const engine::CellStaticData& rStaticData);
	static void Destroy(Frame& __restrict rFrame, const engine::CellStaticData& rStaticData);
	static void Spawn(Frame& __restrict rFrame, const engine::CellStaticData& rStaticData);

	battleship_t* __restrict pIds = nullptr;
	engine::LifecycleFlags_t* __restrict pLifecycleFlags = nullptr;
	ShipController* __restrict pController = nullptr;
	ShipOrders_t* __restrict pOrders = nullptr;
	ShipFlags_t* __restrict pFlags = nullptr;
	XMVECTOR* __restrict pVecVelocities = nullptr;
	XMVECTOR* __restrict pVecDamageDirections = nullptr;
	float* __restrict pfHealths = nullptr;
	std::chrono::duration<float>* __restrict pDestroyedExplosionTimes = nullptr;
	std::chrono::duration<float>* __restrict pNextBlasterSpawnTimes = nullptr;
	std::chrono::duration<float>* __restrict pNextRearBlasterSpawnTimes = nullptr;
	engine::AlignmentIdentifier* __restrict pAlignments = nullptr;
	std::chrono::duration<float>* __restrict pArrivalGracePeriods = nullptr;
	engine::GlobalId* __restrict pGlobalIds = nullptr;
	engine::ClientGuid* __restrict pClientGuids = nullptr;
	engine::GlobalId* __restrict pFlagshipGlobalIds = nullptr;
	engine::GridCoord* __restrict pFleetWantedCoordinates = nullptr;
	uint8_t* __restrict puiPendingFleetWantedCoordinateTicks = nullptr;
	std::chrono::duration<float>* __restrict pNavigationDelays = nullptr;
	std::chrono::duration<float>* __restrict pCellChangeTimers = nullptr;
	XMVECTOR* __restrict pVecAiDirections = nullptr;
	XMVECTOR* __restrict pVecIslandDestinations = nullptr;
#if defined(BT_CLIENT)
	XMVECTOR* __restrict pVecDebugNavigationWaypoints = nullptr;
#endif // BT_CLIENT
	// Server Members() must match SharedMembers() for wire/CRC parity; client-only fields stay outside SharedMembers().
	auto SharedMembers(this auto&& rSelf)
	{
		return std::tie(rSelf.pIds, rSelf.pLifecycleFlags, rSelf.pController, rSelf.pOrders, rSelf.pFlags, rSelf.pVecVelocities, rSelf.pVecDamageDirections, rSelf.pfHealths, rSelf.pDestroyedExplosionTimes, rSelf.pNextBlasterSpawnTimes, rSelf.pNextRearBlasterSpawnTimes, rSelf.pAlignments, rSelf.pArrivalGracePeriods, rSelf.pGlobalIds, rSelf.pClientGuids, rSelf.pFlagshipGlobalIds, rSelf.pFleetWantedCoordinates, rSelf.puiPendingFleetWantedCoordinateTicks, rSelf.pNavigationDelays, rSelf.pCellChangeTimers, rSelf.pVecAiDirections, rSelf.pVecIslandDestinations);
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
		return std::tie(rSelf.pIds, rSelf.pLifecycleFlags, rSelf.pController, rSelf.pOrders, rSelf.pVecDamageDirections, rSelf.pAlignments, rSelf.pGlobalIds, rSelf.pClientGuids, rSelf.pFlagshipGlobalIds);
	}

	// CRC-only subset: excludes pGlobalIds and pClientGuids, which are server-side bookkeeping
	auto SharedCrcMembers(this auto&& rSelf)
	{
		return std::tie(rSelf.pIds, rSelf.pLifecycleFlags, rSelf.pController, rSelf.pOrders, rSelf.pFlags, rSelf.pVecVelocities, rSelf.pVecDamageDirections, rSelf.pfHealths, rSelf.pDestroyedExplosionTimes, rSelf.pNextBlasterSpawnTimes, rSelf.pNextRearBlasterSpawnTimes, rSelf.pAlignments, rSelf.pArrivalGracePeriods, rSelf.pFlagshipGlobalIds, rSelf.pFleetWantedCoordinates, rSelf.puiPendingFleetWantedCoordinateTicks, rSelf.pNavigationDelays, rSelf.pCellChangeTimers, rSelf.pVecAiDirections, rSelf.pVecIslandDestinations);
	}

	bool LogDifferences(const BattleshipsPostRender& rOther) const;
	static void AvoidTerrain(Frame& __restrict rFrame, const Frame& __restrict rPreviousFrame, const engine::CellStaticData& rStaticData, int64_t iStart, int64_t iEnd);

	struct SpawnInfo
	{
		XMVECTOR vecPosition = DirectX::XMVectorZero();
		XMVECTOR vecDirection = DirectX::XMVectorZero();
		XMVECTOR vecVelocity = DirectX::XMVectorZero();
		engine::AlignmentIdentifier alignment {};
		ShipController eController {};
		ShipOrders_t orders {};
		float fHealth = 0.0f;
		std::chrono::duration<float> nextBlasterSpawnTime = std::chrono::duration<float>::zero();
		std::chrono::duration<float> nextRearBlasterSpawnTime = std::chrono::duration<float>::zero();
		std::chrono::duration<float> arrivalGracePeriod = std::chrono::duration<float>::zero();
		float fDeltaRotation = 0.0f;
		std::chrono::duration<float> navigationDelay = std::chrono::duration<float>(60.0f);
		engine::GlobalId globalId {};
		engine::ClientGuid clientGuid {};
		engine::GlobalId flagshipGlobalId {};
		engine::GridCoord fleetWantedCoordinate {};
		uint8_t uiPendingFleetWantedCoordinateTicks = 0;
		// Transfers preserve the supplied health; navigation resets at spawn.
		bool bTransfer = false;

		// Status-change payload members. clientGuid is server-local, so it is not listed.
		static constexpr auto Fields()
		{
			return engine::FrameColumnList<FRAME_COLUMN(SpawnInfo, vecPosition), FRAME_COLUMN(SpawnInfo, vecDirection), FRAME_COLUMN(SpawnInfo, vecVelocity), FRAME_COLUMN(SpawnInfo, alignment), FRAME_COLUMN(SpawnInfo, eController), FRAME_COLUMN(SpawnInfo, orders), FRAME_COLUMN(SpawnInfo, fHealth), FRAME_COLUMN(SpawnInfo, nextBlasterSpawnTime), FRAME_COLUMN(SpawnInfo, nextRearBlasterSpawnTime), FRAME_COLUMN(SpawnInfo, arrivalGracePeriod), FRAME_COLUMN(SpawnInfo, fDeltaRotation), FRAME_COLUMN(SpawnInfo, navigationDelay), FRAME_COLUMN(SpawnInfo, globalId), FRAME_COLUMN(SpawnInfo, flagshipGlobalId), FRAME_COLUMN(SpawnInfo, fleetWantedCoordinate), FRAME_COLUMN(SpawnInfo, uiPendingFleetWantedCoordinateTicks), FRAME_COLUMN(SpawnInfo, bTransfer)> {};
		}
	};

	static bool Spawn(Frame& __restrict rFrame, const SpawnInfo& rInfo);
};

} // namespace game

namespace engine
{
extern template struct Collection<game::BattleshipsInterpolate, CollectionFlags::kIdToIndex>;
extern template struct Collection<game::BattleshipsPostRender>;
} // namespace engine
