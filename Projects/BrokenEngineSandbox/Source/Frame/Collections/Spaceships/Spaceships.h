#pragma once

#include "Frame/Collections/Pushers/Pushers.h"
#include "Frame/Collections/Collection.h"
#include "Frame/Alignments.h"
#include "Frame/GridCoord.h"
#if defined(BT_CLIENT)
#include "Frame/Collections/WindTrails/WindTrails.h"
#endif

namespace engine
{
struct CellStaticData;
} // namespace engine

namespace game
{

inline constexpr float kfSpaceshipDestroyTime = 0.25f;
inline constexpr float kfSpaceshipDestroyExplosionInterval = 0.024f;

inline constexpr float kfSpaceshipRadius = 1.5f;

inline constexpr float kfSpaceshipAcceleration = 10.0f;
inline constexpr float kfSpaceshipDrag = 0.25f;
inline constexpr float kfSpaceshipMaxSpeed = 40.0f;
inline constexpr float kfSpaceshipMaxTurnRate = 4.0f;

inline constexpr float kfSpaceshipPusherRadius = kfSpaceshipRadius * 1.5f;
inline constexpr float kfSpaceshipPusherIntensity = 36.0f;
inline constexpr float kfSpaceshipPusherPower = 3.0f;
inline constexpr float kfSpaceshipMaxPusherPushVelocity = kfSpaceshipMaxSpeed * 0.5f;

struct SpaceshipsInterpolate : public engine::Collection<SpaceshipsInterpolate>
{
	static constexpr int64_t kiVersion = 3;
	static constexpr const char* kpcName = "Spaceships";
	static constexpr common::crc_t kuiCrc = common::CrcConsteval("Spaceships");

	static void Register();

	static void AllocateAndCopy(SpaceshipsInterpolate& rCurrent, const SpaceshipsInterpolate& rPrevious);

	static void Update(FrameInterpolate& __restrict rCurrentFrameInterpolate, const Frame& __restrict rPreviousFrame);

#if defined(BT_CLIENT)
	static void ClientInitialize(Frame& rFrame, int64_t iIndex);
	static void ClientInitializeAll(Frame& rFrame);
#endif

	XMVECTOR* __restrict pVecPositions = nullptr;
	XMVECTOR* __restrict pVecDirections = nullptr;
	float* __restrict pfDestroyedTimes = nullptr;
	engine::pusher_t* __restrict puiPushers = nullptr;
	engine::registry_id_t* __restrict puiRegistryIds = nullptr;
#if defined(BT_CLIENT)
	engine::wind_trail_t* __restrict puiWindTrails = nullptr;
#endif
	float* __restrict pfDeltaRotations = nullptr;
#if defined(BT_CLIENT)
	float* __restrict pfAnimationTimes = nullptr;
#endif
	auto SharedMembers(this auto&& rSelf)
	{
		return std::tie(rSelf.pVecPositions, rSelf.pVecDirections, rSelf.pfDestroyedTimes, rSelf.puiPushers, rSelf.puiRegistryIds, rSelf.pfDeltaRotations);
	}
#if defined(BT_CLIENT)
	auto ClientMembers(this auto&& rSelf)
	{
		return std::tie(rSelf.pfAnimationTimes, rSelf.puiWindTrails);
	}
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
		return std::tie(rSelf.puiPushers, rSelf.puiRegistryIds, rSelf.puiWindTrails);
#else
		return std::tie(rSelf.puiPushers, rSelf.puiRegistryIds);
#endif
	}

	bool LogDifferences(const SpaceshipsInterpolate& rOther) const;

	static void GraphicsResources();

	static void BeginRender(int64_t iCommandBuffer, const std::unordered_map<engine::GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<engine::GridCoord>& rActiveCoordinates);
	static void Render(const FrameInterpolate& __restrict rFrameInterpolate, int64_t iCommandBuffer);
	static void EndRender(int64_t iCommandBuffer);
};

enum class SpaceshipFlags : uint8_t
{
	kFleePlayer           = 0x01,
	kExploding            = 0x02,
	kReturnToIslandCenter = 0x04,
	kTransfer             = 0x08,
};
using SpaceshipFlags_t = common::Flags<SpaceshipFlags>;

struct SpaceshipsPostRender : public engine::Collection<SpaceshipsPostRender>
{
	static constexpr int64_t kiVersion = 6;

	static void AllocateAndCopy(SpaceshipsPostRender& rCurrent, const SpaceshipsPostRender& rPrevious);

	// Simulation never produces non-finite health; reject it as a corrupt stream, since NaN or +inf health never dies.
	static void PostRead(const SpaceshipsPostRender& rCurrent)
	{
		for (int64_t i = 0; i < rCurrent.iCount; ++i)
		{
			if (!std::isfinite(rCurrent.pfHealths[i]))
			{
				throw std::ios_base::failure("SpaceshipsPostRender pfHealths");
			}
		}
	}

