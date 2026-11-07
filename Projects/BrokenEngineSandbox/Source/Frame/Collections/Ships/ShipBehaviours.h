#pragma once

#include "Frame/Collections/Pushers/Pushers.h"
#include "Frame/Collections/CollectionId.h"
#include "Frame/Alignments.h"
#include "Frame/GridCoord.h"

#include "Frame/Collections/Ships/ShipTypes.h"

namespace engine
{

struct CellStaticData;
struct NavData;
struct NavRectangle;

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

// Firing and target picking test line of sight on this clearance tier's graphs for every shooter; movement decisions use
// the ship type's own tier (ShipTypeClearanceTier, TerrainUtils.h).
inline constexpr int64_t kiFiringClearanceTier = 0;

// A fleet Omni in its flagship's cell patrols random points within this x/y distance of the flagship, and heals within it.
inline constexpr float kfOmniPatrolRadius = 100.0f;

// The type's ki<Type>HighestFlightPlane; each type header asserts it indexes game::kFlightPlanes.
[[nodiscard]] int64_t HighestFlightPlane(ShipType eType);
// The plane a ship of the type flies while not engaging: the lowest for Omnis, the type's highest otherwise.
[[nodiscard]] int64_t DefaultFlightPlane(ShipType eType);
[[nodiscard]] bool IsFlightPlaneAllowed(ShipType eType, int64_t iFlightPlane);
// The upright model's minimum origin or the authored plane height, whichever is higher.
[[nodiscard]] float EffectiveShipFlightHeight(ShipType eType, ShipController eController, int64_t iFlightPlane);
// The plane whose authored height equals fHeight exactly, or none.
[[nodiscard]] std::optional<int64_t> FlightPlaneAtHeight(float fHeight);
// The plane nearest the position's height, which target choice treats the position as on.
[[nodiscard]] int64_t XM_CALLCONV NearestFlightPlane(FXMVECTOR vecPosition);
// The plane whose graph a ship paths and steers on: min(highest plane at or below its height, target plane).
[[nodiscard]] int64_t NavigationFlightPlane(float fHeight, int64_t iTargetPlane);
// True on a plane without kTerrainCollision.
[[nodiscard]] bool SkipsTerrainContact(float fHeight);
// Tier iClearanceTier's graph on the plane. The empty graph stands in on a plane the tier has no graph on,
// kuiNoFlightPlane included, and in a client cell whose static data has not arrived, which holds no graphs.
[[nodiscard]] const engine::NavData& NavigationPlaneData(const engine::CellStaticData& rStaticData, int64_t iClearanceTier, int64_t iNavigationPlane);
// Moves Z toward the ordered plane, or the upper plane while the lower clearance graph blocks descent at vecPosition.
// Moves at most fClimbRate times the delta and lands on the effective ship height; W is untouched. Stores the plane on
// arrival and kuiNoFlightPlane in transit.
[[nodiscard]] XMVECTOR XM_CALLCONV ClimbTowardsFlightPlane(const engine::CellStaticData& rStaticData, ShipType eType, ShipController eController, int64_t iClearanceTier, FXMVECTOR vecPosition, int64_t iTargetFlightPlane, float fClimbRate, std::chrono::duration<float> deltaTime, uint8_t& ruiFlightPlane);

// Raises a type that allows the next plane to terrain plus the lowest plane's clearance, without changing XY/W.
// A raised position stores its exact effective plane, or kuiNoFlightPlane between or above planes.
[[nodiscard]] XMVECTOR XM_CALLCONV RaiseAboveTerrain(const engine::CellStaticData& rStaticData, ShipType eType, ShipController eController, FXMVECTOR vecPosition, uint8_t& ruiFlightPlane);

void CountdownPendingOrders(int64_t& riPendingFleetWantedCoordinateTicks);

// One rectangle per row of the cell's previous frame with a pusher and a navigation-obstacle model, in ForEachShipType
// order, sized for one asking ship type and held in the thread's workbuffer for one Update.
class NavRectangleBoard
{
public:

