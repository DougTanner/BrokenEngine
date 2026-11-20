#include "Pch.h"

#if defined(BT_SERVER)

#include "File/DifferenceStreamWriter.h"

#include "File/DifferenceStream.h"

namespace engine
{

DifferenceStreamWriter::DifferenceStreamWriter(const game::Frame& rSavedStart, const game::FrameInput& rInitialDifference, bool bRecordInitialChecksum)
{
	mDifferences.reserve(1'024);

	int64_t iStartTick = rSavedStart.interpolate.iTick;
	TransferViaStream(rSavedStart, mSavedStart);
	mInitialDifference = rInitialDifference;
	mCurrentDifference = rInitialDifference;
	LOG(kDefault, kVerbose, "DifferenceStreamWriter at frame {}: Saved start: {} Initial difference: {}", iStartTick, mSavedStart.Crc(), mInitialDifference.Crc());

	// Normal streams validate their saved start on the first replay tick. A coordinate activated by
	// a transfer instead validates its post-transfer frame when its one-shot input is consumed.
	mChecksums.reserve(1'024);
	mbRecordsInitialChecksum = bRecordInitialChecksum;
	if (mbRecordsInitialChecksum)
	{
		mChecksums.push_back(mSavedStart.Crc());
		LOG(kReplay, kVerbose, "Checksum DifferenceStreamWriter {}: {}", iStartTick, *std::prev(mChecksums.end()));
	}

	if constexpr (kbReplayFullFrames)
	{
		if (mbRecordsInitialChecksum)
		{
			mFullFramesStream << mSavedStart;
		}
	}
}

void DifferenceStreamWriter::Update(int64_t iTick, const game::FrameInput& rDifference, const game::Frame& rSavedCurrent)
{
	mChecksums.push_back(rSavedCurrent.Crc());
	LOG(kReplay, kVerbose, "Checksum DifferenceStreamWriter Update {}: {}", rSavedCurrent.interpolate.iTick, *std::prev(mChecksums.end()));

	if constexpr (kbReplayFullFrames)
	{
		mFullFramesStream << rSavedCurrent;
	}

	if (rDifference.Crc() == mCurrentDifference.Crc())
	{
		return;
	}

	mDifferences.emplace_back(iTick, rDifference);
	mCurrentDifference = rDifference;
}

bool DifferenceStreamWriter::Save(FileFlags_t fileFlags, const std::filesystem::path& rFilename, const game::Frame& rSavedEnd)
{
	int64_t iDifferenceCount = std::ssize(mDifferences);
	int64_t iPostDispatchCount = std::ssize(mPostDispatchRecords);

	bool bHeaderWritten = gpFileManager->WriteFileAtomically(fileFlags, rFilename, [&](std::fstream& rHeaderStream)
	{
		WriteVersionHeader<game::Frame>(rHeaderStream);
		WriteVersionHeader<game::FrameInput>(rHeaderStream);

		rHeaderStream << mSavedStart;
		rHeaderStream << mInitialDifference;
		common::Write(rHeaderStream, iDifferenceCount);
		rHeaderStream << rSavedEnd;
		common::Write(rHeaderStream, iPostDispatchCount);
		for (const DifferenceRecord& rRecord : mPostDispatchRecords)
		{
			WriteDifferenceRecord(rHeaderStream, rRecord.iTick, rRecord.input);
		}
	});
	LOG(kReplay, kVerbose, "DifferenceStreamWriter save at frame {}: Count {} Checksum {}", rSavedEnd.interpolate.iTick, iDifferenceCount, rSavedEnd.Crc());

	std::filesystem::path framesFilename = std::filesystem::path(rFilename).concat(".frames");
	bool bFramesWritten = gpFileManager->WriteFileAtomically(fileFlags, framesFilename, [&](std::fstream& rFramesStream)
	{
		for (const DifferenceRecord& rRecord : mDifferences)
		{
			WriteDifferenceRecord(rFramesStream, rRecord.iTick, rRecord.input);
		}
	});

	// The caller records the terminal input and its saved end frame before Save. Do not duplicate
	// either boundary here: the reader consumes that terminal input before retiring the coord. That
	// input carries no post-dispatch events; those are exact-tick one-shots outside carry-forward.
	std::filesystem::path checksumsFilename = std::filesystem::path(rFilename).concat(".checksums");
	bool bChecksumsWritten = gpFileManager->WriteFileAtomically(fileFlags, checksumsFilename, [&](std::fstream& rChecksumStream)
	{
		if (!mChecksums.empty())
		{
			common::Write(rChecksumStream, mChecksums);
		}
	});

	// Track the first sibling that failed so a torn set can be reported and cleaned up as a whole.
	std::filesystem::path failedFilename;
	if (!bHeaderWritten)
	{
		failedFilename = rFilename;
	}
	else if (!bFramesWritten)
	{
		failedFilename = std::move(framesFilename);
	}
	else if (!bChecksumsWritten)
	{
		failedFilename = std::move(checksumsFilename);
	}

	if constexpr (kbReplayFullFrames)
	{
		std::filesystem::path fullFramesFilename = std::filesystem::path(rFilename).concat(".fullframes");
		bool bFullFramesWritten = gpFileManager->WriteFileAtomically(fileFlags, fullFramesFilename, [&](std::fstream& rFullFramesStream)
		{
			rFullFramesStream << mFullFramesStream.view();
		});
		if (!bFullFramesWritten && failedFilename.empty())
		{
			failedFilename = std::move(fullFramesFilename);
		}
	}

	// Any in-process write failure leaves a torn recording; delete the whole sibling set so a partial replay isn't loaded.
	if (!failedFilename.empty())
	{
		LOG(kDefault, kError, "DifferenceStreamWriter save failed writing \"{}\"; deleting partial replay set", failedFilename.string());
		CleanupFiles(rFilename);
	}

	return failedFilename.empty();
}

void DifferenceStreamWriter::CleanupFiles(const std::filesystem::path& rFilename) const
{
	auto RemovePartialFile = [&](const std::filesystem::path& rPartialFilename)
	{
		try
		{
			gpFileManager->RemoveFile(rPartialFilename);
		}
		catch (const std::filesystem::filesystem_error& rException)
		{
			LOG(kDefault, kError, "DifferenceStreamWriter cleanup failed removing \"{}\": {}", rPartialFilename.string(), rException.what());
		}
	};

	RemovePartialFile(rFilename);
	RemovePartialFile(std::filesystem::path(rFilename).concat(".frames"));
	RemovePartialFile(std::filesystem::path(rFilename).concat(".checksums"));
	if constexpr (kbReplayFullFrames)
	{
		RemovePartialFile(std::filesystem::path(rFilename).concat(".fullframes"));
	}
}

void DifferenceStreamWriter::WriteDifferenceRecord(std::ostream& rStream, int64_t iTick, const game::FrameInput& rDifference)
{
	common::Write(rStream, iTick);
	rStream << rDifference;
}

} // namespace engine

#endif // defined(BT_SERVER)
