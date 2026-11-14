#pragma once

#include "Frame/Collections/Battleships/Battleships.h"
#include "Frame/Collections/Blasters/Blasters.h"
#include "Frame/Collections/Fighters/Fighters.h"
#include "Frame/Collections/Missiles/Missiles.h"
#include "Frame/Collections/Omnis/Omnis.h"
#if defined(BT_CLIENT)
#include "Frame/Collections/Wrecks/Wrecks.h"
#endif
#include "Frame/Frame.h"

namespace game
{

// The ship types follow Blasters and Missiles in ShipType order, the order ForEachShipType and the registry layers share.
// The client-only Wrecks follows Battleships, whose dying rows its Spawn captures.
inline auto GameInterpolateCollections(auto&& rSelf)
{
#if defined(BT_CLIENT)
	return std::tie(*rSelf.pBlasters, *rSelf.pMissiles, *rSelf.pFighters, *rSelf.pOmnis, *rSelf.pBattleships, *rSelf.pWrecks);
#else
	return std::tie(*rSelf.pBlasters, *rSelf.pMissiles, *rSelf.pFighters, *rSelf.pOmnis, *rSelf.pBattleships);
#endif
}

inline auto GamePostRenderCollections(auto&& rSelf)
{
#if defined(BT_CLIENT)
	return std::tie(*rSelf.pBlasters, *rSelf.pMissiles, *rSelf.pFighters, *rSelf.pOmnis, *rSelf.pBattleships, *rSelf.pWrecks);
#else
	return std::tie(*rSelf.pBlasters, *rSelf.pMissiles, *rSelf.pFighters, *rSelf.pOmnis, *rSelf.pBattleships);
#endif
}

// The collections both builds hold: the version, CRCs, difference logging, and the shared cross-build read walk these.
inline auto GameServerInterpolateCollections(auto&& rSelf)
{
	return std::tie(*rSelf.pBlasters, *rSelf.pMissiles, *rSelf.pFighters, *rSelf.pOmnis, *rSelf.pBattleships);
}

inline auto GameServerPostRenderCollections(auto&& rSelf)
{
	return std::tie(*rSelf.pBlasters, *rSelf.pMissiles, *rSelf.pFighters, *rSelf.pOmnis, *rSelf.pBattleships);
}

using GameInterpolateTypes = engine::TupleToTypeList_t<decltype(GameInterpolateCollections(std::declval<FrameInterpolate&>()))>;
using GamePostRenderTypes = engine::TupleToTypeList_t<decltype(GamePostRenderCollections(std::declval<FramePostRender&>()))>;
using GameServerInterpolateTypes = engine::TupleToTypeList_t<decltype(GameServerInterpolateCollections(std::declval<FrameInterpolate&>()))>;
using GameServerPostRenderTypes = engine::TupleToTypeList_t<decltype(GameServerPostRenderCollections(std::declval<FramePostRender&>()))>;

#if defined(BT_SERVER)
static_assert(std::is_same_v<GameInterpolateTypes, GameServerInterpolateTypes>, "Server build: GameInterpolateCollections() and GameServerInterpolateCollections() must match; Write walks the first while ServerRead and Crcs walk the second.");
static_assert(std::is_same_v<GamePostRenderTypes, GameServerPostRenderTypes>, "Server build: GamePostRenderCollections() and GameServerPostRenderCollections() must match; Write walks the first while ServerRead and Crcs walk the second.");
#endif

} // namespace game
