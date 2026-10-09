#include "Agent/Commands/RegistryHarnessRig.h"

namespace game
{


// An id type the registry cannot name, so an ownership layer over it must go through the type-erased
// RegistryIdBytes bind instead of a cross-type pointer view.
struct RegistryHarnessRigOwnerTag;
using RegistryHarnessRigOwnerId = engine::Id<RegistryHarnessRigOwnerTag>;


// Registry queries use fixed local data without a frame or collection and must not allocate.
void CommandRegistryHarnessRig([[maybe_unused]] const nlohmann::json& rParameters, [[maybe_unused]] nlohmann::json& rResult)
{
	if constexpr (!kbDebugInput)
	{
		throw std::runtime_error("registry_harness_rig requires kbDebugInput build");
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
			return engine::registry_id_t {engine::Uuid {iValue}};
		};

		// The consumer alignment collides with the source alignment and with nothing else, so a consumer bound
		// to the neutral alignment must be refused every candidate.
		static constexpr engine::AlignmentIdentifier kConsumerAlignment {1ui32};
		static constexpr engine::AlignmentIdentifier kSourceAlignment {2ui32};
		static constexpr engine::AlignmentIdentifier kNeutralAlignment {3ui32};
		engine::Alignments alignments;
		alignments.AddAlignment(kConsumerAlignment, kSourceAlignment, engine::AlignmentFlags::kuiEnemies);

		// Three sources directly ahead of the consumers at strictly increasing angle, roughly 100 m away.
		static constexpr int64_t kiSourceCount = 3;
		const engine::registry_id_t pSourceIds[kiSourceCount] = {MakeRegistryId(1), MakeRegistryId(2), MakeRegistryId(3)};
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
		const engine::AlignmentIdentifier pSourceAlignments[kiSourceCount] = {kSourceAlignment, kSourceAlignment, kSourceAlignment};
		const int64_t piSourceRows[kiSourceCount] = {0, 1, 2};
		const int64_t piSourceRowsWithoutThird[2] = {0, 1};

		// One consumer column: rows 0-2 already subscribe (id 1 twice, id 2 once, id 3 never), rows 3-5 acquire.
		static constexpr int64_t kiConsumerCount = 6;
		static constexpr int64_t kiAcquireCount = 3;
		engine::registry_id_t pConsumerTargets[kiConsumerCount] = {MakeRegistryId(1), MakeRegistryId(1), MakeRegistryId(2), {}, {}, {}};
		XMVECTOR vecConsumerOrigin = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
		XMVECTOR vecConsumerDirection = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
		XMVECTOR pVecConsumerOrigins[kiConsumerCount] = {};
		XMVECTOR pVecConsumerDirections[kiConsumerCount] = {};
		engine::AlignmentIdentifier pConsumerAlignments[kiConsumerCount] = {};
		engine::AlignmentIdentifier pNeutralConsumerAlignments[kiConsumerCount] = {};
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

		static constexpr float kfRadius = 200.0f;
		static constexpr float kfShortRadius = 50.0f;

		// Registry-internal scratch, one fixed block large enough for every window below so nothing here
		// allocates. Each window relays its eligible rows into the front of the block and rebinds them there,
		// the same layout a real query window produces from its single workbuffer allocation; the registry
		// derives the subscriber counts directly behind that prefix.
		alignas(int64_t) std::byte puiScratch[128] = {};
		auto BuildContext = [&](std::span<engine::RegistrySourceLayer> layers, std::span<const engine::RegistrySubscriptionLayer> subscriptions)
		{
			int64_t iEligibleRows = 0;
			for (const engine::RegistrySourceLayer& rLayer : layers)
			{
				iEligibleRows += static_cast<int64_t>(rLayer.rows.size());
			}
			int64_t iScratchBytes = engine::RegistryScratchBytes(iEligibleRows);
			if (iScratchBytes > static_cast<int64_t>(sizeof(puiScratch)))
			{
				throw std::runtime_error("registry_harness_rig scratch buffer too small");
			}

			int64_t* piRows = reinterpret_cast<int64_t*>(puiScratch);
			for (engine::RegistrySourceLayer& rLayer : layers)
			{
				// memmove, not copy: a layer reused by a later window already has its rows in this exact slot.
				std::memmove(piRows, rLayer.rows.data(), rLayer.rows.size() * sizeof(int64_t));
				rLayer.rows = std::span<const int64_t>(piRows, rLayer.rows.size());
				piRows += rLayer.rows.size();
			}

			return engine::BuildRegistryQueryContext(alignments, layers, subscriptions, std::span<std::byte>(puiScratch, static_cast<size_t>(iScratchBytes)));
		};

		engine::RegistrySourceLayer sourceLayer {};
		sourceLayer.pIds = pSourceIds;
		sourceLayer.pVecCurrentPositions = pVecSourceCurrent;
		sourceLayer.pVecPreviousPositions = pVecSourcePrevious;
		sourceLayer.pAlignments = pSourceAlignments;
		sourceLayer.rows = std::span<const int64_t>(piSourceRows, kiSourceCount);
		sourceLayer.iSourceCount = kiSourceCount;

		engine::RegistrySubscriptionLayer subscriptionLayer {};
		subscriptionLayer.pTargets = pConsumerTargets;
		subscriptionLayer.rows = std::span<const int64_t>(piConsumerRows, kiConsumerCount);
		subscriptionLayer.iSourceCount = kiConsumerCount;

		engine::RegistryResult pResults[kiAcquireCount] = {};
		engine::RegistryBatch batch {};
		batch.pTargets = pConsumerTargets;
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
			pConsumerTargets[3] = MakeRegistryId(99);
			pConsumerTargets[4] = MakeRegistryId(99);
			pConsumerTargets[5] = MakeRegistryId(99);
		};
		auto AllAcquiredInvalid = [&]()
		{
			return !(pConsumerTargets[3].uuid.iValue != 0) && !(pConsumerTargets[4].uuid.iValue != 0) && !(pConsumerTargets[5].uuid.iValue != 0);
		};

