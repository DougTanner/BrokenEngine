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

// A fleet Omni in its flagship's cell patrols random points within this x/y distance of the flagship, and heals within it.
inline constexpr float kfOmniPatrolRadius = 100.0f;

// The type's ki<Type>HighestFlightPlane; each type header asserts it indexes game::kFlightPlanes.
[[nodiscard]] int64_t HighestFlightPlane(ShipType eType);
// The plane a ship of the type flies while not engaging: the lowest for Omnis, the type's highest otherwise.
[[nodiscard]] int64_t DefaultFlightPlane(ShipType eType);
// The upright model's minimum origin or the authored plane height, whichever is higher.
[[nodiscard]] float EffectiveShipFlightHeight(ShipType eType, ShipController eController, int64_t iFlightPlane);
// The plane nearest the position's height, which target choice treats the position as on.
[[nodiscard]] int64_t XM_CALLCONV NearestFlightPlane(FXMVECTOR vecPosition);
// The plane whose graph a ship paths and steers on: min(highest plane at or below its height, target plane).
[[nodiscard]] int64_t NavigationFlightPlane(float fHeight, int64_t iTargetPlane);
// True on a plane without kTerrainCollision.
[[nodiscard]] bool SkipsTerrainContact(float fHeight);
// A dying Fighter or Omni at fHeight takes terrain contact: off an exact plane without terrain collision, and its
// NavigationFlightPlane has terrain collision.
[[nodiscard]] bool TakesDeathTerrainContact(float fHeight, int64_t iTargetPlane);
// Tier iClearanceTier's graph on the plane. The empty graph stands in on a plane the tier has no graph on,
// kuiNoFlightPlane included, and in a client cell whose static data has not arrived, which holds no graphs.
[[nodiscard]] const engine::NavData& NavigationPlaneData(const engine::CellStaticData& rStaticData, int64_t iClearanceTier, int64_t iNavigationPlane);
// Moves fHeight toward fTargetHeight by at most fStep, landing exactly on it within one step; rbArrived: it landed.
[[nodiscard]] float StepHeightTowards(float fHeight, float fTargetHeight, float fStep, bool& rbArrived);
// Moves Z toward the ordered plane, or the upper plane while the type's lower clearance graph blocks descent at
// vecPosition. Moves at most fClimbRate times the delta and lands on the effective ship height; W is untouched. Stores
// the plane on arrival and kuiNoFlightPlane in transit.
[[nodiscard]] XMVECTOR XM_CALLCONV ClimbTowardsFlightPlane(const engine::CellStaticData& rStaticData, ShipType eType, ShipController eController, FXMVECTOR vecPosition, int64_t iTargetFlightPlane, float fClimbRate, std::chrono::duration<float> deltaTime, uint8_t& ruiFlightPlane);

// Raises vecPosition to terrain plus the lowest plane's clearance, without changing XY/W; a raised position stores
// kuiNoFlightPlane, and the next ClimbTowardsFlightPlane arrival stores its plane.
[[nodiscard]] XMVECTOR XM_CALLCONV RaiseAboveTerrain(const engine::CellStaticData& rStaticData, FXMVECTOR vecPosition, uint8_t& ruiFlightPlane);

// vecBase's XY plus vecOffset's XY rotated by the horizontal unit vecHeadingDirection, whose (x, y) is the yaw's
// (cos, sin); Z and W are zero.
[[nodiscard]] XMVECTOR XM_CALLCONV RotateByHeading(FXMVECTOR vecHeadingDirection, FXMVECTOR vecOffset, FXMVECTOR vecBase);

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

// The next route waypoint, as NextRouteWaypoint gives it, toward the neighbouring cell center most nearly away from
// vecThreat as seen from vecPosition. Recomputed from the current positions on every call; nothing is stored.
[[nodiscard]] XMVECTOR XM_CALLCONV FleeRouteWaypoint(const engine::CellStaticData& rStaticData, int64_t iClearanceTier, FXMVECTOR vecPosition, FXMVECTOR vecThreat, NavRectangleBoard& rNavRectangleBoard, int64_t iTargetFlightPlane);

