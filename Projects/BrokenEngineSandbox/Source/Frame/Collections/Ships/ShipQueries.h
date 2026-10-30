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

// Calls rVisitor(ShipLocator, position, velocity) for every non-exploding row past arrival grace whose alignment is an
// enemy of alignment, in ForEachShipType order.
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

// Returns false when no enemy row qualifies; ties keep the first row in ForEachShipType order. With
// optionalFlightPlane, only rows whose nearest flight plane is that plane qualify.
[[nodiscard]] bool XM_CALLCONV FindNearestEnemyShip(const Frame& rFrame, FXMVECTOR vecFrom, engine::AlignmentIdentifier alignment, std::optional<int64_t> optionalFlightPlane, XMVECTOR& rVecResult);

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

// One registry query window: one context per flight plane, indexed by plane, plus the single workbuffer allocation
// their spans point into. The owner keeps the whole object alive for the window; the allocation pops when it dies, so
// no context span outlives it.
struct RegistryWindow
{
	common::ScopedWorkbufferAllocation<std::byte*> buffer;
	std::array<engine::RegistryQueryContext, kFlightPlanes.size()> contexts {};
};

// Builds one context per flight plane. Each binds the three ship types as source layers in ForEachShipType order and
// the missile handles as the subscription layer. A row is eligible while it publishes a registry id and its arrival
// grace has run out, and it joins only the context of the flight plane nearest its current Interpolate position, so a
// ship climbing between planes joins its nearest plane's. With pPreviousFrame, its arrival grace decides eligibility and
// its positions are bound as previous positions; without, the current grace decides and no previous positions are bound.
[[nodiscard]] RegistryWindow BuildShipRegistryWindow(const Frame& rFrame, const Frame* pPreviousFrame, const MissilesPostRender& rSubscribers);

} // namespace game