		// Radius rejection: every source sits ~100 m out, so the short radius must leave all three handles clear.
		SeedAcquireRows();
		{
			engine::RegistryQueryContext context = BuildContext(std::span<engine::RegistrySourceLayer>(&sourceLayer, 1), std::span<const engine::RegistrySubscriptionLayer>(&subscriptionLayer, 1));
			engine::AcquireRegistryTargets(context, batch, kfShortRadius);
		}
		bool bRadiusRejected = AllAcquiredInvalid();

		// Alignment rejection: the neutral consumer alignment collides with nothing, so range alone is not enough.
		SeedAcquireRows();
		{
			engine::RegistryBatch neutralBatch = batch;
			neutralBatch.pAlignments = pNeutralConsumerAlignments;
			engine::RegistryQueryContext context = BuildContext(std::span<engine::RegistrySourceLayer>(&sourceLayer, 1), std::span<const engine::RegistrySubscriptionLayer>(&subscriptionLayer, 1));
			engine::AcquireRegistryTargets(context, neutralBatch, kfRadius);
		}
		bool bAlignmentRejected = AllAcquiredInvalid();

		// Ranking and radius acceptance. Starting counts are id 1: 2, id 2: 1, id 3: 0, so the fixed policy opens
		// on the least-subscribed id 3, then splits the next shot to id 2 on the smaller angle, then back to id 3.
		{
			engine::RegistryQueryContext context = BuildContext(std::span<engine::RegistrySourceLayer>(&sourceLayer, 1), std::span<const engine::RegistrySubscriptionLayer>(&subscriptionLayer, 1));
			engine::AcquireRegistryTargets(context, batch, kfRadius);
		}
		rResult["rankingDistribution"] = nlohmann::json::array({pConsumerTargets[3].uuid.iValue, pConsumerTargets[4].uuid.iValue, pConsumerTargets[5].uuid.iValue});
		bool bRankingCorrect = pConsumerTargets[3] == MakeRegistryId(3) && pConsumerTargets[4] == MakeRegistryId(2)
		                     && pConsumerTargets[5] == MakeRegistryId(3);

