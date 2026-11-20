#pragma once

#if defined(BT_SERVER)

namespace engine
{

struct DifferenceRecord
{
	int64_t iTick = 0;
	game::FrameInput input {};
};

class DifferenceStreamWriter
{
public:

	DifferenceStreamWriter(const game::Frame& rSavedStart, const game::FrameInput& rInitialDifference, bool bRecordInitialChecksum = true);

	void Update(int64_t iTick, const game::FrameInput& rDifference, const game::Frame& rSavedCurrent);
	bool Save(FileFlags_t fileFlags, const std::filesystem::path& rFilename, const game::Frame& rSavedEnd);
	void CleanupFiles(const std::filesystem::path& rFilename) const;

private:

	static void WriteDifferenceRecord(std::ostream& rStream, int64_t iTick, const game::FrameInput& rDifference);

	game::Frame mSavedStart {};
	game::FrameInput mInitialDifference {};

	std::vector<DifferenceRecord> mDifferences;
	game::FrameInput mCurrentDifference {};

public:
	// Records an event that happens after iTick's dispatch. Appends are strictly increasing by construction,
	// because the caller harvests at most one batch per tick.
	std::vector<DifferenceRecord> mPostDispatchRecords;
private:

	std::vector<common::crc_t> mChecksums;
	bool mbRecordsInitialChecksum = false;

	std::stringstream mFullFramesStream;
};

} // namespace engine

#endif // defined(BT_SERVER)
