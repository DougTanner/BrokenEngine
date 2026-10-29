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

// A ship's stored flight plane while it climbs between planes.
inline constexpr uint8_t kuiNoFlightPlane = 255;

// The type's ki<Type>HighestFlightPlane; each type header asserts it indexes game::kFlightPlanes.
[[nodiscard]] int64_t HighestFlightPlane(ShipType eType);
[[nodiscard]] bool IsFlightPlaneAllowed(ShipType eType, int64_t iFlightPlane);
// The plane whose height equals fHeight exactly, or none between planes. The climb lands on the table's own height.
[[nodiscard]] std::optional<int64_t> FlightPlaneAtHeight(float fHeight);
// The plane whose graph a ship paths and steers on: min(highest plane at or below its height, target plane).
[[nodiscard]] int64_t NavigationFlightPlane(float fHeight, int64_t iTargetPlane);
// True on a plane without kTerrainCollision.
[[nodiscard]] bool SkipsTerrainContact(float fHeight);
// Moves Z toward the target plane's height by at most fClimbRate times the delta, landing exactly on it; W is untouched.
// A stored plane equal to the target plane stays; any other becomes the target plane on arrival and kuiNoFlightPlane
// while the climb is under way.
[[nodiscard]] XMVECTOR XM_CALLCONV ClimbTowardsFlightPlane(FXMVECTOR vecPosition, int64_t iTargetFlightPlane, float fClimbRate, std::chrono::duration<float> deltaTime, uint8_t& ruiFlightPlane);

void CountdownPendingOrders(int64_t& riPendingFleetWantedCoordinateTicks);
// Also counts down the weapon-mode change and applies kPendingUseMissiles when it finishes.
void CountdownPendingOrders(ShipOrders_t& rOrders, int64_t& riPendingFleetWantedCoordinateTicks, int64_t& riPendingWeaponModeTicks);

// Fleet navigation: roaming, cross-cell movement, island destinations, and following the row's own flagship.
void XM_CALLCONV ComputeNavigation(Frame& rFrame, const Frame& rPreviousFrame, const engine::CellStaticData& rStaticData, ShipLocator self, FXMVECTOR vecPosition, FXMVECTOR vecCellCenter, engine::GridCoord fleetWantedCoordinate, int64_t iPendingFleetWantedCoordinateTicks, ShipOrders_t orders, engine::GlobalId flagshipGlobalId, std::chrono::duration<float> deltaTime, int64_t& riNavigationDirection, int64_t& riNavigationWaypointIndex, XMVECTOR& rVecAiDirection, XMVECTOR& rVecIslandDestination, std::chrono::duration<float>& rCellChangeTimer);

// Random-spawn steering destination: return to the nearest island, else the nearest enemy. Updates the kFlee and
// kReturnToIslandCenter hysteresis; rbHasDestination is false in an island-free cell with no enemy.
[[nodiscard]] XMVECTOR XM_CALLCONV ComputeFleeOrReturnDestination(std::span<const XMFLOAT4> islandCandidates, FXMVECTOR vecPosition, bool bEnemyFound, FXMVECTOR vecNearestEnemy, ShipFlags_t& rFlags, bool& rbHasDestination);

// The horizontal AI bearing is inside the inclusive +/-45-degree forward cone; zero AI is outside.
[[nodiscard]] bool XM_CALLCONV IsWithinHeadingCone(FXMVECTOR vecDirection, FXMVECTOR vecAiDirection);

// A negative fTurnRate turns away from the destination; without a destination the turn only decays.
void XM_CALLCONV SteerHeadingTowards(FXMVECTOR vecPosition, FXMVECTOR vecDirection, FXMVECTOR vecDestination, bool bHasDestination, float fTurnRate, float fSmoothing, float fDecay, std::chrono::duration<float> deltaTime, float& rfDeltaRotation);

void XM_CALLCONV ApplyAcceleratedMovement(int64_t iNavigationDirection, FXMVECTOR vecAiDirection, std::chrono::duration<float> deltaTime, float fAccelerationMultiplier, float fDecayMultiplier, const AcceleratedMovementConstants& rConstants, XMVECTOR& rVecVelocity);
void XM_CALLCONV ApplyHeadingMovement(FXMVECTOR vecDirection, ShipFlags_t flags, bool bAllowAcceleration, std::chrono::duration<float> deltaTime, const HeadingMovementConstants& rConstants, XMVECTOR& rVecVelocity);

// Soft push away from terrain inside the band below fHeight that the radius and margin define; vecPosition supplies
// only the XY of the terrain samples.
void XM_CALLCONV ApplyTerrainPush(const engine::CellStaticData& rStaticData, FXMVECTOR vecPosition, float fRadius, float fHeight, float fPushMargin, XMVECTOR& rVecVelocity);
// Hard contact at the height of rVecPosition: rVecPosition slides along the terrain once. An overlapped start snaps on
// the NavigationFlightPlane graph.
void XM_CALLCONV ResolveTerrainContact(const engine::CellStaticData& rStaticData, ShipLocator self, FXMVECTOR vecPreviousPosition, XMVECTOR& rVecPosition, float fRadius, XMVECTOR& rVecVelocity, int64_t iTargetFlightPlane);
// Hard contact at the height of rVecPosition: rVecPosition stops at the terrain and the heading bounces off it. An
// overlapped start snaps on the NavigationFlightPlane graph.
void XM_CALLCONV ApplyTerrainBounce(const engine::CellStaticData& rStaticData, ShipLocator self, FXMVECTOR vecPreviousPosition, XMVECTOR& rVecPosition, FXMVECTOR vecDirection, float fRadius, std::chrono::duration<float> deltaTime, float& rfDeltaRotation, XMVECTOR& rVecVelocity, int64_t iTargetFlightPlane);
// Pass the row's own pusher; adds to the caller-initialized rfPositionCorrectionX, applied after terrain resolution.
void XM_CALLCONV ApplyPusherPush(const Frame& rFrame, engine::pusher_t pusher, float fMaximumPushVelocity, XMVECTOR& rVecVelocity, float& rfPositionCorrectionX);

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
