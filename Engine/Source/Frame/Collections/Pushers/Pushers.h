#pragma once

#include "Frame/Collections/Collection.h"
#include "Frame/GridCoord.h"

namespace game
{

struct Frame;

} // namespace game

namespace engine
{

struct CellStaticData;

enum class PusherFlags : uint8_t
{
	kTypeNone    = 0x00,
	kTypeDefault = 0x01,
	kTypeMines   = 0x02,
};
using PusherFlags_t = common::Flags<PusherFlags>;

// Limits the added push; an existing velocity above fMaxPushVelocity is not reduced.
[[nodiscard]] inline XMVECTOR XM_CALLCONV ApplyClampedPush(FXMVECTOR vecVelocity, FXMVECTOR vecPushDirection, float fPushStrength, float fMaxPushVelocity)
{
	float fCurrentPushVelocity = XMVectorGetX(XMVector3Dot(vecVelocity, vecPushDirection));
	float fAllowedPush = std::max(fMaxPushVelocity - fCurrentPushVelocity, 0.0f);
	return XMVectorMultiplyAdd(XMVectorReplicate(std::min(fPushStrength, fAllowedPush)), vecPushDirection, vecVelocity);
}

struct PushersInterpolate : public Collection<PushersInterpolate, CollectionFlags::kIdToIndex>
{
	// Bump on any SOA layout or push behavior change — feeds the Frame::kiVersion save/replay gate
	static constexpr int64_t kiVersion = 2;

	static void AllocateAndCopy(PushersInterpolate& rCurrent, const PushersInterpolate& rPrevious);

	static void Update(game::FrameInterpolate& __restrict rFrameInterpolate, const game::Frame& __restrict rPreviousFrame);

	struct SyncData
	{
		XMVECTOR vecPosition {};
		float fRadius = 0.0f;
		float fIntensity = 0.0f;
		float fPower = 0.0f;
		PusherFlags_t flags;
	};

	// Sync pusher state from owner collection
	static void XM_CALLCONV Sync(game::FrameInterpolate& rFrameInterpolate, id_t id, const SyncData& rData);

	// Zone system - builds spatial acceleration structure each frame over the cell's own area
	static void XM_CALLCONV SetupZones(const game::Frame& __restrict rFrame, FXMVECTOR vecArea);

	// Query force at position using zone acceleration
	static XMVECTOR XM_CALLCONV ApplyPush(const game::FrameInterpolate& rFrameInterpolate, FXMVECTOR vecPosition, id_t ignorePusher = id_t {}, PusherFlags_t includeFlags = PusherFlags::kTypeDefault, PusherFlags_t excludeFlags = PusherFlags::kTypeMines);

#if defined(BT_CLIENT)
	static void BeginRender(int64_t iCommandBuffer, const std::unordered_map<GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<GridCoord>& rActiveCoords);
	static void Render(const game::FrameInterpolate& __restrict rFrameInterpolate, int64_t iCommandBuffer);
	static void EndRender(int64_t iCommandBuffer);
#endif

	XMVECTOR* __restrict pVecPositions = nullptr;
	float* __restrict pfRadii = nullptr;
	float* __restrict pfIntensities = nullptr;
	float* __restrict pfPowers = nullptr;
	PusherFlags_t* __restrict pFlags = nullptr;

	auto Members(this auto&& rSelf)
	{
		return std::tie(rSelf.pVecPositions, rSelf.pfRadii, rSelf.pfIntensities, rSelf.pfPowers, rSelf.pFlags);
	}

	bool LogDifferences(const PushersInterpolate& rOther) const;
};
using pusher_t = PushersInterpolate::id_t;

struct PushersPostRender : public Collection<PushersPostRender>
{
	// Bump on any SOA layout change — feeds the Frame::kiVersion save/replay gate
	static constexpr int64_t kiVersion = 1;

	static void AllocateAndCopy(PushersPostRender& rCurrent, const PushersPostRender& rPrevious);

	static void Update(game::Frame& __restrict rFrame, const game::Frame& __restrict rPreviousFrame, const CellStaticData& rStaticData);

	static void Add(game::Frame& __restrict rFrame, pusher_t& rId);

	static void Remove(game::Frame& __restrict rFrame, pusher_t& rId);

	pusher_t* __restrict pIds = nullptr;
	auto Members(this auto&& rSelf)
	{
		return std::tie(rSelf.pIds);
	}

	bool LogDifferences(const PushersPostRender& rOther) const;
};

extern template struct Collection<PushersInterpolate, CollectionFlags::kIdToIndex>;
extern template struct Collection<PushersPostRender>;

} // namespace engine
