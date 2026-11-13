#pragma once

#include "Frame/Collections/Pushers/Pushers.h"
#include "Frame/Collections/Collection.h"
#include "Frame/Alignments.h"
#include "Frame/FlightPlanes.h"
#include "Frame/FrameRegistry.h"
#include "Frame/GridCoord.h"
#if defined(BT_CLIENT)
#include "Frame/Collections/AreaLights/AreaLights.h"
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

inline constexpr std::chrono::duration<float> kFighterDestroyTime = std::chrono::duration<float>(0.25f);
inline constexpr std::chrono::duration<float> kFighterDestroyExplosionInterval = std::chrono::duration<float>(0.024f);

inline constexpr float kfFighterRadius = 1.5f;

inline constexpr float kfFighterAcceleration = 12.0f;
inline constexpr float kfFighterDrag = 0.25f;
inline constexpr float kfFighterMaxSpeed = 45.0f;

inline constexpr float kfFighterClimbRate = kfFighterMaxSpeed * kfShipClimbSpeedFraction;
inline constexpr int64_t kiFighterHighestFlightPlane = 1;
static_assert(kiFighterHighestFlightPlane == std::ssize(kFlightPlanes) - 1);
// Render-only nose pitch in radians at the full climb rate.
inline constexpr float kfFighterClimbTilt = 0.35f;

inline constexpr HeadingMovementConstants kFighterMovement
{
	.fChaseAcceleration = kfFighterAcceleration,
	.fFleeAcceleration = 12.0f,
	.fReturnAcceleration = kfFighterAcceleration,
	.fDrag = kfFighterDrag,
	.fMaximumSpeed = kfFighterMaxSpeed,
	.fVelocityToDirection = 4.0f,
	.fMaxTurnRate = 3.0f,
	.fTurnAcceleration = 12.0f,
	.fCaptureAngle = XM_PI / 60.0f,
	.fCaptureRate = 4.0f,
};

inline constexpr float kfFighterPusherIntensity = 36.0f;
inline constexpr float kfFighterPusherPower = 3.0f;
inline constexpr float kfFighterMaxPusherPushVelocity = kfFighterMaxSpeed * 0.5f;

inline constexpr std::chrono::duration<float> kFighterBeamCooldown = std::chrono::duration<float>(1.0f);
inline constexpr float kfFighterBeamRange = 50.0f;

struct FightersInterpolate : public engine::Collection<FightersInterpolate, engine::CollectionFlags::kIdToIndex>
{
	static constexpr int64_t kiVersion = 8;

	static void Register();

	static void AllocateAndCopy(FightersInterpolate& rCurrent, const FightersInterpolate& rPrevious);

	// Simulation keeps the destroyed time at the -1 sentinel or in [0, kFighterDestroyTime]; reject anything else as a corrupt stream, since a
	// huge or +inf value puts the death-explosion particle count cast out of range. Written as one range test so NaN and both infinities fail it too.
	// Terrain rise can exceed the highest plane; admit finite heights at or above the lowest plane.
	// The stored flight plane is kuiNoFlightPlane or a plane the type flies, and every simulation write and ship spawn stores a horizontal unit heading.
	static void PostRead(const FightersInterpolate& rCurrent)
	{
		for (int64_t i = 0; i < rCurrent.iCount; ++i)
		{
			std::chrono::duration<float> destroyedTime = rCurrent.pDestroyedTimes[i];
			if (!(destroyedTime == std::chrono::duration<float>(-1.0f) || (destroyedTime.count() >= 0.0f && destroyedTime.count() <= kFighterDestroyTime.count())))
			{
				throw std::ios_base::failure("FightersInterpolate pDestroyedTimes");
			}
			float fHeight = XMVectorGetZ(rCurrent.pVecPositions[i]);
			if (!(std::isfinite(fHeight) && fHeight >= engine::gpFlightPlanes->Lowest().fHeightMeters))
			{
				throw std::ios_base::failure("FightersInterpolate position height");
			}
			if (rCurrent.puiFlightPlanes[i] != kuiNoFlightPlane && !IsFlightPlaneAllowed(ShipType::kFighter, rCurrent.puiFlightPlanes[i]))
			{
				throw std::ios_base::failure("FightersInterpolate puiFlightPlanes");
			}
			if (!IsShipHeadingValid(rCurrent.pVecDirections[i]))
			{
				throw std::ios_base::failure("FightersInterpolate pVecDirections");
			}
		}
	}

