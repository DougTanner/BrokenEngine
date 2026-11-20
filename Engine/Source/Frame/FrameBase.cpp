#include "FrameBase.h"

#include "Frame/CellStaticData.h"
#include "Frame/IslandTerrain.h"
#include "Frame/NavBuild.h"

#include "Frame/Frame.h"
#if defined(BT_CLIENT)
#include "Game.h"
#endif

namespace engine
{

common::crc_t FrameInterpolateBase::Crcs() const
{
	common::crc_t uiSharedCrc = 0;

	uiSharedCrc = (uiSharedCrc ^ common::Crc(iTick)) * common::kCrcMultiplier;
	uiSharedCrc = (uiSharedCrc ^ common::Crc(fDeltaTime)) * common::kCrcMultiplier;

	return CollectionsCrc(uiSharedCrc, ServerCollections());
}

bool FrameInterpolateBase::LogDifferences(const FrameInterpolateBase& rOther) const
{
	common::ScopedLogDifferenceContext context("FrameInterpolate");
	bool bEqual = true;
	bEqual &= common::LogDifference<"iTick">(iTick, rOther.iTick);
	bEqual &= common::LogDifference<"fDeltaTime">(fDeltaTime, rOther.fDeltaTime);
	bEqual &= LogDifferencesCollections(ServerCollections(), rOther.ServerCollections(), std::make_integer_sequence<int64_t, static_cast<int64_t>(std::tuple_size_v<decltype(ServerCollections())>)> {});
	return bEqual;
}

void FrameInterpolateBase::Write(std::ostream& rStream) const
{
	FrameValuesWrite(rStream, *this, Values());

	CollectionsWrite(rStream, Collections());
}

void FrameInterpolateBase::Read(std::istream& rStream)
{
	FrameValuesRead<false>(rStream, *this, Values());

	CollectionsRead(rStream, Collections());
}

void FrameInterpolateBase::ServerRead(std::istream& rStream)
{
	FrameValuesRead<true>(rStream, *this, Values());

	SharedCollectionsRead(rStream, ServerCollections());
}

FramePostRenderBase::FramePostRenderBase()
{
	transferRequests.reserve(static_cast<size_t>(kiInitialTransferCapacity));
}

common::crc_t FramePostRenderBase::Crcs() const
{
	common::crc_t uiCrc = 0;

	uiCrc = (uiCrc ^ randomEngine.Crc()) * common::kCrcMultiplier;
	uiCrc = (uiCrc ^ common::Crc(uiNextUuid)) * common::kCrcMultiplier;
	uiCrc = (uiCrc ^ common::Crc(uiFrameIdentifier)) * common::kCrcMultiplier;
	uiCrc = (uiCrc ^ common::Crc(uiPinned)) * common::kCrcMultiplier;
	uiCrc = (uiCrc ^ alignments.Crc()) * common::kCrcMultiplier;

	return CollectionsCrc(uiCrc, ServerCollections());
}

bool FramePostRenderBase::LogDifferences(const FramePostRenderBase& rOther) const
{
	common::ScopedLogDifferenceContext context("FramePostRender");
	bool bEqual = true;
	bEqual &= common::LogDifference<"randomEngine">(randomEngine, rOther.randomEngine);
	bEqual &= common::LogDifference<"uiNextUuid">(uiNextUuid, rOther.uiNextUuid);
	// Skip uiNextSoundUuid and uiNextVisualUuid (client-only)
	bEqual &= common::LogDifference<"uiFrameIdentifier">(uiFrameIdentifier, rOther.uiFrameIdentifier);
	bEqual &= common::LogDifference<"uiPinned">(uiPinned, rOther.uiPinned);
	if (!(alignments == rOther.alignments))
	{
		bEqual = false;
		LOG(kNetwork, kError, "LogDifferences {} alignments differ", common::gpLogDifferenceContext);
	}
	bEqual &= LogDifferencesCollections(ServerCollections(), rOther.ServerCollections(), std::make_integer_sequence<int64_t, static_cast<int64_t>(std::tuple_size_v<decltype(ServerCollections())>)> {});
	return bEqual;
}

// Write/Read include client-only UUID counters under BT_CLIENT, so their streams require the same build.
// GridSave and Replay are server-only stream owners; callers enforce this restriction.
// Cross-build network snapshots use ServerRead, which omits those counters.
void FramePostRenderBase::Write(std::ostream& rStream) const
{
	FrameValuesWrite(rStream, *this, Values());

	CollectionsWrite(rStream, Collections());
}

void FramePostRenderBase::Read(std::istream& rStream)
{
	FrameValuesRead<false>(rStream, *this, Values());

	CollectionsRead(rStream, Collections());
}

void FramePostRenderBase::ServerRead(std::istream& rStream)
{
	FrameValuesRead<true>(rStream, *this, Values());

	SharedCollectionsRead(rStream, ServerCollections());
}

bool FramePostRenderBase::PrepareTransferRequest(game::TransferRequest& rRequest) const
{
	XMVECTOR& rVecPosition = game::SpawnPositionRef(rRequest.change);
	int64_t iDirection = CellExitDirection(rVecPosition);
	rRequest.iDirection = static_cast<int8_t>(iDirection);

	// The payload leaves here already expressed in the destination cell's local frame: the exit direction's center
	// offset is subtracted and the landing nudged inside the destination. Every downstream consumer — network transfer,
	// ApplySpawnStatusChange, replay reconcile — forwards the position unchanged, so this is the single conversion point.
	rVecPosition = NudgeIntoCell(XMVectorSubtract(rVecPosition, XMLoadFloat2(&kCellDirections.pEntries[iDirection].f2CenterOffset)));
	ASSERT(IsInsideCell(rVecPosition));

	// Transfer producers are unbounded, so a full buffer can reallocate during a shared per-tick burst.
	return std::ssize(transferRequests) < static_cast<int64_t>(transferRequests.capacity());
}

void FramePostRenderBase::PushTransferRequest(const game::TransferRequest& rRequest)
{
	game::ValidateSpawnVectors(rRequest.change);
	{
		// Heap: no finite reserve can be proven sufficient because engine::GrowPairedCollections
		// grows collections unbounded
		ScopedSuppressAllocationTracking suppress;
		transferRequests.push_back(rRequest);
	}
}

void ValidateBaseCollectionPairs(FrameInterpolateBase& rInterpolate, const FramePostRenderBase& rPostRender)
{
	ValidateCollectionPairs(rInterpolate.Collections(), rPostRender.Collections());
}

void ValidateBaseServerCollectionPairs(FrameInterpolateBase& rInterpolate, const FramePostRenderBase& rPostRender)
{
	ValidateCollectionPairs(rInterpolate.ServerCollections(), rPostRender.ServerCollections());
}

void FrameInterpolateBase::AllocateAndCopy([[maybe_unused]] game::FrameInterpolate& __restrict rCurrent, [[maybe_unused]] const game::FrameInterpolate& __restrict rPrevious)
{
	FrameInterpolateBase& rCurrentBase = rCurrent;
	const FrameInterpolateBase& rPreviousBase = rPrevious;
	AllocateAndCopyCollections(rCurrentBase.Collections(), rPreviousBase.Collections(), std::make_integer_sequence<int64_t, static_cast<int64_t>(std::tuple_size_v<decltype(rCurrentBase.Collections())>)> {});
	rCurrentBase.iTick = rPreviousBase.iTick;
}

void FrameInterpolateBase::Update([[maybe_unused]] game::FrameInterpolate& __restrict rCurrent, [[maybe_unused]] const game::Frame& __restrict rPreviousFrame, [[maybe_unused]] float fDeltaTime)
{
	rCurrent.fDeltaTime = fDeltaTime;

#if defined(BT_CLIENT)
	// On a reconcile tick, rCurrent's kRecalculated is that tick's replay marker, assigned before
	// RunFrameTick; it must reach the PostRender Spawn readers, so only the phase bits change here.
	rCurrent.frameFlags.Clear({FrameFlags::kInterpolate, FrameFlags::kPostRender});
	rCurrent.frameFlags.Set(FrameFlags::kInterpolate);
#endif

	ForEachInterpolateUpdate(InterpolateTypes {}, rCurrent, rPreviousFrame);
}

void FramePostRenderBase::AllocateAndCopy([[maybe_unused]] game::FramePostRender& __restrict rCurrent, [[maybe_unused]] const game::FramePostRender& __restrict rPrevious)
{
	FramePostRenderBase& rCurrentBase = rCurrent;
	const FramePostRenderBase& rPreviousBase = rPrevious;
	AllocateAndCopyCollections(rCurrentBase.Collections(), rPreviousBase.Collections(), std::make_integer_sequence<int64_t, static_cast<int64_t>(std::tuple_size_v<decltype(rCurrentBase.Collections())>)> {});
}

void FramePostRenderBase::Update([[maybe_unused]] game::Frame& __restrict rFrame, [[maybe_unused]] const game::Frame& __restrict rPreviousFrame, [[maybe_unused]] const game::FrameInput& __restrict rFrameInput, [[maybe_unused]] const CellStaticData& rStaticData)
{
	rFrame.postRender.transferRequests.clear();

	game::FramePostRender& rCurrent = rFrame.postRender;
	const game::FramePostRender& rPrevious = rPreviousFrame.postRender;

#if defined(BT_CLIENT)
	rFrame.interpolate.frameFlags.Clear({FrameFlags::kInterpolate, FrameFlags::kPostRender});
	rFrame.interpolate.frameFlags.Set(FrameFlags::kPostRender);
#endif

	rCurrent.randomEngine = rPrevious.randomEngine;
	rCurrent.uiNextUuid = rPrevious.uiNextUuid;
#if defined(BT_CLIENT)
	rCurrent.uiNextSoundUuid = rPrevious.uiNextSoundUuid;
	rCurrent.uiNextVisualUuid = rPrevious.uiNextVisualUuid;
#endif
	rCurrent.uiFrameIdentifier = rPrevious.uiFrameIdentifier;
	rCurrent.uiPinned = rPrevious.uiPinned;
	rCurrent.alignments.CopyFrom(rPrevious.alignments);

	ForEachPostRenderUpdate(PostRenderBaseTypes {}, rFrame, rPreviousFrame, rStaticData);

	// Setup pusher zones for spatial acceleration
	PushersInterpolate::SetupZones(rFrame);
}

#if defined(BT_CLIENT)
void RunFrameTick(const ActiveFrameReference& rReference, int64_t iTickCounter, FrameTickPhaseTimings* pPhaseTimings)
#else
void RunFrameTick(const ActiveFrameReference& rReference, int64_t iTickCounter)
#endif
{
#if defined(BT_CLIENT)
	using Phase = FrameTickPhaseTimings::Phase;
	auto StartPhase = [&]() -> std::chrono::steady_clock::time_point
	{
		if constexpr (kbProfilingFrameSpike)
		{
			if (pPhaseTimings != nullptr)
			{
				return std::chrono::steady_clock::now();
			}
		}
		return {};
	};
	std::chrono::steady_clock::time_point phaseStartTime = StartPhase();
	auto RecordPhase = [&](Phase ePhase, std::chrono::steady_clock::time_point& rStartTime)
	{
		if constexpr (kbProfilingFrameSpike)
		{
			if (pPhaseTimings != nullptr)
			{
				std::chrono::steady_clock::time_point endTime = std::chrono::steady_clock::now();
				pPhaseTimings->nanoseconds[ePhase] += std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - rStartTime).count();
				rStartTime = endTime;
			}
		}
	};
#endif

