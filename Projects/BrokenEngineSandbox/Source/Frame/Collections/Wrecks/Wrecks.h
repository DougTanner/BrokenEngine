#pragma once

#if defined(BT_CLIENT)

#include "Frame/Collections/Collection.h"

#include "Frame/Collections/Ships/ShipTypes.h"

namespace engine
{

struct CellStaticData;

} // namespace engine

namespace game
{

// A dying Battleship's model, captured on its first dying tick, falling until it rests on the terrain or sinks.
// A full state replaces the cell's whole Frame, which drops its wrecks.
struct WrecksInterpolate : public engine::Collection<WrecksInterpolate>
{
	static void Update(FrameInterpolate& __restrict rCurrentFrameInterpolate, const Frame& __restrict rPreviousFrame, const engine::CellStaticData& rStaticData);

	XMVECTOR* __restrict pVecPositions = nullptr; // Cell-local, W = 1.
	XMVECTOR* __restrict pVecDirections = nullptr;
	float* __restrict pfFallSpeeds = nullptr;
	std::chrono::duration<float>* __restrict pFallTimes = nullptr;
	std::chrono::duration<float>* __restrict pAnimationTimes = nullptr; // Frozen at capture.
	auto Members(this auto&& rSelf)
	{
		return std::tie(rSelf.pVecPositions, rSelf.pVecDirections, rSelf.pfFallSpeeds, rSelf.pFallTimes, rSelf.pAnimationTimes);
	}
};

struct WrecksPostRender : public engine::Collection<WrecksPostRender>
{
	static void Update(Frame& __restrict rFrame, const Frame& __restrict rPreviousFrame, const engine::CellStaticData& rStaticData);
	// Drops a sunk wreck and every wreck of a cell that neither the camera's cell loaded area nor, during a camera jump,
	// its previous cell loaded area overlaps.
	static void Destroy(Frame& __restrict rFrame, const engine::CellStaticData& rStaticData);
	// Captures each Battleships row on its first dying tick, then emits one hull explosion per wreck younger than
	// kWreckExplosionDuration.
	static void Spawn(Frame& __restrict rFrame, const engine::CellStaticData& rStaticData);

	engine::LifecycleFlags_t* __restrict pLifecycleFlags = nullptr;
	ShipController* __restrict pController = nullptr;
	XMVECTOR* __restrict pVecTilts = nullptr; // Quaternion from +Z to the terrain normal at the center; identity while sinking.
	float* __restrict pfStartZ = nullptr;
	float* __restrict pfRestZ = nullptr;      // Unused while sinking.
	float* __restrict pfTopOffsets = nullptr; // Hull top above the position.
	bool* __restrict pbSinks = nullptr;
	auto Members(this auto&& rSelf)
	{
		return std::tie(rSelf.pLifecycleFlags, rSelf.pController, rSelf.pVecTilts, rSelf.pfStartZ, rSelf.pfRestZ, rSelf.pfTopOffsets, rSelf.pbSinks);
	}
};

// Returns wreck i's current tilt, applied after its yaw: toward the terrain normal by the fraction of its fall completed,
// complete at rest.
inline XMVECTOR WreckTilt(const WrecksInterpolate& rInterpolate, const WrecksPostRender& rPostRender, int64_t i)
{
	if (rPostRender.pbSinks[i])
	{
		return XMQuaternionIdentity();
	}

	float fStartZ = rPostRender.pfStartZ[i];
	float fRestZ = rPostRender.pfRestZ[i];
	float fTiltPercent = 1.0f;
	if (fStartZ != fRestZ)
	{
		fTiltPercent = std::clamp((fStartZ - XMVectorGetZ(rInterpolate.pVecPositions[i])) / (fStartZ - fRestZ), 0.0f, 1.0f);
	}
	return XMQuaternionSlerp(XMQuaternionIdentity(), rPostRender.pVecTilts[i], fTiltPercent);
}

} // namespace game

namespace engine
{

extern template struct Collection<game::WrecksInterpolate>;
extern template struct Collection<game::WrecksPostRender>;

} // namespace engine

#endif // BT_CLIENT
