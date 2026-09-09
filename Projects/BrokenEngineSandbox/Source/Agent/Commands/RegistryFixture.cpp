#include "Agent/Commands/RegistryFixture.h"

namespace game
{

namespace
{

// registry_fixture exercises the engine::FrameRegistry public API.

// An id type the registry cannot name, so an ownership layer over it must go through the type-erased
// RegistryIdBytes bind instead of a cross-type pointer view.
struct RegistryFixtureOwnerTag;
using registry_fixture_owner_t = engine::id_t<RegistryFixtureOwnerTag>;


} // namespace

// Drives the public engine::FrameRegistry API over fixed local arrays — no frame, no collection, and no
// allocation on the registry paths: the fixed subscriber-then-angle ranking, radius and alignment
// acceptance/rejection, resolution and release across context rebuilds, tie order, and the ownership layer's
// row count, uuid lookup, and single client-GUID write.
void CommandRegistryFixture([[maybe_unused]] const nlohmann::json& rParams, [[maybe_unused]] nlohmann::json& rResult)
{
	if constexpr (!kbDebugInput)
	{
		throw std::runtime_error("registry_fixture requires kbDebugInput build");
	}
	else
	{
#if defined(BT_CLIENT)
		rResult["build"] = "client";
#else
		rResult["build"] = "server";
#endif

		auto MakeRegistryId = [](int64_t iValue)
		{
			return engine::registry_id_t {engine::uuid_t {iValue}};
		};

		// The consumer alignment collides with the source alignment and with nothing else, so a consumer bound
		// to the neutral alignment must be refused every candidate.
		constexpr engine::alignment_t kConsumerAlignment {1u};
		constexpr engine::alignment_t kSourceAlignment {2u};
		constexpr engine::alignment_t kNeutralAlignment {3u};
		engine::Alignments alignments;
		alignments.AddAlignment(kConsumerAlignment, kSourceAlignment, engine::AlignmentFlags::kEnemies);

		// Three sources directly ahead of the consumers at strictly increasing angle, roughly 100 m away.
		constexpr int64_t kiSourceCount = 3;
		const engine::registry_id_t puiSourceIds[kiSourceCount] = {MakeRegistryId(1), MakeRegistryId(2), MakeRegistryId(3)};
		const XMVECTOR pVecSourceCurrent[kiSourceCount] =
		{
			XMVectorSet(100.0f, 0.0f, 0.0f, 1.0f),
			XMVectorSet(100.0f, 10.0f, 0.0f, 1.0f),
			XMVectorSet(100.0f, 20.0f, 0.0f, 1.0f),
		};
		const XMVECTOR pVecSourcePrevious[kiSourceCount] =
		{
			XMVectorSet(99.0f, 0.0f, 0.0f, 1.0f),
			XMVectorSet(99.0f, 10.0f, 0.0f, 1.0f),
			XMVectorSet(99.0f, 20.0f, 0.0f, 1.0f),
		};
		const engine::alignment_t pSourceAlignments[kiSourceCount] = {kSourceAlignment, kSourceAlignment, kSourceAlignment};
		const int64_t piSourceRows[kiSourceCount] = {0, 1, 2};
		const int64_t piSourceRowsWithoutThird[2] = {0, 1};

		// One consumer column: rows 0-2 already subscribe (id 1 twice, id 2 once, id 3 never), rows 3-5 acquire.
		constexpr int64_t kiConsumerCount = 6;
		constexpr int64_t kiAcquireCount = 3;
		engine::registry_id_t puiConsumerTargets[kiConsumerCount] = {MakeRegistryId(1), MakeRegistryId(1), MakeRegistryId(2), {}, {}, {}};
		const XMVECTOR vecConsumerOrigin = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
		const XMVECTOR vecConsumerDirection = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
		XMVECTOR pVecConsumerOrigins[kiConsumerCount] = {};
		XMVECTOR pVecConsumerDirections[kiConsumerCount] = {};
		engine::alignment_t pConsumerAlignments[kiConsumerCount] = {};
		engine::alignment_t pNeutralConsumerAlignments[kiConsumerCount] = {};
		for (int64_t i = 0; i < kiConsumerCount; ++i)
		{
			pVecConsumerOrigins[i] = vecConsumerOrigin;
			pVecConsumerDirections[i] = vecConsumerDirection;
			pConsumerAlignments[i] = kConsumerAlignment;
			pNeutralConsumerAlignments[i] = kNeutralAlignment;
		}
		const int64_t piConsumerRows[kiConsumerCount] = {0, 1, 2, 3, 4, 5};
		const int64_t piAcquireRows[kiAcquireCount] = {3, 4, 5};
		const int64_t piFirstAcquireRow[1] = {3};

		constexpr float kfRadius = 200.0f;
		constexpr float kfShortRadius = 50.0f;

		// Registry-internal scratch, one fixed block large enough for every window below so nothing here
		// allocates. Each window relays its eligible rows into the front of the block and rebinds them there,
		// the same layout a real query window produces from its single workbuffer allocation; the registry
		// derives the subscriber counts directly behind that prefix.
		alignas(int64_t) std::byte pScratch[128] = {};
		auto BuildContext = [&](std::span<engine::RegistrySourceLayer> layers, std::span<const engine::RegistrySubscriptionLayer> subscriptions)
		{
			int64_t iEligibleRows = 0;
			for (const engine::RegistrySourceLayer& rLayer : layers)
			{
				iEligibleRows += static_cast<int64_t>(rLayer.rows.size());
			}
			const int64_t iScratchBytes = engine::RegistryScratchBytes(iEligibleRows);
			if (iScratchBytes > static_cast<int64_t>(sizeof(pScratch)))
			{
				throw std::runtime_error("registry_fixture scratch buffer too small");
			}

			int64_t* piRows = reinterpret_cast<int64_t*>(pScratch);
			for (engine::RegistrySourceLayer& rLayer : layers)
			{
				// memmove, not copy: a layer reused by a later window already has its rows in this exact slot.
				std::memmove(piRows, rLayer.rows.data(), rLayer.rows.size() * sizeof(int64_t));
				rLayer.rows = std::span<const int64_t>(piRows, rLayer.rows.size());
				piRows += rLayer.rows.size();
			}

			return engine::BuildRegistryQueryContext(alignments, layers, subscriptions, std::span<std::byte>(pScratch, static_cast<size_t>(iScratchBytes)));
		};

		engine::RegistrySourceLayer sourceLayer {};
		sourceLayer.puiIds = puiSourceIds;
		sourceLayer.pVecCurrentPositions = pVecSourceCurrent;
		sourceLayer.pVecPreviousPositions = pVecSourcePrevious;
		sourceLayer.pAlignments = pSourceAlignments;
		sourceLayer.rows = std::span<const int64_t>(piSourceRows, kiSourceCount);
		sourceLayer.iSourceCount = kiSourceCount;

		engine::RegistrySubscriptionLayer subscriptionLayer {};
		subscriptionLayer.puiTargets = puiConsumerTargets;
		subscriptionLayer.rows = std::span<const int64_t>(piConsumerRows, kiConsumerCount);
		subscriptionLayer.iSourceCount = kiConsumerCount;

		engine::RegistryResult pResults[kiAcquireCount] = {};
		engine::RegistryBatch batch {};
		batch.puiTargets = puiConsumerTargets;
		batch.pVecOrigins = pVecConsumerOrigins;
		batch.pVecDirections = pVecConsumerDirections;
		batch.pAlignments = pConsumerAlignments;
		batch.rows = std::span<const int64_t>(piAcquireRows, kiAcquireCount);
		batch.results = std::span<engine::RegistryResult>(pResults, kiAcquireCount);
		batch.iSourceCount = kiConsumerCount;

		// Seeds the acquiring rows with an id no source publishes, so a rejection has to be observed as the
		// acquisition clearing the handle rather than as it never having been set. The sentinel matches no
		// eligible row, so it contributes no subscriber count.
		auto SeedAcquireRows = [&]()
		{
			puiConsumerTargets[3] = MakeRegistryId(99);
			puiConsumerTargets[4] = MakeRegistryId(99);
			puiConsumerTargets[5] = MakeRegistryId(99);
		};
		auto AllAcquiredInvalid = [&]()
		{
			return !puiConsumerTargets[3].IsValid() && !puiConsumerTargets[4].IsValid() && !puiConsumerTargets[5].IsValid();
		};

		// Radius rejection: every source sits ~100 m out, so the short radius must leave all three handles clear.
		SeedAcquireRows();
		{
			engine::RegistryQueryContext context = BuildContext(std::span<engine::RegistrySourceLayer>(&sourceLayer, 1), std::span<const engine::RegistrySubscriptionLayer>(&subscriptionLayer, 1));
			engine::AcquireRegistryTargets(context, batch, kfShortRadius);
		}
		const bool bRadiusRejected = AllAcquiredInvalid();

		// Alignment rejection: the neutral consumer alignment collides with nothing, so range alone is not enough.
		SeedAcquireRows();
		{
			engine::RegistryBatch neutralBatch = batch;
			neutralBatch.pAlignments = pNeutralConsumerAlignments;
			engine::RegistryQueryContext context = BuildContext(std::span<engine::RegistrySourceLayer>(&sourceLayer, 1), std::span<const engine::RegistrySubscriptionLayer>(&subscriptionLayer, 1));
			engine::AcquireRegistryTargets(context, neutralBatch, kfRadius);
		}
		const bool bAlignmentRejected = AllAcquiredInvalid();

		// Ranking and radius acceptance. Starting counts are id 1: 2, id 2: 1, id 3: 0, so the fixed policy opens
		// on the least-subscribed id 3, then splits the next shot to id 2 on the smaller angle, then back to id 3.
		{
			engine::RegistryQueryContext context = BuildContext(std::span<engine::RegistrySourceLayer>(&sourceLayer, 1), std::span<const engine::RegistrySubscriptionLayer>(&subscriptionLayer, 1));
			engine::AcquireRegistryTargets(context, batch, kfRadius);
		}
		rResult["rankingDistribution"] = nlohmann::json::array(
			{puiConsumerTargets[3].ToUuid().Value(), puiConsumerTargets[4].ToUuid().Value(), puiConsumerTargets[5].ToUuid().Value()});
		const bool bRankingCorrect = puiConsumerTargets[3] == MakeRegistryId(3) && puiConsumerTargets[4] == MakeRegistryId(2)
		                           && puiConsumerTargets[5] == MakeRegistryId(3);

		// Permuting the source rows and rebuilding the context resolves every handle to the same row.
		const engine::registry_id_t puiPermutedIds[kiSourceCount] = {puiSourceIds[2], puiSourceIds[0], puiSourceIds[1]};
		const XMVECTOR pVecPermutedCurrent[kiSourceCount] = {pVecSourceCurrent[2], pVecSourceCurrent[0], pVecSourceCurrent[1]};
		const XMVECTOR pVecPermutedPrevious[kiSourceCount] = {pVecSourcePrevious[2], pVecSourcePrevious[0], pVecSourcePrevious[1]};
		bool bResolveStableAfterPermutation = true;
		{
			engine::RegistrySourceLayer permutedLayer = sourceLayer;
			permutedLayer.puiIds = puiPermutedIds;
			permutedLayer.pVecCurrentPositions = pVecPermutedCurrent;
			permutedLayer.pVecPreviousPositions = pVecPermutedPrevious;
			engine::RegistryQueryContext context = BuildContext(std::span<engine::RegistrySourceLayer>(&permutedLayer, 1), std::span<const engine::RegistrySubscriptionLayer>(&subscriptionLayer, 1));
			for (int64_t i = 0; i < kiSourceCount; ++i)
			{
				engine::RegistryResult result {};
				bResolveStableAfterPermutation = bResolveStableAfterPermutation
				                              && engine::ResolveRegistryHandle(context, puiSourceIds[i], result)
				                              && result.id == puiSourceIds[i]
				                              && XMVector4Equal(result.vecCurrentPosition, pVecSourceCurrent[i])
				                              && XMVector4Equal(result.vecPreviousPosition, pVecSourcePrevious[i]);
			}
		}

		// Dropping id 3 from the eligible rows: the retained handle stops resolving, release clears it, and the
		// next acquisition falls to the smallest-angle of the two survivors, which now tie on subscriber count.
		bool bRemovedIdResolves = true;
		bool bReleaseClearedHandle = false;
		{
			engine::RegistrySourceLayer reducedLayer = sourceLayer;
			reducedLayer.rows = std::span<const int64_t>(piSourceRowsWithoutThird, 2);
			engine::RegistryQueryContext context = BuildContext(std::span<engine::RegistrySourceLayer>(&reducedLayer, 1), std::span<const engine::RegistrySubscriptionLayer>(&subscriptionLayer, 1));

			engine::RegistryResult result {};
			bRemovedIdResolves = engine::ResolveRegistryHandle(context, puiConsumerTargets[3], result);
			engine::ReleaseRegistryTarget(context, puiConsumerTargets[3]);
			bReleaseClearedHandle = !puiConsumerTargets[3].IsValid();

			engine::RegistryBatch reacquireBatch = batch;
			reacquireBatch.rows = std::span<const int64_t>(piFirstAcquireRow, 1);
			reacquireBatch.results = std::span<engine::RegistryResult>(pResults, 1);
			engine::AcquireRegistryTargets(context, reacquireBatch, kfRadius);
		}
		rResult["reacquiredId"] = puiConsumerTargets[3].ToUuid().Value();
		const bool bReacquireCorrect = puiConsumerTargets[3] == MakeRegistryId(1);

		// Counts use uint16_t storage, so prove that 256 existing subscriptions remain representable and that
		// ranking still prefers the less-subscribed source. Releasing one source-A handle must clear that handle
		// and decrement only source A to 255 while the context is live.
		constexpr int64_t kiHighSourceCount = 2;
		constexpr int64_t kiHighExistingSubscriptionCount = 257;
		constexpr int64_t kiHighConsumerCount = kiHighExistingSubscriptionCount + 1;
		const engine::registry_id_t puiHighSourceIds[kiHighSourceCount] = {MakeRegistryId(21), MakeRegistryId(22)};
		const XMVECTOR pVecHighSourcePositions[kiHighSourceCount] =
		{
			XMVectorSet(100.0f, 0.0f, 0.0f, 1.0f),
			XMVectorSet(100.0f, 10.0f, 0.0f, 1.0f),
		};
		const engine::alignment_t pHighSourceAlignments[kiHighSourceCount] = {kSourceAlignment, kSourceAlignment};
		const int64_t piHighSourceRows[kiHighSourceCount] = {0, 1};
		// The five high-count arrays live in the thread-local workbuffer: ~13 KiB of stack here would push this
		// function past the 16 KiB the analysis build allows. Workbuffer frames start 16-byte aligned, so the
		// XMVECTOR storage is SIMD-safe, and the fill loops below write every element the queries read, which is
		// what makes the unzeroed reservations safe.
		common::ScopedWorkbufferAllocation<engine::registry_id_t*> highConsumerTargetsAllocation = common::gpThreadLocal->mWorkbuffer.PushBuffer<engine::registry_id_t*>(kiHighConsumerCount * static_cast<int64_t>(sizeof(engine::registry_id_t)));
		engine::registry_id_t* puiHighConsumerTargets = highConsumerTargetsAllocation;
		common::ScopedWorkbufferAllocation<int64_t*> highSubscriptionRowsAllocation = common::gpThreadLocal->mWorkbuffer.PushBuffer<int64_t*>(kiHighExistingSubscriptionCount * static_cast<int64_t>(sizeof(int64_t)));
		int64_t* piHighSubscriptionRows = highSubscriptionRowsAllocation;
		for (int64_t i = 0; i < kiHighExistingSubscriptionCount - 1; ++i)
		{
			puiHighConsumerTargets[i] = MakeRegistryId(21);
			piHighSubscriptionRows[i] = i;
		}
		puiHighConsumerTargets[kiHighExistingSubscriptionCount - 1] = MakeRegistryId(22);
		piHighSubscriptionRows[kiHighExistingSubscriptionCount - 1] = kiHighExistingSubscriptionCount - 1;
		const int64_t piHighAcquireRow[1] = {kiHighConsumerCount - 1};
		common::ScopedWorkbufferAllocation<XMVECTOR*> highConsumerOriginsAllocation = common::gpThreadLocal->mWorkbuffer.PushBuffer<XMVECTOR*>(kiHighConsumerCount * static_cast<int64_t>(sizeof(XMVECTOR)));
		XMVECTOR* pVecHighConsumerOrigins = highConsumerOriginsAllocation;
		common::ScopedWorkbufferAllocation<XMVECTOR*> highConsumerDirectionsAllocation = common::gpThreadLocal->mWorkbuffer.PushBuffer<XMVECTOR*>(kiHighConsumerCount * static_cast<int64_t>(sizeof(XMVECTOR)));
		XMVECTOR* pVecHighConsumerDirections = highConsumerDirectionsAllocation;
		common::ScopedWorkbufferAllocation<engine::alignment_t*> highConsumerAlignmentsAllocation = common::gpThreadLocal->mWorkbuffer.PushBuffer<engine::alignment_t*>(kiHighConsumerCount * static_cast<int64_t>(sizeof(engine::alignment_t)));
		engine::alignment_t* pHighConsumerAlignments = highConsumerAlignmentsAllocation;
		for (int64_t i = 0; i < kiHighConsumerCount; ++i)
		{
			pVecHighConsumerOrigins[i] = vecConsumerOrigin;
			pVecHighConsumerDirections[i] = vecConsumerDirection;
			pHighConsumerAlignments[i] = kConsumerAlignment;
		}
		engine::RegistryResult pHighResults[1] = {};
		bool bHighCountRankingCorrect = false;
		bool bHighCountReleaseCleared = false;
		bool bHighCountReleaseCountCorrect = false;
		{
			engine::RegistrySourceLayer highSourceLayer {};
			highSourceLayer.puiIds = puiHighSourceIds;
			highSourceLayer.pVecCurrentPositions = pVecHighSourcePositions;
			highSourceLayer.pAlignments = pHighSourceAlignments;
			highSourceLayer.rows = std::span<const int64_t>(piHighSourceRows, kiHighSourceCount);
			highSourceLayer.iSourceCount = kiHighSourceCount;

			engine::RegistrySubscriptionLayer highSubscriptionLayer {};
			highSubscriptionLayer.puiTargets = puiHighConsumerTargets;
			highSubscriptionLayer.rows = std::span<const int64_t>(piHighSubscriptionRows, kiHighExistingSubscriptionCount);
			highSubscriptionLayer.iSourceCount = kiHighConsumerCount;

			engine::RegistryBatch highBatch {};
			highBatch.puiTargets = puiHighConsumerTargets;
			highBatch.pVecOrigins = pVecHighConsumerOrigins;
			highBatch.pVecDirections = pVecHighConsumerDirections;
			highBatch.pAlignments = pHighConsumerAlignments;
			highBatch.rows = std::span<const int64_t>(piHighAcquireRow, 1);
			highBatch.results = std::span<engine::RegistryResult>(pHighResults, 1);
			highBatch.iSourceCount = kiHighConsumerCount;

			engine::RegistryQueryContext context = BuildContext(std::span<engine::RegistrySourceLayer>(&highSourceLayer, 1), std::span<const engine::RegistrySubscriptionLayer>(&highSubscriptionLayer, 1));
			engine::AcquireRegistryTargets(context, highBatch, kfRadius);
			bHighCountRankingCorrect = puiHighConsumerTargets[kiHighConsumerCount - 1] == MakeRegistryId(22);
			engine::ReleaseRegistryTarget(context, puiHighConsumerTargets[0]);
			bHighCountReleaseCleared = !puiHighConsumerTargets[0].IsValid();
			bHighCountReleaseCountCorrect = context.subscriberCounts[0] == 255;
		}

		// Exact tie: three candidates share one position, so the lowest layer and row must win.
		const engine::registry_id_t puiTieIdsA[2] = {MakeRegistryId(11), MakeRegistryId(12)};
		const engine::registry_id_t puiTieIdsB[1] = {MakeRegistryId(13)};
		const XMVECTOR vecTiePosition = XMVectorSet(100.0f, 0.0f, 0.0f, 1.0f);
		const XMVECTOR pVecTieCurrentA[2] = {vecTiePosition, vecTiePosition};
		const XMVECTOR pVecTieCurrentB[1] = {vecTiePosition};
		const engine::alignment_t pTieAlignments[2] = {kSourceAlignment, kSourceAlignment};
		const int64_t piTieRows[2] = {0, 1};
		engine::registry_id_t puiTieTarget[1] = {};
		{
			engine::RegistrySourceLayer tieLayers[2] = {};
			tieLayers[0].puiIds = puiTieIdsA;
			tieLayers[0].pVecCurrentPositions = pVecTieCurrentA;
			tieLayers[0].pAlignments = pTieAlignments;
			tieLayers[0].rows = std::span<const int64_t>(piTieRows, 2);
			tieLayers[0].iSourceCount = 2;
			tieLayers[1].puiIds = puiTieIdsB;
			tieLayers[1].pVecCurrentPositions = pVecTieCurrentB;
			tieLayers[1].pAlignments = pTieAlignments;
			tieLayers[1].rows = std::span<const int64_t>(piTieRows, 1);
			tieLayers[1].iSourceCount = 1;

			engine::RegistryResult tieResult {};
			engine::RegistryBatch tieBatch {};
			tieBatch.puiTargets = puiTieTarget;
			tieBatch.pVecOrigins = pVecConsumerOrigins;
			tieBatch.pVecDirections = pVecConsumerDirections;
			tieBatch.pAlignments = pConsumerAlignments;
			tieBatch.rows = std::span<const int64_t>(piTieRows, 1);
			tieBatch.results = std::span<engine::RegistryResult>(&tieResult, 1);
			tieBatch.iSourceCount = 1;

			engine::RegistryQueryContext context = BuildContext(std::span<engine::RegistrySourceLayer>(tieLayers, 2), std::span<const engine::RegistrySubscriptionLayer>());
			engine::AcquireRegistryTargets(context, tieBatch, kfRadius);
		}
		rResult["tieWinnerId"] = puiTieTarget[0].ToUuid().Value();
		const bool bTieCorrect = puiTieTarget[0] == MakeRegistryId(11);

		// Ownership layers: one natively typed, one bound through RegistryIdBytes over a foreign id type, and one
		// with no global-id column at all.
		constexpr int64_t kiOwnerCount = 4;
		const engine::registry_id_t puiOwnerIds[kiOwnerCount] = {MakeRegistryId(41), MakeRegistryId(42), MakeRegistryId(43), MakeRegistryId(44)};
		const registry_fixture_owner_t puiForeignOwnerIds[kiOwnerCount] =
		{
			registry_fixture_owner_t {engine::uuid_t {41}},
			registry_fixture_owner_t {engine::uuid_t {42}},
			registry_fixture_owner_t {engine::uuid_t {43}},
			registry_fixture_owner_t {engine::uuid_t {44}},
		};
		const engine::global_id_t pOwnerGlobalIds[kiOwnerCount] = {{101}, {102}, {103}, {104}};
		engine::ClientGuid pOwnerClientGuids[kiOwnerCount] = {};

		engine::RegistryOwnershipLayer ownerLayer {};
		ownerLayer.pIdBytes = engine::RegistryIdBytes(puiOwnerIds);
		ownerLayer.pGlobalIds = pOwnerGlobalIds;
		ownerLayer.pClientGuids = pOwnerClientGuids;
		ownerLayer.iCount = kiOwnerCount;

		engine::RegistryOwnershipLayer foreignLayer {};
		foreignLayer.pIdBytes = engine::RegistryIdBytes(puiForeignOwnerIds);
		foreignLayer.pGlobalIds = pOwnerGlobalIds;
		foreignLayer.iCount = kiOwnerCount;

		engine::RegistryOwnershipLayer anonymousLayer {};
		anonymousLayer.pIdBytes = engine::RegistryIdBytes(puiOwnerIds);
		anonymousLayer.iCount = kiOwnerCount;

		rResult["ownershipRowCount"] = engine::CountRegistryRows(ownerLayer);
		const bool bOwnershipCountCorrect = engine::CountRegistryRows(ownerLayer) == kiOwnerCount;

		constexpr engine::global_id_t kMissingGlobalId {999};
		bool bForeignLookupMatches = true;
		for (int64_t i = 0; i < kiOwnerCount; ++i)
		{
			bForeignLookupMatches = bForeignLookupMatches
			                     && engine::RegistryUuidByGlobalId(foreignLayer, pOwnerGlobalIds[i]) == puiOwnerIds[i].ToUuid()
			                     && engine::RegistryUuidByGlobalId(foreignLayer, pOwnerGlobalIds[i]) == engine::RegistryUuidByGlobalId(ownerLayer, pOwnerGlobalIds[i]);
		}
		const bool bUuidLookupHit = engine::RegistryUuidByGlobalId(ownerLayer, pOwnerGlobalIds[2]) == puiOwnerIds[2].ToUuid();
		const bool bUuidLookupMiss = engine::RegistryUuidByGlobalId(ownerLayer, kMissingGlobalId) == engine::uuid_t {};
		const bool bUuidLookupWithoutGlobalIds = engine::RegistryUuidByGlobalId(anonymousLayer, pOwnerGlobalIds[2]) == engine::uuid_t {};

		// The registry's only write: exactly the matched row changes, and a miss changes nothing.
		const engine::ClientGuid assignedGuid {0x1122334455667788ULL, 0x99aabbccddeeff00ULL};
		const engine::ClientGuid rejectedGuid {0x0123456789abcdefULL, 0xfedcba9876543210ULL};
		const bool bAssignHitReturnedTrue = engine::AssignRegistryClientGuid(ownerLayer, pOwnerGlobalIds[1], assignedGuid);
		bool bAssignHitIsolated = pOwnerClientGuids[1] == assignedGuid;
		for (int64_t i = 0; i < kiOwnerCount; ++i)
		{
			bAssignHitIsolated = bAssignHitIsolated && (i == 1 || pOwnerClientGuids[i].IsEmpty());
		}
		const bool bAssignMissReturnedFalse = !engine::AssignRegistryClientGuid(ownerLayer, kMissingGlobalId, rejectedGuid);
		bool bAssignMissChangedNothing = pOwnerClientGuids[1] == assignedGuid;
		for (int64_t i = 0; i < kiOwnerCount; ++i)
		{
			bAssignMissChangedNothing = bAssignMissChangedNothing && (i == 1 || pOwnerClientGuids[i].IsEmpty());
		}

		rResult["radiusRejected"] = bRadiusRejected;
		rResult["alignmentRejected"] = bAlignmentRejected;
		rResult["rankingCorrect"] = bRankingCorrect;
		rResult["resolveStableAfterPermutation"] = bResolveStableAfterPermutation;
		rResult["removedIdResolves"] = bRemovedIdResolves;
		rResult["releaseClearedHandle"] = bReleaseClearedHandle;
		rResult["reacquireCorrect"] = bReacquireCorrect;
		rResult["highCountRankingCorrect"] = bHighCountRankingCorrect;
		rResult["highCountReleaseCleared"] = bHighCountReleaseCleared;
		rResult["highCountReleaseCountCorrect"] = bHighCountReleaseCountCorrect;
		rResult["tieCorrect"] = bTieCorrect;
		rResult["ownershipCountCorrect"] = bOwnershipCountCorrect;
		rResult["foreignLookupMatches"] = bForeignLookupMatches;
		rResult["uuidLookupHit"] = bUuidLookupHit;
		rResult["uuidLookupMiss"] = bUuidLookupMiss;
		rResult["uuidLookupWithoutGlobalIds"] = bUuidLookupWithoutGlobalIds;
		rResult["assignHitReturnedTrue"] = bAssignHitReturnedTrue;
		rResult["assignHitIsolated"] = bAssignHitIsolated;
		rResult["assignMissReturnedFalse"] = bAssignMissReturnedFalse;
		rResult["assignMissChangedNothing"] = bAssignMissChangedNothing;

		rResult["passed"] = bRadiusRejected && bAlignmentRejected && bRankingCorrect && bResolveStableAfterPermutation
		                 && !bRemovedIdResolves && bReleaseClearedHandle && bReacquireCorrect && bHighCountRankingCorrect
		                 && bHighCountReleaseCleared && bHighCountReleaseCountCorrect && bTieCorrect && bOwnershipCountCorrect
		                 && bForeignLookupMatches && bUuidLookupHit && bUuidLookupMiss && bUuidLookupWithoutGlobalIds
		                 && bAssignHitReturnedTrue && bAssignHitIsolated && bAssignMissReturnedFalse && bAssignMissChangedNothing;
	}
}

} // namespace game