	struct BoardRectangle;

	NavRectangleBoard(const Frame& rPreviousFrame, ShipType eSelfType);

	// In board order, the rectangles sharing a flight plane with a meSelfType ship at vecPosition, within their reach of it,
	// and holding neither point, which also leaves out the asker's own. The span is valid until the next Select.
	[[nodiscard]] std::span<const engine::NavRectangle> XM_CALLCONV Select(FXMVECTOR vecPosition, FXMVECTOR vecDestination);

	ShipType meSelfType = ShipType::kFighter;
	int64_t miRectangleCount = 0;
	common::ScopedWorkbufferAllocation<BoardRectangle*> mRectangles;
	common::ScopedWorkbufferAllocation<engine::NavRectangle*> mPicked;
};

// The next route waypoint toward vecDestination on clearance tier iClearanceTier's NavigationFlightPlane graph, around
// the rectangles rNavRectangleBoard selects; rbFinal: it is the destination snapped clear of terrain.
[[nodiscard]] XMVECTOR XM_CALLCONV NextRouteWaypoint(const engine::CellStaticData& rStaticData, int64_t iClearanceTier, FXMVECTOR vecPosition, FXMVECTOR vecDestination, NavRectangleBoard& rNavRectangleBoard, int64_t iTargetFlightPlane, bool& rbFinal);

// Fleet navigation: roaming, cross-cell movement, island destinations, following the row's own flagship, and a fleet
// Omni's patrol around its flagship while that flagship is in the cell.
void XM_CALLCONV ComputeNavigation(Frame& rFrame, const Frame& rPreviousFrame, const engine::CellStaticData& rStaticData, ShipLocator self, FXMVECTOR vecPosition, FXMVECTOR vecCellCenter, NavRectangleBoard& rNavRectangleBoard, engine::GridCoord fleetWantedCoordinate, int64_t iPendingFleetWantedCoordinateTicks, ShipOrders_t orders, engine::GlobalId flagshipGlobalId, std::chrono::duration<float> deltaTime, int64_t& riNavigationDirection, int64_t& riNavigationWaypointIndex, XMVECTOR& rVecAiDirection, XMVECTOR& rVecIslandDestination, std::chrono::duration<float>& rCellChangeTimer);

// Random-spawn steering destination: return to the nearest island, else the nearest enemy. Updates the kFlee and
// kReturnToIslandCenter hysteresis; rbHasDestination is false in an island-free cell with no enemy.
[[nodiscard]] XMVECTOR XM_CALLCONV ComputeFleeOrReturnDestination(std::span<const XMFLOAT4> islandCandidates, FXMVECTOR vecPosition, bool bEnemyFound, FXMVECTOR vecNearestEnemy, ShipFlags_t& rFlags, bool& rbHasDestination);

// The horizontal AI bearing is inside the inclusive +/-45-degree forward cone.
[[nodiscard]] bool XM_CALLCONV IsWithinHeadingCone(FXMVECTOR vecDirection, FXMVECTOR vecAiDirection);

// Aims rVecAiDirection along vecOffset flattened to x/y. An offset with zero x/y length keeps the previous bearing, so
// the AI direction stays a unit vector.
void XM_CALLCONV AimAiDirection(FXMVECTOR vecOffset, XMVECTOR& rVecAiDirection);

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

// Firing and look targets from previous-frame data, ignoring enemies above iHighestFlightPlane: sets kFireBlaster for an
// enemy within fBlasterTargetRange on the shooter's stored plane uiFlightPlane with clear line of sight, so a climbing
// shooter holds fire. The look position leads the nearest such target, preferring the kept attack target registryTarget
// when it qualifies, else is the nearest enemy in line of sight at any range.
void XM_CALLCONV AcquireTarget(const Frame& rPreviousFrame, const engine::CellStaticData& rStaticData, FXMVECTOR vecPosition, engine::AlignmentIdentifier alignment, engine::registry_id_t registryTarget, uint8_t uiFlightPlane, int64_t iHighestFlightPlane, float fBlasterSpeed, float fBlasterTargetRange, ShipFlags_t& rFlags, bool& rbLookTargetFound, XMVECTOR& rVecLookPosition);
// No terrain on clearance tier iClearanceTier's graph of the shooter's plane crosses the segment between the two
// positions, each first snapped clear of terrain as a route destination is; always true on a plane without terrain
// collision.
[[nodiscard]] bool XM_CALLCONV HasLineOfSight(const engine::CellStaticData& rStaticData, int64_t iClearanceTier, uint8_t uiFlightPlane, FXMVECTOR vecShooterPosition, FXMVECTOR vecTargetPosition);
// HasLineOfSight, and no model box of a live rFrame Battleship occupying uiFlightPlane crosses the unsnapped segment. A
// box holding either position does not block, which spares the shooter's and the target's own.
[[nodiscard]] bool XM_CALLCONV HasShipLineOfSight(const Frame& rFrame, const engine::CellStaticData& rStaticData, int64_t iClearanceTier, uint8_t uiFlightPlane, FXMVECTOR vecShooterPosition, FXMVECTOR vecTargetPosition);

void RegenerateShield(std::chrono::duration<float> deltaTime, std::chrono::duration<float> shieldCooldown, float fMaximumShield, float fRegeneration, float& rfShield);

// Navigation delays in seconds match the HUD slider's [0, 60] range. Written as one range test so NaN and both
// infinities fail it too.
[[nodiscard]] constexpr bool IsNavigationDelayInRange(float fDelay)
{
	return fDelay >= 0.0f && fDelay <= 60.0f;
}

// A ship heading or AI direction has Z and W exactly 0 and an x/y length squared in [0.25, 4]: a coarse corruption test
// far wider than the rounding of the unit vectors every simulation write and ship spawn stores, and the only bound on
// an edit_frame write. The length is one range test so NaN and both infinities in any lane fail it.
[[nodiscard]] inline bool XM_CALLCONV IsShipHeadingValid(FXMVECTOR vecDirection)
{
	float fLengthSquared = XMVectorGetX(XMVector2LengthSq(vecDirection));
	return XMVectorGetZ(vecDirection) == 0.0f && XMVectorGetW(vecDirection) == 0.0f && fLengthSquared >= 0.25f && fLengthSquared <= 4.0f;
}

// Covers the float rounding of a unit vector's components and of its length squared.
inline constexpr float kfShipHeadingUnitTolerance = 4.0f * kfEpsilon;

// A horizontal unit heading, as a harness ship spawn must supply: Z and W exactly 0 and an x/y length squared within
// kfShipHeadingUnitTolerance of 1. NaN and both infinities in any lane fail it.
[[nodiscard]] inline bool XM_CALLCONV IsShipHeadingUnit(FXMVECTOR vecDirection)
{
	float fLengthSquared = XMVectorGetX(XMVector2LengthSq(vecDirection));
	return XMVectorGetZ(vecDirection) == 0.0f && XMVectorGetW(vecDirection) == 0.0f && std::abs(fLengthSquared - 1.0f) <= kfShipHeadingUnitTolerance;
}

#if defined(BT_CLIENT)
// One fleet row's navigation waypoint and destination. Positions come from the interpolated frame; rRenderFrame is the
// snapshot it was interpolated from, which supplies the flagship lookup.
void XM_CALLCONV DebugRenderNavigation(const FrameInterpolate& rFrameInterpolate, const Frame& rRenderFrame, FXMVECTOR vecPosition, FXMVECTOR vecDebugWaypoint, FXMVECTOR vecIslandDestination, ShipFlags_t flags, engine::GlobalId flagshipGlobalId, float fRadius);
#endif // BT_CLIENT

} // namespace game