	static void Update(Frame& __restrict rFrame, const Frame& __restrict rPreviousFrame, const engine::CellStaticData& rStaticData);
	static void PreCollision(Frame& __restrict rFrame, const Frame& __restrict rPreviousFrame, const engine::CellStaticData& rStaticData);
	static void PostCollision(Frame& __restrict rFrame, const Frame& __restrict rPreviousFrame, const engine::CellStaticData& rStaticData);
	static void AreaDamage(Frame& __restrict rFrame, const Frame& __restrict rPreviousFrame, const engine::CellStaticData& rStaticData);
	static void Transfer(Frame& __restrict rFrame, const engine::CellStaticData& rStaticData);
	static void Destroy(Frame& __restrict rFrame, const engine::CellStaticData& rStaticData);
	static void Spawn(Frame& __restrict rFrame, const engine::CellStaticData& rStaticData);

private:
	// Defined in SpaceshipsNavigation.cpp:
	static void XM_CALLCONV ComputeSteering(std::span<const XMFLOAT4> islandCandidates, FXMVECTOR vecPosition, FXMVECTOR vecDirection, bool bPlayerAlive, FXMVECTOR vecNearestPlayer, float fDeltaTime, SpaceshipFlags_t& rFlags, float& rfDeltaRotation);
	static void XM_CALLCONV ApplyMovement(const Frame& __restrict rFrame, const SpaceshipsInterpolate& __restrict rCurrentInterpolate, int64_t i, SpaceshipFlags_t flags, float fDeltaTime, XMVECTOR& rVecVelocity);
	static void XM_CALLCONV ApplyPusherResponse(const Frame& __restrict rFrame, const SpaceshipsInterpolate& __restrict rCurrentInterpolate, int64_t i, XMVECTOR& rVecVelocity);
	static void ApplyTerrainBounce(const engine::CellStaticData& rStaticData, const SpaceshipsInterpolate& rPreviousInterpolate, SpaceshipsInterpolate& __restrict rCurrentInterpolate, int64_t i, float fDeltaTime, float& rfDeltaRotation, XMVECTOR& rVecVelocity);

	// Defined in SpaceshipsCombat.cpp:
	static void XM_CALLCONV RegenerateHealth(FXMVECTOR vecPosition, bool bPlayerAlive, FXMVECTOR vecNearestPlayer, SpaceshipFlags_t flags, std::chrono::duration<float> deltaTime, float& rfHealth);

public:
	SpaceshipFlags_t* __restrict pFlags = nullptr;
	XMVECTOR* __restrict pVecVelocities = nullptr;
	XMVECTOR* __restrict pVecDamageDirections = nullptr;
	float* __restrict pfHealths = nullptr;
	float* __restrict pfDestroyedExplosionTimes = nullptr;
	float* __restrict pfNextBlasterSpawnTimes = nullptr;
	engine::AlignmentIdentifier* __restrict pAlignments = nullptr;
	float* __restrict pfArrivalGracePeriods = nullptr;
	// Server Members() must match SharedMembers() for wire/CRC parity; client-only fields stay outside SharedMembers().
	auto SharedMembers(this auto&& rSelf)
	{
		return std::tie(rSelf.pFlags, rSelf.pVecVelocities, rSelf.pVecDamageDirections, rSelf.pfHealths, rSelf.pfDestroyedExplosionTimes, rSelf.pfNextBlasterSpawnTimes, rSelf.pAlignments, rSelf.pfArrivalGracePeriods);
	}
	auto Members(this auto&& rSelf)
	{
		return rSelf.SharedMembers();
	}
	auto PersistentMembers(this auto&& rSelf)
	{
		return std::tie(rSelf.pVecDamageDirections, rSelf.pAlignments);
	}

	bool LogDifferences(const SpaceshipsPostRender& rOther) const;
	static void AvoidTerrain(Frame& __restrict rFrame, const Frame& __restrict rPreviousFrame, const engine::CellStaticData& rStaticData, int64_t iStart, int64_t iEnd);

	struct SpawnInfo
	{
		XMVECTOR vecPosition = DirectX::XMVectorZero();
		XMVECTOR vecDirection = DirectX::XMVectorZero();
		XMVECTOR vecVelocity = DirectX::XMVectorZero();
		engine::AlignmentIdentifier alignment {};
		float fHealth = 0.0f;
		float fNextBlasterSpawnTime = 0.0f;
		float fArrivalGracePeriod = 0.0f;
		float fDeltaRotation = 0.0f;
	};

	static bool Spawn(Frame& __restrict rFrame, const SpawnInfo& rInfo);
};

} // namespace game

namespace engine
{
extern template struct Collection<game::SpaceshipsInterpolate>;
extern template struct Collection<game::SpaceshipsPostRender>;
} // namespace engine
