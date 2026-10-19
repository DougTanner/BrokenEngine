#pragma once

#include "Frame/Collections/Pushers/Pushers.h"
#include "Frame/Collections/CollectionId.h"
#include "Frame/Alignments.h"
#include "Frame/GridCoord.h"

#include "Frame/Collections/Ships/ShipTypes.h"

namespace engine
{
struct CellStaticData;
} // namespace engine

namespace game
{

struct Frame;
struct FrameInterpolate;

// Tuning for ApplyAcceleratedMovement, instantiated as kOmniMovement in Omnis.h.
struct AcceleratedMovementConstants
{
	float fAcceleration = 0.0f;
	float fCatchUpAcceleration = 0.0f;
	float fDrag = 0.0f;
	float fMaximumSpeed = 0.0f;
	float fCatchUpMaximumSpeed = 0.0f;
};

// Tuning for ApplyHeadingMovement, instantiated in Fighters.h and Battleships.h.
struct HeadingMovementConstants
{
	float fChaseAcceleration = 0.0f;
	float fFleeAcceleration = 0.0f;
	float fReturnAcceleration = 0.0f;
	float fDrag = 0.0f;
	float fMaximumSpeed = 0.0f;
	// Velocity-to-direction blend (airplane constraint)
	float fVelocityToDirection = 0.0f;
};

void CountdownPendingOrders(int64_t& riPendingFleetWantedCoordinateTicks);
// Also counts down the weapon-mode change and applies kPendingUseMissiles when it finishes.
void CountdownPendingOrders(ShipOrders_t& rOrders, int64_t& riPendingFleetWantedCoordinateTicks, int64_t& riPendingWeaponModeTicks);

// Fleet navigation: roaming, cross-cell movement, island destinations, and following the row's own flagship.
void XM_CALLCONV ComputeNavigation(Frame& rFrame, const Frame& rPreviousFrame, const engine::CellStaticData& rStaticData, ShipLocator self, FXMVECTOR vecPosition, FXMVECTOR vecCellCenter, engine::GridCoord fleetWantedCoordinate, int64_t iPendingFleetWantedCoordinateTicks, ShipOrders_t orders, engine::GlobalId flagshipGlobalId, std::chrono::duration<float> deltaTime, int64_t& riNavigationDirection, int64_t& riNavigationWaypointIndex, XMVECTOR& rVecAiDirection, XMVECTOR& rVecIslandDestination, std::chrono::duration<float>& rCellChangeTimer);

// Random-spawn steering destination: return to the nearest island, else the nearest enemy. Updates the kFlee and
// kReturnToIslandCenter hysteresis; rbHasDestination is false in an island-free cell with no enemy.
[[nodiscard]] XMVECTOR XM_CALLCONV ComputeFleeOrReturnDestination(std::span<const XMFLOAT4> islandCandidates, FXMVECTOR vecPosition, bool bEnemyFound, FXMVECTOR vecNearestEnemy, ShipFlags_t& rFlags, bool& rbHasDestination);

// A negative fTurnRate turns away from the destination; without a destination the turn only decays.
void XM_CALLCONV SteerHeadingTowards(FXMVECTOR vecPosition, FXMVECTOR vecDirection, FXMVECTOR vecDestination, bool bHasDestination, float fTurnRate, float fSmoothing, float fDecay, std::chrono::duration<float> deltaTime, float& rfDeltaRotation);

void XM_CALLCONV ApplyAcceleratedMovement(int64_t iNavigationDirection, FXMVECTOR vecAiDirection, std::chrono::duration<float> deltaTime, float fAccelerationMultiplier, float fDecayMultiplier, const AcceleratedMovementConstants& rConstants, XMVECTOR& rVecVelocity);
void XM_CALLCONV ApplyHeadingMovement(FXMVECTOR vecDirection, ShipFlags_t flags, std::chrono::duration<float> deltaTime, const HeadingMovementConstants& rConstants, XMVECTOR& rVecVelocity);

// Soft push away from terrain inside the band below the lowest flight plane that the radius and margin define.
void XM_CALLCONV ApplyTerrainPush(const engine::CellStaticData& rStaticData, FXMVECTOR vecPosition, float fRadius, float fPushMargin, XMVECTOR& rVecVelocity);
// Hard contact at the lowest flight plane: rVecPosition slides along the terrain once.
void XM_CALLCONV ResolveTerrainContact(const engine::CellStaticData& rStaticData, ShipLocator self, FXMVECTOR vecPreviousPosition, XMVECTOR& rVecPosition, float fRadius, XMVECTOR& rVecVelocity);
// Hard contact at the lowest flight plane: rVecPosition stops at the terrain and the heading bounces off it.
void XM_CALLCONV ApplyTerrainBounce(const engine::CellStaticData& rStaticData, ShipLocator self, FXMVECTOR vecPreviousPosition, XMVECTOR& rVecPosition, FXMVECTOR vecDirection, float fRadius, std::chrono::duration<float> deltaTime, float& rfDeltaRotation, XMVECTOR& rVecVelocity);
// Pass the row's own pusher so it ignores its own push.
void XM_CALLCONV ApplyPusherPush(const Frame& rFrame, FXMVECTOR vecPosition, engine::pusher_t pusher, float fMaximumPushVelocity, XMVECTOR& rVecVelocity);

// Firing and look targets from previous-frame data: sets kFireMissile for an enemy within fMissileTargetRange and
// kFireBlaster when it is also within fBlasterTargetRange; the look position leads an in-range target.
void XM_CALLCONV AcquireTarget(const Frame& rPreviousFrame, FXMVECTOR vecPosition, engine::AlignmentIdentifier alignment, float fBlasterSpeed, float fBlasterTargetRange, float fMissileTargetRange, ShipFlags_t& rFlags, bool& rbLookTargetFound, XMVECTOR& rVecLookPosition);

void RegenerateShield(std::chrono::duration<float> deltaTime, std::chrono::duration<float> shieldCooldown, float fMaximumShield, float fRegeneration, float& rfShield);
void XM_CALLCONV RegenerateHealth(FXMVECTOR vecPosition, bool bEnemyFound, FXMVECTOR vecNearestEnemy, std::chrono::duration<float> deltaTime, float fMaximumHealth, float& rfHealth);

// Navigation delays in seconds match the HUD slider's [0, 60] range. Written as one range test so NaN and both
// infinities fail it too.
[[nodiscard]] constexpr bool IsNavigationDelayInRange(float fDelay)
{
	return fDelay >= 0.0f && fDelay <= 60.0f;
}

#if defined(BT_CLIENT)
// One fleet row's navigation waypoint and destination. Positions come from the interpolated frame; rRenderFrame is the
// snapshot it was interpolated from, which supplies the flagship lookup.
void XM_CALLCONV DebugRenderNavigation(const FrameInterpolate& rFrameInterpolate, const Frame& rRenderFrame, FXMVECTOR vecPosition, FXMVECTOR vecDebugWaypoint, FXMVECTOR vecIslandDestination, ShipFlags_t flags, engine::GlobalId flagshipGlobalId, float fRadius);
#endif // BT_CLIENT

} // namespace game
