#pragma once

#if defined(BT_SERVER)

#include "File/DifferenceStreamWriter.h"

namespace engine
{

class DifferenceStreamReader
{
public:

	DifferenceStreamReader(const std::filesystem::path& rFilename, game::Frame& rSavedStart, game::FrameInput& rInitialDifference, bool& rbLoaded, bool& rbVersionMismatch, int64_t& riMismatchFileVersion, int64_t& riMismatchExpectedVersion, bool bRecordsInitialChecksum = true);

	bool IsTerminalTick(int64_t iTick) const;
	bool TerminalConsumed() const;
	void ValidateChecksum(int64_t iTick, const game::Frame& rSavedCurrent);

	// Post-dispatch records are one-shot events, so an exact tick match is the only hit: a record is never
	// carried forward the way a difference is.
	bool LoadPostDispatch(int64_t iTick, game::FrameInput& rPostDispatch);
	bool LoadDifference(int64_t iTick, game::FrameInput& rDifference);

private:

	enum class ReaderFlags : uint8_t
	{
		kRecordsInitialChecksum    = 0x01,
		kTerminalChecksumValidated = 0x02,
		kFullFramesActive          = 0x04,
		kFullFramesInvalid         = 0x08,
	};

	static bool ReadDifferenceRecord(std::istream& rStream, int64_t& riTick, game::FrameInput& rDifference);
	bool TryReadFullFrame(game::Frame& rSavedFrame);
	void DisableFullFrames(bool bStaleBaseline = false);

public:
	game::Frame mSavedEnd {};

	int64_t miDifferenceCount = 0;

private:

	std::vector<DifferenceRecord> mDifferences;
	typename std::vector<DifferenceRecord>::iterator mDifferencesIterator = mDifferences.end();
	game::FrameInput mCurrentDifference {};

	std::vector<DifferenceRecord> mPostDispatchRecords;
	typename std::vector<DifferenceRecord>::iterator mPostDispatchRecordsIterator = mPostDispatchRecords.end();

	std::vector<common::crc_t> mChecksums;
public:
	int64_t miStartTick = 0;
private:
	common::Flags<ReaderFlags> mReaderFlags;

	std::stringstream mFullFramesStream;
	int64_t miFullFramesIndex = 0;
};

} // namespace engine

#endif // defined(BT_SERVER)
