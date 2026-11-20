#include "Pch.h"

#if defined(BT_SERVER)

#include "File/DifferenceStreamReader.h"

namespace engine
{

DifferenceStreamReader::DifferenceStreamReader(const std::filesystem::path& rFilename, game::Frame& rSavedStart, game::FrameInput& rInitialDifference, bool& rbLoaded, bool& rbVersionMismatch, int64_t& riMismatchFileVersion, int64_t& riMismatchExpectedVersion, bool bRecordsInitialChecksum)
{
	rbLoaded = false;
	rbVersionMismatch = false;
	riMismatchFileVersion = 0;
	riMismatchExpectedVersion = 0;

	std::fstream headerStream = gpFileManager->OpenFile({FileFlags::kRead}, rFilename);
	if (!headerStream)
	{
		return;
	}

	int64_t iSavedVersion = 0;
	int64_t iSavedSize = 0;
	if (!ReadAndValidateVersionHeader<game::Frame>(headerStream, iSavedVersion, iSavedSize))
	{
		LOG(kDefault, kWarning, "DifferenceStreamReader SAVED_TYPE version mismatch: file {} {}, expected {} {}", iSavedVersion, iSavedSize, game::Frame::kiVersion, sizeof(game::Frame));
		// Only a fully read header naming a different version is a version mismatch; a short read leaves the
		// values meaningless, and a matching version that failed the size check is a damaged header.
		if (headerStream && iSavedVersion != game::Frame::kiVersion)
		{
			rbVersionMismatch = true;
			riMismatchFileVersion = iSavedVersion;
			riMismatchExpectedVersion = game::Frame::kiVersion;
		}
		return;
	}

	int64_t iDifferenceVersion = 0;
	int64_t iDifferenceSize = 0;
	if (!ReadAndValidateVersionHeader<game::FrameInput>(headerStream, iDifferenceVersion, iDifferenceSize))
	{
		LOG(kDefault, kWarning, "DifferenceStreamReader DIFFERENCE_TYPE version mismatch: file {} {}, expected {} {}", iDifferenceVersion, iDifferenceSize, game::FrameInput::kiVersion, sizeof(game::FrameInput));
		if (headerStream && iDifferenceVersion != game::FrameInput::kiVersion)
		{
			rbVersionMismatch = true;
			riMismatchFileVersion = iDifferenceVersion;
			riMismatchExpectedVersion = game::FrameInput::kiVersion;
		}
		return;
	}

	headerStream >> rSavedStart;
	headerStream >> rInitialDifference;
	mCurrentDifference = rInitialDifference;
	common::Read(headerStream, miDifferenceCount);
	headerStream >> mSavedEnd;
	if (!headerStream || miDifferenceCount < 0 || rSavedStart.interpolate.iTick < 0
	 || mSavedEnd.interpolate.iTick < rSavedStart.interpolate.iTick
	 || mSavedEnd.interpolate.iTick > std::numeric_limits<int64_t>::max() - 1)
	{
		LOG(kDefault, kWarning, "DifferenceStreamReader header is invalid");
		return;
	}

	miStartTick = rSavedStart.interpolate.iTick;

	int64_t iPostDispatchCount = 0;
	common::Read(headerStream, iPostDispatchCount);
	if (!headerStream)
	{
		LOG(kDefault, kWarning, "DifferenceStreamReader post-dispatch count is missing");
		return;
	}
	// Bound the replay header's record count against the stream before allocating;
	// each record serializes an int64 tick plus at least the payload's own int64 count prefix.
	common::ValidateDeserializedCount(iPostDispatchCount, 2 * sizeof(int64_t), headerStream, "DifferenceStreamReader post-dispatch records");

	mPostDispatchRecords.reserve(iPostDispatchCount);
	for (int64_t i = 0; i < iPostDispatchCount; ++i)
	{
		int64_t iTick = 0;
		game::FrameInput postDispatch {};
		if (!ReadDifferenceRecord(headerStream, iTick, postDispatch))
		{
			LOG(kDefault, kWarning, "DifferenceStreamReader post-dispatch section doesn't match its count");
			return;
		}
		// A coord activated by a transfer records that transfer at its own start tick, so unlike a
		// difference record the start tick is in range. The terminal tick is not: recording stops
		// before it is dispatched, so no post-dispatch event can exist there.
		if (iTick < miStartTick || iTick > mSavedEnd.interpolate.iTick
		 || (!mPostDispatchRecords.empty() && iTick <= mPostDispatchRecords.back().iTick))
		{
			LOG(kDefault, kWarning, "DifferenceStreamReader post-dispatch section has non-canonical tick data");
			return;
		}
		mPostDispatchRecords.emplace_back(iTick, std::move(postDispatch));
	}
	mPostDispatchRecordsIterator = mPostDispatchRecords.begin();

	if (headerStream.peek() != std::char_traits<char>::eof())
	{
		LOG(kDefault, kWarning, "DifferenceStreamReader header has trailing data");
		return;
	}
	// end + 1 is representable by the header gate above. Bound the span before loading differences
	// so a hostile tick range cannot overflow later byte-count calculations or drive allocations.
	int64_t iChecksumSpan = mSavedEnd.interpolate.iTick - rSavedStart.interpolate.iTick + 1;
	if (iChecksumSpan > std::numeric_limits<int64_t>::max() / static_cast<int64_t>(sizeof(common::crc_t)))
	{
		LOG(kDefault, kWarning, "Checksum range is too large");
		return;
	}

	LOG(kDefault, kVerbose, "DifferenceStreamReader at frame {}: Saved start: {} Initial difference: {}", miStartTick, rSavedStart.Crc(), rInitialDifference.Crc());

	// Every writer publishes an empty .frames sibling when no input changes. Require and fully consume it
	// so replay staging never accepts a torn stream set.
	std::fstream fileStream = gpFileManager->OpenFile({FileFlags::kRead}, std::filesystem::path(rFilename).concat(".frames"));
	if (!fileStream)
	{
		LOG(kDefault, kWarning, "Recorded frames file is missing");
		return;
	}
	// Bound the replay .frames file's difference count against the stream before
	// allocating; each record serializes at least an int64 tick plus one byte of difference.
	common::ValidateDeserializedCount(miDifferenceCount, sizeof(int64_t) + 1, fileStream, "DifferenceStreamReader differences");

	mDifferences.reserve(miDifferenceCount);
	for (int64_t i = 0; i < miDifferenceCount; ++i)
	{
		int64_t iTick = 0;
		game::FrameInput difference {};
		if (!ReadDifferenceRecord(fileStream, iTick, difference))
		{
			LOG(kDefault, kWarning, "Recorded frames file size doesn't match header");
			return;
		}
		if (iTick <= miStartTick || iTick > mSavedEnd.interpolate.iTick + 1
		 || (!mDifferences.empty() && iTick <= mDifferences.back().iTick))
		{
			LOG(kDefault, kWarning, "Recorded frames file has non-canonical tick data");
			return;
		}
		mDifferences.emplace_back(iTick, std::move(difference));
	}
	if (fileStream.peek() != std::char_traits<char>::eof())
	{
		LOG(kDefault, kWarning, "Recorded frames file has trailing data");
		return;
	}

	mDifferencesIterator = mDifferences.begin();

	std::fstream checksumStream = gpFileManager->OpenFile({FileFlags::kRead}, std::filesystem::path(rFilename).concat(".checksums"));
	if (!checksumStream)
	{
		LOG(kDefault, kWarning, "Checksum file is missing");
		return;
	}
	int64_t iChecksumBytes = common::StreamBytesRemaining(checksumStream);
	if (iChecksumBytes < 0 || iChecksumBytes % static_cast<int64_t>(sizeof(common::crc_t)) != 0)
	{
		LOG(kDefault, kWarning, "Checksum file size is invalid");
		return;
	}
	int64_t iChecksumCount = iChecksumBytes / static_cast<int64_t>(sizeof(common::crc_t));
	int64_t iExpectedChecksumCount = iChecksumSpan;
	if (iChecksumCount != iExpectedChecksumCount)
	{
		LOG(kDefault, kWarning, "Checksum file size doesn't match replay tick span");
		return;
	}
	mReaderFlags.Set(ReaderFlags::kRecordsInitialChecksum, bRecordsInitialChecksum);
	// Bound the replay .checksums file's checksum count against the stream before resize.
	common::ValidateDeserializedCount(iChecksumCount, sizeof(common::crc_t), checksumStream, "DifferenceStreamReader checksums");
	mChecksums.resize(iChecksumCount);
	if (iChecksumCount > 0)
	{
		common::Read(checksumStream, mChecksums);
		int64_t iBytesRead = checksumStream.gcount();
		if (iBytesRead != static_cast<int64_t>(sizeof(common::crc_t)) * iChecksumCount)
		{
			LOG(kDefault, kWarning, "Checksum file size doesn't match expected count (expected {}, got {})", iChecksumCount, iBytesRead / sizeof(common::crc_t));
			return;
		}
	}
	if (checksumStream.peek() != std::char_traits<char>::eof())
	{
		LOG(kDefault, kWarning, "Checksum file has trailing data");
		return;
	}

	if constexpr (kbReplayFullFrames)
	{
		std::fstream fullFramesFile = gpFileManager->OpenFile({FileFlags::kRead}, std::filesystem::path(rFilename).concat(".fullframes"));
		if (fullFramesFile)
		{
			mReaderFlags.Set(ReaderFlags::kFullFramesActive);
			mFullFramesStream << fullFramesFile.rdbuf();
			if (mReaderFlags & ReaderFlags::kRecordsInitialChecksum)
			{
				game::Frame firstFrame;
				if (TryReadFullFrame(firstFrame))
				{
					if (firstFrame.Crc() != rSavedStart.Crc())
					{
						// Stale/mismatched debug file: discard it so frame comparisons don't reference the wrong baseline
						DisableFullFrames(true);
						DEBUG_BREAK();
					}
					else
					{
						++miFullFramesIndex;
					}
				}
			}
		}
	}

	rbLoaded = true;
}

bool DifferenceStreamReader::IsTerminalTick(int64_t iTick) const
{
	return iTick == mSavedEnd.interpolate.iTick + 1;
}

bool DifferenceStreamReader::TerminalConsumed() const
{
	return (mReaderFlags & ReaderFlags::kTerminalChecksumValidated) && mDifferencesIterator == mDifferences.end();
}

void DifferenceStreamReader::ValidateChecksum(int64_t iTick, const game::Frame& rSavedCurrent)
{
	if (mChecksums.empty())
	{
		return;
	}

	int64_t iChecksumIndex = iTick - miStartTick - 1;
	if (iChecksumIndex < 0 || iChecksumIndex >= std::ssize(mChecksums))
	{
		return;
	}

	LOG(kReplay, kVerbose, "Checksum DifferenceStreamReader {}: {}", rSavedCurrent.interpolate.iTick, rSavedCurrent.Crc());

	[[maybe_unused]] game::Frame savedFrame {};
	[[maybe_unused]] bool bSavedFrameValid = false;
	if constexpr (kbReplayFullFrames)
	{
		// Read full frame snapshot to maintain stream synchronization
		if (iChecksumIndex == miFullFramesIndex && (mReaderFlags & ReaderFlags::kFullFramesActive) && !(mReaderFlags & ReaderFlags::kFullFramesInvalid))
		{
			if (TryReadFullFrame(savedFrame))
			{
				++miFullFramesIndex;
				bSavedFrameValid = true;
			}
		}
	}

	common::crc_t uiCurrentChecksum = rSavedCurrent.Crc();
	common::crc_t uiSavedChecksum = mChecksums.at(iChecksumIndex);

	if (uiCurrentChecksum != uiSavedChecksum)
	{
		if constexpr (kbReplayFullFrames)
		{
			if (bSavedFrameValid)
			{
				savedFrame.LogDifferences(rSavedCurrent);
			}
		}
		LOG(kNetwork, kError, "LogDifferences CRC Client: {} Server: {}", uiSavedChecksum, uiCurrentChecksum);
	}

	mReaderFlags.Set(ReaderFlags::kTerminalChecksumValidated, IsTerminalTick(iTick));
}

bool DifferenceStreamReader::LoadPostDispatch(int64_t iTick, game::FrameInput& rPostDispatch)
{
	if (mPostDispatchRecordsIterator == mPostDispatchRecords.end() || iTick != mPostDispatchRecordsIterator->iTick)
	{
		return false;
	}

	rPostDispatch = mPostDispatchRecordsIterator->input;
	++mPostDispatchRecordsIterator;

	LOG(kDefault, kVerbose, "Loaded post-dispatch record {}: {}", iTick, rPostDispatch.Crc());

	return true;
}

bool DifferenceStreamReader::LoadDifference(int64_t iTick, game::FrameInput& rDifference)
{
	// The terminal tick applies its final input to the retained saved end frame, validates it,
	// and only then lets the game retire this reader without dispatching another frame.
	if (iTick > mSavedEnd.interpolate.iTick + 1)
	{
		return false;
	}

	if (mDifferencesIterator != mDifferences.end() && iTick == mDifferencesIterator->iTick)
	{
		rDifference = mDifferencesIterator->input;
		++mDifferencesIterator;
		mCurrentDifference = rDifference;

		LOG(kDefault, kVerbose, "Loaded difference {}: {}", iTick, mCurrentDifference.Crc());
	}
	else
	{
		rDifference = mCurrentDifference;
	}

	return true;
}

bool DifferenceStreamReader::ReadDifferenceRecord(std::istream& rStream, int64_t& riTick, game::FrameInput& rDifference)
{
	common::Read(rStream, riTick);
	if (rStream.gcount() != static_cast<std::streamsize>(sizeof(riTick)))
	{
		return false;
	}
	rStream >> rDifference;
	return static_cast<bool>(rStream);
}

bool DifferenceStreamReader::TryReadFullFrame(game::Frame& rSavedFrame)
{
	bool bReadSucceeded = false;
	try
	{
		mFullFramesStream >> rSavedFrame;
		bReadSucceeded = static_cast<bool>(mFullFramesStream);
	}
	catch (const std::ios_base::failure&)
	{
		bReadSucceeded = false;
	}

	if (!bReadSucceeded)
	{
		DisableFullFrames();
	}

	return bReadSucceeded;
}

void DifferenceStreamReader::DisableFullFrames(bool bStaleBaseline)
{
	if (mReaderFlags & ReaderFlags::kFullFramesInvalid)
	{
		return;
	}

	mReaderFlags.Set(ReaderFlags::kFullFramesInvalid);
	mFullFramesStream.str({});
	if (bStaleBaseline)
	{
		LOG(kDefault, kWarning, "Full frames file doesn't match saved start frame, discarding");
	}
	else
	{
		LOG(kDefault, kWarning, "Full frames file read failed, discarding");
	}
}

} // namespace engine

#endif // defined(BT_SERVER)