// Random-spawn steering destination: the next route waypoint toward the nearest island center when returning or with
// no enemy, else the nearest enemy. Updates the kReturnToIslandCenter hysteresis from the raw distance to that island
// center; rbHasDestination is false in an island-free cell with no enemy.
[[nodiscard]] XMVECTOR XM_CALLCONV ComputeReturnDestination(const engine::CellStaticData& rStaticData, int64_t iClearanceTier, FXMVECTOR vecPosition, bool bEnemyFound, FXMVECTOR vecNearestEnemy, NavRectangleBoard& rNavRectangleBoard, int64_t iTargetFlightPlane, ShipFlags_t& rFlags, bool& rbHasDestination);

// The horizontal AI bearing is inside the inclusive +/-45-degree forward cone.
[[nodiscard]] bool XM_CALLCONV IsWithinHeadingCone(FXMVECTOR vecDirection, FXMVECTOR vecAiDirection);

// Aims rVecAiDirection along vecOffset flattened to x/y. An offset with zero x/y length keeps the previous bearing, so
// the AI direction stays a unit vector.
void XM_CALLCONV AimAiDirection(FXMVECTOR vecOffset, XMVECTOR& rVecAiDirection);

// Steps the turn rate from fPreviousDeltaRotation toward fWantedDeltaRotation by at most the turn acceleration, within the max turn rate.
[[nodiscard]] float RampHeadingTurnRate(float fPreviousDeltaRotation, float fWantedDeltaRotation, std::chrono::duration<float> deltaTime, const HeadingMovementConstants& rConstants);
// The Fighter or Battleship terrain avoidance pass over every row, run once after the type's PostRender Update row loop:
// a row that is not dying, past arrival grace, inside the heading cone when fleet-controlled, and not facing a near enemy
// in line of sight ramps its turn rate toward ComputeTerrainAvoidance's.
template <ShipType TYPE>
void AvoidTerrain(Frame& rFrame, const Frame& rPreviousFrame, const engine::CellStaticData& rStaticData);
template <>
void AvoidTerrain<ShipType::kFighter>(Frame& rFrame, const Frame& rPreviousFrame, const engine::CellStaticData& rStaticData);
template <>
void AvoidTerrain<ShipType::kBattleship>(Frame& rFrame, const Frame& rPreviousFrame, const engine::CellStaticData& rStaticData);
[[nodiscard]] float XM_CALLCONV SteerHeading(FXMVECTOR vecDirection, FXMVECTOR vecWantedNormal, const HeadingMovementConstants& rConstants);
[[nodiscard]] XMVECTOR XM_CALLCONV CaptureHeading(FXMVECTOR vecDirection, FXMVECTOR vecWantedNormal, std::chrono::duration<float> deltaTime, const HeadingMovementConstants& rConstants);

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

// No terrain on clearance tier iClearanceTier's graph of the shooter's plane crosses the segment between the two
// positions, each first snapped clear of terrain as a route destination is; always true on a plane without terrain
// collision.
[[nodiscard]] bool XM_CALLCONV HasLineOfSight(const engine::CellStaticData& rStaticData, int64_t iClearanceTier, uint8_t uiFlightPlane, FXMVECTOR vecShooterPosition, FXMVECTOR vecTargetPosition);
// HasLineOfSight, and no model box of a live rFrame Battleship occupying uiFlightPlane crosses the unsnapped segment. A
// box holding either position does not block, which spares the shooter's and the target's own.
[[nodiscard]] bool XM_CALLCONV HasShipLineOfSight(const Frame& rFrame, const engine::CellStaticData& rStaticData, int64_t iClearanceTier, uint8_t uiFlightPlane, FXMVECTOR vecShooterPosition, FXMVECTOR vecTargetPosition);
// The unsnapped segment crosses a polygon edge of rNavData or the box of one of the rectangles, boundary included.
[[nodiscard]] bool XM_CALLCONV IsSegmentBlocked(const engine::NavData& rNavData, std::span<const engine::NavRectangle> rectangles, FXMVECTOR vecStart, FXMVECTOR vecEnd);

void RegenerateShield(std::chrono::duration<float> deltaTime, std::chrono::duration<float> shieldCooldown, float fMaximumShield, float fRegeneration, float& rfShield);

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
