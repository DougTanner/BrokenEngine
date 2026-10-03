#include "Collision.h"

#include "Frame/Frame.h"
#include "Profile/ProfileManager.h"

namespace engine
{

// Accepted collision result retained after globally sorting candidate events.
struct PendingCollisionResult
{
	int64_t iLayerIndex = 0;
	int64_t iObjectIndex = 0;
	CollisionResult result {};
};

// Candidate event collected across every active layer pair before any collision flags mutate.
struct CollisionCandidate
{
	float fTimeOfImpact = 0.0f;
	int64_t iLayerA = 0;
	int64_t iObjectA = 0;
	int64_t iLayerB = 0;
	int64_t iObjectB = 0;
	XMVECTOR vecContactPoint {};
	XMVECTOR vecPositionA {};
	XMVECTOR vecPositionB {};
};

// Values shared by every object pair generated for one layer pair.
struct CollisionPairContext
{
	const Alignments& rAlignments;
	const CollisionLayer& rLayerA;
	const CollisionLayer& rLayerB;
	int64_t iLayerA = 0;
	int64_t iLayerB = 0;
	bool bSweptPair = false;
};

struct CollisionEventScratch
{
	// Reserved counts live here rather than at the growth sites because the owner is default-constructed
	common::StableVector<CollisionCandidate> candidates = common::StableVector<CollisionCandidate>(64 * kiCollisionCandidatePreallocate);
	common::StableVector<PendingCollisionResult> pendingResults = common::StableVector<PendingCollisionResult>(64 * kiCollisionResultPreallocate);
	int64_t iCandidateCount = 0;
	int64_t iPendingResultCount = 0;
};

static CollisionEventScratch& GetCollisionEventScratch()
{
	// Function-local TLS defers construction until first use; StableVector constructors record reservation sizes without allocating.
	static thread_local CollisionEventScratch sScratch;
	return sScratch;
}

struct ZoneRange
{
	int32_t iStartX = 0;
	int32_t iEndX = 0;
	int32_t iStartY = 0;
	int32_t iEndY = 0;
};

struct ZonePair
{
	// Reserved counts live here rather than at the growth sites because the owner is default-constructed
	common::StableVector<int64_t> indicesA = common::StableVector<int64_t>(4 * kiCollisionZonePreallocate);  // Object indices from layer A
	common::StableVector<int64_t> indicesB = common::StableVector<int64_t>(4 * kiCollisionZonePreallocate);  // Object indices from layer B
	int64_t iCountA = 0;
	int64_t iCountB = 0;
};

struct LayerPairZones
{
	int64_t iLayerA = 0;
	int64_t iLayerB = 0;
	ZonePair zones[kiCollisionZonesY][kiCollisionZonesX];