		// Permuting the source rows and rebuilding the context resolves every handle to the same row.
		const engine::registry_id_t pPermutedIds[kiSourceCount] = {pSourceIds[2], pSourceIds[0], pSourceIds[1]};
		const XMVECTOR pVecPermutedCurrent[kiSourceCount] = {pVecSourceCurrent[2], pVecSourceCurrent[0], pVecSourceCurrent[1]};
		const XMVECTOR pVecPermutedPrevious[kiSourceCount] = {pVecSourcePrevious[2], pVecSourcePrevious[0], pVecSourcePrevious[1]};
		bool bResolveStableAfterPermutation = true;
		{
			engine::RegistrySourceLayer permutedLayer = sourceLayer;
			permutedLayer.pIds = pPermutedIds;
			permutedLayer.pVecCurrentPositions = pVecPermutedCurrent;
			permutedLayer.pVecPreviousPositions = pVecPermutedPrevious;
			engine::RegistryQueryContext context = BuildContext(std::span<engine::RegistrySourceLayer>(&permutedLayer, 1), std::span<const engine::RegistrySubscriptionLayer>(&subscriptionLayer, 1));
			for (int64_t i = 0; i < kiSourceCount; ++i)
			{
				engine::RegistryResult result {};
				bResolveStableAfterPermutation = bResolveStableAfterPermutation
				                              && engine::ResolveRegistryHandle(context, pSourceIds[i], result)
				                              && result.id == pSourceIds[i]
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
			bRemovedIdResolves = engine::ResolveRegistryHandle(context, pConsumerTargets[3], result);
			engine::ReleaseRegistryTarget(context, pConsumerTargets[3]);
			bReleaseClearedHandle = !(pConsumerTargets[3].uuid.iValue != 0);

			engine::RegistryBatch reacquireBatch = batch;
			reacquireBatch.rows = std::span<const int64_t>(piFirstAcquireRow, 1);
			reacquireBatch.results = std::span<engine::RegistryResult>(pResults, 1);
			engine::AcquireRegistryTargets(context, reacquireBatch, kfRadius);
		}
		rResult["reacquiredId"] = pConsumerTargets[3].uuid.iValue;
		bool bReacquireCorrect = pConsumerTargets[3] == MakeRegistryId(1);

		// Counts use uint16_t storage, so prove that 256 existing subscriptions remain representable and that
		// ranking still prefers the less-subscribed source. Releasing one source-A handle must clear that handle
		// and decrement only source A to 255 while the context is live.
		static constexpr int64_t kiHighSourceCount = 2;
		static constexpr int64_t kiHighExistingSubscriptionCount = 257;
		static constexpr int64_t kiHighConsumerCount = kiHighExistingSubscriptionCount + 1;
		const engine::registry_id_t pHighSourceIds[kiHighSourceCount] = {MakeRegistryId(21), MakeRegistryId(22)};
		const XMVECTOR pVecHighSourcePositions[kiHighSourceCount] =
		{
			XMVectorSet(100.0f, 0.0f, 0.0f, 1.0f),
			XMVectorSet(100.0f, 10.0f, 0.0f, 1.0f),
		};
		const engine::AlignmentIdentifier pHighSourceAlignments[kiHighSourceCount] = {kSourceAlignment, kSourceAlignment};
		const int64_t piHighSourceRows[kiHighSourceCount] = {0, 1};
		// The five high-count arrays total ~13 KiB and use the thread-local workbuffer to fit the analysis build's 16 KiB stack budget.
		// Workbuffer frames are 16-byte aligned for XMVECTOR storage; fill every queried element before reading an unzeroed reservation.
		common::ScopedWorkbufferAllocation<engine::registry_id_t*> highConsumerTargetsAllocation = common::gpThreadLocal->mWorkbuffer.PushBuffer<engine::registry_id_t*>(kiHighConsumerCount * static_cast<int64_t>(sizeof(engine::registry_id_t)));
		engine::registry_id_t* pHighConsumerTargets = highConsumerTargetsAllocation.mpData;
		common::ScopedWorkbufferAllocation<int64_t*> highSubscriptionRowsAllocation = common::gpThreadLocal->mWorkbuffer.PushBuffer<int64_t*>(kiHighExistingSubscriptionCount * static_cast<int64_t>(sizeof(int64_t)));
		int64_t* piHighSubscriptionRows = highSubscriptionRowsAllocation.mpData;
		for (int64_t i = 0; i < kiHighExistingSubscriptionCount - 1; ++i)
		{
			pHighConsumerTargets[i] = MakeRegistryId(21);
			piHighSubscriptionRows[i] = i;
		}
		pHighConsumerTargets[kiHighExistingSubscriptionCount - 1] = MakeRegistryId(22);
		piHighSubscriptionRows[kiHighExistingSubscriptionCount - 1] = kiHighExistingSubscriptionCount - 1;
		const int64_t piHighAcquireRow[1] = {kiHighConsumerCount - 1};
		common::ScopedWorkbufferAllocation<XMVECTOR*> highConsumerOriginsAllocation = common::gpThreadLocal->mWorkbuffer.PushBuffer<XMVECTOR*>(kiHighConsumerCount * static_cast<int64_t>(sizeof(XMVECTOR)));
		XMVECTOR* pVecHighConsumerOrigins = highConsumerOriginsAllocation.mpData;
		common::ScopedWorkbufferAllocation<XMVECTOR*> highConsumerDirectionsAllocation = common::gpThreadLocal->mWorkbuffer.PushBuffer<XMVECTOR*>(kiHighConsumerCount * static_cast<int64_t>(sizeof(XMVECTOR)));
		XMVECTOR* pVecHighConsumerDirections = highConsumerDirectionsAllocation.mpData;
		common::ScopedWorkbufferAllocation<engine::AlignmentIdentifier*> highConsumerAlignmentsAllocation = common::gpThreadLocal->mWorkbuffer.PushBuffer<engine::AlignmentIdentifier*>(kiHighConsumerCount * static_cast<int64_t>(sizeof(engine::AlignmentIdentifier)));
		engine::AlignmentIdentifier* pHighConsumerAlignments = highConsumerAlignmentsAllocation.mpData;
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
			highSourceLayer.pIds = pHighSourceIds;
			highSourceLayer.pVecCurrentPositions = pVecHighSourcePositions;
			highSourceLayer.pAlignments = pHighSourceAlignments;
			highSourceLayer.rows = std::span<const int64_t>(piHighSourceRows, kiHighSourceCount);
			highSourceLayer.iSourceCount = kiHighSourceCount;

			engine::RegistrySubscriptionLayer highSubscriptionLayer {};
			highSubscriptionLayer.pTargets = pHighConsumerTargets;
			highSubscriptionLayer.rows = std::span<const int64_t>(piHighSubscriptionRows, kiHighExistingSubscriptionCount);
			highSubscriptionLayer.iSourceCount = kiHighConsumerCount;

			engine::RegistryBatch highBatch {};
			highBatch.pTargets = pHighConsumerTargets;
			highBatch.pVecOrigins = pVecHighConsumerOrigins;
			highBatch.pVecDirections = pVecHighConsumerDirections;
			highBatch.pAlignments = pHighConsumerAlignments;
			highBatch.rows = std::span<const int64_t>(piHighAcquireRow, 1);
			highBatch.results = std::span<engine::RegistryResult>(pHighResults, 1);
			highBatch.iSourceCount = kiHighConsumerCount;

			engine::RegistryQueryContext context = BuildContext(std::span<engine::RegistrySourceLayer>(&highSourceLayer, 1), std::span<const engine::RegistrySubscriptionLayer>(&highSubscriptionLayer, 1));
			engine::AcquireRegistryTargets(context, highBatch, kfRadius);
			bHighCountRankingCorrect = pHighConsumerTargets[kiHighConsumerCount - 1] == MakeRegistryId(22);
			engine::ReleaseRegistryTarget(context, pHighConsumerTargets[0]);
			bHighCountReleaseCleared = !(pHighConsumerTargets[0].uuid.iValue != 0);
			bHighCountReleaseCountCorrect = context.subscriberCounts[0] == 255;
		}