	// Mark this thread as inside a deterministic tick so a stray render-path GlobalElevation
	// call (which walks mCells with libm trig) fails fast instead of silently desyncing across CPUs.
	common::FrameTickScope frameTickScope;

	// Verify MXCSR has not been corrupted by external calls (audio, Vulkan, etc.)
	unsigned int uiControlWord = 0;
	_controlfp_s(&uiControlWord, 0, 0);
	ASSERT((uiControlWord & _MCW_DN) == _DN_FLUSH);
	ASSERT((uiControlWord & _MCW_RC) == _RC_NEAR);

	game::Frame& rNext = *rReference.pNext;
	const game::Frame& rCurrent = *rReference.pCurrent;
	const CellStaticData& rStaticData = *rReference.pStaticData;

#if defined(BT_SERVER)
	// NavData is derived from placements + per-template NavContour, and the elevation grid from
	// placements. Build both here on the per-coord dispatch thread (naturally parallel across coords)
	// when a coord reaches a tick with them not yet built. Client receives prebuilt navigationData over
	// the wire, prepares its own elevation grid, and never makes this call (server-only NavContour).
	rStaticData.BuildNavigationDataIfUnbuilt();
#endif

#if defined(BT_CLIENT)
	// Live client cells arrive prepared; menu cells prepare terrain synchronously.
	bool bSynchronousTerrain = static_cast<bool>(game::gpGame->mGameFlags & GameFlags::kMainMenu);
	ASSERT(bSynchronousTerrain || (rStaticData.flags & CellStaticDataFlags::kTerrainPrepared));
	if (bSynchronousTerrain)
	{
		if (rStaticData.elevationGrid.empty() && !rStaticData.islands.empty())
		{
			// Heap: BuildElevationGrid sizes the cell's elevation grid on its first menu tick, and again after a menu island change clears it
			ScopedSuppressAllocationTracking suppress;
			std::chrono::steady_clock::time_point buildStartTime = StartPhase();
			gpIslandTerrain->BuildElevationGrid(rStaticData.islands, rStaticData.elevationGrid);
			RecordPhase(Phase::kElevationGridBuild, buildStartTime);
		}

		if (rStaticData.islandRenderQueries.empty() && !rStaticData.islands.empty())
		{
			// Heap: BuildRenderPlacementCache sizes one query per island on a menu tick, and again after a menu island change clears the cache
			ScopedSuppressAllocationTracking suppress;
			std::chrono::steady_clock::time_point buildStartTime = StartPhase();
			rStaticData.BuildRenderPlacementCache(*gpIslandTerrain);
			RecordPhase(Phase::kRenderPlacementCacheBuild, buildStartTime);
		}
		rStaticData.flags.Set(CellStaticDataFlags::kTerrainPrepared);
	}
	RecordPhase(Phase::kCellSetup, phaseStartTime);
#endif

