#pragma once

#include "Frame/Collections/Battleships/Battleships.h"
#include "Frame/Collections/Blasters/Blasters.h"
#include "Frame/Collections/Fighters/Fighters.h"
#include "Frame/Collections/Missiles/Missiles.h"
#include "Frame/Collections/Omnis/Omnis.h"
#include "Frame/Frame.h"

namespace game
{

// The ship types follow Blasters and Missiles in ShipType order, the order ForEachShipType and the registry layers share.
inline auto GameInterpolateCollections(auto&& rSelf)
{
	return std::tie(*rSelf.pBlasters, *rSelf.pMissiles, *rSelf.pFighters, *rSelf.pOmnis, *rSelf.pBattleships);
}

inline auto GamePostRenderCollections(auto&& rSelf)
{
	return std::tie(*rSelf.pBlasters, *rSelf.pMissiles, *rSelf.pFighters, *rSelf.pOmnis, *rSelf.pBattleships);
}

using GameInterpolateTypes = engine::TupleToTypeList_t<decltype(GameInterpolateCollections(std::declval<FrameInterpolate&>()))>;
using GamePostRenderTypes = engine::TupleToTypeList_t<decltype(GamePostRenderCollections(std::declval<FramePostRender&>()))>;

} // namespace game