		// Exact tie: three candidates share one position, so the lowest layer and row must win.
		const engine::registry_id_t pFirstTieIds[2] = {MakeRegistryId(11), MakeRegistryId(12)};
		const engine::registry_id_t pSecondTieIds[1] = {MakeRegistryId(13)};
		XMVECTOR vecTiePosition = XMVectorSet(100.0f, 0.0f, 0.0f, 1.0f);
		const XMVECTOR pVecFirstTieCurrent[2] = {vecTiePosition, vecTiePosition};
		const XMVECTOR pVecSecondTieCurrent[1] = {vecTiePosition};
		const engine::AlignmentIdentifier pTieAlignments[2] = {kSourceAlignment, kSourceAlignment};
		const int64_t piTieRows[2] = {0, 1};
		engine::registry_id_t pTieTarget[1] = {};
		{
			engine::RegistrySourceLayer tieLayers[2] = {};
			tieLayers[0].pIds = pFirstTieIds;
			tieLayers[0].pVecCurrentPositions = pVecFirstTieCurrent;
			tieLayers[0].pAlignments = pTieAlignments;
			tieLayers[0].rows = std::span<const int64_t>(piTieRows, 2);
			tieLayers[0].iSourceCount = 2;
			tieLayers[1].pIds = pSecondTieIds;
			tieLayers[1].pVecCurrentPositions = pVecSecondTieCurrent;
			tieLayers[1].pAlignments = pTieAlignments;
			tieLayers[1].rows = std::span<const int64_t>(piTieRows, 1);
			tieLayers[1].iSourceCount = 1;

			engine::RegistryResult tieResult {};
			engine::RegistryBatch tieBatch {};
			tieBatch.pTargets = pTieTarget;
			tieBatch.pVecOrigins = pVecConsumerOrigins;
			tieBatch.pVecDirections = pVecConsumerDirections;
			tieBatch.pAlignments = pConsumerAlignments;
			tieBatch.rows = std::span<const int64_t>(piTieRows, 1);
			tieBatch.results = std::span<engine::RegistryResult>(&tieResult, 1);
			tieBatch.iSourceCount = 1;

			engine::RegistryQueryContext context = BuildContext(std::span<engine::RegistrySourceLayer>(tieLayers, 2), std::span<const engine::RegistrySubscriptionLayer>());
			engine::AcquireRegistryTargets(context, tieBatch, kfRadius);
		}
		rResult["tieWinnerId"] = pTieTarget[0].uuid.iValue;
		bool bTieCorrect = pTieTarget[0] == MakeRegistryId(11);