	LayerPairZones()
	{
		for (ZonePair (&rRow)[kiCollisionZonesX] : zones)
		{
			for (ZonePair& rZonePair : rRow)
			{
				rZonePair.indicesA.Resize(kiCollisionZonePreallocate);
				rZonePair.indicesB.Resize(kiCollisionZonePreallocate);
			}
		}
	}
};

// StableVector construction only records the reserved count, so these reserve no address space and make
// no OS call until their first Resize; growth then commits more of that reservation without moving.
thread_local common::StableVector<LayerPairZones> Collision::sLayerPairZones(64 * kiCollisionLayerPairPreallocate);

using enum CollisionFlags;

int64_t Collision::AddLayer(const CollisionLayer& rLayer)
{
	if (sLayers.empty())
	{
		ScopedSuppressAllocationTracking suppress;
		sLayers.resize(kiCollisionLayerPreallocate);
	}

	int64_t iLayerIndex = siLayerCount;
	// Growing past the pre-allocation would overrun the fixed sLayerBaseOffsets array; layer count is
	// compile-time-determined by the registering collections, so overflow is a developer error — fail loud
	if (siLayerCount >= std::ssize(sLayers))
	{
		LOG(kDefault, kError, "Collision: sLayers overflow (count: {}, capacity: {}). Increase kiCollisionLayerPreallocate in Collision.h", siLayerCount, std::ssize(sLayers));
		ASSERT(false);
	}
	sLayers.at(iLayerIndex) = rLayer;
	++siLayerCount;
	return iLayerIndex;
}

ZoneRange Collision::CalculateZoneRange(float fMinimumX, float fMaximumX, float fMinimumY, float fMaximumY, float fRadius)
{
	return
	{
		.iStartX = std::clamp(static_cast<int32_t>((fMinimumX - fRadius - sfAreaMinimumX) / sfZoneWidth), 0, kiCollisionZonesX - 1),
		.iEndX = std::clamp(static_cast<int32_t>((fMaximumX + fRadius - sfAreaMinimumX) / sfZoneWidth), 0, kiCollisionZonesX - 1),
		.iStartY = std::clamp(static_cast<int32_t>((fMinimumY - fRadius - sfAreaMinimumY) / sfZoneHeight), 0, kiCollisionZonesY - 1),
		.iEndY = std::clamp(static_cast<int32_t>((fMaximumY + fRadius - sfAreaMinimumY) / sfZoneHeight), 0, kiCollisionZonesY - 1),
	};
}

ZoneRange Collision::CalculateObjectZoneRange(const CollisionLayer& rLayer, int64_t iIndex, bool bSweptPair)
{
	float fMinimumX = 0.0f, fMaximumX = 0.0f, fMinimumY = 0.0f, fMaximumY = 0.0f;
	if (bSweptPair)
	{
		XMVECTOR vecMinimum = XMVectorMin(rLayer.pVecStartPositions[iIndex], rLayer.pVecEndPositions[iIndex]);
		XMVECTOR vecMaximum = XMVectorMax(rLayer.pVecStartPositions[iIndex], rLayer.pVecEndPositions[iIndex]);
		XMFLOAT4A f4Minimum {};
		XMFLOAT4A f4Maximum {};
		XMStoreFloat4A(&f4Minimum, vecMinimum);
		XMStoreFloat4A(&f4Maximum, vecMaximum);
		fMinimumX = f4Minimum.x;
		fMaximumX = f4Maximum.x;
		fMinimumY = f4Minimum.y;
		fMaximumY = f4Maximum.y;
	}
	else
	{
		XMFLOAT4A f4Position {};
		XMStoreFloat4A(&f4Position, rLayer.pVecEndPositions[iIndex]);
		fMinimumX = f4Position.x;
		fMaximumX = f4Position.x;
		fMinimumY = f4Position.y;
		fMaximumY = f4Position.y;
	}
	return CalculateZoneRange(fMinimumX, fMaximumX, fMinimumY, fMaximumY, rLayer.pfRadii[iIndex]);
}

void Collision::InsertObjectIntoZones(LayerPairZones& rPairZones, int64_t iIndex, const ZoneRange& rRange, bool bIsLayerA)
{
	for (int64_t i = rRange.iStartY; i <= rRange.iEndY; ++i)
	{
		for (int64_t j = rRange.iStartX; j <= rRange.iEndX; ++j)
		{
			ZonePair& rZonePair = rPairZones.zones[i][j];
			common::StableVector<int64_t>& rIndices = bIsLayerA ? rZonePair.indicesA : rZonePair.indicesB;
			int64_t& riCount = bIsLayerA ? rZonePair.iCountA : rZonePair.iCountB;
			if (riCount >= rIndices.Size())
			{
				// sLayers lookups feed only this rare overflow LOG, so fetch them here, not every zone iteration
				const CollisionLayer& rLayerA = sLayers.at(rPairZones.iLayerA);
				const CollisionLayer& rLayerB = sLayers.at(rPairZones.iLayerB);
				LOG(kDefault, kWarning, "Collision: ZonePair.indices{} overflow (count: {}, capacity: {}) pair A={}(cat={},total={}) B={}(cat={},total={}) zone=({},{}). Increase kiCollisionZonePreallocate in Collision.h", bIsLayerA ? 'A' : 'B', riCount, rIndices.Size(), rPairZones.iLayerA, rLayerA.uiCategory, rLayerA.iCount, rPairZones.iLayerB, rLayerB.uiCategory, rLayerB.iCount, j, i);
				DEBUG_BREAK();
				rIndices.Resize(riCount * 2);
			}
			rIndices[riCount] = iIndex;
			++riCount;
		}
	}
}

void Collision::InsertLayerObjectsIntoZones(LayerPairZones& rPairZones, const CollisionLayer& rLayer, bool bIsLayerA, bool bSweptPair)
{
	for (int64_t i = 0; i < rLayer.iCount; ++i)
	{
		if (rLayer.pFlags[i] & kAlreadyCollided)
		{
			continue;
		}

		ZoneRange range = CalculateObjectZoneRange(rLayer, i, bSweptPair);
		InsertObjectIntoZones(rPairZones, i, range, bIsLayerA);
	}
}

static XMVECTOR XM_CALLCONV PositionAtTime(const CollisionLayer& rLayer, int64_t iIndex, float fTime)
{
	float fStartTime = rLayer.pfStartTimes[iIndex];
	float fEndTime = rLayer.pfEndTimes[iIndex];
	if (fEndTime <= fStartTime)
	{
		return rLayer.pVecStartPositions[iIndex];
	}

	float fPercent = (fTime - fStartTime) / (fEndTime - fStartTime);
	return XMVectorLerp(rLayer.pVecStartPositions[iIndex], rLayer.pVecEndPositions[iIndex], fPercent);
}

static bool XM_CALLCONV SweptSphereTest(FXMVECTOR vecStartA, FXMVECTOR vecEndA, float fRadiusA, FXMVECTOR vecStartB, GXMVECTOR vecEndB, float fRadiusB, float fStartTime, float fEndTime, float& rfTimeOfImpact)
{
	XMVECTOR vecRelativeStart = XMVectorSubtract(vecStartB, vecStartA);
	XMVECTOR vecRelativeDelta = XMVectorSubtract(XMVectorSubtract(vecEndB, vecStartB), XMVectorSubtract(vecEndA, vecStartA));
	float fCombinedRadius = fRadiusA + fRadiusB;

	float fQuadraticConstant = XMVectorGetX(XMVector3Dot(vecRelativeStart, vecRelativeStart)) - fCombinedRadius * fCombinedRadius;
	if (fQuadraticConstant <= 0.0f)
	{
		rfTimeOfImpact = fStartTime;
		return true;
	}

	float fQuadraticLinear = 2.0f * XMVectorGetX(XMVector3Dot(vecRelativeStart, vecRelativeDelta));
	if (fQuadraticLinear >= 0.0f)
	{
		// Moving apart
		return false;
	}

	float fQuadraticSquared = XMVectorGetX(XMVector3Dot(vecRelativeDelta, vecRelativeDelta));
	if (fQuadraticSquared < 1.0e-8f)
	{
		// No relative motion
		return false;
	}

	float fDiscriminant = fQuadraticLinear * fQuadraticLinear - 4.0f * fQuadraticSquared * fQuadraticConstant;
	if (fDiscriminant < 0.0f)
	{
		return false;
	}

	// Earliest contact time within the common absolute tick interval
	float fNormalizedTime = (-fQuadraticLinear - std::sqrt(fDiscriminant)) / (2.0f * fQuadraticSquared);
	if (fNormalizedTime >= 0.0f && fNormalizedTime <= 1.0f)
	{
		rfTimeOfImpact = fStartTime + fNormalizedTime * (fEndTime - fStartTime);
		return true;
	}
	return false;
}

void Collision::SetupZones(FXMVECTOR vecArea)
{
	if (sLayerPairZones.Size() == 0)
	{
		sLayerPairZones.Resize(kiCollisionLayerPairPreallocate);
	}

	// Compute zone dimensions from vecArea (x=minX, y=maxY, z=maxX, w=minY)
	XMFLOAT4A f4Area {};
	XMStoreFloat4A(&f4Area, vecArea);
	sfAreaMinimumX = f4Area.x;
	sfAreaMinimumY = f4Area.w;
	sfZoneWidth = (f4Area.z - f4Area.x) / kiCollisionZonesX;
	sfZoneHeight = (f4Area.y - f4Area.w) / kiCollisionZonesY;

	// Reset counts while retaining zone storage.
	for (int64_t i = 0; i < siLayerPairCount; ++i)
	{
		LayerPairZones& rPairZones = sLayerPairZones[i];
		for (ZonePair (&rRow)[kiCollisionZonesX] : rPairZones.zones)
		{
			for (ZonePair& rZonePair : rRow)
			{
				rZonePair.iCountA = 0;
				rZonePair.iCountB = 0;
			}
		}
	}

	// Same-layer collision is unsupported; each layer's collision mask must exclude its own category.
	for (int64_t i = 0; i < siLayerCount; ++i)
	{
		ASSERT((sLayers.at(static_cast<size_t>(i)).uiCollidesWith & sLayers.at(static_cast<size_t>(i)).uiCategory) == 0 && "Same-layer collision not implemented");
	}

	int64_t iPairIndex = 0;
	for (int64_t i = 0; i < siLayerCount; ++i)
	{
		for (int64_t j = i + 1; j < siLayerCount; ++j)
		{
			bool bACollidesWithB = (sLayers.at(static_cast<size_t>(i)).uiCollidesWith & sLayers.at(static_cast<size_t>(j)).uiCategory) != 0;
			bool bBCollidesWithA = (sLayers.at(static_cast<size_t>(j)).uiCollidesWith & sLayers.at(static_cast<size_t>(i)).uiCategory) != 0;

			ASSERT(bACollidesWithB == bBCollidesWithA && "Collision masks must be bi-directional");

			if (!bACollidesWithB)
			{
				continue;
			}

			if (iPairIndex >= sLayerPairZones.Size())
			{
				LOG(kDefault, kWarning, "Collision: sLayerPairZones overflow (index: {}, capacity: {}). Increase kiCollisionLayerPairPreallocate in Collision.h", iPairIndex, sLayerPairZones.Size());
				DEBUG_BREAK();
				sLayerPairZones.Resize(iPairIndex * 2);
			}
			LayerPairZones& rPairZones = sLayerPairZones[iPairIndex];
			rPairZones.iLayerA = i;
			rPairZones.iLayerB = j;

			const CollisionLayer& rLayerA = sLayers.at(static_cast<size_t>(i));
			const CollisionLayer& rLayerB = sLayers.at(static_cast<size_t>(j));
			bool bSweptPair = rLayerA.bSweptTest || rLayerB.bSweptTest;

			// Insert both layers' objects into the zone grid (skip already-collided destroy-on-collide objects)
			InsertLayerObjectsIntoZones(rPairZones, rLayerA, true, bSweptPair);
			InsertLayerObjectsIntoZones(rPairZones, rLayerB, false, bSweptPair);

			++iPairIndex;
		}
	}
	siLayerPairCount = iPairIndex;
}

void Collision::Collide(const Alignments& rAlignments, FXMVECTOR vecArea)
{
	CollisionEventScratch& rScratch = GetCollisionEventScratch();
	ScopedCpuProfile scopedCpuProfile(game::kCpuTimerPostRenderCollide);

	// Build zone acceleration structure (includes layer pair filtering and same-layer collision assert)
	SetupZones(vecArea);

	// Collect every candidate before mutating collision flags so traversal order cannot select winners.
	if (rScratch.candidates.Size() == 0)
	{
		rScratch.candidates.Resize(kiCollisionCandidatePreallocate);
	}
	rScratch.iCandidateCount = 0;
	if (rScratch.pendingResults.Size() == 0)
	{
		rScratch.pendingResults.Resize(kiCollisionResultPreallocate);
	}
	rScratch.iPendingResultCount = 0;

	for (int64_t i = 0; i < siLayerPairCount; ++i)
	{
		CollideLayerPair(rAlignments, sLayerPairZones[i]);
	}

	// Client and server share layer-registration order; sorting globally by time and layer/object keys gives equal-time events a reproducible total order.
	std::sort(rScratch.candidates.Data(), rScratch.candidates.Data() + rScratch.iCandidateCount, [](const CollisionCandidate& rLeftCandidate, const CollisionCandidate& rRightCandidate)
	{
		return std::tie(rLeftCandidate.fTimeOfImpact, rLeftCandidate.iLayerA, rLeftCandidate.iObjectA, rLeftCandidate.iLayerB, rLeftCandidate.iObjectB) < std::tie(rRightCandidate.fTimeOfImpact, rRightCandidate.iLayerA, rRightCandidate.iObjectA, rRightCandidate.iLayerB, rRightCandidate.iObjectB);
	});

	// Commit accepted candidates into retained pending-result scratch.
	for (int64_t i = 0; i < rScratch.iCandidateCount; ++i)
	{
		CommitCandidate(rScratch.candidates[i]);
	}

	AllocateResultStorage();

	for (int64_t i = 0; i < rScratch.iPendingResultCount; ++i)
	{
		const PendingCollisionResult& rPending = rScratch.pendingResults[i];
		int64_t iSpanIndex = sLayerBaseOffsets[static_cast<size_t>(rPending.iLayerIndex)] + rPending.iObjectIndex;
		++sResultSpans[iSpanIndex].iCount;
	}

	// Counts become fill cursors after offsets are assigned.
	int64_t iTotalResults = 0;
	for (int64_t i = 0; i < siResultSpanCount; ++i)
	{
		CollisionResultSpan& rSpan = sResultSpans[i];
		if (rSpan.iCount > 0)
		{
			rSpan.iOffset = iTotalResults;
			iTotalResults += rSpan.iCount;
			rSpan.iCount = 0;
		}
	}

	if (iTotalResults > 0)
	{
		if (sResultEntries.Size() == 0)
		{
			sResultEntries.Resize(std::max(kiCollisionResultPreallocate, iTotalResults));
		}
		else if (iTotalResults > sResultEntries.Size())
		{
			sResultEntries.Resize(iTotalResults);
		}
	}
	for (int64_t i = 0; i < rScratch.iPendingResultCount; ++i)
	{
		const PendingCollisionResult& rPending = rScratch.pendingResults[i];
		int64_t iSpanIndex = sLayerBaseOffsets[static_cast<size_t>(rPending.iLayerIndex)] + rPending.iObjectIndex;
		CollisionResultSpan& rSpan = sResultSpans[iSpanIndex];
		sResultEntries[rSpan.iOffset + rSpan.iCount] = rPending.result;
		++rSpan.iCount;
	}

}

void Collision::AllocateResultStorage()
{
	int64_t iTotal = 0;
	for (int64_t i = 0; i < siLayerCount; ++i)
	{
		sLayerBaseOffsets[static_cast<size_t>(i)] = iTotal;
		iTotal += sLayers.at(static_cast<size_t>(i)).iCount;
	}
	siResultSpanCount = iTotal;

	if (sResultSpans.Size() == 0)
	{
		sResultSpans.Resize(std::max(kiCollisionResultSpanPreallocate, iTotal));
	}
	else if (iTotal > sResultSpans.Size())
	{
		sResultSpans.Resize(iTotal);
	}

	for (int64_t i = 0; i < iTotal; ++i)
	{
		sResultSpans[i] = {.iOffset = -1, .iCount = 0};
	}
}

// Record one side of a bidirectional collision: self receives the other side's damage and velocity.
static void XM_CALLCONV RecordCollision(const CollisionCandidate& rCandidate, FXMVECTOR vecSelfPosition, int64_t iSelf, int64_t iSelfLayer, const CollisionLayer& rOtherLayer, int64_t iOther, int64_t iOtherLayer)
{
	CollisionEventScratch& rScratch = GetCollisionEventScratch();
	if (rScratch.iPendingResultCount >= rScratch.pendingResults.Size()) [[unlikely]]
	{
		LOG(kDefault, kWarning, "Collision: pendingResults overflow (count: {}, capacity: {}). Increase kiCollisionResultPreallocate in Collision.h", rScratch.iPendingResultCount, rScratch.pendingResults.Size());
		DEBUG_BREAK();
		rScratch.pendingResults.Resize(rScratch.iPendingResultCount * 2);
	}
	rScratch.pendingResults[rScratch.iPendingResultCount] =
	{
		.iLayerIndex = iSelfLayer,
		.iObjectIndex = iSelf,
		.result =
		{
			.iOtherIndex = iOther,
			.iOtherLayerIndex = iOtherLayer,
			.uiOtherCategory = rOtherLayer.uiCategory,
			.fDamageReceived = rOtherLayer.pfDamages[iOther],
			.fTimeOfImpact = rCandidate.fTimeOfImpact,
			.vecContactPoint = rCandidate.vecContactPoint,
			.vecSelfPosition = vecSelfPosition,
			.vecOtherVelocity = rOtherLayer.pVecVelocities != nullptr ? rOtherLayer.pVecVelocities[iOther] : XMVectorZero(),
		},
	};
	++rScratch.iPendingResultCount;
}

void Collision::CommitCandidate(const CollisionCandidate& rCandidate)
{
	CollisionLayer& rLayerA = Collision::sLayers.at(rCandidate.iLayerA);
	CollisionLayer& rLayerB = Collision::sLayers.at(rCandidate.iLayerB);
	if ((rLayerA.pFlags[rCandidate.iObjectA] & kAlreadyCollided) || (rLayerB.pFlags[rCandidate.iObjectB] & kAlreadyCollided))
	{
		return;
	}

	RecordCollision(rCandidate, rCandidate.vecPositionA, rCandidate.iObjectA, rCandidate.iLayerA, rLayerB, rCandidate.iObjectB, rCandidate.iLayerB);
	RecordCollision(rCandidate, rCandidate.vecPositionB, rCandidate.iObjectB, rCandidate.iLayerB, rLayerA, rCandidate.iObjectA, rCandidate.iLayerA);

	// Mutate flags only after both result sides are committed.
	if (rLayerA.pFlags[rCandidate.iObjectA] & kDestroyOnCollide)
	{
		rLayerA.pFlags[rCandidate.iObjectA].Set(kAlreadyCollided);
	}
	if (rLayerB.pFlags[rCandidate.iObjectB] & kDestroyOnCollide)
	{
		rLayerB.pFlags[rCandidate.iObjectB].Set(kAlreadyCollided);
	}
}

// The caller deduplicates each B object for the current A object.
static void TestAndCollectPair(const CollisionPairContext& rPairContext, int64_t i, int64_t j)
{
	const Alignments& rAlignments = rPairContext.rAlignments;
	const CollisionLayer& rLayerA = rPairContext.rLayerA;
	const CollisionLayer& rLayerB = rPairContext.rLayerB;
	int64_t iLayerA = rPairContext.iLayerA;
	int64_t iLayerB = rPairContext.iLayerB;
	bool bSweptPair = rPairContext.bSweptPair;

	if (!rAlignments.CanCollide(rLayerA.pAlignments[i], rLayerB.pAlignments[j]))
	{
		return;
	}

	// Pre-existing inactive entries never generate candidates. Newly accepted destroy collisions are
	// resolved later after the global sort.
	if ((rLayerA.pFlags[i] & kAlreadyCollided) || (rLayerB.pFlags[j] & kAlreadyCollided))
	{
		return;
	}

	float fStartTime = std::max(rLayerA.pfStartTimes[i], rLayerB.pfStartTimes[j]);
	float fEndTime = std::min(rLayerA.pfEndTimes[i], rLayerB.pfEndTimes[j]);
	if (fStartTime > fEndTime)
	{
		return;
	}

	XMVECTOR vecStartA = PositionAtTime(rLayerA, i, fStartTime);
	XMVECTOR vecEndA = PositionAtTime(rLayerA, i, fEndTime);
	XMVECTOR vecStartB = PositionAtTime(rLayerB, j, fStartTime);
	XMVECTOR vecEndB = PositionAtTime(rLayerB, j, fEndTime);
	float fRadiusA = rLayerA.pfRadii[i];
	float fRadiusB = rLayerB.pfRadii[j];

	bool bCollided = false;
	float fTimeOfImpact = fEndTime;
	XMVECTOR vecImpactA = vecEndA;
	XMVECTOR vecImpactB = vecEndB;

	if (bSweptPair)
	{
		if (SweptSphereTest(vecStartA, vecEndA, fRadiusA, vecStartB, vecEndB, fRadiusB, fStartTime, fEndTime, fTimeOfImpact))
		{
			vecImpactA = PositionAtTime(rLayerA, i, fTimeOfImpact);
			vecImpactB = PositionAtTime(rLayerB, j, fTimeOfImpact);
			bCollided = true;
		}
	}

	if (!bCollided)
	{
		XMVECTOR vecDifference = XMVectorSubtract(vecImpactA, vecImpactB);
		float fDistanceSquared = XMVectorGetX(XMVector3LengthSq(vecDifference));
		float fCombinedRadius = fRadiusA + fRadiusB;

		if (fDistanceSquared > fCombinedRadius * fCombinedRadius)
		{
			return;
		}

	}

	float fMaximumTimeA = rLayerA.pfMaxTimes != nullptr ? rLayerA.pfMaxTimes[i] : std::numeric_limits<float>::max();
	float fMaximumTimeB = rLayerB.pfMaxTimes != nullptr ? rLayerB.pfMaxTimes[j] : std::numeric_limits<float>::max();
	if (fTimeOfImpact >= fMaximumTimeA || fTimeOfImpact >= fMaximumTimeB)
	{
		return;
	}

	XMVECTOR vecDifference = XMVectorSubtract(vecImpactA, vecImpactB);
	float fDistance = XMVectorGetX(XMVector3Length(vecDifference));
	XMVECTOR vecContactPoint = vecImpactA;
	if (fDistance > 0.0f)
	{
		vecContactPoint = XMVectorSubtract(vecImpactA, XMVectorScale(vecDifference, fRadiusA / fDistance));
	}

	CollisionEventScratch& rScratch = GetCollisionEventScratch();
	if (rScratch.iCandidateCount >= rScratch.candidates.Size()) [[unlikely]]
	{
		LOG(kDefault, kWarning, "Collision: candidates overflow (count: {}, capacity: {}). Increase kiCollisionCandidatePreallocate in Collision.h", rScratch.iCandidateCount, rScratch.candidates.Size());
		DEBUG_BREAK();
		rScratch.candidates.Resize(rScratch.iCandidateCount * 2);
	}
	rScratch.candidates[rScratch.iCandidateCount] =
	{
		.fTimeOfImpact = fTimeOfImpact,
		.iLayerA = iLayerA,
		.iObjectA = i,
		.iLayerB = iLayerB,
		.iObjectB = j,
		.vecContactPoint = vecContactPoint,
		.vecPositionA = vecImpactA,
		.vecPositionB = vecImpactB,
	};
	++rScratch.iCandidateCount;
}

void Collision::CollideLayerPair(const Alignments& rAlignments, const LayerPairZones& rPairZones)
{
	int64_t iLayerA = rPairZones.iLayerA;
	int64_t iLayerB = rPairZones.iLayerB;

	const CollisionLayer& rLayerA = sLayers.at(iLayerA);
	const CollisionLayer& rLayerB = sLayers.at(iLayerB);

	bool bSweptPair = rLayerA.bSweptTest || rLayerB.bSweptTest;
	CollisionPairContext pairContext
	{
		.rAlignments = rAlignments,
		.rLayerA = rLayerA,
		.rLayerB = rLayerB,
		.iLayerA = iLayerA,
		.iLayerB = iLayerB,
		.bSweptPair = bSweptPair,
	};

	// Track tested B objects to avoid duplicates from multi-zone presence (generation counter)
	if (rLayerB.iCount > sTestedBGeneration.Size())
	{
		sTestedBGeneration.Resize(rLayerB.iCount);
	}

	for (int64_t i = 0; i < rLayerA.iCount; ++i)
	{
		if (rLayerA.pFlags[i] & kAlreadyCollided)
		{
			continue;
		}

		// Calculate A's zone range with clamping (expand to swept AABB if swept)
		ZoneRange range = CalculateObjectZoneRange(rLayerA, i, bSweptPair);

		// A generation identifies B objects tested for one A object; clear stored generations only on wraparound.
		++suiTestedBCurrentGeneration;
		if (suiTestedBCurrentGeneration == 0)
		{
			std::memset(sTestedBGeneration.Data(), 0, static_cast<size_t>(sTestedBGeneration.Size()) * sizeof(uint32_t));
			suiTestedBCurrentGeneration = 1;
		}

		for (int64_t j = range.iStartY; j <= range.iEndY; ++j)
		{
			for (int64_t k = range.iStartX; k <= range.iEndX; ++k)
			{
				const ZonePair& rZonePair = rPairZones.zones[j][k];
				if (rZonePair.iCountB == 0)
				{
					continue;
				}

				// Layer filtering is complete before object pairs are tested.
				for (int64_t iOther : std::span(rZonePair.indicesB.Data(), static_cast<size_t>(rZonePair.iCountB)))
				{
					if (sTestedBGeneration[iOther] == suiTestedBCurrentGeneration)
					{
						continue;
					}
					sTestedBGeneration[iOther] = suiTestedBCurrentGeneration;

					TestAndCollectPair(pairContext, i, iOther);
				}
			}
		}
	}
}

std::span<const CollisionResult> Collision::GetCollisions(int64_t iLayerIndex, int64_t iIndex)
{
	const CollisionResultSpan& rSpan = sResultSpans[sLayerBaseOffsets[iLayerIndex] + iIndex];
	if (rSpan.iCount > 0)
	{
		return {sResultEntries.Data() + rSpan.iOffset, static_cast<size_t>(rSpan.iCount)};
	}
	return {};
}

} // namespace engine
