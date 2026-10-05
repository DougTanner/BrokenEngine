#pragma once

#include "Frame/Alignments.h"

namespace engine
{

inline constexpr int64_t kiCollisionZonesX = 8;
inline constexpr int64_t kiCollisionZonesY = 8;
inline constexpr int64_t kiCollisionZonePreallocate = 2'048;
inline constexpr int64_t kiCollisionLayerPreallocate = 16;
inline constexpr int64_t kiCollisionLayerPairPreallocate = 16;
inline constexpr int64_t kiCollisionCandidatePreallocate = 512;
inline constexpr int64_t kiCollisionResultPreallocate = 1'024;
inline constexpr int64_t kiCollisionResultSpanPreallocate = 1'024;
inline constexpr int64_t kiCollisionLiveDataReserveBytes = 64 * 1'024 * 1'024;

enum class CollisionFlags : uint8_t
{
	kDestroyOnCollide = 0x01,
	kAlreadyCollided = 0x02,
};
using CollisionFlags_t = common::Flags<CollisionFlags>;

// Per-layer binding data (provided by collections each frame)
struct CollisionLayer
{
	// Pointers to collection-owned ephemeral static buffers
	const XMVECTOR* pVecStartPositions = nullptr;
	const XMVECTOR* pVecEndPositions = nullptr;
	const float* pfStartTimes = nullptr;       // Normalized absolute tick time
	const float* pfEndTimes = nullptr;         // Normalized absolute tick time
	const float* pfMaxTimes = nullptr;         // Optional exclusive entity-collision cutoff
	const float* pfRadii = nullptr;
	const float* pfDamages = nullptr;
	CollisionFlags_t* pFlags = nullptr;       // Per-object flags (read/write for kAlreadyCollided)
	const XMVECTOR* pVecVelocities = nullptr; // Optional: velocity/direction per object
	int64_t iCount = 0;
	bool bSweptTest = false;                  // Sweep every pair involving this layer

	uint16_t uiCategory = 0;
	uint16_t uiCollidesWith = 0;

	const AlignmentIdentifier* pAlignments = nullptr;
};

struct CollisionResult
{
	int64_t iOtherIndex = 0;
	int64_t iOtherLayerIndex = 0;             // Which layer (index into sLayers)
	uint16_t uiOtherCategory = 0;
	float fDamageReceived = 0.0f;
	float fTimeOfImpact = 0.0f;
	XMVECTOR vecContactPoint {};
	XMVECTOR vecSelfPosition {};
	XMVECTOR vecOtherVelocity {};  // Velocity of the colliding object (zero if not provided)
};

// Result span for a single object (offset into sResultEntries + count)
struct CollisionResultSpan
{
	int64_t iOffset = -1;  // -1 means no collision
	int64_t iCount = 0;
};

struct ZoneRange;
struct LayerPairZones;
struct CollisionCandidate;

class Collision
{
public:

	static inline thread_local int64_t siLayerCount = 0;
	static inline thread_local common::StableVector<CollisionResultSpan> sResultSpans = common::StableVector<CollisionResultSpan>(kiCollisionLiveDataReserveBytes / static_cast<int64_t>(sizeof(CollisionResultSpan)));
	static inline thread_local int64_t sLayerBaseOffsets[kiCollisionLayerPreallocate] {};

	// Per-frame layer registration and binding (called in PreCollision phase)
	static int64_t AddLayer(const CollisionLayer& rLayer);

	// Frame drives collision detection; layer category masks and alignment pairs filter collisions.
	static void Collide(const Alignments& rAlignments, FXMVECTOR vecArea);

	static std::span<const CollisionResult> GetCollisions(int64_t iLayerIndex, int64_t iIndex);


private:

	static void SetupZones(FXMVECTOR vecArea);
	static ZoneRange CalculateZoneRange(float fMinimumX, float fMaximumX, float fMinimumY, float fMaximumY, float fRadius);
	static ZoneRange CalculateObjectZoneRange(const CollisionLayer& rLayer, int64_t iIndex, bool bSweptPair);
	static void InsertObjectIntoZones(LayerPairZones& rPairZones, int64_t iIndex, const ZoneRange& rRange, bool bIsLayerA);
	static void InsertLayerObjectsIntoZones(LayerPairZones& rPairZones, const CollisionLayer& rLayer, bool bIsLayerA, bool bSweptPair);
	static void CollideLayerPair(const Alignments& rAlignments, const LayerPairZones& rPairZones);
	static void CommitCandidate(const CollisionCandidate& rCandidate);
	static void AllocateResultStorage();

	// thread_local: each Dispatch worker and reconcile thread gets its own copy
	static inline thread_local float sfAreaMinimumX = 0.0f;
	static inline thread_local float sfAreaMinimumY = 0.0f;
	static inline thread_local float sfZoneWidth = 0.0f;
	static inline thread_local float sfZoneHeight = 0.0f;

	// Default-constructed (no allocation): thread_local constructors run during
	// mi_process_init before the allocator is ready, so pre-allocation would crash.
	static inline thread_local std::vector<CollisionLayer> sLayers;

	static thread_local common::StableVector<LayerPairZones> sLayerPairZones;
	static inline thread_local int64_t siLayerPairCount = 0;

	// Each result and generation buffer has a fixed 64 MiB reservation ceiling; live counts determine committed storage.
	static inline thread_local common::StableVector<CollisionResult> sResultEntries = common::StableVector<CollisionResult>(kiCollisionLiveDataReserveBytes / static_cast<int64_t>(sizeof(CollisionResult)));
	static inline thread_local int64_t siResultSpanCount = 0;
	static inline thread_local common::StableVector<uint32_t> sTestedBGeneration = common::StableVector<uint32_t>(kiCollisionLiveDataReserveBytes / static_cast<int64_t>(sizeof(uint32_t)));
	static inline thread_local uint32_t suiTestedBCurrentGeneration = 0;
};

} // namespace engine
