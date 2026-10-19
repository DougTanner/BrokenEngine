#pragma once

#include "Frame/Collections/Collection.h"
#include "Frame/Alignments.h"
#if defined(BT_CLIENT)
#include "Frame/Collections/AreaLights/AreaLights.h"
#include "Frame/Collections/PointLights/PointLights.h"
#include "Frame/Collections/WindTrails/WindTrails.h"
#endif

namespace engine
{
struct CellStaticData;
} // namespace engine

namespace game
{

struct BlastersType
{
	XMFLOAT2 f2Size {0.11f, 1.5f};
	int64_t iAreaLightTypeIndex = 0;
	int64_t iPointLightTypeIndex = 0xFF; // 0xFF = use area light
};

struct BlastersInterpolate : public engine::Collection<BlastersInterpolate>, public engine::TypeRegistry<BlastersType>
{
	static constexpr int64_t kiVersion = 1;
	static constexpr char kpcName[] = "Blasters";
	static constexpr common::crc_t kCrc = common::CrcConsteval(kpcName);

	static void Register();

	static void Update(FrameInterpolate& __restrict rCurrentFrameInterpolate, const Frame& __restrict rPreviousFrame);

#if defined(BT_CLIENT)
	static void ClientInitialize(Frame& rFrame, int64_t iIndex);
	static void ClientInitializeAll(Frame& rFrame);
#endif

	uint8_t* __restrict puiTypeIndices = nullptr;
	XMVECTOR* __restrict pVecPositions = nullptr;
	XMVECTOR* __restrict pVecDirections = nullptr;
#if defined(BT_CLIENT)
	engine::area_lights_t* __restrict pAreaLights = nullptr;
	engine::point_lights_t* __restrict pPointLights = nullptr;
	engine::wind_trail_t* __restrict pWindTrails = nullptr;
	float* __restrict pfWindTrailIntensities = nullptr;
	float* __restrict pfWindTrailWidths = nullptr;
	float* __restrict pfWindTrailLengthMultipliers = nullptr;
#endif
	auto SharedMembers(this auto&& rSelf) { return std::tie(rSelf.puiTypeIndices, rSelf.pVecPositions, rSelf.pVecDirections); }
#if defined(BT_CLIENT)
	auto ClientMembers(this auto&& rSelf) { return std::tie(rSelf.pAreaLights, rSelf.pPointLights, rSelf.pWindTrails, rSelf.pfWindTrailIntensities, rSelf.pfWindTrailWidths, rSelf.pfWindTrailLengthMultipliers); }
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
		return std::tie(rSelf.puiTypeIndices, rSelf.pAreaLights, rSelf.pPointLights, rSelf.pWindTrails, rSelf.pfWindTrailIntensities, rSelf.pfWindTrailWidths, rSelf.pfWindTrailLengthMultipliers);
#else
		return std::tie(rSelf.puiTypeIndices);
#endif
	}
	auto OwnedHandles([[maybe_unused]] this auto&& rSelf)
	{
#if defined(BT_CLIENT)
		return std::tie(rSelf.pAreaLights, rSelf.pPointLights, rSelf.pWindTrails);
#else
		return std::tie();
#endif
	}

	bool LogDifferences(const BlastersInterpolate& rOther) const;
};

struct BlastersPostRender : public engine::Collection<BlastersPostRender>
{
	static constexpr int64_t kiVersion = 3;

	static void Update(Frame& __restrict rFrame, const Frame& __restrict rPreviousFrame, const engine::CellStaticData& rStaticData);
	static void PreCollision(Frame& __restrict rFrame, const Frame& __restrict rPreviousFrame, const engine::CellStaticData& rStaticData);
	static void PostCollision(Frame& __restrict rFrame, const Frame& __restrict rPreviousFrame, const engine::CellStaticData& rStaticData);
	static void Transfer(Frame& __restrict rFrame, const engine::CellStaticData& rStaticData);

	engine::LifecycleFlags_t* __restrict pLifecycleFlags = nullptr;
	XMVECTOR* __restrict pVecVelocities = nullptr;
	engine::AlignmentIdentifier* __restrict pAlignments = nullptr;
	auto SharedMembers(this auto&& rSelf) { return std::tie(rSelf.pLifecycleFlags, rSelf.pVecVelocities, rSelf.pAlignments); }
	auto Members(this auto&& rSelf) { return rSelf.SharedMembers(); }

	bool LogDifferences(const BlastersPostRender& rOther) const;

	struct SpawnInfo
	{
		XMVECTOR vecPosition = DirectX::XMVectorZero();
		XMVECTOR vecVelocity = DirectX::XMVectorZero();
		int64_t iTypeIndex = 0;
		engine::AlignmentIdentifier alignment {};
		float fWindTrailIntensity = 0.0f;
		float fWindTrailWidth = 0.0f;
		float fWindTrailLengthMultiplier = 1.0f;

		// Status-change payload members.
		static constexpr auto Fields()
		{
			return engine::FrameColumnList<FRAME_COLUMN(SpawnInfo, vecPosition), FRAME_COLUMN(SpawnInfo, vecVelocity), FRAME_COLUMN(SpawnInfo, iTypeIndex), FRAME_COLUMN(SpawnInfo, alignment), FRAME_COLUMN(SpawnInfo, fWindTrailIntensity), FRAME_COLUMN(SpawnInfo, fWindTrailWidth), FRAME_COLUMN(SpawnInfo, fWindTrailLengthMultiplier)> {};
		}
	};

	static bool Spawn(Frame& __restrict rFrame, const SpawnInfo& rSpawnInformation);
};

} // namespace game

namespace engine
{
extern template struct Collection<game::BlastersInterpolate>;
extern template struct Collection<game::BlastersPostRender>;
} // namespace engine
