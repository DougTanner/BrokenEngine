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
class Wrapper;

#if defined(BT_CLIENT)
struct SmokeTrailsInterpolate;
using smoke_trails_t = Id<SmokeTrailsInterpolate>;

// Live-tunable explosion content values. The game owns the sliders; the engine owns the effect mechanism.
// Contract: the game fills every field once at client startup, before ForEachRegister(). The
// server never reads it, and the engine dereferences each pointer without a null fallback.
struct ExplosionTuning
{
	Wrapper* pPrimaryVisibleAreaOne = nullptr;
	Wrapper* pPrimaryVisibleAreaTwo = nullptr;
	Wrapper* pPrimaryVisibleAreaThree = nullptr;
	Wrapper* pPrimaryVisibleIntensityOne = nullptr;
	Wrapper* pPrimaryVisibleIntensityTwo = nullptr;
	Wrapper* pPrimaryVisibleIntensityThree = nullptr;
	Wrapper* pPrimaryLightingAreaOne = nullptr;
	Wrapper* pPrimaryLightingAreaTwo = nullptr;
	Wrapper* pPrimaryLightingAreaThree = nullptr;
	Wrapper* pPrimaryLightingIntensityOne = nullptr;
	Wrapper* pPrimaryLightingIntensityTwo = nullptr;
	Wrapper* pPrimaryLightingIntensityThree = nullptr;

	Wrapper* pSecondaryVisibleAreaOne = nullptr;
	Wrapper* pSecondaryVisibleAreaTwo = nullptr;
	Wrapper* pSecondaryVisibleAreaThree = nullptr;
	Wrapper* pSecondaryVisibleIntensityOne = nullptr;
	Wrapper* pSecondaryVisibleIntensityTwo = nullptr;
	Wrapper* pSecondaryVisibleIntensityThree = nullptr;
	Wrapper* pSecondaryLightingAreaOne = nullptr;
	Wrapper* pSecondaryLightingAreaTwo = nullptr;
	Wrapper* pSecondaryLightingAreaThree = nullptr;
	Wrapper* pSecondaryLightingIntensityOne = nullptr;
	Wrapper* pSecondaryLightingIntensityTwo = nullptr;
	Wrapper* pSecondaryLightingIntensityThree = nullptr;

	Wrapper* pPrimaryPuffAreaOne = nullptr;
	Wrapper* pPrimaryPuffAreaTwo = nullptr;
	Wrapper* pPrimaryPuffIntensityOne = nullptr;
	Wrapper* pPrimaryPuffIntensityTwo = nullptr;
	Wrapper* pSecondaryPuffAreaOne = nullptr;
	Wrapper* pSecondaryPuffAreaTwo = nullptr;
	Wrapper* pSecondaryPuffIntensityOne = nullptr;
	Wrapper* pSecondaryPuffIntensityTwo = nullptr;

	Wrapper* pPrimaryTrailLength = nullptr;
	Wrapper* pPrimaryTrailDuration = nullptr;
	Wrapper* pPrimaryTrailIntensity = nullptr;
	Wrapper* pSecondaryTrailLength = nullptr;
	Wrapper* pSecondaryTrailDuration = nullptr;
	Wrapper* pSecondaryTrailIntensity = nullptr;

	Wrapper* pWindIntensity = nullptr;
	Wrapper* pWindWidth = nullptr;
};
#endif

inline constexpr int64_t kiMaxExplosionTrails = 8;
inline constexpr int64_t kiInvalidTrailType = 255;

enum class ExplosionFlags : uint8_t
{
	kDestroysSelf = 0x01,
	kYellow       = 0x02,
	kRed          = 0x04,
};
using ExplosionFlags_t = common::Flags<ExplosionFlags>;

struct ExplosionType
{
	// Controller type indices for fire-and-forget effects
	int64_t iPrimaryLightControllerTypeIndex = kiInvalidControllerType;
	int64_t iSecondaryLightControllerTypeIndex = kiInvalidControllerType;
	int64_t iPrimaryPuffControllerTypeIndex = kiInvalidControllerType;
	int64_t iSecondaryPuffControllerTypeIndex = kiInvalidControllerType;
	int64_t iTrailTypeIndex = kiInvalidTrailType;
	int64_t iWindRadialControllerTypeIndex = kiInvalidControllerType;

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
	static constexpr int64_t kiVersion = 1;

	// Register default explosion effect types (called from ForEachRegister)
	static void Register();

	static void AllocateAndCopy(ExplosionsInterpolate& rCurrent, const ExplosionsInterpolate& rPrevious);

	// Spawn caps the trail count at the fixed trail slots; reject any other count as a corrupt stream, since trail loops index those slots by it.
	static void PostRead(const ExplosionsInterpolate& rCurrent)
	{
		for (int64_t i = 0; i < rCurrent.iCount; ++i)
		{
			if (rCurrent.piTrailCounts[i] < 0 || rCurrent.piTrailCounts[i] > kiMaxExplosionTrails)
			{
				throw std::ios_base::failure("ExplosionsInterpolate piTrailCounts");
			}
		}
	}

#if defined(BT_CLIENT)
	static inline ExplosionTuning sTuning;
	static inline int64_t siTotalCount = 0;

