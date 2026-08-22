#include "FrameBase.h"

#include "Frame/Frame.h"
#include "Frame/FrameStaticData.h"
#include "Frame/IslandTerrain.h"
#include "Frame/NavBuild.h"

namespace engine
{

common::crc_t FrameInterpolateBase::Crcs() const
{
	common::crc_t sharedCrc = 0;

	sharedCrc = (sharedCrc ^ common::Crc(iTick)) * common::kCrcMultiplier;
	sharedCrc = (sharedCrc ^ common::Crc(fCurrentTime)) * common::kCrcMultiplier;
	sharedCrc = (sharedCrc ^ common::Crc(fDeltaTime)) * common::kCrcMultiplier;

	std::apply([&](const auto&... collections)
	{
		((sharedCrc = (sharedCrc ^ SharedCollectionCrc(collections)) * common::kCrcMultiplier), ...);
	}, ServerCollections());

	return sharedCrc;
}

bool FrameInterpolateBase::LogDifferences(const FrameInterpolateBase& rOther) const
{
	common::ScopedLogDifferenceContext context("FrameInterpolate");
	bool bEqual = true;
	bEqual &= common::LogDifference<"iTick">(iTick, rOther.iTick);
	bEqual &= common::LogDifference<"fCurrentTime">(fCurrentTime, rOther.fCurrentTime);
	bEqual &= common::LogDifference<"fDeltaTime">(fDeltaTime, rOther.fDeltaTime);
	bEqual &= LogDifferencesCollections(ServerCollections(), rOther.ServerCollections(), std::make_index_sequence<std::tuple_size_v<decltype(ServerCollections())>>{});
	return bEqual;
}

void FrameInterpolateBase::Write(std::ostream& rStream) const
{
	common::Write(rStream, iTick);
	common::Write(rStream, fCurrentTime);
	common::Write(rStream, fDeltaTime);

	std::apply([&](const auto&... collections)
	{
		(CollectionWrite(rStream, collections, collections.Members()), ...);
	}, Collections());
}

void FrameInterpolateBase::Read(std::istream& rStream)
{
	common::Read(rStream, iTick);
	common::Read(rStream, fCurrentTime);
	common::Read(rStream, fDeltaTime);

	std::apply([&](auto&... collections)
	{
		(CollectionRead(rStream, collections, collections.Members()), ...);
	}, Collections());
}

void FrameInterpolateBase::ServerRead(std::istream& rStream)
{
	common::Read(rStream, iTick);
	common::Read(rStream, fCurrentTime);
	common::Read(rStream, fDeltaTime);

	std::apply([&](auto&... collections)
	{
		(SharedCollectionRead(rStream, collections), ...);
	}, ServerCollections());
}

common::crc_t FramePostRenderBase::Crcs() const
{
	common::crc_t crc = 0;

	crc = (crc ^ randomEngine.Crc()) * common::kCrcMultiplier;
	crc = (crc ^ common::Crc(uiNextUuid)) * common::kCrcMultiplier;
	crc = (crc ^ common::Crc(uiFrameId)) * common::kCrcMultiplier;
	crc = (crc ^ alignments.Crc()) * common::kCrcMultiplier;

	std::apply([&](const auto&... collections)
	{
		((crc = (crc ^ SharedCollectionCrc(collections)) * common::kCrcMultiplier), ...);
	}, ServerCollections());

	return crc;
}

bool FramePostRenderBase::LogDifferences(const FramePostRenderBase& rOther) const
{
	common::ScopedLogDifferenceContext context("FramePostRender");
	bool bEqual = true;
	bEqual &= common::LogDifference<"randomEngine">(randomEngine, rOther.randomEngine);
	bEqual &= common::LogDifference<"uiNextUuid">(uiNextUuid, rOther.uiNextUuid);
	// Skip uiNextSoundUuid and uiNextVisualUuid (client-only)
	bEqual &= common::LogDifference<"uiFrameId">(uiFrameId, rOther.uiFrameId);
	if (!(alignments == rOther.alignments))
	{
		bEqual = false;
		LOG(kNetwork, kError, "LogDifferences {} alignments differ", common::gpLogDifferenceContext);
	}
	bEqual &= LogDifferencesCollections(ServerCollections(), rOther.ServerCollections(), std::make_index_sequence<std::tuple_size_v<decltype(ServerCollections())>>{});
	return bEqual;
}

// Write/Read serialize the client-only UUID counters under BT_CLIENT, so the two builds'
// stream layouts differ structurally — a client-written stream is unreadable by a server
// Read (and vice versa). Safe today only because the stream owners are server-only
// (engine::GridSave and engine::Replay); the guard is convention, not structure. ServerRead handles the
// cross-build (network) direction and documents the skip.
void FramePostRenderBase::Write(std::ostream& rStream) const
{
	common::Write(rStream, randomEngine);
	common::Write(rStream, uiNextUuid);
#if defined(BT_CLIENT)
	common::Write(rStream, uiNextSoundUuid);
	common::Write(rStream, uiNextVisualUuid);
#endif
	common::Write(rStream, uiFrameId);
	alignments.Write(rStream);

	std::apply([&](const auto&... collections)
	{
		(CollectionWrite(rStream, collections, collections.Members()), ...);
	}, Collections());
}

void FramePostRenderBase::Read(std::istream& rStream)
{
	uint64_t uiRandomState = 0;
	common::Read(rStream, uiRandomState);
	randomEngine.SetSerializedState(uiRandomState);
	common::Read(rStream, uiNextUuid);
#if defined(BT_CLIENT)
	common::Read(rStream, uiNextSoundUuid);
	common::Read(rStream, uiNextVisualUuid);
#endif
	common::Read(rStream, uiFrameId);
	alignments.Read(rStream);

	std::apply([&](auto&... collections)
	{
		(CollectionRead(rStream, collections, collections.Members()), ...);
	}, Collections());
}

void FramePostRenderBase::ServerRead(std::istream& rStream)
{
	uint64_t uiRandomState = 0;
	common::Read(rStream, uiRandomState);
	randomEngine.SetSerializedState(uiRandomState);
	common::Read(rStream, uiNextUuid);
	// Server does not write uiNextSoundUuid or uiNextVisualUuid
	common::Read(rStream, uiFrameId);
	alignments.Read(rStream);

	std::apply([&](auto&... collections)
	{
		(SharedCollectionRead(rStream, collections), ...);
	}, ServerCollections());
}

void FrameInterpolateBase::Register()
{
	ForEachRegister(InterpolateTypes{});
}

#if defined(BT_CLIENT)
void FrameInterpolateBase::GraphicsResources()
{
	ForEachGraphicsResources(InterpolateTypes{});
}
#endif

void FrameInterpolateBase::AllocateAndCopy([[maybe_unused]] game::FrameInterpolate& __restrict rCurrent, [[maybe_unused]] const game::FrameInterpolate& __restrict rPrevious)
{
	FrameInterpolateBase& rCurrentBase = rCurrent;
	const FrameInterpolateBase& rPreviousBase = rPrevious;
	AllocateAndCopyCollections(rCurrentBase.Collections(), rPreviousBase.Collections(), std::make_index_sequence<std::tuple_size_v<decltype(rCurrentBase.Collections())>>{});
}

void FrameInterpolateBase::Update([[maybe_unused]] game::FrameInterpolate& __restrict rCurrent, [[maybe_unused]] const game::Frame& __restrict rPreviousFrame, [[maybe_unused]] float fDeltaTime)
{
	// Store delta time in frame
	rCurrent.fDeltaTime = fDeltaTime;

#if defined(BT_CLIENT)
	// kRecalculated propagates through the rPrevious chain: Reconcile pre-stamps it on each
	// replay pNext (ReconcileReplay.cpp), and clears it on validated/catch-up snapshots so
	// normal-tick rPrevious never carries the bit. Reading rCurrent.frameFlags here would
	// surface stale ring-memory from a prior replay use of the slot — silencing audio.
	const FrameInterpolateBase& rPrevious = rPreviousFrame.interpolate;
	FrameFlags_t frameFlags = rPrevious.frameFlags;
	frameFlags.Clear({FrameFlags::kInterpolate, FrameFlags::kPostRender});
	frameFlags.Set(FrameFlags::kInterpolate);
	rCurrent.frameFlags = frameFlags;
#endif

	ForEachInterpolateUpdate(InterpolateTypes{}, rCurrent, rPreviousFrame);
}

void FramePostRenderBase::AllocateAndCopy([[maybe_unused]] game::FramePostRender& __restrict rCurrent, [[maybe_unused]] const game::FramePostRender& __restrict rPrevious)
{
	FramePostRenderBase& rCurrentBase = rCurrent;
	const FramePostRenderBase& rPreviousBase = rPrevious;
	AllocateAndCopyCollections(rCurrentBase.Collections(), rPreviousBase.Collections(), std::make_index_sequence<std::tuple_size_v<decltype(rCurrentBase.Collections())>>{});
}

void FramePostRenderBase::Update([[maybe_unused]] game::Frame& __restrict rFrame, [[maybe_unused]] const game::Frame& __restrict rPreviousFrame, [[maybe_unused]] const game::FrameInput& __restrict rFrameInput, [[maybe_unused]] const FrameStaticData& rStaticData)
{
	game::FramePostRender& rCurrent = rFrame.postRender;
	const game::FramePostRender& rPrevious = rPreviousFrame.postRender;

#if defined(BT_CLIENT)
	rFrame.interpolate.frameFlags.Clear({FrameFlags::kInterpolate, FrameFlags::kPostRender});
	rFrame.interpolate.frameFlags.Set(FrameFlags::kPostRender);
#endif

	// Carry persistent state forward from the previous frame
	rCurrent.randomEngine = rPrevious.randomEngine;
	rCurrent.uiNextUuid = rPrevious.uiNextUuid;
#if defined(BT_CLIENT)
	rCurrent.uiNextSoundUuid = rPrevious.uiNextSoundUuid;
	rCurrent.uiNextVisualUuid = rPrevious.uiNextVisualUuid;
#endif
	rCurrent.uiFrameId = rPrevious.uiFrameId;
	rCurrent.alignments.CopyFrom(rPrevious.alignments);

	ForEachPostRenderUpdate(PostRenderBaseTypes{}, rFrame, rPreviousFrame, rStaticData);

	// Setup pusher zones for spatial acceleration
	PushersInterpolate::SetupZones(rFrame, rStaticData.vecArea);
}

void FramePostRenderBase::PreCollision([[maybe_unused]] game::Frame& __restrict rFrame, [[maybe_unused]] const game::Frame& __restrict rPreviousFrame, [[maybe_unused]] const FrameStaticData& rStaticData)
{
	ForEachPostRenderPreCollision(PostRenderBaseTypes{}, rFrame, rPreviousFrame, rStaticData);
}

void FramePostRenderBase::PostCollision([[maybe_unused]] game::Frame& __restrict rFrame, [[maybe_unused]] const game::Frame& __restrict rPreviousFrame, [[maybe_unused]] const FrameStaticData& rStaticData)
{
	ForEachPostRenderPostCollision(PostRenderBaseTypes{}, rFrame, rPreviousFrame, rStaticData);
}

void FramePostRenderBase::AreaDamage([[maybe_unused]] game::Frame& __restrict rFrame, [[maybe_unused]] const game::Frame& __restrict rPreviousFrame, [[maybe_unused]] const FrameStaticData& rStaticData)
{
	ForEachPostRenderAreaDamage(PostRenderBaseTypes{}, rFrame, rPreviousFrame, rStaticData);
}

void FramePostRenderBase::Transfer([[maybe_unused]] game::Frame& __restrict rFrame, [[maybe_unused]] const FrameStaticData& rStaticData)
{
	ForEachPostRenderTransfer(PostRenderBaseTypes{}, rFrame, rStaticData);
}

void FramePostRenderBase::Destroy([[maybe_unused]] game::Frame& __restrict rFrame, [[maybe_unused]] const FrameStaticData& rStaticData)
{
	ForEachPostRenderDestroy(PostRenderBaseTypes{}, rFrame, rStaticData);
}

void FramePostRenderBase::Spawn([[maybe_unused]] game::Frame& __restrict rFrame, [[maybe_unused]] const FrameStaticData& rStaticData)
{
	ForEachPostRenderSpawn(PostRenderBaseTypes{}, rFrame, rStaticData);
}

#if defined(BT_CLIENT)
void FrameInterpolateBase::BeginRender([[maybe_unused]] int64_t iCommandBuffer, [[maybe_unused]] const std::unordered_map<GridCoord, game::FrameInterpolate>& rRenderInterpolates, [[maybe_unused]] const std::vector<GridCoord>& rActiveCoords)
{
	ForEachBeginRender(InterpolateTypes{}, iCommandBuffer, rRenderInterpolates, rActiveCoords);
}

void FrameInterpolateBase::Render([[maybe_unused]] const game::FrameInterpolate& __restrict rCurrent, [[maybe_unused]] int64_t iCommandBuffer)
{
	ForEachInterpolateRender(InterpolateTypes{}, rCurrent, iCommandBuffer);
}

void FrameInterpolateBase::EndRender([[maybe_unused]] int64_t iCommandBuffer)
{
	ForEachEndRender(InterpolateTypes{}, iCommandBuffer);
}
#endif // BT_CLIENT

void RunFrameTick(const ActiveFrameRef& rRef, int64_t iTickCounter, float fCurrentTime)
{
	// Mark this thread as inside a deterministic tick so a stray render-path GlobalElevation/GlobalNormal
	// call (which walks mCoordFrames with libm trig) fails fast instead of silently desyncing across CPUs.
	common::FrameTickScope frameTickScope;

	// Verify MXCSR has not been corrupted by external calls (audio, Vulkan, etc.)
	unsigned int uiControlWord = 0;
	_controlfp_s(&uiControlWord, 0, 0);
	ASSERT((uiControlWord & _MCW_DN) == _DN_FLUSH);
	ASSERT((uiControlWord & _MCW_RC) == _RC_NEAR);

	game::Frame& rNext = *rRef.pNext;
	const game::Frame& rCurrent = *rRef.pCurrent;
	const FrameStaticData& rStaticData = *rRef.pStaticData;

#if defined(BT_SERVER)
	// NavData is derived from placements + per-template NavContour. Build it here on the
	// per-coord dispatch thread (naturally parallel across coords) on the first tick after
	// a coord is created or reloaded from save. Client receives prebuilt navData over the
	// wire and never enters this branch (server-only NavContour).
	if (!rStaticData.bNavDataBuilt && !rStaticData.islands.empty())
	{
		ScopedSuppressAllocationTracking suppress;
		BuildCellNavData(rStaticData.navData, rStaticData.islands);
		rStaticData.bNavDataBuilt = true;
	}
#endif

	// Per-cell elevation grid (purely derived from islands + shared heightmaps). Both client
	// and server build their own bit-identical copy here — same deterministic placements, same
	// shared heightmaps, /fp:strict math — so it stays out of the CRC and is never serialized.
	// Builds before any sim phase below so every FrameElevation/FrameNormal caller this tick
	// sees a populated grid.
	if (rStaticData.elevationGrid.empty() && !rStaticData.islands.empty())
	{
		ScopedSuppressAllocationTracking suppress;
		gpIslandTerrain->BuildElevationGrid(rStaticData.coord, rStaticData.islands, rStaticData.elevationGrid);
	}

#if defined(BT_CLIENT)
	// Render-only cache: per-placement flattened query (inverse-rotation sin/cos + template footprint /
	// heightmap pointer / dims) for GlobalElevation/GlobalNormal (ProjectToBaseHeight). Built here alongside
	// the elevation grid — placement rotation and template are static for the cell's life — so the render
	// path does zero hash lookups and zero libm trig per call. The server never calls GlobalElevation, so
	// skip it there.
	if (rStaticData.islandRenderQueries.empty() && !rStaticData.islands.empty())
	{
		ScopedSuppressAllocationTracking suppress;
		rStaticData.BuildRenderPlacementCache(*gpIslandTerrain);
	}
#endif

	// Phase 1: Interpolate
	game::FrameInterpolate::AllocateAndCopy(rNext.interpolate, rCurrent.interpolate);
	game::FrameInterpolate::Update(rNext.interpolate, rCurrent, kfDeltaTime);
	rNext.interpolate.iTick = iTickCounter;
	rNext.interpolate.fCurrentTime = fCurrentTime;

	// Phase 2: PostRender
	game::FramePostRender::AllocateAndCopy(rNext.postRender, rCurrent.postRender);
	game::FramePostRender::Update(rNext, rCurrent, *rRef.pFrameInput, rStaticData);

	// Phase 3: Collision
	game::FramePostRender::PreCollision(rNext, rCurrent, rStaticData);
	Collision::Collide(rNext.postRender.alignments, rStaticData.vecArea);
	game::FramePostRender::PostCollision(rNext, rCurrent, rStaticData);
	game::FramePostRender::AreaDamage(rNext, rCurrent, rStaticData);

	// Phase 4: Transfer
	game::FramePostRender::Transfer(rNext, rStaticData);

	// Phase 5: Destroy/Spawn
	game::FramePostRender::Destroy(rNext, rStaticData);
	game::FramePostRender::Spawn(rNext, *rRef.pFrameInput, rStaticData);

	// Compute CRCs after all phases complete
	rNext.postRender.sharedCrc = rNext.Crcs();
}

} // namespace engine