	static void Update(FrameInterpolate& __restrict rCurrentFrameInterpolate, const Frame& __restrict rPreviousFrame, const engine::CellStaticData& rStaticData);

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
	// The plane the ship is on, kuiNoFlightPlane while it climbs between planes.
	uint8_t* __restrict puiFlightPlanes = nullptr;
#if defined(BT_CLIENT)
	std::chrono::duration<float>* __restrict pAnimationTimes = nullptr;
	// Render-only nose pitch from this tick's climb.
	float* __restrict pfClimbTilts = nullptr;
	// The beam line, held from the fire tick until PostRender Update releases it once the fade ends.
	engine::area_lights_t* __restrict pBeamAreaLights = nullptr;
	// The target position at fire time, where the beam line ends.
	XMVECTOR* __restrict pVecBeamEnds = nullptr;
#endif
	auto SharedMembers(this auto&& rSelf)
	{
		return std::tie(rSelf.pVecPositions, rSelf.pVecDirections, rSelf.pDestroyedTimes, rSelf.pPushers, rSelf.pRegistryIds, rSelf.pfDeltaRotations, rSelf.puiFlightPlanes);
	}
#if defined(BT_CLIENT)
	auto ClientMembers(this auto&& rSelf)
	{
		return std::tie(rSelf.pAnimationTimes, rSelf.pWindTrails, rSelf.pfClimbTilts, rSelf.pBeamAreaLights, rSelf.pVecBeamEnds);
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
		return std::tie(rSelf.pPushers, rSelf.pRegistryIds, rSelf.pWindTrails, rSelf.pBeamAreaLights, rSelf.pVecBeamEnds);
#else
		return std::tie(rSelf.pPushers, rSelf.pRegistryIds);
#endif
	}
	auto OwnedHandles(this auto&& rSelf)
	{
#if defined(BT_CLIENT)
		return std::tie(rSelf.pPushers, rSelf.pWindTrails, rSelf.pBeamAreaLights);
#else
		return std::tie(rSelf.pPushers);
#endif
	}

	bool LogDifferences(const FightersInterpolate& rOther) const;

	static void GraphicsResources();

	static void BeginRender(int64_t iCommandBuffer, const std::unordered_map<engine::GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<engine::GridCoord>& rActiveCoordinates);
	// Requests the textures of each controller's model pipeline that has a row in an active cell.
	static void RequestTextures(const std::unordered_map<engine::GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<engine::GridCoord>& rActiveCoordinates);
	static void Render(const FrameInterpolate& __restrict rFrameInterpolate, int64_t iCommandBuffer);
	static void EndRender(int64_t iCommandBuffer);
	static void DebugRender(const FrameInterpolate& __restrict rFrameInterpolate, engine::GridCoord coordinate);
};

using fighter_t = engine::Id<FightersInterpolate>;

struct FightersPostRender : public engine::Collection<FightersPostRender>
{
	static constexpr int64_t kiVersion = 11;

	// Simulation keeps the beam countdown finite and at most kFighterBeamCooldown; the floor is only finite because a
	// fighter with no target counts down without a clamp. Written as one range test so NaN and both infinities fail it too.
	static bool IsBeamFireTimeInRange(float fTime)
	{
		return fTime >= -std::numeric_limits<float>::max() && fTime <= kFighterBeamCooldown.count();
	}