		// Ownership layers: one natively typed, one bound through RegistryIdBytes over a foreign id type, and one
		// with no global-id column at all.
		static constexpr int64_t kiOwnerCount = 4;
		const engine::registry_id_t pOwnerIds[kiOwnerCount] = {MakeRegistryId(41), MakeRegistryId(42), MakeRegistryId(43), MakeRegistryId(44)};
		const RegistryHarnessRigOwnerId pForeignOwnerIds[kiOwnerCount] =
		{
			RegistryHarnessRigOwnerId {engine::Uuid {41}},
			RegistryHarnessRigOwnerId {engine::Uuid {42}},
			RegistryHarnessRigOwnerId {engine::Uuid {43}},
			RegistryHarnessRigOwnerId {engine::Uuid {44}},
		};
		const engine::GlobalId pOwnerGlobalIds[kiOwnerCount] = {{.iValue = 101}, {.iValue = 102}, {.iValue = 103}, {.iValue = 104}};
		engine::ClientGuid pOwnerClientGuids[kiOwnerCount] = {};

		engine::RegistryOwnershipLayer ownerLayer {};
		ownerLayer.pIdBytes = engine::RegistryIdBytes(pOwnerIds);
		ownerLayer.pGlobalIds = pOwnerGlobalIds;
		ownerLayer.pClientGuids = pOwnerClientGuids;
		ownerLayer.iCount = kiOwnerCount;

