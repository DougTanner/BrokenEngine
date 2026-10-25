#include "FrameBase.h"

#include "Frame/CellStaticData.h"
#include "Frame/IslandTerrain.h"
#include "Frame/NavBuild.h"

#include "Frame/Frame.h"

namespace engine
{

common::crc_t FrameInterpolateBase::Crcs() const
{
	common::crc_t uiSharedCrc = 0;

	uiSharedCrc = (uiSharedCrc ^ common::Crc(iTick)) * common::kCrcMultiplier;
	uiSharedCrc = (uiSharedCrc ^ common::Crc(fDeltaTime)) * common::kCrcMultiplier;

	std::apply([&](const auto&... rCollections)
	{
		((uiSharedCrc = (uiSharedCrc ^ SharedCollectionCrc(rCollections)) * common::kCrcMultiplier), ...);
	}, ServerCollections());

	return uiSharedCrc;
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

	std::apply([&](const auto&... rCollections)
	{
		(CollectionWrite(rStream, rCollections, rCollections.Members()), ...);
	}, Collections());
}

void FrameInterpolateBase::Read(std::istream& rStream)
{
	FrameValuesRead<false>(rStream, *this, Values());

	std::apply([&](auto&... rCollections)
	{
		(CollectionRead(rStream, rCollections, rCollections.Members()), ...);
	}, Collections());
}

void FrameInterpolateBase::ServerRead(std::istream& rStream)
{
	FrameValuesRead<true>(rStream, *this, Values());

	std::apply([&](auto&... rCollections)
	{
		(SharedCollectionRead(rStream, rCollections), ...);
	}, ServerCollections());
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

	std::apply([&](const auto&... rCollections)
	{
		((uiCrc = (uiCrc ^ SharedCollectionCrc(rCollections)) * common::kCrcMultiplier), ...);
	}, ServerCollections());

	return uiCrc;
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

	std::apply([&](const auto&... rCollections)
	{
		(CollectionWrite(rStream, rCollections, rCollections.Members()), ...);
	}, Collections());
}

void FramePostRenderBase::Read(std::istream& rStream)
{
	FrameValuesRead<false>(rStream, *this, Values());

	std::apply([&](auto&... rCollections)
	{
		(CollectionRead(rStream, rCollections, rCollections.Members()), ...);
	}, Collections());
}

void FramePostRenderBase::ServerRead(std::istream& rStream)
{
	FrameValuesRead<true>(rStream, *this, Values());

	std::apply([&](auto&... rCollections)
	{
		(SharedCollectionRead(rStream, rCollections), ...);
	}, ServerCollections());
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

void RunFrameTick(const ActiveFrameReference& rReference, int64_t iTickCounter)
{
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
	// NavData is derived from placements + per-template NavContour. Build it here on the
	// per-coord dispatch thread (naturally parallel across coords) on the first tick after
	// a coord is created or reloaded from save. Client receives prebuilt navigationData over the
	// wire and never enters this branch (server-only NavContour).
	if (!rStaticData.bNavigationDataBuilt)
	{
		ScopedSuppressAllocationTracking suppress;
		BuildCellNavigationData(rStaticData.navigationData, rStaticData.islands);
		rStaticData.bNavigationDataBuilt = true;
	}
#endif

	// Per-cell elevation grid (purely derived from islands + shared heightmaps). Both client
	// and server build their own bit-identical copy here — same deterministic placements, same
	// shared heightmaps, /fp:strict math — so it stays out of the CRC and is never serialized.
	// Builds before any sim phase below so every CellElevationSampler::Sample/CellNormal caller this tick
	// sees a populated grid.
	if (rStaticData.elevationGrid.empty() && !rStaticData.islands.empty())
	{
		ScopedSuppressAllocationTracking suppress;
		gpIslandTerrain->BuildElevationGrid(rStaticData.islands, rStaticData.elevationGrid);
	}

#if defined(BT_CLIENT)
	// Render-only cache: per-placement flattened query (inverse-rotation sin/cos + template footprint /
	// heightmap pointer / dims) for GlobalElevation (ProjectToBaseHeight). Built here alongside
	// the elevation grid — placement rotation and template are static for the cell's life — so the render
	// path does zero hash lookups and zero libm trig per call. The server never calls GlobalElevation, so
	// skip it there.
	if (rStaticData.islandRenderQueries.empty() && !rStaticData.islands.empty())
	{
		ScopedSuppressAllocationTracking suppress;
		rStaticData.BuildRenderPlacementCache(*gpIslandTerrain);
	}
#endif

	game::FrameInterpolate::AllocateAndCopy(rNext.interpolate, rCurrent.interpolate);
	game::FrameInterpolate::Update(rNext.interpolate, rCurrent, kfDeltaTime);
	rNext.interpolate.iTick = iTickCounter;

	game::FramePostRender::AllocateAndCopy(rNext.postRender, rCurrent.postRender);
	game::FramePostRender::Update(rNext, rCurrent, *rReference.pFrameInput, rStaticData);

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

	ForEachPostRenderTransfer(PostRenderBaseTypes {}, rNext, rStaticData);
	game::FramePostRender::Transfer(rNext, rStaticData);

	ForEachPostRenderDestroy(PostRenderBaseTypes {}, rNext, rStaticData);
	game::FramePostRender::Destroy(rNext, rStaticData);
	ForEachPostRenderSpawn(PostRenderBaseTypes {}, rNext, rStaticData);
	game::FramePostRender::Spawn(rNext, *rReference.pFrameInput, rStaticData);

	// Compute CRCs after all phases complete
	rNext.postRender.uiSharedCrc = rNext.Crc();
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