	game::FrameInterpolate::AllocateAndCopy(rNext.interpolate, rCurrent.interpolate);
	game::FrameInterpolate::Update(rNext.interpolate, rCurrent, kfDeltaTime, rStaticData);
	rNext.interpolate.iTick = iTickCounter;
#if defined(BT_CLIENT)
	RecordPhase(Phase::kInterpolateCopyUpdate, phaseStartTime);
#endif

	game::FramePostRender::AllocateAndCopy(rNext.postRender, rCurrent.postRender);
	game::FramePostRender::Update(rNext, rCurrent, *rReference.pFrameInput, rStaticData);
#if defined(BT_CLIENT)
	RecordPhase(Phase::kPostRenderCopyUpdate, phaseStartTime);
#endif

	// Each phase walks the engine collections ahead of the game's, and each shared queue clears only after
	// every collection has run the phase that consumes it.
	ForEachPostRenderPreCollision(PostRenderBaseTypes {}, rNext, rCurrent, rStaticData);
	game::FramePostRender::PreCollision(rNext, rCurrent, rStaticData);
	Collision::Collide(rNext.postRender.alignments);
	ForEachPostRenderPostCollision(PostRenderBaseTypes {}, rNext, rCurrent, rStaticData);
	game::FramePostRender::PostCollision(rNext, rCurrent, rStaticData);
	Collision::siLayerCount = 0;
	ForEachPostRenderAreaDamage(PostRenderBaseTypes {}, rNext, rCurrent, rStaticData);
	game::FramePostRender::AreaDamage(rNext, rCurrent, rStaticData);
	AreaDamage::siAreaDamageSourceCount = 0;
#if defined(BT_CLIENT)
	RecordPhase(Phase::kCollisionAreaDamage, phaseStartTime);
#endif

