#include "Pch.h"

#if defined(BT_SERVER)

#include "File/GridSave.h"

#include "GameBase.h"

namespace engine
{

bool WriteGridSave(GameBase& rGameBase, const FileFlags_t& rFlags, const std::filesystem::path& rFilename, GridCoord clientGridCoord)
{
	int64_t iFrameCount = static_cast<int64_t>(rGameBase.mCoordFrames.size());
	int64_t iVersion = game::Frame::kiVersion;

	bool bWritten = engine::gpFileManager->WriteFileAtomically(rFlags, rFilename, [&](std::fstream& fileStream)
	{
		engine::WriteVersionHeader<game::Frame>(fileStream);

		common::Write(fileStream, iFrameCount);
		clientGridCoord.Write(fileStream);
		common::Write(fileStream, rGameBase.NextGlobalId());

		game::WriteSaveState(fileStream);

		// Sort by coord key for deterministic output
		std::vector<uint64_t> keys;
		keys.reserve(rGameBase.mCoordFrames.size());
		for (const auto& [rCoord, rFrames] : rGameBase.mCoordFrames)
		{
			keys.push_back(rCoord.ToKey());
		}
		std::sort(keys.begin(), keys.end());

		for (uint64_t uiKey : keys)
		{
			engine::GridCoord coord = engine::GridCoord::FromKey(uiKey);
			coord.Write(fileStream);
			// NavData is rebuilt lazily on first RunFrameTick — don't persist it.
			rGameBase.mCoordFrames.at(coord).staticData.Write(fileStream, /*bIncludeNavData=*/false);
			fileStream << *rGameBase.mCoordFrames.at(coord).pCurrent;
		}
	});

	LOG(kDefault, kDebug, "WriteGrid {} iVersion: {} iFrameCount: {} Committed: {}", rFilename, iVersion, iFrameCount, bWritten);
	return bWritten;
}

bool ReadGridSave(GameBase& rGameBase, const FileFlags_t& rFlags, const std::filesystem::path& rFilename, GridCoord& rClientGridCoord)
{
	StagedGridSave stagedGrid;
	if (!ReadGridSave(rFlags, rFilename, stagedGrid))
	{
		if (stagedGrid.bHeaderValidated)
		{
			// Keep the established save-load failure state for callers that rebuild a fresh game after false.
			rGameBase.mCoordFrames.clear();
			game::ResetSaveState();
		}
		return false;
	}

	rClientGridCoord = stagedGrid.clientGridCoord;
	AdoptGridSave(rGameBase, std::move(stagedGrid));
	return true;
}

bool ReadGridSave(const FileFlags_t& rFlags, const std::filesystem::path& rFilename, StagedGridSave& rStagedGrid)
{
	std::fstream fileStream = engine::gpFileManager->OpenFile(rFlags, rFilename);

	int64_t iVersion = 0;
	int64_t iSize = 0;
	if (!engine::ReadAndValidateVersionHeader<game::Frame>(fileStream, iVersion, iSize))
	{
		LOG(kDefault, kError, "ReadGrid {} failed: version {} != {}", rFilename, iVersion, game::Frame::kiVersion);
		return false;
	}
	rStagedGrid.bHeaderValidated = true;

	int64_t iFrameCount = 0;
	common::Read(fileStream, iFrameCount);
	// Trust boundary (save file): a corrupt count/capacity anywhere in the grid / fleet / frame
	// deserialization throws CorruptStreamException (or .at()/bad_alloc) — abort the load gracefully
	// (return false) so a hand-crafted or truncated save file can't overrun a buffer or crash the server.
	bool bHasLoadedClock = false;
	try
	{
		// Bound the frame count against the stream (each coord frame serializes at least its GridCoord).
		common::ValidateDeserializedCount(iFrameCount, sizeof(engine::GridCoord), fileStream, "ReadGrid frames");
		rStagedGrid.clientGridCoord.Read(fileStream);
		common::Read(fileStream, rStagedGrid.iNextGlobalId);
		// Trust boundary (save file): the global-id counter is a monotonic positive int64 (fresh games start at
		// 1). Validate now but apply only past the stream-good gate below, so a failed or silently-torn load
		// leaves the fresh-fallback game minting from a clean base rather than a garbage/advanced one.
		if (rStagedGrid.iNextGlobalId <= 0)
		{
			throw common::CorruptStreamException("iNextGlobalId");
		}

		game::ReadSaveState(fileStream, rStagedGrid.saveState);

		for (int64_t i = 0; i < iFrameCount; ++i)
		{
			engine::GridCoord coord;
			coord.Read(fileStream);
			auto [itFrames, bInserted] = rStagedGrid.coordFrames.try_emplace(coord);
			if (!bInserted)
			{
				throw common::CorruptStreamException("duplicate grid coord");
			}

			engine::CoordFrames& rSub = itFrames->second;
			rSub.staticData.Read(fileStream, /*bIncludeNavData=*/false);
			rSub.staticData.coord = coord;
			// The serialized area is derived data, not trusted: the coord that was just read defines it.
			rSub.staticData.vecArea = engine::ComputeCanonicalFrameArea(coord);
			auto pFrame = std::make_unique<game::Frame>();
			fileStream >> *pFrame;
			rSub.pCurrent = std::move(pFrame);
			rSub.pNext = std::make_unique<game::Frame>();

			const int64_t iTick = rSub.pCurrent->interpolate.iTick;
			const float fCurrentTime = rSub.pCurrent->interpolate.fCurrentTime;
			if (!bHasLoadedClock)
			{
				if (iTick < 0 || iTick > std::numeric_limits<int64_t>::max() - engine::TimeStep::kiMaxAccumulatorTicks || !std::isfinite(fCurrentTime))
				{
					throw common::CorruptStreamException("invalid frame clock");
				}

				rStagedGrid.iTick = iTick;
				rStagedGrid.fCurrentTime = fCurrentTime;
				bHasLoadedClock = true;
			}
			else if (iTick != rStagedGrid.iTick || std::bit_cast<uint32_t>(fCurrentTime) != std::bit_cast<uint32_t>(rStagedGrid.fCurrentTime))
			{
				throw common::CorruptStreamException("inconsistent frame clocks");
			}
		}

		// Trust boundary (save file): the client grid coord is file-derived and every load entry
		// (ServerLoad/Autoload/Quickload) adopts it as the followed cell — it must name a frame we just read,
		// else the grid is torn. Reject uniformly here (replaces the former Quickload ASSERT on file data).
		if (!rStagedGrid.coordFrames.contains(rStagedGrid.clientGridCoord))
		{
			throw common::CorruptStreamException("client grid coord absent from frames");
		}

	}
	catch (const std::exception& rException)
	{
		LOG(kDefault, kError, "ReadGrid {} aborted: corrupt data: {}", rFilename, rException.what());
		return false;
	}

	if (!fileStream.good())
	{
		LOG(kDefault, kError, "ReadGrid {} aborted: stream failure after read", rFilename);
		return false;
	}

	LOG(kDefault, kDebug, "ReadGrid {} iVersion: {} iFrameCount: {}", rFilename, iVersion, iFrameCount);
	return true;
}

void AdoptGridSave(GameBase& rGameBase, StagedGridSave&& rStagedGrid)
{
	rGameBase.mCoordFrames = std::move(rStagedGrid.coordFrames);
	game::AdoptSaveState(std::move(rStagedGrid.saveState));
	rGameBase.SetNextGlobalId(rStagedGrid.iNextGlobalId);
	rGameBase.SetTickCounter(rStagedGrid.iTick);
	rGameBase.SetCurrentTime(rStagedGrid.fCurrentTime);
}

} // namespace engine

#endif // BT_SERVER