	// Reject a value the simulation never produces as a corrupt stream: NaN or +inf health never dies, a NaN or +inf
	// beam countdown never fires and one above the cooldown delays the shot past it, an unknown controller selects no
	// script, no navigation-delay admission accepts an out-of-range delay, every simulation write and ship spawn stores
	// a horizontal unit AI direction, no controller writes a target flight plane the type disallows, and only a fleet
	// spawn writes a client GUID or a global ID.
	static void PostRead(const FightersPostRender& rCurrent)
	{
		for (int64_t i = 0; i < rCurrent.iCount; ++i)
		{
			if (!IsBeamFireTimeInRange(rCurrent.pNextBeamFireTimes[i].count()))
			{
				throw std::ios_base::failure("FightersPostRender pNextBeamFireTimes");
			}
			if (!std::isfinite(rCurrent.pfHealths[i]))
			{
				throw std::ios_base::failure("FightersPostRender pfHealths");
			}
			if (rCurrent.pController[i] >= ShipController::kCount)
			{
				throw std::ios_base::failure("FightersPostRender pController");
			}
			if (rCurrent.pClientGuids[i] != engine::ClientGuid {} && rCurrent.pController[i] != ShipController::kFleet)
			{
				throw std::ios_base::failure("FightersPostRender pClientGuids");
			}
			if (rCurrent.pGlobalIds[i] != engine::GlobalId {} && rCurrent.pController[i] != ShipController::kFleet)
			{
				throw std::ios_base::failure("FightersPostRender pGlobalIds");
			}
			if (!IsNavigationDelayInRange(rCurrent.pNavigationDelays[i].count()))
			{
				throw std::ios_base::failure("FightersPostRender pNavigationDelays");
			}
			if (!IsShipHeadingValid(rCurrent.pVecAiDirections[i]))
			{
				throw std::ios_base::failure("FightersPostRender pVecAiDirections");
			}
			if (!IsFlightPlaneAllowed(ShipType::kFighter, GetTargetFlightPlane(rCurrent.pOrders[i])))
			{
				throw std::ios_base::failure("FightersPostRender target flight plane");
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

	static void XM_CALLCONV BeginExplosion(Frame& rFrame, engine::GridCoord emitterCoordinate, int64_t i, FXMVECTOR vecDamageDirection);

	// A beam hit of fDamage from vecShooterPosition, applied during PostCollision.
	static void XM_CALLCONV ApplyBeamDamage(Frame& rFrame, engine::GridCoord emitterCoordinate, int64_t i, FXMVECTOR vecShooterPosition, float fDamage);

	fighter_t* __restrict pIds = nullptr;
	engine::LifecycleFlags_t* __restrict pLifecycleFlags = nullptr;
	ShipController* __restrict pController = nullptr;
	ShipOrders_t* __restrict pOrders = nullptr;
	ShipFlags_t* __restrict pFlags = nullptr;
	XMVECTOR* __restrict pVecVelocities = nullptr;
	XMVECTOR* __restrict pVecDamageDirections = nullptr;
	float* __restrict pfHealths = nullptr;
	std::chrono::duration<float>* __restrict pDestroyedExplosionTimes = nullptr;
	std::chrono::duration<float>* __restrict pNextBeamFireTimes = nullptr;
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
	// The enemy position a Veer-off flies away from, read only while ShipFlags::kVeerOff is set.
	XMVECTOR* __restrict pVecVeerOffPoints = nullptr;
	// The kept attack target's registry id, zero for none.
	engine::registry_id_t* __restrict pRegistryTargets = nullptr;
#if defined(BT_CLIENT)
	XMVECTOR* __restrict pVecDebugNavigationWaypoints = nullptr;
#endif // BT_CLIENT
	// Server Members() must match SharedMembers() for wire/CRC parity; client-only fields stay outside SharedMembers().
	auto SharedMembers(this auto&& rSelf)
	{
		return std::tie(rSelf.pIds, rSelf.pLifecycleFlags, rSelf.pController, rSelf.pOrders, rSelf.pFlags, rSelf.pVecVelocities, rSelf.pVecDamageDirections, rSelf.pfHealths, rSelf.pDestroyedExplosionTimes, rSelf.pNextBeamFireTimes, rSelf.pAlignments, rSelf.pArrivalGracePeriods, rSelf.pGlobalIds, rSelf.pClientGuids, rSelf.pFlagshipGlobalIds, rSelf.pFleetWantedCoordinates, rSelf.puiPendingFleetWantedCoordinateTicks, rSelf.pNavigationDelays, rSelf.pCellChangeTimers, rSelf.pVecAiDirections, rSelf.pVecIslandDestinations, rSelf.pVecVeerOffPoints, rSelf.pRegistryTargets);
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
		return std::tie(rSelf.pIds, rSelf.pLifecycleFlags, rSelf.pController, rSelf.pOrders, rSelf.pFlags, rSelf.pVecVelocities, rSelf.pVecDamageDirections, rSelf.pfHealths, rSelf.pDestroyedExplosionTimes, rSelf.pNextBeamFireTimes, rSelf.pAlignments, rSelf.pArrivalGracePeriods, rSelf.pFlagshipGlobalIds, rSelf.pFleetWantedCoordinates, rSelf.puiPendingFleetWantedCoordinateTicks, rSelf.pNavigationDelays, rSelf.pCellChangeTimers, rSelf.pVecAiDirections, rSelf.pVecIslandDestinations, rSelf.pVecVeerOffPoints, rSelf.pRegistryTargets);
	}

	bool LogDifferences(const FightersPostRender& rOther) const;
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
		std::chrono::duration<float> nextBeamFireTime = std::chrono::duration<float>::zero();
		std::chrono::duration<float> arrivalGracePeriod = std::chrono::duration<float>::zero();
		float fDeltaRotation = 0.0f;
		std::chrono::duration<float> navigationDelay = std::chrono::duration<float>(60.0f);
		engine::GlobalId globalId {};
		engine::ClientGuid clientGuid {};
		engine::GlobalId flagshipGlobalId {};
		engine::GridCoord fleetWantedCoordinate {};
		uint8_t uiPendingFleetWantedCoordinateTicks = 0;
		uint8_t uiFlightPlane = kuiNoFlightPlane;
		// Transfers preserve the supplied health; navigation resets at spawn.
		bool bTransfer = false;

		// Status-change payload members. clientGuid is server-local, so it is not listed.
		static constexpr engine::FrameColumnList<FRAME_COLUMN(SpawnInfo, vecPosition), FRAME_COLUMN(SpawnInfo, vecDirection), FRAME_COLUMN(SpawnInfo, vecVelocity), FRAME_COLUMN(SpawnInfo, alignment), FRAME_COLUMN(SpawnInfo, eController), FRAME_COLUMN(SpawnInfo, orders), FRAME_COLUMN(SpawnInfo, fHealth), FRAME_COLUMN(SpawnInfo, nextBeamFireTime), FRAME_COLUMN(SpawnInfo, arrivalGracePeriod), FRAME_COLUMN(SpawnInfo, fDeltaRotation), FRAME_COLUMN(SpawnInfo, navigationDelay), FRAME_COLUMN(SpawnInfo, globalId), FRAME_COLUMN(SpawnInfo, flagshipGlobalId), FRAME_COLUMN(SpawnInfo, fleetWantedCoordinate), FRAME_COLUMN(SpawnInfo, uiPendingFleetWantedCoordinateTicks), FRAME_COLUMN(SpawnInfo, uiFlightPlane), FRAME_COLUMN(SpawnInfo, bTransfer)> Fields()
		{
			return {};
		}
	};

	static bool Spawn(Frame& __restrict rFrame, const SpawnInfo& rInfo);
};

} // namespace game

namespace engine
{

extern template struct Collection<game::FightersInterpolate, CollectionFlags::kIdToIndex>;
extern template struct Collection<game::FightersPostRender>;

} // namespace engine
