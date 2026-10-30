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

struct CellStaticData;
struct SegmentHit;

// Per-layer binding data (provided by collections each frame)
struct CollisionLayer
{
	// Collection-owned columns must stay valid from AddLayer through Collide
	const XMVECTOR* pVecStartPositions = nullptr;
	const XMVECTOR* pVecEndPositions = nullptr;
	// AddLayer points these at Collision-owned columns; callers leave them null
	const float* pfMaxTimes = nullptr;         // Exclusive entity-collision cutoff
	CollisionFlags_t* pFlags = nullptr;       // Per-object flags (read/write for kAlreadyCollided)
	const XMVECTOR* pVecVelocities = nullptr; // Velocity/direction per object
	int64_t iCount = 0;
	float fRadius = 0.0f;
	float fDamage = 0.0f;
	bool bSweptTest = false;                  // Sweep every pair involving this layer

	uint16_t uiCategory = 0;
	uint16_t uiCollidesWith = 0;

	const AlignmentIdentifier* pAlignments = nullptr;
	const CellStaticData* pTerrainStaticData = nullptr; // When set, each row's segment is also traced against this cell's terrain
};

enum class CollisionOutcomeType : uint8_t
{
	kNone,
	kEntity,
	kTerrain,
	kCellExit,
};

struct CollisionOutcome
{
	CollisionOutcomeType eType = CollisionOutcomeType::kNone;
	XMVECTOR vecTerrainPosition {}; // Terrain hit position; set only for kTerrain
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
	XMVECTOR vecOtherVelocity {};  // Velocity of the colliding object
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

	// Per-frame layer registration and binding (called in PreCollision phase). Traces each row's start-to-end segment
	// against the cell exit, and against terrain when pTerrainStaticData is set, and stores the hits and the row's
	// exclusive maximum time; the rows stay valid through the owning collection's PostCollision.
	static int64_t AddLayer(const CollisionLayer& rLayer);

	// The caller writes every row's flags here after AddLayer and before Collide.
	static std::span<CollisionFlags_t> GetLayerFlags(int64_t iLayerIndex);

	// Frame drives collision detection; layer category masks and alignment pairs filter collisions.
	static void Collide(const Alignments& rAlignments);

	static std::span<const CollisionResult> GetCollisions(int64_t iLayerIndex, int64_t iIndex);

	// Valid after Collide. Entity contacts win, since the maximum time already excludes contacts at or after either
	// cutoff; terrain wins an exact-time tie with the cell exit.
	static CollisionOutcome GetOutcome(int64_t iLayerIndex, int64_t iIndex);


private:

	// Every per-row column shares one element ceiling, so no column limits the row count below another.
	static constexpr int64_t kiRowReserveCount = kiCollisionLiveDataReserveBytes / static_cast<int64_t>(sizeof(CollisionResultSpan));

	static void SetupZones();
	static ZoneRange CalculateZoneRange(float fMinimumX, float fMaximumX, float fMinimumY, float fMaximumY, float fRadius);
	static ZoneRange CalculateObjectZoneRange(const CollisionLayer& rLayer, int64_t iIndex, bool bSweptPair);
	static void InsertObjectIntoZones(LayerPairZones& rPairZones, int64_t iIndex, const ZoneRange& rRange);
	static void InsertLayerObjectsIntoZones(LayerPairZones& rPairZones, const CollisionLayer& rLayer, bool bSweptPair);
	static void CollideLayerPair(const Alignments& rAlignments, const LayerPairZones& rPairZones);
	static void CommitCandidate(const CollisionCandidate& rCandidate);
	static void AllocateResultStorage();

	// Default-constructed (no allocation): thread_local constructors run during
	// mi_process_init before the allocator is ready, so pre-allocation would crash.
	static inline thread_local std::vector<CollisionLayer> sLayers;

	static thread_local common::StableVector<LayerPairZones> sLayerPairZones;
	static inline thread_local int64_t siLayerPairCount = 0;

	// Each result and generation buffer has a fixed 64 MiB reservation ceiling; live counts determine committed storage.
	static inline thread_local common::StableVector<CollisionResult> sResultEntries = common::StableVector<CollisionResult>(kiCollisionLiveDataReserveBytes / static_cast<int64_t>(sizeof(CollisionResult)));
	static inline thread_local common::StableVector<CollisionResultSpan> sResultSpans = common::StableVector<CollisionResultSpan>(kiRowReserveCount);
	static inline thread_local int64_t spiLayerBaseOffsets[kiCollisionLayerPreallocate] {};
	static inline thread_local int64_t siResultSpanCount = 0;
	static inline thread_local common::StableVector<uint32_t> sTestedBGeneration = common::StableVector<uint32_t>(kiCollisionLiveDataReserveBytes / static_cast<int64_t>(sizeof(uint32_t)));
	static inline thread_local uint32_t suiTestedBCurrentGeneration = 0;

	// Per-row columns indexed like sResultSpans; AddLayer fills them except sFlags, which callers write through
	// GetLayerFlags. sLayers' max-time and flag pointers point into the first two, so they only grow, never move.
	static inline thread_local common::StableVector<float> sMaximumTimes = common::StableVector<float>(kiRowReserveCount);
	static inline thread_local common::StableVector<CollisionFlags_t> sFlags = common::StableVector<CollisionFlags_t>(kiRowReserveCount);
	static thread_local common::StableVector<SegmentHit> sTerrainHits;
	static thread_local common::StableVector<SegmentHit> sBoundaryHits;
};

} // namespace engine