		engine::RegistryOwnershipLayer foreignLayer {};
		foreignLayer.pIdBytes = engine::RegistryIdBytes(pForeignOwnerIds);
		foreignLayer.pGlobalIds = pOwnerGlobalIds;
		foreignLayer.iCount = kiOwnerCount;

		engine::RegistryOwnershipLayer anonymousLayer {};
		anonymousLayer.pIdBytes = engine::RegistryIdBytes(pOwnerIds);
		anonymousLayer.iCount = kiOwnerCount;

		rResult["ownershipRowCount"] = (ownerLayer).iCount;
		bool bOwnershipCountCorrect = (ownerLayer).iCount == kiOwnerCount;

		static constexpr engine::GlobalId kMissingGlobalId {.iValue = 999};
		bool bForeignLookupMatches = std::ranges::equal(pOwnerGlobalIds, pOwnerIds, [&foreignLayer, &ownerLayer](const engine::GlobalId& rGlobalId, const engine::registry_id_t& rId)
		{
			return engine::RegistryUuidByGlobalId(foreignLayer, rGlobalId) == rId.uuid && engine::RegistryUuidByGlobalId(foreignLayer, rGlobalId) == engine::RegistryUuidByGlobalId(ownerLayer, rGlobalId);
		});
		bool bUuidLookupHit = engine::RegistryUuidByGlobalId(ownerLayer, pOwnerGlobalIds[2]) == pOwnerIds[2].uuid;
		bool bUuidLookupMiss = engine::RegistryUuidByGlobalId(ownerLayer, kMissingGlobalId) == engine::Uuid {};
		bool bUuidLookupWithoutGlobalIds = engine::RegistryUuidByGlobalId(anonymousLayer, pOwnerGlobalIds[2]) == engine::Uuid {};

		// Ownership writes affect only the matched client-GUID row; a missing global ID leaves all rows unchanged.
		static constexpr engine::ClientGuid kAssignedGuid {.uiHigh = 0x1122'3344'5566'7788ui64, .uiLow = 0x99aa'bbcc'ddee'ff00ui64};
		static constexpr engine::ClientGuid kRejectedGuid {.uiHigh = 0x0123'4567'89ab'cdefui64, .uiLow = 0xfedc'ba98'7654'3210ui64};
		bool bAssignHitReturnedTrue = engine::AssignRegistryClientGuid(ownerLayer, pOwnerGlobalIds[1], kAssignedGuid);
		bool bAssignHitIsolated = pOwnerClientGuids[1] == kAssignedGuid;
		for (int64_t i = 0; i < kiOwnerCount; ++i)
		{
			bAssignHitIsolated = bAssignHitIsolated && (i == 1 || (pOwnerClientGuids[i].uiHigh == 0 && pOwnerClientGuids[i].uiLow == 0));
		}
		bool bAssignMissReturnedFalse = !engine::AssignRegistryClientGuid(ownerLayer, kMissingGlobalId, kRejectedGuid);
		bool bAssignMissChangedNothing = pOwnerClientGuids[1] == kAssignedGuid;
		for (int64_t i = 0; i < kiOwnerCount; ++i)
		{
			bAssignMissChangedNothing = bAssignMissChangedNothing && (i == 1 || (pOwnerClientGuids[i].uiHigh == 0 && pOwnerClientGuids[i].uiLow == 0));
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
