#include "FrameBase.h"

#include "Frame/FrameStaticData.h"
#include "Frame/IslandTerrain.h"
#include "Frame/NavBuild.h"

#include "Frame/Frame.h"

namespace engine
{

common::crc_t FrameInterpolateBase::Crcs() const
{
	common::crc_t uiSharedCrc = 0;

	uiSharedCrc = (uiSharedCrc ^ common::Crc(iTick)) * common::kCrcMultiplier;
	uiSharedCrc = (uiSharedCrc ^ common::Crc(fCurrentTime)) * common::kCrcMultiplier;
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
	bEqual &= common::LogDifference<"fCurrentTime">(fCurrentTime, rOther.fCurrentTime);
	bEqual &= common::LogDifference<"fDeltaTime">(fDeltaTime, rOther.fDeltaTime);
	bEqual &= LogDifferencesCollections(ServerCollections(), rOther.ServerCollections(), std::make_integer_sequence<int64_t, static_cast<int64_t>(std::tuple_size_v<decltype(ServerCollections())>)> {});
	return bEqual;
}

void FrameInterpolateBase::Write(std::ostream& rStream) const
{
	common::Write(rStream, iTick);
	common::Write(rStream, fCurrentTime);
	common::Write(rStream, fDeltaTime);

	std::apply([&](const auto&... rCollections)
	{
		(CollectionWrite(rStream, rCollections, rCollections.Members()), ...);
	}, Collections());
}

void FrameInterpolateBase::Read(std::istream& rStream)
{
	common::Read(rStream, iTick);
	common::Read(rStream, fCurrentTime);
	common::Read(rStream, fDeltaTime);

	std::apply([&](auto&... rCollections)
	{
		(CollectionRead(rStream, rCollections, rCollections.Members()), ...);
	}, Collections());
}

void FrameInterpolateBase::ServerRead(std::istream& rStream)
{
	common::Read(rStream, iTick);
	common::Read(rStream, fCurrentTime);
	common::Read(rStream, fDeltaTime);

	std::apply([&](auto&... rCollections)
	{
		(SharedCollectionRead(rStream, rCollections), ...);
	}, ServerCollections());
}

common::crc_t FramePostRenderBase::Crcs() const
{
	common::crc_t uiCrc = 0;

	uiCrc = (uiCrc ^ randomEngine.Crc()) * common::kCrcMultiplier;
	uiCrc = (uiCrc ^ common::Crc(uiNextUuid)) * common::kCrcMultiplier;
	uiCrc = (uiCrc ^ common::Crc(uiFrameIdentifier)) * common::kCrcMultiplier;
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
	common::Write(rStream, randomEngine);
	common::Write(rStream, uiNextUuid);
#if defined(BT_CLIENT)
	common::Write(rStream, uiNextSoundUuid);
	common::Write(rStream, uiNextVisualUuid);
#endif
	common::Write(rStream, uiFrameIdentifier);
	alignments.Write(rStream);

	std::apply([&](const auto&... rCollections)
	{
		(CollectionWrite(rStream, rCollections, rCollections.Members()), ...);
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
	common::Read(rStream, uiFrameIdentifier);
	alignments.Read(rStream);

	std::apply([&](auto&... rCollections)
	{
		(CollectionRead(rStream, rCollections, rCollections.Members()), ...);
	}, Collections());
}

void FramePostRenderBase::ServerRead(std::istream& rStream)
{
	uint64_t uiRandomState = 0;
	common::Read(rStream, uiRandomState);
	randomEngine.SetSerializedState(uiRandomState);
	common::Read(rStream, uiNextUuid);
	// Server does not write uiNextSoundUuid or uiNextVisualUuid
	common::Read(rStream, uiFrameIdentifier);
	alignments.Read(rStream);

	std::apply([&](auto&... rCollections)
	{
		(SharedCollectionRead(rStream, rCollections), ...);
	}, ServerCollections());
}

void FrameInterpolateBase::AllocateAndCopy([[maybe_unused]] game::FrameInterpolate& __restrict rCurrent, [[maybe_unused]] const game::FrameInterpolate& __restrict rPrevious)
{
	FrameInterpolateBase& rCurrentBase = rCurrent;
	const FrameInterpolateBase& rPreviousBase = rPrevious;
	AllocateAndCopyCollections(rCurrentBase.Collections(), rPreviousBase.Collections(), std::make_integer_sequence<int64_t, static_cast<int64_t>(std::tuple_size_v<decltype(rCurrentBase.Collections())>)> {});
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

void FramePostRenderBase::Update([[maybe_unused]] game::Frame& __restrict rFrame, [[maybe_unused]] const game::Frame& __restrict rPreviousFrame, [[maybe_unused]] const game::FrameInput& __restrict rFrameInput, [[maybe_unused]] const FrameStaticData& rStaticData)
{
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
	rCurrent.alignments.CopyFrom(rPrevious.alignments);

	ForEachPostRenderUpdate(PostRenderBaseTypes {}, rFrame, rPreviousFrame, rStaticData);

	// Setup pusher zones for spatial acceleration
	PushersInterpolate::SetupZones(rFrame, LocalFrameArea());
}

void RunFrameTick(const ActiveFrameReference& rReference, int64_t iTickCounter, float fCurrentTime)
{
	// Mark this thread as inside a deterministic tick so a stray render-path GlobalElevation/GlobalNormal
	// call (which walks mCoordinateFrames with libm trig) fails fast instead of silently desyncing across CPUs.
	common::FrameTickScope frameTickScope;

	// Verify MXCSR has not been corrupted by external calls (audio, Vulkan, etc.)
	unsigned int uiControlWord = 0;
	_controlfp_s(&uiControlWord, 0, 0);
	ASSERT((uiControlWord & _MCW_DN) == _DN_FLUSH);
	ASSERT((uiControlWord & _MCW_RC) == _RC_NEAR);

	game::Frame& rNext = *rReference.pNext;
	const game::Frame& rCurrent = *rReference.pCurrent;
	const FrameStaticData& rStaticData = *rReference.pStaticData;

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
	// Builds before any sim phase below so every FrameElevationSampler::Sample/FrameNormal caller this tick
	// sees a populated grid.
	if (rStaticData.elevationGrid.empty() && !rStaticData.islands.empty())
	{
		ScopedSuppressAllocationTracking suppress;
		gpIslandTerrain->BuildElevationGrid(rStaticData.islands, rStaticData.elevationGrid);
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

	game::FrameInterpolate::AllocateAndCopy(rNext.interpolate, rCurrent.interpolate);
	game::FrameInterpolate::Update(rNext.interpolate, rCurrent, kfDeltaTime);
	rNext.interpolate.iTick = iTickCounter;
	rNext.interpolate.fCurrentTime = fCurrentTime;

	game::FramePostRender::AllocateAndCopy(rNext.postRender, rCurrent.postRender);
	game::FramePostRender::Update(rNext, rCurrent, *rReference.pFrameInput, rStaticData);

	game::FramePostRender::PreCollision(rNext, rCurrent, rStaticData);
	Collision::Collide(rNext.postRender.alignments, LocalFrameArea());
	game::FramePostRender::PostCollision(rNext, rCurrent, rStaticData);
	game::FramePostRender::AreaDamage(rNext, rCurrent, rStaticData);

	game::FramePostRender::Transfer(rNext, rStaticData);

	game::FramePostRender::Destroy(rNext, rStaticData);
	game::FramePostRender::Spawn(rNext, *rReference.pFrameInput, rStaticData);

	// Compute CRCs after all phases complete
	rNext.postRender.uiSharedCrc = rNext.Crc();
}

} // namespace engine
