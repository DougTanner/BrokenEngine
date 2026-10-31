#pragma once

#include "Frame/Collections/CollectionId.h"
#include "Frame/Alignments.h"
#include "Frame/FrameRegistry.h"

#include "Frame/Collections/Ships/ShipTypes.h"

namespace game
{

struct Frame;

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
	// False: no destination; a fleet ship then runs the shared fleet navigation instead.
	bool bHasDestination = false;
};

// For fleet Fighters and Omnis (eType). Escort flies to the ship's own slot around its flagship, Attack and Veer-off take
// over while the fleet is weapons free, and a ship outside its flagship's cell navigates on its type's highest plane.
// Attack targets the kept rRegistryTarget while it names an enemy row, else keeps and targets the enemy nearest the
// flagship in x/y; escort leaves it untouched and only the outside-the-cell branch clears it. No tactic targets a plane
// above the type's highest one. Reads only the previous frame.
[[nodiscard]] ShipTactic XM_CALLCONV ChooseFleetTactic(ShipType eType, const Frame& rPreviousFrame, const FleetView& rView, engine::GlobalId globalId, FXMVECTOR vecPosition, engine::AlignmentIdentifier alignment, ShipFlags_t& rFlags, XMVECTOR& rVecVeerOffPoint, engine::registry_id_t& rRegistryTarget);

// Veer-off until 150 m from the veer-off point, else Attack the horizontally nearest enemy at or below
// iHighestFlightPlane on its plane, veering off within 20 m. Returns false, leaving rTactic unchanged, when not veering
// off and the previous frame holds no such enemy.
[[nodiscard]] bool XM_CALLCONV ChooseAttackTactic(const Frame& rPreviousFrame, FXMVECTOR vecPosition, engine::AlignmentIdentifier alignment, int64_t iHighestFlightPlane, ShipFlags_t& rFlags, XMVECTOR& rVecVeerOffPoint, ShipTactic& rTactic);

} // namespace game
