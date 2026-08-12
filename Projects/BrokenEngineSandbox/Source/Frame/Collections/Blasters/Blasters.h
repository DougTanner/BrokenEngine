#pragma once

#include "Frame/Alignments.h"
#if defined(BT_CLIENT)
#include "Frame/Collections/AreaLights/AreaLights.h"
#include "Frame/Collections/PointLights/PointLights.h"
#endif
#include "Frame/Collections/Collection.h"

namespace engine { struct FrameStaticData; }
#if defined(BT_CLIENT)
#include "Frame/Collections/WindTrails/WindTrails.h"
#endif

namespace game
{

struct BlastersType
{
	XMFLOAT2 f2Size {0.11f, 1.5f};
	uint8_t uiAreaLightTypeIndex = 0;
	uint8_t uiPointLightTypeIndex = 0xFF; // 0xFF = use area light
};

struct BlastersInterpolate : public engine::Collection<BlastersInterpolate>,
                             public engine::TypeRegistry<BlastersType>
{
	static constexpr int64_t kiVersion = 1;
	static constexpr char kName[] = "Blasters";
	static constexpr common::crc_t kCrc = common::CrcConsteval(kName);

	// Register
	static void Register();

	// Allocate and copy
	static void AllocateAndCopy(BlastersInterpolate& rCurrent, const BlastersInterpolate& rPrevious);

	// Interpolate
	static void Update(FrameInterpolate& __restrict rCurrentFrameInterpolate, const Frame& __restrict rPreviousFrame);

#if defined(BT_CLIENT)
	static void ClientInit(Frame& rFrame, int64_t iIndex);
	static void ClientInitAll(Frame& rFrame);
#endif

	// Render
#if defined(BT_CLIENT)
	static void Render(const FrameInterpolate& __restrict rFrameInterpolate, int64_t iCommandBuffer);
#endif

	uint8_t* __restrict puiTypeIndices = nullptr;
	XMVECTOR* __restrict pVecPositions = nullptr;
	XMVECTOR* __restrict pVecDirections = nullptr;
#if defined(BT_CLIENT)
	engine::area_lights_t* __restrict puiAreaLights = nullptr;
	engine::point_lights_t* __restrict puiPointLights = nullptr;
	engine::wind_trail_t* __restrict puiWindTrails = nullptr;
	float* __restrict pfWindTrailIntensities = nullptr;
	float* __restrict pfWindTrailWidths = nullptr;
	float* __restrict pfWindTrailLengthMultipliers = nullptr;
#endif
	auto SharedMembers(this auto&& rSelf) { return std::tie(rSelf.puiTypeIndices, rSelf.pVecPositions, rSelf.pVecDirections); }
#if defined(BT_CLIENT)
	auto ClientMembers(this auto&& rSelf) { return std::tie(rSelf.puiAreaLights, rSelf.puiPointLights, rSelf.puiWindTrails, rSelf.pfWindTrailIntensities, rSelf.pfWindTrailWidths, rSelf.pfWindTrailLengthMultipliers); }
#endif
	auto Members(this auto&& rSelf)
	{
#if defined(BT_CLIENT)
		return std::tuple_cat(rSelf.SharedMembers(), rSelf.ClientMembers());
#else
		return rSelf.SharedMembers();
#endif
	}
	auto PersistentMembers(this auto&& rSelf)
	{
#if defined(BT_CLIENT)
		return std::tie(rSelf.puiTypeIndices, rSelf.puiAreaLights, rSelf.puiPointLights, rSelf.puiWindTrails, rSelf.pfWindTrailIntensities, rSelf.pfWindTrailWidths, rSelf.pfWindTrailLengthMultipliers);
#else
		return std::tie(rSelf.puiTypeIndices);
#endif
	}

	// Utility
	bool LogDifferences(const BlastersInterpolate& rOther) const;
};

enum class BlasterFlags : uint8_t
{
	kDestroy       = 0x01,
	kTransfer      = 0x02,
};
using BlasterFlags_t = common::Flags<BlasterFlags>;

struct BlastersPostRender : public engine::Collection<BlastersPostRender>
{
	static constexpr int64_t kiVersion = 2;

	// Allocate and copy
	static void AllocateAndCopy(BlastersPostRender& rCurrent, const BlastersPostRender& rPrevious);

	// Update
	static void Update(Frame& __restrict rFrame, const Frame& __restrict rPreviousFrame, const engine::FrameStaticData& rStaticData);
	static void PreCollision(Frame& __restrict rFrame, const Frame& __restrict rPreviousFrame, const engine::FrameStaticData& rStaticData);
	static void PostCollision(Frame& __restrict rFrame, const Frame& __restrict rPreviousFrame, const engine::FrameStaticData& rStaticData);
	static void Transfer(Frame& __restrict rFrame, const engine::FrameStaticData& rStaticData);
	static void Destroy(Frame& __restrict rFrame, const engine::FrameStaticData& rStaticData);

	BlasterFlags_t* __restrict pFlags = nullptr;
	XMVECTOR* __restrict pVecVelocities = nullptr;
	engine::alignment_t* __restrict pAlignments = nullptr;
	auto SharedMembers(this auto&& rSelf) { return std::tie(rSelf.pFlags, rSelf.pVecVelocities, rSelf.pAlignments); }
	auto Members(this auto&& rSelf) { return rSelf.SharedMembers(); }

	// Utility
	bool LogDifferences(const BlastersPostRender& rOther) const;

	// SpawnInfo for spawn parameters
	struct SpawnInfo
	{
		XMVECTOR vecPosition = DirectX::XMVectorZero();
		XMVECTOR vecVelocity = DirectX::XMVectorZero();
		uint8_t uiTypeIndex;
		BlasterFlags_t flags {};
		engine::alignment_t alignment {};
		float fWindTrailIntensity = 0.0f;
		float fWindTrailWidth = 0.0f;
		float fWindTrailLengthMultiplier = 1.0f;
	};

	static void Spawn(Frame& __restrict rFrame, const SpawnInfo& rInfo);
};

} // namespace game

namespace engine
{
extern template struct Collection<game::BlastersInterpolate>;
extern template struct Collection<game::BlastersPostRender>;
}
