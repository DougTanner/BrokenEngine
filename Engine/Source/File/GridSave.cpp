#include "Pch.h"

#if defined(BT_SERVER)

#include "File/GridSave.h"

#include "GameBase.h"

#include "Game.h"

namespace engine
{

bool WriteGridSave(const FileFlags_t& rFlags, const std::filesystem::path& rFilename, GridCoord clientGridCoordinate)
{
	int64_t iFrameCount = static_cast<int64_t>(game::gpGame->mCells.size());
	int64_t iVersion = game::Frame::kiVersion;

	bool bWritten = engine::gpFileManager->WriteFileAtomically(rFlags, rFilename, [&](std::fstream& fileStream)
	{
		engine::WriteVersionHeader<game::Frame>(fileStream);

		common::Write(fileStream, iFrameCount);
		clientGridCoordinate.Write(fileStream);
		common::Write(fileStream, game::gpGame->miNextGlobalId);

		game::WriteSaveState(fileStream);

		// Sort by coord key for deterministic output
		std::vector<uint64_t> keys;
		keys.reserve(game::gpGame->mCells.size());
		for (const auto& [rCoordinate, rCell] : game::gpGame->mCells)
		{
			keys.push_back(rCoordinate.ToKey());
		}
		std::sort(keys.begin(), keys.end());

		for (uint64_t uiKey : keys)
		{
			engine::GridCoord coordinate = engine::GridCoord::FromKey(uiKey);
			coordinate.Write(fileStream);
			// NavData is rebuilt lazily on first RunFrameTick — don't persist it.
			game::gpGame->mCells.at(coordinate).staticData.Write(fileStream, /*bIncludeNavigationData=*/false);
			fileStream << *game::gpGame->mCells.at(coordinate).pCurrent;
		}
	});

	LOG(kDefault, kDebug, "WriteGrid {} iVersion: {} iFrameCount: {} Committed: {}", rFilename, iVersion, iFrameCount, bWritten);
	return bWritten;
}

bool ReadGridSave(const FileFlags_t& rFlags, const std::filesystem::path& rFilename, GridCoord& rClientGridCoordinate)
{
	StagedGridSave stagedGrid;
	if (!ReadGridSave(rFlags, rFilename, stagedGrid))
	{
		if (stagedGrid.bHeaderValidated)
		{
			// Keep the established save-load failure state for callers that rebuild a fresh game after false.
			game::gpGame->mCells.clear();
			game::ResetSaveState();
		}
		return false;
	}

	rClientGridCoordinate = stagedGrid.clientGridCoordinate;
	AdoptGridSave(std::move(stagedGrid));
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
	// deserialization throws std::ios_base::failure (or .at()/bad_alloc) — abort the load gracefully
	// (return false) so a hand-crafted or truncated save file can't overrun a buffer or crash the server.
	bool bHasLoadedClock = false;
	try
	{
		// Bound the frame count against the stream (each coord frame serializes at least its GridCoord).
		common::ValidateDeserializedCount(iFrameCount, sizeof(engine::GridCoord), fileStream, "ReadGrid frames");
		rStagedGrid.clientGridCoordinate.Read(fileStream);
		common::Read(fileStream, rStagedGrid.iNextGlobalId);
		// Trust boundary (save file): the global-id counter is a monotonic positive int64 (fresh games start at
		// 1). Validate now but apply only past the stream-good gate below, so a failed or silently-torn load
		// leaves the fresh-fallback game minting from a clean base rather than a garbage/advanced one.
		if (rStagedGrid.iNextGlobalId <= 0)
		{
			throw std::ios_base::failure("iNextGlobalId");
		}

		game::ReadSaveState(fileStream, rStagedGrid.saveState);

		for (int64_t i = 0; i < iFrameCount; ++i)
		{
			engine::GridCoord coordinate;
			coordinate.Read(fileStream);
			auto [itCell, bInserted] = rStagedGrid.cells.try_emplace(coordinate);
			if (!bInserted)
			{
				throw std::ios_base::failure("duplicate grid coord");
			}

			engine::Cell& rCell = itCell->second;
			rCell.staticData.Read(fileStream, /*bIncludeNavigationData=*/false);
			rCell.staticData.coordinate = coordinate;
			// Trust boundary (save file): generation places only loaded templates with centers inside the cell's base
			// area. The center tests are range tests, so NaN and both infinities fail them too.
			for (const IslandPlacement& rPlacement : rCell.staticData.islands)
			{
				if (!gpIslandTerrain->mIslands.contains(rPlacement.islandCrc))
				{
					throw std::ios_base::failure("island placement template");
				}
				if (!(rPlacement.f2WorldPosition.x >= kfBaseAreaMinimumX && rPlacement.f2WorldPosition.x <= kfBaseAreaMaximumX)
				 || !(rPlacement.f2WorldPosition.y >= kfBaseAreaMinimumY && rPlacement.f2WorldPosition.y <= kfBaseAreaMaximumY))
				{
					throw std::ios_base::failure("island placement center");
				}
				if (!std::isfinite(rPlacement.fRotation))
				{
					throw std::ios_base::failure("island placement rotation");
				}
			}
			auto pFrame = std::make_unique<game::Frame>();
			fileStream >> *pFrame;
			rCell.pCurrent = std::move(pFrame);
			rCell.pNext = std::make_unique<game::Frame>();

			int64_t iTick = rCell.pCurrent->interpolate.iTick;
			float fCurrentTime = rCell.pCurrent->interpolate.fCurrentTime;
			if (!bHasLoadedClock)
			{
				if (iTick < 0 || iTick > std::numeric_limits<int64_t>::max() - engine::TimeStep::kiMaximumAccumulatorTicks || !std::isfinite(fCurrentTime))
				{
					throw std::ios_base::failure("invalid frame clock");
				}

				rStagedGrid.iTick = iTick;
				rStagedGrid.fCurrentTime = fCurrentTime;
				bHasLoadedClock = true;
			}
			else if (iTick != rStagedGrid.iTick || std::bit_cast<uint32_t>(fCurrentTime) != std::bit_cast<uint32_t>(rStagedGrid.fCurrentTime))
			{
				throw std::ios_base::failure("inconsistent frame clocks");
			}
		}

		// Save-file clientGridCoordinate is adopted by ServerLoad, Autoload, and Quickload as the followed cell, so it
		// must match a cell just read; a mismatch is a torn grid.
		if (!rStagedGrid.cells.contains(rStagedGrid.clientGridCoordinate))
		{
			throw std::ios_base::failure("client grid coord absent from cells");
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

void AdoptGridSave(StagedGridSave&& rStagedGrid)
{
	game::gpGame->mCells = std::move(rStagedGrid.cells);
	game::AdoptSaveState(std::move(rStagedGrid.saveState));
	game::gpGame->miNextGlobalId = rStagedGrid.iNextGlobalId;
	int64_t iTickCounter = rStagedGrid.iTick;
	ASSERT(iTickCounter >= 0);
	game::gpGame->miTickCounter = iTickCounter;
	game::gpGame->mfCurrentTime = rStagedGrid.fCurrentTime;
}

} // namespace engine

#endif // BT_SERVER