	static inline int64_t siPrimaryLightControllerTypeIndex = kiInvalidControllerType;
	static inline int64_t siSecondaryLightControllerTypeIndex = kiInvalidControllerType;
	static inline int64_t siPrimaryPuffControllerTypeIndex = kiInvalidControllerType;
	static inline int64_t siSecondaryPuffControllerTypeIndex = kiInvalidControllerType;
	static inline int64_t siExplosionTrailTypeIndex = kiInvalidTrailType;
	static inline int64_t siWindRadialControllerTypeIndex = kiInvalidControllerType;
#endif // BT_CLIENT

	static void Update(game::FrameInterpolate& __restrict rCurrentFrameInterpolate, const game::Frame& __restrict rPreviousFrame);

	uint8_t* __restrict puiTypeIndices = nullptr;
	ExplosionFlags_t* __restrict pFlags = nullptr;
	SimulationTime* __restrict pStartTimes = nullptr;
	XMVECTOR* __restrict pVecPositions = nullptr;
	XMVECTOR* __restrict pVecDirections = nullptr;

	float* __restrict pfTimePercents = nullptr;

	// Trail state (8 separate arrays - pTrails[j] is array of all explosions' j-th trail)
	int32_t* __restrict piTrailCounts = nullptr;
#if defined(BT_CLIENT)
	smoke_trails_t* pTrails[kiMaxExplosionTrails] = {};
#endif
	std::chrono::duration<float>* pTrailTimes[kiMaxExplosionTrails] = {};
#if defined(BT_CLIENT)
	float* pfTrailIntensities[kiMaxExplosionTrails] = {};
	XMVECTOR* pVecTrailStartPositions[kiMaxExplosionTrails] = {};
	XMVECTOR* pVecTrailEndPositions[kiMaxExplosionTrails] = {};
#endif

	auto SharedMembers(this auto&& rSelf)
	{
		return std::tie(rSelf.puiTypeIndices, rSelf.pFlags, rSelf.pStartTimes, rSelf.pVecPositions, rSelf.pVecDirections, rSelf.pfTimePercents, rSelf.piTrailCounts, rSelf.pTrailTimes);
	}
#if defined(BT_CLIENT)
	auto ClientMembers(this auto&& rSelf)
	{
		return std::tie(rSelf.pTrails, rSelf.pfTrailIntensities, rSelf.pVecTrailStartPositions, rSelf.pVecTrailEndPositions);
	}
#endif // BT_CLIENT
	auto Members(this auto&& rSelf)
	{
#if defined(BT_CLIENT)
		return std::tuple_cat(rSelf.SharedMembers(), rSelf.ClientMembers());
#else
		return rSelf.SharedMembers();
#endif
	}
	auto PersistentMembers([[maybe_unused]] this auto&& rSelf)
	{
#if defined(BT_CLIENT)
		return std::tie(rSelf.pTrails);
#else
		return std::tie();
#endif
	}

	bool LogDifferences(const ExplosionsInterpolate& rOther) const;
};

struct ExplosionsPostRender : public Collection<ExplosionsPostRender>
{
	// Bump on any SOA layout change — feeds the Frame::kiVersion save/replay gate
	static constexpr int64_t kiVersion = 1;

	static void AllocateAndCopy(ExplosionsPostRender& rCurrent, const ExplosionsPostRender& rPrevious);

	static void Update(game::Frame& __restrict rFrame, const game::Frame& __restrict rPreviousFrame, const CellStaticData& rStaticData);

	static void Destroy(game::Frame& __restrict rFrame, const CellStaticData& rStaticData);

	auto Members([[maybe_unused]] this auto&& rSelf)
	{
		return std::tie();
	}

	bool LogDifferences(const ExplosionsPostRender& rOther) const;

	struct SpawnInfo
	{
		int64_t iTypeIndex = 0;
		XMVECTOR vecPosition = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
		XMVECTOR vecDirection = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
		ExplosionFlags_t flags {};
		int64_t iTrailCount = 0;
		float fTrailAngle = XM_2PI;
		int64_t iParticleCount = 0;
		float fParticleAngle = XM_2PI;
		float fSizePercent = 1.0f;
		float fSmokePercent = 1.0f;
		float fTimePercent = 1.0f;
	};

	// coordinate is the cell rSpawnInformation.vecPosition is local to.
	static bool Spawn(game::Frame& __restrict rFrame, GridCoord coordinate, SimulationTime currentTime, const SpawnInfo& rSpawnInformation);
};

extern template struct Collection<ExplosionsInterpolate>;
extern template struct Collection<ExplosionsPostRender>;

} // namespace engine
