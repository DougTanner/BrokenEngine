#pragma once

#if defined(BT_CLIENT)

#include "Frame/Collections/Collection.h"
#include "Frame/GridCoord.h"

namespace game
{

struct Frame;
struct FrameInterpolate;

} // namespace game

namespace engine
{

struct CellStaticData;

struct SoundsInterpolate : public Collection<SoundsInterpolate, CollectionFlags::kIdToIndex>
{
	// SyncData for parent-provided values
	struct SyncData
	{
		XMVECTOR vecPosition {};
		XMVECTOR vecVelocity {};
		common::crc_t uiCrc = 0;
		float fVolume = 0.0f;
		float fPitch = 0.0f;
		float fFadeOutTime = 0.0f;
	};

	// Sync owned sound with parent-provided data
	static void Sync(game::FrameInterpolate& rFrameInterpolate, id_t id, const SyncData& rData);

	static void Update(game::FrameInterpolate& __restrict rFrameInterpolate, const game::Frame& __restrict rPreviousFrame);

	common::crc_t* __restrict puiCrcs = nullptr;
	float* __restrict pfVolumes = nullptr;
	float* __restrict pfPitches = nullptr;
	float* __restrict pfFadeOutTimes = nullptr;
	XMVECTOR* __restrict pVecPositions = nullptr;
	XMVECTOR* __restrict pVecVelocities = nullptr;

	auto Members(this auto&& rSelf)
	{
		return std::tie(rSelf.puiCrcs, rSelf.pfVolumes, rSelf.pfPitches, rSelf.pfFadeOutTimes, rSelf.pVecPositions, rSelf.pVecVelocities);
	}

};
using sound_t = SoundsInterpolate::id_t;

struct SoundsPostRender : public Collection<SoundsPostRender>
{
	static void Update(game::Frame& __restrict rFrame, const game::Frame& __restrict rPreviousFrame, const CellStaticData& rStaticData);

	static void Add(game::Frame& __restrict rFrame, sound_t& rId);


	sound_t* __restrict pIds = nullptr;
	auto Members(this auto&& rSelf)
	{
		return std::tie(rSelf.pIds);
	}

};

extern template struct Collection<SoundsInterpolate, CollectionFlags::kIdToIndex>;
extern template struct Collection<SoundsPostRender>;

} // namespace engine

#endif // BT_CLIENT
