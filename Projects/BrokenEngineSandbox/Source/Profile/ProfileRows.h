#pragma once

namespace game
{

enum GameCpuCounters : int64_t
{
	// The engine pins its contiguous engine->game counter index contract to this anchor by name; keep it first and keep the counter after it explicitly initialized from it.
	kGameCpuCounterFirst = engine::kEngineCpuCounterCount,

	kCpuCounterOmnis = kGameCpuCounterFirst,
	kCpuCounterBlasters,
		kCpuCounterBlastersRendered,
	kCpuCounterMissiles,
		kCpuCounterMissilesRendered,
	kCpuCounterFighters,
		kCpuCounterFightersRendered,
	kCpuCounterBattleships,
		kCpuCounterBattleshipsRendered,

	kGameCpuCounterCount
};

enum GameCpuTimers : int64_t
{
	kCpuTimerFrameUpdate = engine::kEngineCpuTimerCount,
		kCpuTimerFrameInterpolate,
			kCpuTimerInterpolateAllocateAndCopy,
				kCpuTimerInterpolateAllocateAndCopyFighters,
				kCpuTimerInterpolateAllocateAndCopyBattleships,
			kCpuTimerInterpolateUpdate,
				kCpuTimerInterpolateUpdateFighters,
				kCpuTimerInterpolateUpdateBattleships,
		kCpuTimerFramePostRender,
			kCpuTimerPostRenderAllocateAndCopy,
			kCpuTimerPostRenderUpdate,
				kCpuTimerPostRenderUpdateNavigationQuery,
				kCpuTimerPostRenderUpdateFighters,
				kCpuTimerPostRenderUpdateBattleships,
			kCpuTimerPostRenderPreCollision,
			kCpuTimerPostRenderCollide,
			kCpuTimerPostRenderPostCollision,
			kCpuTimerPostRenderAreaDamage,
			kCpuTimerPostRenderDestroy,
			kCpuTimerPostRenderSpawn,

	kCpuTimerRender,
		kCpuTimerRenderOmnis,
		kCpuTimerRenderFighters,
			kCpuTimerRenderFightersAnimate,
		kCpuTimerRenderBattleships,

	kGameCpuTimerCount
};

// One entry per game enumerator, in order; static_assert guards a dropped/extra name (which would otherwise
// silently misalign the overlay rows). Mirrors the engine kXxx*Names tables — see engine ProfileManager.h.
inline constexpr std::string_view kpGameCpuCounterNames[]
{
	"Omnis",
	"Blasters",
	"    Rendered",
	"Missiles",
	"    Rendered",
	"Fighters",
	"    Rendered",
	"Battleships",
	"    Rendered",
};
static_assert(std::size(kpGameCpuCounterNames) == static_cast<size_t>(kGameCpuCounterCount) - static_cast<size_t>(engine::kEngineCpuCounterCount));

inline constexpr std::string_view kpGameCpuTimerNames[]
{
	"Frame update",
	"    Interpolate",
	"        AllocateAndCopy",
	"            Fighters",
	"            Battleships",
	"        Update",
	"            Fighters",
	"            Battleships",
	"    PostRender",
	"        AllocateAndCopy",
	"        Update",
	"            NavQuery",
	"            Fighters",
	"            Battleships",
	"        PreCollision",
	"        Collide",
	"        PostCollision",
	"        AreaDamage",
	"        Destroy",
	"        Spawn",
	"Render",
	"    Omnis",
	"    Fighters",
	"        Animate",
	"    Battleships",
};
static_assert(std::size(kpGameCpuTimerNames) == static_cast<size_t>(kGameCpuTimerCount) - static_cast<size_t>(engine::kEngineCpuTimerCount));

} // namespace game