	ForEachPostRenderTransfer(PostRenderBaseTypes {}, rNext, rStaticData);
	game::FramePostRender::Transfer(rNext, rStaticData);

	ForEachPostRenderDestroy(PostRenderBaseTypes {}, rNext, rStaticData);
	game::FramePostRender::Destroy(rNext, rStaticData);
	ForEachPostRenderSpawn(PostRenderBaseTypes {}, rNext, rStaticData);
	game::FramePostRender::Spawn(rNext, *rReference.pFrameInput, rStaticData);
#if defined(BT_CLIENT)
	RecordPhase(Phase::kTransferDestroySpawn, phaseStartTime);
#endif

	// Compute CRCs after all phases complete
	rNext.postRender.uiSharedCrc = rNext.Crc();
#if defined(BT_CLIENT)
	RecordPhase(Phase::kSharedCrc, phaseStartTime);
#endif
}

void WriteFrameInput(std::ostream& rStream, const game::FrameInput& rInput)
{
	int64_t iStatusCount = std::ssize(rInput.statusChanges);
	common::Write(rStream, iStatusCount);
	for (const game::StatusChange& rChange : rInput.statusChanges)
	{
		common::Write(rStream, rChange.eType);
		std::visit([&](const auto& rPayload)
		{
			common::Write(rStream, rPayload);
		}, rChange.data);
	}
}

