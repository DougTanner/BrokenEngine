#pragma once

#if defined(BT_CLIENT)

#include "Frame/Collections/Collection.h"
#include "Frame/FrameUtils.h"
#include "Frame/GridCoord.h"

namespace game
{

struct Frame;

} // namespace game

namespace engine
{

struct CellStaticData;
class Wrapper;

struct SmokeTrailsInterpolate;
using smoke_trails_t = Id<SmokeTrailsInterpolate>;

inline constexpr int64_t kiMaxExplosionTrails = 8;
inline constexpr int64_t kiInvalidTrailType = 255;

enum class ExplosionFlags : uint8_t
{
	kYellow = 0x02,
	kRed    = 0x04,
};
using ExplosionFlags_t = common::Flags<ExplosionFlags>;

// Engine effect controllers. ExplosionsInterpolate::Register sets them before the game registers any ExplosionType.
inline int64_t giExplosionPrimaryLightControllerTypeIndex = kiInvalidControllerType;
inline int64_t giExplosionSecondaryLightControllerTypeIndex = kiInvalidControllerType;
inline int64_t giExplosionPrimaryPuffControllerTypeIndex = kiInvalidControllerType;
inline int64_t giExplosionSecondaryPuffControllerTypeIndex = kiInvalidControllerType;
inline int64_t giExplosionWindRadialControllerTypeIndex = kiInvalidControllerType;

struct ExplosionType
{
	// Controller type indices for fire-and-forget effects; kiInvalidControllerType disables that effect.
	int64_t iPrimaryLightControllerTypeIndex = giExplosionPrimaryLightControllerTypeIndex;
	int64_t iSecondaryLightControllerTypeIndex = giExplosionSecondaryLightControllerTypeIndex;
	int64_t iPrimaryPuffControllerTypeIndex = giExplosionPrimaryPuffControllerTypeIndex;
	int64_t iSecondaryPuffControllerTypeIndex = giExplosionSecondaryPuffControllerTypeIndex;
	int64_t iWindRadialControllerTypeIndex = giExplosionWindRadialControllerTypeIndex;

	int64_t iBaseParticleCount = 0;
	common::crc_t particleCrc = common::CrcConsteval(R"(Textures\Particles\[BC4]Long\5.png)");
	uint32_t uiParticleColor = 0xFF0000FF;
	uint32_t uiParticleColorEnd = 0x800000FF;

	float fParticlePositionJitter = 0.5f;
	float fParticleVelocityMinimum = 1.0f;
	float fParticleVelocityRandom = 10.0f;
	float fParticleVerticalVelocityMinimum = 0.0f;
	float fParticleVerticalVelocityRandom = 20.0f;
	float fParticleVelocityDecay = 1.0f;
	float fParticleGravity = 30.0f;
	float fParticleWidth = 0.035f;
	float fParticleLength = 0.1f;
	float fParticleIntensityMinimum = 0.25f;
	float fParticleIntensityRandom = 2.0f;
	float fParticleIntensityDecay = 2.4f;
	float fParticleIntensityPower = 2.5f;

	std::chrono::duration<float> primaryTime = std::chrono::duration<float>(0.075f);

	std::chrono::duration<float> trailDelayTime = std::chrono::duration<float>::zero();
	std::chrono::duration<float> trailTimeMinimum = std::chrono::duration<float>(0.2f);
	std::chrono::duration<float> trailTimeRandom = std::chrono::duration<float>(0.2f);
	float fTrailIntensityMinimum = 0.025f;
	float fTrailIntensityRandom = 0.025f;
	float fTrailStart = 0.6f;
	float fTrailLengthMinimum = 0.75f;
	float fTrailLengthRandom = 4.5f;
	float fTrailGravity = 2.0f;

	// Secondary explosion count (lights and puffs)
	int64_t iSecondaryExplosionCount = 4;

	float fSecondaryPositionMinimum = 0.25f;
	float fSecondaryPositionJitter = 1.0f;

