#include "Pch.h"

#if defined(BT_SERVER)

#include "File/Replay.h"

#include "File/GridSave.h"
#include "Game.h"
#include "GameBase.h"
#include "Network/Server/ServerBroadcaster.h"
#include "Network/Server/ServerSession.h"
#include "Network/Server/ServerTransferManager.h"
#include "Profile/ProfileManager.h"

namespace engine
{

// On-disk version of the F7.replay.manifest generation inventory. Old manifests are rejected on read.
static constexpr int64_t kiReplayManifestVersion = 3;
static constexpr int64_t kiInvalidReplayManifestVersion = 0;

namespace
{

template <std::integral TYPE>
void WriteReplayManifestValue(std::ostream& rStream, TYPE value)
{
	using UnsignedType = std::make_unsigned_t<TYPE>;
	const UnsignedType uiValue = std::bit_cast<UnsignedType>(value);
	for (size_t i = 0; i < sizeof(TYPE); ++i)
	{
		rStream.put(static_cast<char>((uiValue >> (i * 8)) & 0xFFu));
	}
}

template <std::integral TYPE>
bool ReadReplayManifestValue(std::istream& rStream, TYPE& rValue)
{
	using UnsignedType = std::make_unsigned_t<TYPE>;
	UnsignedType uiValue = 0;
	for (size_t i = 0; i < sizeof(TYPE); ++i)
	{
		const int iByte = rStream.get();
		if (iByte == std::char_traits<char>::eof())
		{
			return false;
		}
		uiValue |= static_cast<UnsignedType>(static_cast<uint8_t>(iByte)) << (i * 8);
	}
	rValue = std::bit_cast<TYPE>(uiValue);
	return true;
}

struct ReplayManifestRecord
{
	int64_t iActivationTick = 0;
	engine::GridCoord coord {};
};

bool ReplayManifestRecordLess(const ReplayManifestRecord& rLeft, const ReplayManifestRecord& rRight)
{
	return std::tie(rLeft.iActivationTick, rLeft.coord.x, rLeft.coord.y) < std::tie(rRight.iActivationTick, rRight.coord.x, rRight.coord.y);
}

enum class ReplayArtifactKind : uint8_t
{
	kGrid = 0,
	kMeta = 1,
	kCoordHeader = 2,
	kFrames = 3,
	kChecksums = 4,
	kFullFrames = 5,
};

struct ReplayManifestInventoryEntry
{
	ReplayArtifactKind eKind {};
	uint64_t uiCoordKey = 0;
	engine::FileContentDigest digest;
};

struct ReplayManifest
{
	int64_t iInitialTick = 0;
	std::vector<ReplayManifestRecord> records;
	bool bHasFullFrames = false;
	std::vector<ReplayManifestInventoryEntry> inventory;
};

constexpr std::string_view kReplayManifestGenerationDomain = "broken-engine/replay-manifest-generation/v3";
static_assert(kReplayManifestGenerationDomain.size() == 43);

std::filesystem::path ReplayArtifactFilename(ReplayArtifactKind eKind, uint64_t uiCoordKey)
{
	switch (eKind)
	{
	case ReplayArtifactKind::kGrid: return "F7.replay.grid";
	case ReplayArtifactKind::kMeta: return "F7.replay.meta";
	case ReplayArtifactKind::kCoordHeader: return "F7.replay." + std::to_string(uiCoordKey);
	case ReplayArtifactKind::kFrames: return "F7.replay." + std::to_string(uiCoordKey) + ".frames";
	case ReplayArtifactKind::kChecksums: return "F7.replay." + std::to_string(uiCoordKey) + ".checksums";
	case ReplayArtifactKind::kFullFrames: return "F7.replay." + std::to_string(uiCoordKey) + ".fullframes";
	}
	DEBUG_BREAK();
	return {};
}

bool ReplayInventoryEntryLess(const ReplayManifestInventoryEntry& rLeft, const ReplayManifestInventoryEntry& rRight)
{
	return std::tie(rLeft.eKind, rLeft.uiCoordKey) < std::tie(rRight.eKind, rRight.uiCoordKey);
}

bool AppendReplayManifestPayload(const ReplayManifest& rManifest, std::vector<std::byte>& rPayload)
{
	if (rManifest.records.size() > static_cast<size_t>(std::numeric_limits<int64_t>::max()) || rManifest.inventory.size() > static_cast<size_t>(std::numeric_limits<int64_t>::max()))
	{
		return false;
	}
	const size_t uiFixedBytes = sizeof(int64_t) * 4 + 1;
	if (rManifest.records.size() > (std::numeric_limits<size_t>::max() - uiFixedBytes) / 16 ||
		rManifest.inventory.size() > (std::numeric_limits<size_t>::max() - uiFixedBytes - rManifest.records.size() * 16) / 49)
	{
		return false;
	}

	rPayload.clear();
	rPayload.reserve(uiFixedBytes + rManifest.records.size() * 16 + rManifest.inventory.size() * 49);
	const auto appendValue = [&rPayload]<std::integral TYPE>(TYPE value)
	{
		using UnsignedType = std::make_unsigned_t<TYPE>;
		const UnsignedType uiValue = std::bit_cast<UnsignedType>(value);
		for (size_t i = 0; i < sizeof(TYPE); ++i)
		{
			rPayload.push_back(static_cast<std::byte>((uiValue >> (i * 8)) & 0xFFu));
		}
	};

	appendValue(kiReplayManifestVersion);
	appendValue(rManifest.iInitialTick);
	appendValue(static_cast<int64_t>(rManifest.records.size()));
	for (const ReplayManifestRecord& rRecord : rManifest.records)
	{
		appendValue(rRecord.iActivationTick);
		appendValue(rRecord.coord.x);
		appendValue(rRecord.coord.y);
	}
	appendValue(static_cast<uint8_t>(rManifest.bHasFullFrames ? 1 : 0));
	appendValue(static_cast<int64_t>(rManifest.inventory.size()));
	for (const ReplayManifestInventoryEntry& rEntry : rManifest.inventory)
	{
		appendValue(static_cast<uint8_t>(rEntry.eKind));
		appendValue(rEntry.uiCoordKey);
		appendValue(rEntry.digest.iByteCount);
		rPayload.insert(rPayload.end(), reinterpret_cast<const std::byte*>(rEntry.digest.sha256.data()), reinterpret_cast<const std::byte*>(rEntry.digest.sha256.data() + rEntry.digest.sha256.size()));
	}
	return true;
}

bool ComputeReplayGenerationDigest(const ReplayManifest& rManifest, std::array<uint8_t, 32>& rDigest)
{
	std::vector<std::byte> payload;
	if (!AppendReplayManifestPayload(rManifest, payload))
	{
		return false;
	}
	if (payload.size() > std::numeric_limits<size_t>::max() - sizeof(uint32_t) - kReplayManifestGenerationDomain.size())
	{
		return false;
	}

	std::vector<std::byte> rootPreimage;
	rootPreimage.reserve(sizeof(uint32_t) + kReplayManifestGenerationDomain.size() + payload.size());
	for (size_t i = 0; i < sizeof(uint32_t); ++i)
	{
		rootPreimage.push_back(static_cast<std::byte>((static_cast<uint32_t>(kReplayManifestGenerationDomain.size()) >> (i * 8)) & 0xFFu));
	}
	rootPreimage.insert(rootPreimage.end(), reinterpret_cast<const std::byte*>(kReplayManifestGenerationDomain.data()), reinterpret_cast<const std::byte*>(kReplayManifestGenerationDomain.data() + kReplayManifestGenerationDomain.size()));
	rootPreimage.insert(rootPreimage.end(), payload.begin(), payload.end());
	return engine::gpFileManager->ComputeSha256(rootPreimage, rDigest);
}

bool BuildExpectedReplayInventory(ReplayManifest& rManifest, bool bHashFiles)
{
	if (rManifest.records.size() > (std::numeric_limits<size_t>::max() - 2) / 4)
	{
		return false;
	}
	rManifest.inventory.clear();
	rManifest.inventory.reserve(2 + rManifest.records.size() * (rManifest.bHasFullFrames ? 4 : 3));
	for (ReplayArtifactKind eKind : {ReplayArtifactKind::kGrid, ReplayArtifactKind::kMeta, ReplayArtifactKind::kCoordHeader, ReplayArtifactKind::kFrames, ReplayArtifactKind::kChecksums, ReplayArtifactKind::kFullFrames})
	{
		if (eKind == ReplayArtifactKind::kFullFrames && !rManifest.bHasFullFrames)
		{
			continue;
		}
		if (eKind == ReplayArtifactKind::kGrid || eKind == ReplayArtifactKind::kMeta)
		{
			rManifest.inventory.push_back({.eKind = eKind});
			continue;
		}
		for (const ReplayManifestRecord& rRecord : rManifest.records)
		{
			rManifest.inventory.push_back({.eKind = eKind, .uiCoordKey = rRecord.coord.ToKey()});
		}
	}
	std::ranges::sort(rManifest.inventory, ReplayInventoryEntryLess);
	for (size_t i = 1; i < rManifest.inventory.size(); ++i)
	{
		if (rManifest.inventory.at(i - 1).eKind == rManifest.inventory.at(i).eKind && rManifest.inventory.at(i - 1).uiCoordKey == rManifest.inventory.at(i).uiCoordKey)
		{
			return false;
		}
	}

	if (!bHashFiles)
	{
		return true;
	}
	for (ReplayManifestInventoryEntry& rEntry : rManifest.inventory)
	{
		if (!engine::gpFileManager->ComputeOrdinaryFileSha256({engine::FileFlags::kAppDataDirectory, engine::FileFlags::kRead}, ReplayArtifactFilename(rEntry.eKind, rEntry.uiCoordKey), rEntry.digest))
		{
			return false;
		}
	}
	return true;
}

bool InvalidateReplayManifest()
{
	return engine::gpFileManager->WriteFileAtomically({engine::FileFlags::kAppDataDirectory, engine::FileFlags::kWrite}, std::filesystem::path("F7.replay.manifest"), [&](std::fstream& rManifestStream)
	{
		WriteReplayManifestValue(rManifestStream, kiInvalidReplayManifestVersion);
	});
}

bool PublishReplayManifest(const ReplayManifest& rManifest, const std::array<uint8_t, 32>& rGenerationDigest)
{
	std::vector<std::byte> payload;
	if (!AppendReplayManifestPayload(rManifest, payload))
	{
		return false;
	}
	return engine::gpFileManager->WriteFileAtomically({engine::FileFlags::kAppDataDirectory, engine::FileFlags::kWrite}, std::filesystem::path("F7.replay.manifest"), [&](std::fstream& rManifestStream)
	{
		rManifestStream.write(reinterpret_cast<const char*>(payload.data()), static_cast<std::streamsize>(payload.size()));
		rManifestStream.write(reinterpret_cast<const char*>(rGenerationDigest.data()), static_cast<std::streamsize>(rGenerationDigest.size()));
	});
}

} // namespace

Replay::Replay(GameBase& rGameBase)
	: mrGameBase(rGameBase)
{
	ASSERT(gpReplay == nullptr);

	gpReplay = this;
}

Replay::~Replay()
{
	if (gpReplay == this)
	{
		gpReplay = nullptr;
	}
}

void Replay::PublishReplayingState()
{
	mrGameBase.mbReplaying = !mReplayReaders.empty() || !mPendingReplayReaders.empty();
}

void Replay::ClearReplayTransientState()
{
	// Manifest validation can fail while the live game is still running; only discard state owned exclusively by replay.
	mReplayReaders.clear();
	mPendingReplayReaders.clear();
	PublishReplayingState();
	game::OnReplayStreamsInvalidated();
}

void Replay::ClearReplayAbortState()
{
	// Keep the current frames, inputs, and active set intact so an aborted playback resumes live simulation.
	ClearReplayTransientState();
	game::gpServerSession->mpTransferManager->mTransfers.clear();
	game::gpServerSession->mpBroadcaster->mBroadcastStatusChanges.clear();
}

void Replay::ResetStreams()
{
	mReplayWriters.clear();
	ClearReplayTransientState();
	miReplayInitialTick = 0;
	mReplayTransferCaptureInfo = {};
	meReplayPersistenceFailurePoint = ReplayPersistenceFailurePoint::kNone;
}

void Replay::UpdateTerminalReplayWriter(GridCoord coord, ReplayWriterState& rWriterState, const game::Frame& rEndFrame)
{
	game::FrameInput emptyInput {};
	const auto inputIt = mrGameBase.mFrameInputs.find(coord);
	const game::FrameInput& rLiveInput = inputIt != mrGameBase.mFrameInputs.end() ? inputIt->second : emptyInput;
	rWriterState.pWriter->Update(rEndFrame.interpolate.iTick + 1, rLiveInput, rEndFrame);
}

void Replay::CaptureHarvestedTransfers(GridCoord coord, std::span<const game::StatusChange> transfers, const game::Frame& rPreTransferFrame)
{
	if (mReplayWriters.empty() || transfers.empty())
	{
		return;
	}

	auto [writerIt, bInserted] = mReplayWriters.try_emplace(coord);
	ReplayWriterState& rWriterState = writerIt->second;
	if (bInserted)
	{
		rWriterState.iActivationTick = rPreTransferFrame.interpolate.iTick;
		game::FrameInput emptyInput {};
		rWriterState.pWriter = std::make_unique<engine::DifferenceStreamWriter<game::Frame, game::FrameInput>>(rPreTransferFrame, emptyInput, /*bRecordInitialChecksum=*/false);
	}
	if (rWriterState.bTerminal)
	{
		return;
	}

	game::FrameInput postDispatchInput {};
	postDispatchInput.statusChanges.assign(transfers.begin(), transfers.end());
	rWriterState.pWriter->RecordPostDispatch(rPreTransferFrame.interpolate.iTick, postDispatchInput);
	if (mReplayTransferCaptureInfo.iRecordingEventTick != rPreTransferFrame.interpolate.iTick)
	{
		mReplayTransferCaptureInfo.iRecordingEventTick = rPreTransferFrame.interpolate.iTick;
		mReplayTransferCaptureInfo.iPlaybackEventTick = -1;
		mReplayTransferCaptureInfo.transferCounts = {};
	}
	game::CountCapturedReplayTransfers(transfers, mReplayTransferCaptureInfo.transferCounts);
}

void Replay::ActivateReplayReader(GridCoord coord, PendingReplayReader&& rPendingReader)
{
	if (!mrGameBase.mCoordFrames.contains(coord))
	{
		mrGameBase.CreateFrameAtCoord(coord);
	}
	engine::CoordFrames& rFrames = mrGameBase.mCoordFrames.at(coord);
	rFrames.pCurrent = std::move(rPendingReader.pSavedStart);
	rFrames.pNext = std::make_unique<game::Frame>();
	engine::TransferViaStream(*rFrames.pCurrent, *rFrames.pNext);
	mrGameBase.mFrameInputs.insert_or_assign(coord, std::move(rPendingReader.initialInput));
	mReplayReaders.emplace(coord, std::move(rPendingReader.pReader));
	PublishReplayingState();
}

void Replay::RetainReplayEndFrame(GridCoord coord, std::unique_ptr<game::Frame>& rpFrame)
{
	auto it = mReplayWriters.find(coord);
	if (it == mReplayWriters.end() || it->second.bTerminal)
	{
		return;
	}

	UpdateTerminalReplayWriter(coord, it->second, *rpFrame);
	it->second.pRetainedEndFrame = std::move(rpFrame);
	it->second.bTerminal = true;
}

bool Replay::DropRetainedReplayEndFrame(GridCoord coord)
{
	auto it = mReplayWriters.find(coord);
	if (it == mReplayWriters.end() || !it->second.bTerminal || it->second.pRetainedEndFrame == nullptr)
	{
		return false;
	}

	it->second.pRetainedEndFrame.reset();
	return true;
}

bool Replay::ArmReplayPersistenceFailure(ReplayPersistenceFailurePoint eFailurePoint, GridCoord coord)
{
	if (eFailurePoint == ReplayPersistenceFailurePoint::kCoordinateWriter || eFailurePoint == ReplayPersistenceFailurePoint::kFullFramesRecord)
	{
		auto it = mReplayWriters.find(coord);
		if (it == mReplayWriters.end() || (it->second.bTerminal && it->second.pRetainedEndFrame == nullptr))
		{
			return false;
		}
	}

	meReplayPersistenceFailurePoint = eFailurePoint;
	mReplayPersistenceFailureCoord = coord;
	return true;
}

bool Replay::ConsumeReplayPersistenceFailure(ReplayPersistenceFailurePoint eFailurePoint, GridCoord coord)
{
	if (meReplayPersistenceFailurePoint != eFailurePoint)
	{
		return false;
	}
	if ((eFailurePoint == ReplayPersistenceFailurePoint::kCoordinateWriter || eFailurePoint == ReplayPersistenceFailurePoint::kFullFramesRecord) &&
		mReplayPersistenceFailureCoord != coord)
	{
		return false;
	}

	meReplayPersistenceFailurePoint = ReplayPersistenceFailurePoint::kNone;
	return true;
}

void Replay::SaveLoadReplay()
{
	if constexpr (kbDebugInput)
	{
		if (mrGameBase.mGameFlags & engine::GameFlags::kLoadReplay)
		{
			// Heap: DifferenceStream reader + Frame deserialization + ReplayMeta file I/O
			ScopedSuppressAllocationTracking suppress;
			gpProfileManager->LatchRawCpuTimers(false, mrGameBase.TickCounter());

			mrGameBase.mGameFlags.Clear(engine::GameFlags::kLoadReplay);

			try
			{
				if (!mReplayReaders.empty() || !mPendingReplayReaders.empty())
				{
					ClearReplayAbortState();
					mReplayTransferCaptureInfo = {};
					return;
				}
				ClearReplayTransientState();

				// The valid manifest is the generation commit marker. Validate it before reading any component.
				std::fstream manifestStream = engine::gpFileManager->OpenFile({engine::FileFlags::kAppDataDirectory, engine::FileFlags::kRead}, std::filesystem::path("F7.replay.manifest"));
				if (!manifestStream)
				{
					LOG(kDefault, kError, "Failed to read replay manifest");
					mReplayTransferCaptureInfo = {};
					return;
				}
				int64_t iManifestVersion = 0;
				if (!ReadReplayManifestValue(manifestStream, iManifestVersion))
				{
					throw common::CorruptStreamException("ReplayManifest version");
				}
				if (iManifestVersion != kiReplayManifestVersion)
				{
					LOG(kDefault, kError, "Replay manifest version {} != {}", iManifestVersion, kiReplayManifestVersion);
					mReplayTransferCaptureInfo = {};
					return;
				}

				ReplayManifest manifest;
				if (!ReadReplayManifestValue(manifestStream, manifest.iInitialTick))
				{
					throw common::CorruptStreamException("ReplayManifest initial tick");
				}
				int64_t iCoordCount = 0;
				if (!ReadReplayManifestValue(manifestStream, iCoordCount))
				{
					throw common::CorruptStreamException("ReplayManifest activation count");
				}
				const int64_t iInitialTick = manifest.iInitialTick;
				if (iInitialTick < 0 || iInitialTick > std::numeric_limits<int64_t>::max() - 1)
				{
					throw common::CorruptStreamException("ReplayManifest initial tick");
				}
				// Trust boundary (replay manifest file): bound the record count before reserve.
				common::ValidateDeserializedCount(iCoordCount, 16, manifestStream, "ReplayManifest records");
				if (iCoordCount <= 0)
				{
					throw common::CorruptStreamException("ReplayManifest empty records");
				}
				manifest.records.reserve(iCoordCount);
				std::unordered_set<uint64_t> recordedCoordKeys;
				recordedCoordKeys.reserve(iCoordCount);
				for (int64_t i = 0; i < iCoordCount; ++i)
				{
					ReplayManifestRecord record {};
					if (!ReadReplayManifestValue(manifestStream, record.iActivationTick) || !ReadReplayManifestValue(manifestStream, record.coord.x) || !ReadReplayManifestValue(manifestStream, record.coord.y))
					{
						throw common::CorruptStreamException("ReplayManifest activation record");
					}
					if (record.iActivationTick < iInitialTick || record.iActivationTick > std::numeric_limits<int64_t>::max() - 1 || !recordedCoordKeys.insert(record.coord.ToKey()).second ||
						(!manifest.records.empty() && !ReplayManifestRecordLess(manifest.records.back(), record)))
					{
						throw common::CorruptStreamException("ReplayManifest non-canonical record");
					}
					manifest.records.push_back(record);
				}
				uint8_t uiHasFullFrames = 0;
				if (!ReadReplayManifestValue(manifestStream, uiHasFullFrames) || uiHasFullFrames > 1)
				{
					throw common::CorruptStreamException("ReplayManifest fullframes flag");
				}
				manifest.bHasFullFrames = uiHasFullFrames != 0;
				if (manifest.bHasFullFrames != kbReplayFullFrames)
				{
					throw common::CorruptStreamException("ReplayManifest fullframes build mismatch");
				}

				int64_t iInventoryCount = 0;
				if (!ReadReplayManifestValue(manifestStream, iInventoryCount))
				{
					throw common::CorruptStreamException("ReplayManifest inventory count");
				}
				common::ValidateDeserializedCount(iInventoryCount, 49, manifestStream, "ReplayManifest inventory");
				if (iInventoryCount <= 0)
				{
					throw common::CorruptStreamException("ReplayManifest empty inventory");
				}
				manifest.inventory.reserve(iInventoryCount);
				for (int64_t i = 0; i < iInventoryCount; ++i)
				{
					uint8_t uiKind = 0;
					ReplayManifestInventoryEntry entry;
					if (!ReadReplayManifestValue(manifestStream, uiKind) || uiKind > static_cast<uint8_t>(ReplayArtifactKind::kFullFrames) ||
						!ReadReplayManifestValue(manifestStream, entry.uiCoordKey) || !ReadReplayManifestValue(manifestStream, entry.digest.iByteCount) || entry.digest.iByteCount < 0)
					{
						throw common::CorruptStreamException("ReplayManifest inventory entry");
					}
					entry.eKind = static_cast<ReplayArtifactKind>(uiKind);
					manifestStream.read(reinterpret_cast<char*>(entry.digest.sha256.data()), static_cast<std::streamsize>(entry.digest.sha256.size()));
					if (!manifestStream)
					{
						throw common::CorruptStreamException("ReplayManifest inventory digest");
					}
					if (!manifest.inventory.empty() && !ReplayInventoryEntryLess(manifest.inventory.back(), entry))
					{
						throw common::CorruptStreamException("ReplayManifest non-canonical inventory");
					}
					manifest.inventory.push_back(entry);
				}
				std::array<uint8_t, 32> generationDigest {};
				manifestStream.read(reinterpret_cast<char*>(generationDigest.data()), static_cast<std::streamsize>(generationDigest.size()));
				if (!manifestStream || manifestStream.peek() != std::char_traits<char>::eof())
				{
					throw common::CorruptStreamException("ReplayManifest trailing data");
				}

				ReplayManifest expectedManifest = manifest;
				if (!BuildExpectedReplayInventory(expectedManifest, false) || expectedManifest.inventory.size() != manifest.inventory.size())
				{
					throw common::CorruptStreamException("ReplayManifest inventory shape");
				}
				for (size_t i = 0; i < manifest.inventory.size(); ++i)
				{
					if (expectedManifest.inventory.at(i).eKind != manifest.inventory.at(i).eKind || expectedManifest.inventory.at(i).uiCoordKey != manifest.inventory.at(i).uiCoordKey)
					{
						throw common::CorruptStreamException("ReplayManifest inventory identity");
					}
				}
				std::array<uint8_t, 32> expectedGenerationDigest {};
				if (!ComputeReplayGenerationDigest(manifest, expectedGenerationDigest) || expectedGenerationDigest != generationDigest)
				{
					throw common::CorruptStreamException("ReplayManifest generation digest");
				}
				for (const ReplayManifestInventoryEntry& rEntry : manifest.inventory)
				{
					engine::FileContentDigest actualDigest;
					const std::filesystem::path filename = ReplayArtifactFilename(rEntry.eKind, rEntry.uiCoordKey);
					if (!engine::gpFileManager->ComputeOrdinaryFileSha256({engine::FileFlags::kAppDataDirectory, engine::FileFlags::kRead}, filename, actualDigest) ||
						actualDigest.iByteCount != rEntry.digest.iByteCount || actualDigest.sha256 != rEntry.digest.sha256)
					{
						throw common::CorruptStreamException("ReplayManifest inventory file");
					}
				}
				const std::vector<ReplayManifestRecord>& recordedRecords = manifest.records;

				game::ReplayStagedMeta stagedMeta {};
				if (!game::ReadReplayMeta({engine::FileFlags::kAppDataDirectory, engine::FileFlags::kRead}, ReplayArtifactFilename(ReplayArtifactKind::kMeta, 0), stagedMeta))
				{
					LOG(kDefault, kError, "Failed to read replay metadata");
					mReplayTransferCaptureInfo = {};
					return;
				}

				struct StagedReplayReader
				{
					ReplayManifestRecord record;
					PendingReplayReader pendingReader;
				};
				std::vector<StagedReplayReader> stagedReaders;
				stagedReaders.reserve(recordedRecords.size());
				for (const ReplayManifestRecord& rRecord : recordedRecords)
				{
					StagedReplayReader staged {.record = rRecord};
					staged.pendingReader.iActivationTick = rRecord.iActivationTick;
					staged.pendingReader.pSavedStart = std::make_unique<game::Frame>();
					std::filesystem::path coordReplayPath = std::filesystem::path("F7.replay." + std::to_string(rRecord.coord.ToKey()));
					bool bLoaded = false;
					bool bVersionMismatch = false;
					int64_t iFileVersion = 0;
					int64_t iExpectedVersion = 0;
					staged.pendingReader.pReader = std::make_unique<engine::DifferenceStreamReader<game::Frame, game::FrameInput>>(engine::FileFlags_t {engine::FileFlags::kAppDataDirectory, engine::FileFlags::kRead}, coordReplayPath, *staged.pendingReader.pSavedStart, staged.pendingReader.initialInput, bLoaded, bVersionMismatch, iFileVersion, iExpectedVersion, rRecord.iActivationTick == iInitialTick);
					if (bVersionMismatch)
					{
						// An older build recorded this replay: an expected refusal, not damaged data.
						LOG(kDefault, kError, "SaveLoadReplay aborted: replay version {} != expected version {}", iFileVersion, iExpectedVersion);
						mReplayTransferCaptureInfo = {};
						return;
					}
					if (!bLoaded || staged.pendingReader.pReader->GetStartTick() != rRecord.iActivationTick ||
						staged.pendingReader.pReader->GetSavedEnd().interpolate.iTick < rRecord.iActivationTick)
					{
						throw common::CorruptStreamException("ReplayManifest stream bounds");
					}
					stagedReaders.push_back(std::move(staged));
				}

				if (recordedRecords.front().iActivationTick != iInitialTick)
				{
					throw common::CorruptStreamException("ReplayManifest missing initial record");
				}

				// Parse the grid into isolated state. Its membership must correlate with the manifest before Reset
				// and adoption, because those operations notify clients and replace the live game state.
				engine::StagedGridSave stagedGrid;
				if (!engine::ReadGridSave({engine::FileFlags::kAppDataDirectory, engine::FileFlags::kRead}, std::filesystem::path("F7.replay.grid"), stagedGrid))
				{
					LOG(kDefault, kError, "Failed to read replay grid");
					mReplayTransferCaptureInfo = {};
					return;
				}

				int64_t iInitialRecordCount = 0;
				if (stagedGrid.iTick != iInitialTick)
				{
					throw common::CorruptStreamException("ReplayManifest grid initial tick mismatch");
				}
				for (const StagedReplayReader& rStagedReader : stagedReaders)
				{
					if (rStagedReader.record.iActivationTick == iInitialTick)
					{
						++iInitialRecordCount;
						const auto gridFrameIt = stagedGrid.coordFrames.find(rStagedReader.record.coord);
						if (gridFrameIt == stagedGrid.coordFrames.end())
						{
							throw common::CorruptStreamException("ReplayManifest initial coord absent from grid");
						}
						const game::Frame& rGridFrame = *gridFrameIt->second.pCurrent;
						const game::Frame& rSavedStart = *rStagedReader.pendingReader.pSavedStart;
						if (rSavedStart.interpolate.iTick != iInitialTick ||
							std::bit_cast<uint32_t>(rSavedStart.interpolate.fCurrentTime) != std::bit_cast<uint32_t>(stagedGrid.fCurrentTime) ||
							rSavedStart.Crc() != rGridFrame.Crc())
						{
							throw common::CorruptStreamException("ReplayManifest initial stream does not match grid");
						}
					}
					else if (stagedGrid.coordFrames.contains(rStagedReader.record.coord))
					{
						throw common::CorruptStreamException("ReplayManifest delayed coord present in grid");
					}
				}
				if (iInitialRecordCount != static_cast<int64_t>(stagedGrid.coordFrames.size()))
				{
					throw common::CorruptStreamException("ReplayManifest initial coords do not match grid");
				}

				const ReplayTransferCaptureInfo recordingCaptureInfo = mReplayTransferCaptureInfo;
				game::gpGame->Reset();
				engine::AdoptGridSave(mrGameBase, std::move(stagedGrid));

				const float fInitialTime = stagedReaders.front().pendingReader.pSavedStart->interpolate.fCurrentTime;
				for (StagedReplayReader& rStagedReader : stagedReaders)
				{
					if (rStagedReader.record.iActivationTick == iInitialTick)
					{
						ActivateReplayReader(rStagedReader.record.coord, std::move(rStagedReader.pendingReader));
					}
					else
					{
						mPendingReplayReaders.emplace(rStagedReader.record.coord, std::move(rStagedReader.pendingReader));
					}
				}
				PublishReplayingState();
				mrGameBase.SetTickCounter(iInitialTick);
				mrGameBase.SetCurrentTime(fInitialTime);

				game::AdoptReplayMeta(std::move(stagedMeta));
				game::OnStateReplaced();

				// Replay owns every recorded coord until its reader reaches the recorded end. Rebuild directly from
				// the successfully loaded readers so normal subscription/player pruning cannot erase an empty coord.
				mrGameBase.mActiveCoords.clear();
				for (const auto& [rCoord, rpReader] : mReplayReaders)
				{
					mrGameBase.mActiveCoords.push_back(rCoord);
				}

				// Game::Reset clears stream-owned diagnostic state. Restore recording evidence so each replay loop
				// relatches playback.
				mReplayTransferCaptureInfo = {
					.iRecordingEventTick = recordingCaptureInfo.iRecordingEventTick,
					.iPlaybackEventTick = recordingCaptureInfo.iPlaybackEventTick,
					.iFirstWriterInputTick = recordingCaptureInfo.iFirstWriterInputTick,
					.iWriterInputCount = recordingCaptureInfo.iWriterInputCount,
					.transferCounts = recordingCaptureInfo.transferCounts,
				};
				// Replay I/O is outside sim time; do not carry its wall time or pre-load debt into the new loop.
				mrGameBase.mTimeStep.ClearAccumulator();
				mrGameBase.mTimeStep.mRealTime.Reset();
				mrGameBase.mfLastDeltaTime = 0.0f;
			}
			catch (const std::exception& rException)
			{
				LOG(kDefault, kError, "SaveLoadReplay aborted: corrupt replay data: {}", rException.what());
				if (mrGameBase.mbReplaying)
				{
					ClearReplayAbortState();
				}
				else
				{
					ClearReplayTransientState();
				}
				mReplayTransferCaptureInfo = {};
				return;
			}
		}
	}
}

bool Replay::SyncReplayTick()
{
	// Heap: DifferenceStream reader/writer persist across frames, growing vectors for diffs and checksums.
	//   Workbuffer is popped each frame so can't hold cross-frame state; size depends on recording length
	ScopedSuppressAllocationTracking suppress;

	if constexpr (kbDebugInput)
	{
		// Recording start: create one writer per active coord
		if ((mrGameBase.mGameFlags & engine::GameFlags::kSaveReplay) && mReplayWriters.empty())
		{
			mrGameBase.mGameFlags.Clear(engine::GameFlags::kSaveReplay);

			if (ConsumeReplayPersistenceFailure(ReplayPersistenceFailurePoint::kManifestInvalidation))
			{
				LOG(kDefault, kError, "Injected replay manifest invalidation failure; recording not started");
				mReplayTransferCaptureInfo = {};
				game::OnReplayStreamsInvalidated();
				return true;
			}
			if (!InvalidateReplayManifest())
			{
				meReplayPersistenceFailurePoint = ReplayPersistenceFailurePoint::kNone;
				LOG(kDefault, kError, "Replay manifest invalidation failed; recording not started");
				mReplayTransferCaptureInfo = {};
				game::OnReplayStreamsInvalidated();
				return true;
			}

			mReplayReaders.clear();
			mPendingReplayReaders.clear();
			PublishReplayingState();

			const bool bGridWritten = !ConsumeReplayPersistenceFailure(ReplayPersistenceFailurePoint::kGrid) &&
				engine::WriteGridSave(mrGameBase, {engine::FileFlags::kAppDataDirectory, engine::FileFlags::kWrite}, std::filesystem::path("F7.replay.grid"), mrGameBase.mClientGridCoord);
			if (!bGridWritten)
			{
				LOG(kDefault, kError, "Replay grid write failed; recording not started");
				mReplayTransferCaptureInfo = {};
				game::OnReplayStreamsInvalidated();
				return true;
			}

			miReplayInitialTick = 0;
			bool bHaveInitialTick = false;
			for (const auto& [rCoord, rFrames] : mrGameBase.mCoordFrames)
			{
				if (!bHaveInitialTick)
				{
					miReplayInitialTick = rFrames.pCurrent->interpolate.iTick;
					bHaveInitialTick = true;
				}
				else if (rFrames.pCurrent->interpolate.iTick != miReplayInitialTick)
				{
					LOG(kDefault, kError, "Replay recording start has inconsistent coord ticks");
					mReplayWriters.clear();
					mReplayTransferCaptureInfo = {};
					game::OnReplayStreamsInvalidated();
					return true;
				}

				game::FrameInput& rFrameInput = mrGameBase.mFrameInputs.try_emplace(rCoord).first->second;
				mReplayWriters.emplace(rCoord, ReplayWriterState {
					.pWriter = std::make_unique<engine::DifferenceStreamWriter<game::Frame, game::FrameInput>>(*rFrames.pCurrent, rFrameInput),
					.iActivationTick = miReplayInitialTick,
				});
			}

			// A pending-start fixture arms before this branch; reset its observations without dropping that one-shot request.
			const int64_t iPauseAfterWriterInputCount = mReplayTransferCaptureInfo.iPauseAfterWriterInputCount;
			mReplayTransferCaptureInfo = {.iPauseAfterWriterInputCount = iPauseAfterWriterInputCount};
			LOG(kDefault, kDebug, "Recording started for {} coords", mReplayWriters.size());
			return true;
		}

		// Recording stop: save all writers
		if ((mrGameBase.mGameFlags & engine::GameFlags::kSaveReplay) && !mReplayWriters.empty())
		{
			mrGameBase.mGameFlags.Clear(engine::GameFlags::kSaveReplay);
			game::OnReplayStreamsInvalidated();

			// Preserve the valid manifest's deterministic coord ordering after writer state is cleared.
			std::vector<ReplayManifestRecord> recordedRecords;
			recordedRecords.reserve(mReplayWriters.size());
			for (const auto& [rCoord, rWriterState] : mReplayWriters)
			{
				recordedRecords.push_back({.iActivationTick = rWriterState.iActivationTick, .coord = rCoord});
			}
			std::ranges::sort(recordedRecords, ReplayManifestRecordLess);

			bool bReplayWritten = true;

			for (auto& [rCoord, rWriterState] : mReplayWriters)
			{
				std::filesystem::path coordReplayPath = std::filesystem::path("F7.replay." + std::to_string(rCoord.ToKey()));
				const engine::FileFlags_t fileFlags {engine::FileFlags::kAppDataDirectory, engine::FileFlags::kWrite, engine::FileFlags::kBackup};
				const game::Frame* pEndFrame = nullptr;
				if (rWriterState.bTerminal)
				{
					pEndFrame = rWriterState.pRetainedEndFrame.get();
				}
				else
				{
					auto it = mrGameBase.mCoordFrames.find(rCoord);
					if (it != mrGameBase.mCoordFrames.end())
					{
						pEndFrame = it->second.pCurrent.get();
					}
				}

				bool bWriterSaved = false;
				if (pEndFrame != nullptr)
				{
					if (!rWriterState.bTerminal)
					{
						UpdateTerminalReplayWriter(rCoord, rWriterState, *pEndFrame);
					}
					bWriterSaved = rWriterState.pWriter->Save(fileFlags, coordReplayPath, *pEndFrame);
					if (ConsumeReplayPersistenceFailure(ReplayPersistenceFailurePoint::kCoordinateWriter, rCoord))
					{
						LOG(kDefault, kError, "Injected replay writer failure for coord ({},{}); deleting replay sibling set", rCoord.x, rCoord.y);
						rWriterState.pWriter->CleanupFiles(fileFlags, coordReplayPath);
						bWriterSaved = false;
					}
					if (bWriterSaved && ConsumeReplayPersistenceFailure(ReplayPersistenceFailurePoint::kFullFramesRecord, rCoord))
					{
						const std::filesystem::path fullFramesPath = std::filesystem::path(coordReplayPath).concat(".fullframes");
						std::fstream fullFramesStream = engine::gpFileManager->OpenFile({engine::FileFlags::kAppDataDirectory, engine::FileFlags::kRead}, fullFramesPath);
						fullFramesStream.seekg(0, std::ios::end);
						const std::streamoff iBeforeBytes = fullFramesStream.tellg();
						bool bFullFramesTruncated = fullFramesStream.is_open() && iBeforeBytes > 0 &&
							iBeforeBytes <= std::numeric_limits<std::streamsize>::max();
						std::vector<std::byte> prefix;
						if (bFullFramesTruncated)
						{
							prefix.resize(static_cast<size_t>(iBeforeBytes - 1));
							fullFramesStream.seekg(0, std::ios::beg);
							if (!prefix.empty())
							{
								fullFramesStream.read(reinterpret_cast<char*>(prefix.data()), static_cast<std::streamsize>(prefix.size()));
								bFullFramesTruncated = fullFramesStream.gcount() == static_cast<std::streamsize>(prefix.size());
							}
						}
						fullFramesStream.close();
						if (bFullFramesTruncated)
						{
							bFullFramesTruncated = engine::gpFileManager->WriteFileAtomically(fileFlags, fullFramesPath, [&](std::fstream& rFullFramesStream)
							{
								if (!prefix.empty())
								{
									rFullFramesStream.write(reinterpret_cast<const char*>(prefix.data()), static_cast<std::streamsize>(prefix.size()));
								}
							});
						}
						if (bFullFramesTruncated)
						{
							LOG(kDefault, kDebug, "Injected malformed replay fullframes for coord ({},{}); bytes {} -> {}", rCoord.x, rCoord.y, iBeforeBytes, prefix.size());
						}
						else
						{
							LOG(kDefault, kError, "Injected malformed replay fullframes failed for coord ({},{}); deleting replay sibling set", rCoord.x, rCoord.y);
							rWriterState.pWriter->CleanupFiles(fileFlags, coordReplayPath);
							bWriterSaved = false;
						}
					}
				}
				else
				{
					LOG(kDefault, kError, "Replay writer for coord ({},{}) has no terminal frame; deleting partial replay set", rCoord.x, rCoord.y);
					rWriterState.pWriter->CleanupFiles(fileFlags, coordReplayPath);
				}
				bReplayWritten = bWriterSaved && bReplayWritten;
			}

			mReplayTransferCaptureInfo.iPauseAfterWriterInputCount = -1;
			mReplayWriters.clear();
			miReplayInitialTick = 0;

			// Write replay metadata for F8 load
			const bool bMetadataWritten = !ConsumeReplayPersistenceFailure(ReplayPersistenceFailurePoint::kMetadata) &&
				game::WriteReplayMeta({engine::FileFlags::kAppDataDirectory, engine::FileFlags::kWrite}, ReplayArtifactFilename(ReplayArtifactKind::kMeta, 0));
			bReplayWritten = bMetadataWritten && bReplayWritten;

			if (bReplayWritten)
			{
				ReplayManifest manifest {
					.iInitialTick = recordedRecords.empty() ? 0 : recordedRecords.front().iActivationTick,
					.records = recordedRecords,
					.bHasFullFrames = kbReplayFullFrames,
				};
				std::array<uint8_t, 32> generationDigest {};
				bReplayWritten = !ConsumeReplayPersistenceFailure(ReplayPersistenceFailurePoint::kInventory) &&
					BuildExpectedReplayInventory(manifest, true) && ComputeReplayGenerationDigest(manifest, generationDigest) &&
					!ConsumeReplayPersistenceFailure(ReplayPersistenceFailurePoint::kFinalManifest) && PublishReplayManifest(manifest, generationDigest);
			}
			meReplayPersistenceFailurePoint = ReplayPersistenceFailurePoint::kNone;

			if (bReplayWritten)
			{
				LOG(kDefault, kDebug, "Recording stopped");
			}
			else
			{
				LOG(kDefault, kError, "Replay persistence failed; recording stopped without a complete replay");
				mReplayTransferCaptureInfo = {};
			}
			return true;
		}

		// Recording tick: update all writers
		if (!mReplayWriters.empty()) [[unlikely]]
		{
			bool bWriterUpdated = false;
			for (auto& [rCoord, rWriterState] : mReplayWriters)
			{
				if (rWriterState.bTerminal)
				{
					continue;
				}
				if (!mrGameBase.mCoordFrames.contains(rCoord))
				{
					continue;
				}

				game::FrameInput& rFrameInput = mrGameBase.mFrameInputs.try_emplace(rCoord).first->second;
				rWriterState.pWriter->Update(mrGameBase.TickCounter(), rFrameInput, mrGameBase.CurrentFrame(rCoord));
				bWriterUpdated = true;
			}
			if (bWriterUpdated)
			{
				if (mReplayTransferCaptureInfo.iFirstWriterInputTick == -1)
				{
					mReplayTransferCaptureInfo.iFirstWriterInputTick = mrGameBase.TickCounter();
				}
				++mReplayTransferCaptureInfo.iWriterInputCount;
				if (mReplayTransferCaptureInfo.iPauseAfterWriterInputCount == mReplayTransferCaptureInfo.iWriterInputCount)
				{
					mrGameBase.mGameFlags.Set(engine::GameFlags::kPaused);
					mReplayTransferCaptureInfo.iPauseAfterWriterInputCount = -1;
				}
			}
		}

		// Replay publication state belongs to one fixed tick. HarvestTransfers is intentionally disabled during
		// playback, so clear the staged maps here before this tick's post-dispatch records are loaded below.
		if (mrGameBase.mbReplaying)
		{
			game::gpServerSession->mpTransferManager->mTransfers.clear();
			game::gpServerSession->mpBroadcaster->mBroadcastStatusChanges.clear();
		}

		auto abortReplay = [&]()
		{
			ClearReplayAbortState();
			mReplayTransferCaptureInfo = {};
			return false;
		};

		for (auto it = mPendingReplayReaders.begin(); it != mPendingReplayReaders.end();)
		{
			if (it->second.iActivationTick > mrGameBase.TickCounter())
			{
				++it;
				continue;
			}
			if (it->second.iActivationTick < mrGameBase.TickCounter())
			{
				LOG(kDefault, kError, "Replay reader activation tick was skipped for coord ({},{})", it->first.x, it->first.y);
				return abortReplay();
			}

			const engine::GridCoord coord = it->first;
			PendingReplayReader pendingReader = std::move(it->second);
			it = mPendingReplayReaders.erase(it);
			ActivateReplayReader(coord, std::move(pendingReader));
		}

		// Terminal readers retire first. Their terminal input is consumed and its saved end frame is checksum
		// validated before the coord is removed; the terminal input is not dispatched or published a second time.
		if (!mReplayReaders.empty()) [[unlikely]]
		{
			for (auto it = mReplayReaders.begin(); it != mReplayReaders.end();)
			{
				const engine::GridCoord coord = it->first;
				std::unique_ptr<engine::DifferenceStreamReader<game::Frame, game::FrameInput>>& rpReader = it->second;
				const bool bTerminalTick = rpReader->IsTerminalTick(mrGameBase.TickCounter());
				if (!bTerminalTick)
				{
					++it;
					continue;
				}

				game::FrameInput& rFrameInput = mrGameBase.mFrameInputs.try_emplace(coord).first->second;
				if (!rpReader->LoadDifference(mrGameBase.TickCounter(), rFrameInput))
				{
					LOG(kDefault, kError, "Replay reader advanced beyond terminal tick for coord ({},{})", coord.x, coord.y);
					return abortReplay();
				}
				rpReader->ValidateChecksum(mrGameBase.TickCounter(), mrGameBase.CurrentFrame(coord));
				if (!rpReader->TerminalConsumed())
				{
					LOG(kDefault, kError, "Replay reader terminal data was not fully consumed for coord ({},{})", coord.x, coord.y);
					return abortReplay();
				}
				rFrameInput.statusChanges.clear();
				mrGameBase.mCoordFrames.erase(coord);
				mrGameBase.mFrameInputs.erase(coord);
				std::erase(mrGameBase.mActiveCoords, coord);
				it = mReplayReaders.erase(it);
			}
			PublishReplayingState();

			// Load each current difference, then the post-dispatch channel record for this exact tick. A transfer
			// harvested after dispatch at event tick E is recorded at E and staged here for publication at E.
			for (auto& [coord, rpReader] : mReplayReaders)
			{
				game::FrameInput& rFrameInput = mrGameBase.mFrameInputs.try_emplace(coord).first->second;
				if (!rpReader->LoadDifference(mrGameBase.TickCounter(), rFrameInput))
				{
					LOG(kDefault, kError, "Replay reader advanced beyond terminal tick for coord ({},{})", coord.x, coord.y);
					return abortReplay();
				}

				// Replay files are a trust boundary: transfers belong only to the post-dispatch channel, so a recorded
				// input carrying one would publish an entry a connected client applies against authoritative state.
				if (std::ranges::any_of(rFrameInput.statusChanges, [](const game::StatusChange& rStatusChange)
				{
					return game::IsTransferType(rStatusChange.eType);
				}))
				{
					LOG(kDefault, kError, "Replay input carries a transfer status change for coord ({},{})", coord.x, coord.y);
					return abortReplay();
				}

				game::FrameInput postDispatchInput {};
				if (rpReader->LoadPostDispatch(mrGameBase.TickCounter(), postDispatchInput))
				{
					// Replay files are a trust boundary: the generic channel cannot know StatusChange semantics, and a
					// valid non-transfer entry would reach std::get<TransferData> in the spawn path.
					if (postDispatchInput.statusChanges.empty() || !std::ranges::all_of(postDispatchInput.statusChanges, [](const game::StatusChange& rStatusChange)
					{
						return game::IsTransferType(rStatusChange.eType);
					}))
					{
						LOG(kDefault, kError, "Replay post-dispatch record is not a transfer batch for coord ({},{})", coord.x, coord.y);
						return abortReplay();
					}
					game::gpServerSession->mpTransferManager->PrepareReplayTransfers(coord, postDispatchInput.statusChanges);
					mReplayTransferCaptureInfo.iPlaybackEventTick = mrGameBase.TickCounter();
				}

				if (!rFrameInput.statusChanges.empty())
				{
					game::gpServerSession->mpBroadcaster->mBroadcastStatusChanges.insert_or_assign(coord, rFrameInput.statusChanges);
				}
				rpReader->ValidateChecksum(mrGameBase.TickCounter(), mrGameBase.CurrentFrame(coord));
			}

			if (mReplayReaders.empty() && mPendingReplayReaders.empty())
			{
				LOG(kDefault, kDebug, "End replay {}, looping", mrGameBase.TickCounter());
				ClearReplayTransientState();
				mrGameBase.mGameFlags.Set(engine::GameFlags::kLoadReplay);
				return false;
			}
		}
	}

	return true;
}

} // namespace engine

#endif // BT_SERVER
