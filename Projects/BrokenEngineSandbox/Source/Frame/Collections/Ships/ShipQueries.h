#pragma once

#include "Frame/Collections/Battleships/Battleships.h"
#include "Frame/Collections/Fighters/Fighters.h"
#include "Frame/Collections/Omnis/Omnis.h"

namespace game
{

template <typename FRAME_PART, typename COLLECTION>
using ShipCollectionReference = std::conditional_t<std::is_const_v<FRAME_PART>, const COLLECTION&, COLLECTION&>;

// Visits (ShipType, Interpolate, PostRender) in the fixed order Fighters, Omnis, Battleships.
template <typename INTERPOLATE_FRAME, typename POST_RENDER_FRAME, typename VISITOR>
void ForEachShipType(INTERPOLATE_FRAME& rInterpolate, POST_RENDER_FRAME& rPostRender, VISITOR&& rVisitor)
{
	rVisitor(ShipType::kFighter, static_cast<ShipCollectionReference<INTERPOLATE_FRAME, FightersInterpolate>>(*rInterpolate.pFighters), static_cast<ShipCollectionReference<POST_RENDER_FRAME, FightersPostRender>>(*rPostRender.pFighters));
	rVisitor(ShipType::kOmni, static_cast<ShipCollectionReference<INTERPOLATE_FRAME, OmnisInterpolate>>(*rInterpolate.pOmnis), static_cast<ShipCollectionReference<POST_RENDER_FRAME, OmnisPostRender>>(*rPostRender.pOmnis));
	rVisitor(ShipType::kBattleship, static_cast<ShipCollectionReference<INTERPOLATE_FRAME, BattleshipsInterpolate>>(*rInterpolate.pBattleships), static_cast<ShipCollectionReference<POST_RENDER_FRAME, BattleshipsPostRender>>(*rPostRender.pBattleships));
}

template <typename FRAME, typename VISITOR>
void ForEachShipType(FRAME& rFrame, VISITOR&& rVisitor)
{
	ForEachShipType(rFrame.interpolate, rFrame.postRender, rVisitor);
}

// Calls rVisitor(Interpolate, PostRender, iRow) for the located row.
template <typename FRAME, typename VISITOR>
decltype(auto) VisitShip(FRAME& rFrame, ShipLocator locator, VISITOR&& rVisitor)
{
	switch (locator.eType)
	{
		case ShipType::kFighter:
			return rVisitor(static_cast<ShipCollectionReference<FRAME, FightersInterpolate>>(*rFrame.interpolate.pFighters), static_cast<ShipCollectionReference<FRAME, FightersPostRender>>(*rFrame.postRender.pFighters), locator.iRow);
		case ShipType::kOmni:
			return rVisitor(static_cast<ShipCollectionReference<FRAME, OmnisInterpolate>>(*rFrame.interpolate.pOmnis), static_cast<ShipCollectionReference<FRAME, OmnisPostRender>>(*rFrame.postRender.pOmnis), locator.iRow);
		case ShipType::kBattleship:
			return rVisitor(static_cast<ShipCollectionReference<FRAME, BattleshipsInterpolate>>(*rFrame.interpolate.pBattleships), static_cast<ShipCollectionReference<FRAME, BattleshipsPostRender>>(*rFrame.postRender.pBattleships), locator.iRow);
		case ShipType::kCount:
			break;
	}
	ASSERT(false);
	std::unreachable();
}

// Calls rVisitor(ShipLocator, position, velocity) for every row past arrival grace that is not kDying, which
// MarkDestroy also sets, and whose alignment is an enemy of alignment, in ForEachShipType order.
template <typename VISITOR>
void ForEachEnemyShip(const FrameInterpolate& rInterpolate, const FramePostRender& rPostRender, engine::AlignmentIdentifier alignment, VISITOR&& rVisitor)
{
	ForEachShipType(rInterpolate, rPostRender, [&](ShipType eType, const auto& rShipInterpolate, const auto& rShipPostRender)
	{
		for (int64_t i = 0; i < rShipPostRender.iCount; ++i)
		{
			if (rShipPostRender.pLifecycleFlags[i] & engine::LifecycleFlags::kDying)
			{
				continue;
			}
			if (rShipPostRender.pArrivalGracePeriods[i] > std::chrono::duration<float>::zero())
			{
				continue;
			}
			if (!rPostRender.alignments.CanCollide(alignment, rShipPostRender.pAlignments[i]))
			{
				continue;
			}
			rVisitor(ShipLocator {.eType = eType, .iRow = i}, rShipInterpolate.pVecPositions[i], rShipPostRender.pVecVelocities[i]);
		}
	});
}

// Global ID 0 names no ship: every random-spawn row carries it.
[[nodiscard]] std::optional<ShipLocator> FindShipByGlobalId(const Frame& rFrame, engine::GlobalId globalId);

// Returns false when no enemy row qualifies; ties keep the first row in ForEachShipType order.
[[nodiscard]] bool XM_CALLCONV FindNearestEnemyShip(const Frame& rFrame, FXMVECTOR vecFrom, engine::AlignmentIdentifier alignment, XMVECTOR& rVecResult);

// The kept attack target rRegistryTarget in line of sight from vecPosition on uiFlightPlane, else the enemy nearest
// vecPickFrom in line of sight, else the kept target hidden, else the nearest enemy hidden. Nearest is x/y when bHorizontal,
// else 3D, ties keeping the first row in ForEachShipType order; enemies above iHighestFlightPlane never qualify. Returns
// false when the previous frame holds no such enemy, clearing rRegistryTarget when its row is above iHighestFlightPlane
// and leaving every other output unchanged.
[[nodiscard]] bool XM_CALLCONV FindAttackTarget(const Frame& rPreviousFrame, const engine::CellStaticData& rStaticData, FXMVECTOR vecPosition, uint8_t uiFlightPlane, FXMVECTOR vecPickFrom, bool bHorizontal, engine::AlignmentIdentifier alignment, int64_t iHighestFlightPlane, engine::registry_id_t& rRegistryTarget, XMVECTOR& rVecResult, bool& rbInLineOfSight);

// Firing and look targets from previous-frame data, ignoring enemies above iHighestFlightPlane: sets kFireBlaster for an
// enemy within fBlasterTargetRange on the shooter's stored plane uiFlightPlane with clear line of sight, so a climbing
// shooter holds fire. The look position leads the nearest such target, preferring the kept attack target registryTarget
// when it qualifies, else is the nearest enemy in line of sight at any range.
void XM_CALLCONV AcquireTarget(const Frame& rPreviousFrame, const engine::CellStaticData& rStaticData, FXMVECTOR vecPosition, engine::AlignmentIdentifier alignment, engine::registry_id_t registryTarget, uint8_t uiFlightPlane, int64_t iHighestFlightPlane, float fBlasterSpeed, float fBlasterTargetRange, ShipFlags_t& rFlags, bool& rbLookTargetFound, XMVECTOR& rVecLookPosition);

// The enemy kIsFlagship Battleship row nearest vecFrom, with its registry id; returns false, leaving both outputs
// unchanged, when none qualifies; ties keep the first row.
[[nodiscard]] bool XM_CALLCONV FindNearestEnemyFlagship(const Frame& rFrame, FXMVECTOR vecFrom, engine::AlignmentIdentifier alignment, engine::registry_id_t& rRegistryId, XMVECTOR& rVecResult);

// A Battleship row whose orders carry kIsFlagship.
[[nodiscard]] bool IsFlagship(const FramePostRender& rPostRender, ShipLocator locator);

struct ShipUuid
{
	ShipType eType {};
	// 0 when no row carries the global ID.
	int64_t iUuid = 0;
};
[[nodiscard]] ShipUuid ShipUuidByGlobalId(const Frame& rFrame, engine::GlobalId globalId);

[[nodiscard]] XMVECTOR ShipPosition(const FrameInterpolate& rInterpolate, ShipLocator locator);
// Armor for Omnis, health otherwise.
[[nodiscard]] float ShipHull(const Frame& rFrame, ShipLocator locator);

// One registry query window: one context plus the single workbuffer allocation its spans point into. The owner keeps
// the whole object alive for the window; the allocation pops when it dies, so no context span outlives it.
struct RegistryWindow
{
	common::ScopedWorkbufferAllocation<std::byte*> buffer;
	engine::RegistryQueryContext context {};
};

// Builds the context for the lowest flight plane. It binds the three ship types as source layers in ForEachShipType
// order and no subscription layer, so the context derives no subscriber counts. A row is eligible while it publishes a
// registry id, its arrival grace has run out, and the flight plane nearest its current Interpolate position is the
// lowest, so a ship climbing away from that plane leaves the context. The previous frame's arrival grace decides
// eligibility and its positions are bound as previous positions.
[[nodiscard]] RegistryWindow BuildShipRegistryWindow(const Frame& rFrame, const Frame& rPreviousFrame);

} // namespace game