	// Per-type runtime tweak multipliers (Particles tab). Optional; null means unscaled.
	Wrapper* pParticleWidthScale = nullptr;
	Wrapper* pParticleLengthScale = nullptr;
	Wrapper* pParticleLengthSpreadScale = nullptr;
	Wrapper* pParticlePositionJitterScale = nullptr;
	Wrapper* pParticleVelocityBaseScale = nullptr;
	Wrapper* pParticleVelocitySpreadScale = nullptr;
	Wrapper* pParticleVerticalVelocityBaseScale = nullptr;
	Wrapper* pParticleVerticalVelocitySpreadScale = nullptr;
	Wrapper* pParticleVelocityDecayScale = nullptr;
	Wrapper* pParticleGravityScale = nullptr;
	Wrapper* pParticleVisibleIntensityScale = nullptr;
	Wrapper* pParticleIntensitySpreadScale = nullptr;
	Wrapper* pParticleIntensityDecayScale = nullptr;
	Wrapper* pParticleIntensityPowerScale = nullptr;

	bool operator==(const ExplosionType& rOther) const = default;
};

struct ExplosionsInterpolate : public Collection<ExplosionsInterpolate>,
	public TypeRegistry<ExplosionType>
{
	// Register default explosion effect types (called from ForEachRegister)
	static void Register();

	static inline int64_t siTotalCount = 0;

	static inline int64_t siExplosionTrailTypeIndex = kiInvalidTrailType;

	static void Update(game::FrameInterpolate& __restrict rCurrentFrameInterpolate, const game::Frame& __restrict rPreviousFrame);

	uint8_t* __restrict puiTypeIndices = nullptr;
	SimulationTime* __restrict pStartTimes = nullptr;
	XMVECTOR* __restrict pVecPositions = nullptr;
	XMVECTOR* __restrict pVecDirections = nullptr;

	float* __restrict pfTimePercents = nullptr;

	// Trail state (8 separate arrays - pTrails[j] is array of all explosions' j-th trail)
	int32_t* __restrict piTrailCounts = nullptr;
	smoke_trails_t* pTrails[kiMaxExplosionTrails] = {};
	std::chrono::duration<float>* pTrailTimes[kiMaxExplosionTrails] = {};
	float* pfTrailIntensities[kiMaxExplosionTrails] = {};
	XMVECTOR* pVecTrailStartPositions[kiMaxExplosionTrails] = {};
	XMVECTOR* pVecTrailEndPositions[kiMaxExplosionTrails] = {};

	auto Members(this auto&& rSelf)
	{
		return std::tie(rSelf.puiTypeIndices, rSelf.pStartTimes, rSelf.pVecPositions, rSelf.pVecDirections, rSelf.pfTimePercents, rSelf.piTrailCounts, rSelf.pTrailTimes, rSelf.pTrails, rSelf.pfTrailIntensities, rSelf.pVecTrailStartPositions, rSelf.pVecTrailEndPositions);
	}
	auto PersistentMembers(this auto&& rSelf)
	{
		return std::tie(rSelf.pTrails);
	}
	auto OwnedHandles(this auto&& rSelf)
	{
		return std::tie(rSelf.pTrails);
	}
};

struct ExplosionsPostRender : public Collection<ExplosionsPostRender>
{
	static void Update(game::Frame& __restrict rFrame, const game::Frame& __restrict rPreviousFrame, const CellStaticData& rStaticData);

	static void Destroy(game::Frame& __restrict rFrame, const CellStaticData& rStaticData);

	LifecycleFlags_t* __restrict pLifecycleFlags = nullptr;
	auto Members(this auto&& rSelf)
	{
		return std::tie(rSelf.pLifecycleFlags);
	}

	struct SpawnInfo
	{
		int64_t iTypeIndex = 0;
		XMVECTOR vecPosition = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
		XMVECTOR vecDirection = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
		ExplosionFlags_t flags {};
		LifecycleFlags_t lifecycleFlags {};
		int64_t iTrailCount = 0;
		float fTrailAngle = XM_2PI;
		int64_t iParticleCount = 0;
		float fParticleAngle = XM_2PI;
		float fSizePercent = 1.0f;
		float fSmokePercent = 1.0f;
		float fTimePercent = 1.0f;
	};

	// coordinate is the cell rSpawnInformation.vecPosition is local to.
	static bool Spawn(game::Frame& __restrict rFrame, GridCoord coordinate, SimulationTime currentTime, const SpawnInfo& rSpawnInformation, common::RandomEngine& rRandomEngine);
};

extern template struct Collection<ExplosionsInterpolate>;
extern template struct Collection<ExplosionsPostRender>;

} // namespace engine

#endif // BT_CLIENT