void ReadFrameInput(std::istream& rStream, game::FrameInput& rInput)
{
	int64_t iStatusCount = 0;
	common::Read(rStream, iStatusCount);
	// Bound the count against the replay stream before the resize — each
	// StatusChange serializes at least a type byte plus one payload byte.
	common::ValidateDeserializedCount(iStatusCount, sizeof(uint8_t) + 1, rStream, "FrameInput statusChanges");
	rInput.statusChanges.resize(iStatusCount);
	for (game::StatusChange& rChange : rInput.statusChanges)
	{
		common::Read(rStream, rChange.eType);
		// An unknown type tag seats the default variant alternative and reads
		// the wrong payload byte count, silently desyncing the rest of the stream — reject it.
		if (!game::IsKnownStatusChangeType(rChange.eType))
		{
			throw std::ios_base::failure("FrameInput StatusChange type");
		}
		rChange.data = game::DefaultDataForType(rChange.eType);
		std::visit([&](auto& rPayload)
		{
			common::Read(rStream, rPayload);
		}, rChange.data);
		game::AdmitFrameInputStatusChange(rChange);
	}
}

// Same-session-only CRC: feeds the replay DifferenceStream writer-side dedup and a verbose reader log,
// never persisted or compared cross-build. Unlike the frame CRC it needs no game::Frame::kiVersion bump
// when the mixing algorithm changes.
common::crc_t FrameInputCrc(const game::FrameInput& rInput)
{
	common::crc_t uiChecksum = 0;
	for (const game::StatusChange& rStatusChange : rInput.statusChanges)
	{
		uiChecksum = (uiChecksum ^ common::Crc(rStatusChange.eType)) * common::kCrcMultiplier;
		std::visit([&](const auto& rPayload)
		{
			uiChecksum = (uiChecksum ^ common::Crc(rPayload)) * common::kCrcMultiplier;
		}, rStatusChange.data);
	}
	return uiChecksum;
}

} // namespace engine
