#pragma once

#include "Frame/Collections/CollectionId.h"
#include "Frame/Alignments.h"
#include "Frame/FrameRegistry.h"

#include "Frame/Collections/Ships/ShipTypes.h"

namespace engine
{

struct CellStaticData;

} // namespace engine

namespace game
{

struct Frame;
class NavRectangleBoard;

// Recomputed every tick from the previous frame, never stored.
enum class FleetStance : uint8_t
{
	kEscort,
	kWeaponsFree,
};

// One flagship as its fleet's ships in the same cell see it this tick.
struct FleetView
{
	XMVECTOR vecFlagshipPosition = DirectX::XMVectorZero();
	engine::GlobalId flagshipGlobalId {};
	FleetStance eStance = FleetStance::kEscort;
};

// One view per flagship Battleship row of the cell's previous frame, held in the thread's workbuffer for one Update.
class FleetBoard
{
public:

	explicit FleetBoard(const Frame& rPreviousFrame);

	// A flagship not in the cell yields a default view with a zero flagshipGlobalId.
	[[nodiscard]] FleetView Find(engine::GlobalId flagshipGlobalId) const;

	common::ScopedWorkbufferAllocation<FleetView*> mViews;
	int64_t miViewCount = 0;
};

struct ShipTactic
{
	XMVECTOR vecDestination = DirectX::XMVectorZero();
	int64_t iTargetFlightPlane = 0;
	// 0: the ship type's own maximum speed; only the Fighter escort tactic sets one.
	float fMaximumSpeed = 0.0f;
	// False: no destination; a fleet ship then runs the shared fleet navigation instead.
	bool bHasDestination = false;
	// True only for an Attack tactic, whose destination is the attacked enemy's position.
	bool bAttacking = false;
};

// For fleet Fighters and Omnis (eType). Escort flies a Fighter to its own slot around its flagship and gives an Omni no
// destination, so ComputeNavigation patrols it; weapons free, Attack takes over; outside its flagship's cell a ship
// navigates on its type's default plane (DefaultFlightPlane), as escort does. Attack targets rRegistryTarget
// (FindAttackTarget, picked from the flagship in x/y, sight from stored plane uiFlightPlane), routing to its next waypoint
// on the type's clearance tier's graph while hidden from the pick or on that tier. Escort leaves rRegistryTarget
// untouched; only the outside-the-cell branch and FindAttackTarget's drop of a target above the highest plane clear it.
// No tactic targets above the type's highest plane.
[[nodiscard]] ShipTactic XM_CALLCONV ChooseFleetTactic(ShipType eType, const Frame& rPreviousFrame, const engine::CellStaticData& rStaticData, const FleetView& rView, engine::GlobalId globalId, FXMVECTOR vecPosition, NavRectangleBoard& rNavRectangleBoard, uint8_t uiFlightPlane, engine::AlignmentIdentifier alignment, engine::registry_id_t& rRegistryTarget);

// ChooseFleetTactic for a fleet Fighter, plus veer-off: with its flagship in the cell and weapons free, a veer-off in
// progress continues before any target pick; an Attack result starts a veer-off within kfVeerOffStartDistanceSquared of
// its enemy in x/y; every other result clears ShipFlags::kVeerOff.
[[nodiscard]] ShipTactic XM_CALLCONV ChooseFighterFleetTactic(const Frame& rPreviousFrame, const engine::CellStaticData& rStaticData, const FleetView& rView, engine::GlobalId globalId, FXMVECTOR vecPosition, NavRectangleBoard& rNavRectangleBoard, uint8_t uiFlightPlane, engine::AlignmentIdentifier alignment, ShipFlags_t& rFlags, XMVECTOR& rVecVeerOffPoint, engine::registry_id_t& rRegistryTarget);

// Veer-off until 150 m from the veer-off point, else Attack its kept attack target rRegistryTarget (FindAttackTarget,
// picked from itself in x/y, at or below iHighestFlightPlane) on its plane, veering off within 20 m, or route to its next
// waypoint on clearance tier iClearanceTier's graph while it is hidden on that tier or from the pick; with bTargetFlagship
// the nearest enemy flagship, when the previous frame holds one, becomes the kept attack target instead. Returns false,
// leaving rTactic unchanged, when not veering off and the previous frame holds no such enemy.
[[nodiscard]] bool XM_CALLCONV ChooseAttackTactic(const Frame& rPreviousFrame, const engine::CellStaticData& rStaticData, int64_t iClearanceTier, FXMVECTOR vecPosition, NavRectangleBoard& rNavRectangleBoard, uint8_t uiFlightPlane, engine::AlignmentIdentifier alignment, int64_t iHighestFlightPlane, bool bTargetFlagship, ShipFlags_t& rFlags, XMVECTOR& rVecVeerOffPoint, engine::registry_id_t& rRegistryTarget, ShipTactic& rTactic);

} // namespace game
