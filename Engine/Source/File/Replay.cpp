#include "Pch.h"

#if defined(BT_SERVER)

#include "File/Replay.h"

#include "Agent/Commands/ReplayHarnessRigs.h"
#include "File/GridSave.h"
#include "Network/Server/ServerBroadcaster.h"
#include "Network/Server/ServerTransferManager.h"
#include "GameBase.h"

#include "Network/Server/ServerSession.h"
#include "Profile/ProfileRows.h"
#include "Game.h"

namespace engine
{

// On-disk version of F7.replay.manifest. Old manifests are rejected on read.
constexpr int64_t kiReplayManifestVersion = 5;
constexpr int64_t kiInvalidReplayManifestVersion = 0;

template <std::integral TYPE>
static void WriteReplayManifestValue(std::ostream& rStream, TYPE value)
{
	using UnsignedType = std::make_unsigned_t<TYPE>;
	UnsignedType uiValue = std::bit_cast<UnsignedType>(value);
	for (int64_t i = 0; i < static_cast<int64_t>(sizeof(TYPE)); ++i)
	{
		rStream.put(static_cast<char>((uiValue >> (i * 8)) & 0xFFui32));
	}
}

template <std::integral TYPE>
static bool ReadReplayManifestValue(std::istream& rStream, TYPE& rValue)
{
	using UnsignedType = std::make_unsigned_t<TYPE>;
	UnsignedType uiValue = 0;
	for (int64_t i = 0; i < static_cast<int64_t>(sizeof(TYPE)); ++i)
	{
		int64_t iByte = rStream.get();
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
	engine::GridCoord coordinate {};
};

static bool ReplayManifestRecordLess(const ReplayManifestRecord& rLeft, const ReplayManifestRecord& rRight)
{
	return std::tie(rLeft.iActivationTick, rLeft.coordinate.iX, rLeft.coordinate.iY) < std::tie(rRight.iActivationTick, rRight.coordinate.iX, rRight.coordinate.iY);
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

struct ReplayManifest
{
	int64_t iInitialTick = 0;
	std::vector<ReplayManifestRecord> records;
	bool bHasFullFrames = false;
};

constexpr std::string_view kReplayManifestGenerationDomain = "broken-engine/replay-manifest-generation/v5";
static_assert(kReplayManifestGenerationDomain.size() == 43);

static std::filesystem::path ReplayArtifactFilename(ReplayArtifactKind eKind, uint64_t uiCoordinateKey, int64_t iActivationTick)
{
	switch (eKind)
	{
		case ReplayArtifactKind::kGrid: return "F7.replay.grid";
		case ReplayArtifactKind::kMeta: return "F7.replay.meta";
		case ReplayArtifactKind::kCoordHeader: return "F7.replay." + std::to_string(uiCoordinateKey) + "." + std::to_string(iActivationTick);
		case ReplayArtifactKind::kFrames: return "F7.replay." + std::to_string(uiCoordinateKey) + "." + std::to_string(iActivationTick) + ".frames";
		case ReplayArtifactKind::kChecksums: return "F7.replay." + std::to_string(uiCoordinateKey) + "." + std::to_string(iActivationTick) + ".checksums";
		case ReplayArtifactKind::kFullFrames: return "F7.replay." + std::to_string(uiCoordinateKey) + "." + std::to_string(iActivationTick) + ".fullframes";
	}
	DEBUG_BREAK();
	return {};
}

static void AppendReplayManifestPayload(const ReplayManifest& rManifest, std::vector<std::byte>& rPayload)
{
	static constexpr int64_t kiFixedBytes = sizeof(int64_t) * 3 + 1;
	rPayload.clear();
	rPayload.reserve(static_cast<size_t>(kiFixedBytes + std::ssize(rManifest.records) * 16));
	auto AppendValue = [&rPayload]<std::integral TYPE>(TYPE value)
	{
		using UnsignedType = std::make_unsigned_t<TYPE>;
		UnsignedType uiValue = std::bit_cast<UnsignedType>(value);
		for (int64_t i = 0; i < static_cast<int64_t>(sizeof(TYPE)); ++i)
		{
			rPayload.push_back(static_cast<std::byte>((uiValue >> (i * 8)) & 0xFFui32));
		}
	};

	AppendValue(kiReplayManifestVersion);
	AppendValue(rManifest.iInitialTick);
	AppendValue(static_cast<int64_t>(std::ssize(rManifest.records)));
	for (const ReplayManifestRecord& rRecord : rManifest.records)
	{
		AppendValue(rRecord.iActivationTick);
		AppendValue(rRecord.coordinate.iX);
		AppendValue(rRecord.coordinate.iY);
	}
	AppendValue(static_cast<uint8_t>(rManifest.bHasFullFrames ? 1 : 0));
}

static bool ComputeReplayGenerationDigest(const ReplayManifest& rManifest, std::array<uint8_t, 32>& rDigest)
{
	std::vector<std::byte> payload;
	AppendReplayManifestPayload(rManifest, payload);

	int64_t iArtifactCount = 2 + std::ssize(rManifest.records) * (rManifest.bHasFullFrames ? 4 : 3);
	std::vector<std::byte> rootPreimage;
	rootPreimage.reserve(sizeof(uint32_t) + kReplayManifestGenerationDomain.size() + static_cast<size_t>(std::ssize(payload) + iArtifactCount * 32));
	for (int64_t i = 0; i < static_cast<int64_t>(sizeof(uint32_t)); ++i)
	{
		rootPreimage.push_back(static_cast<std::byte>((static_cast<uint32_t>(kReplayManifestGenerationDomain.size()) >> (i * 8)) & 0xFFui32));
	}
	rootPreimage.insert(rootPreimage.end(), reinterpret_cast<const std::byte*>(kReplayManifestGenerationDomain.data()), reinterpret_cast<const std::byte*>(kReplayManifestGenerationDomain.data() + kReplayManifestGenerationDomain.size()));
	rootPreimage.append_range(payload);
	auto AppendArtifactDigest = [&rootPreimage](ReplayArtifactKind eKind, uint64_t uiCoordinateKey, int64_t iActivationTick)
	{
		engine::FileContentDigest digest;
		if (!engine::gpFileManager->ComputeOrdinaryFileSha256({engine::FileFlags::kAppDataDirectory, engine::FileFlags::kRead}, ReplayArtifactFilename(eKind, uiCoordinateKey, iActivationTick), digest))
		{
			return false;
		}
		rootPreimage.insert(rootPreimage.end(), reinterpret_cast<const std::byte*>(digest.sha256.data()), reinterpret_cast<const std::byte*>(digest.sha256.data() + digest.sha256.size()));
		return true;
	};
	if (!AppendArtifactDigest(ReplayArtifactKind::kGrid, 0, -1) || !AppendArtifactDigest(ReplayArtifactKind::kMeta, 0, -1))
	{
		return false;
	}
	for (const ReplayManifestRecord& rRecord : rManifest.records)
	{
		uint64_t uiCoordinateKey = rRecord.coordinate.ToKey();
		if (!AppendArtifactDigest(ReplayArtifactKind::kCoordHeader, uiCoordinateKey, rRecord.iActivationTick)
		 || !AppendArtifactDigest(ReplayArtifactKind::kFrames, uiCoordinateKey, rRecord.iActivationTick)
		 || !AppendArtifactDigest(ReplayArtifactKind::kChecksums, uiCoordinateKey, rRecord.iActivationTick)
		 || (rManifest.bHasFullFrames && !AppendArtifactDigest(ReplayArtifactKind::kFullFrames, uiCoordinateKey, rRecord.iActivationTick)))
		{
			return false;
		}
	}
	return engine::gpFileManager->ComputeSha256(rootPreimage, rDigest);
}

static bool InvalidateReplayManifest()
{
	return engine::gpFileManager->WriteFileAtomically({engine::FileFlags::kAppDataDirectory, engine::FileFlags::kWrite}, std::filesystem::path("F7.replay.manifest"), [&](std::fstream& rManifestStream)
	{
		WriteReplayManifestValue(rManifestStream, kiInvalidReplayManifestVersion);
	});
}

static bool PublishReplayManifest(const ReplayManifest& rManifest, const std::array<uint8_t, 32>& rGenerationDigest)
{
	std::vector<std::byte> payload;
	AppendReplayManifestPayload(rManifest, payload);
	return engine::gpFileManager->WriteFileAtomically({engine::FileFlags::kAppDataDirectory, engine::FileFlags::kWrite}, std::filesystem::path("F7.replay.manifest"), [&](std::fstream& rManifestStream)
	{
		rManifestStream.write(reinterpret_cast<const char*>(payload.data()), static_cast<std::streamsize>(std::ssize(payload)));
		rManifestStream.write(reinterpret_cast<const char*>(rGenerationDigest.data()), static_cast<std::streamsize>(rGenerationDigest.size()));
	});
}

static bool ReadReplayManifest(ReplayManifest& rManifest)
{
	std::fstream manifestStream = engine::gpFileManager->OpenFile({engine::FileFlags::kAppDataDirectory, engine::FileFlags::kRead}, std::filesystem::path("F7.replay.manifest"));
	if (!manifestStream)
	{
		LOG(kDefault, kError, "Failed to read replay manifest");
		return false;
	}
	int64_t iManifestVersion = 0;
	if (!ReadReplayManifestValue(manifestStream, iManifestVersion))
	{
		throw std::ios_base::failure("ReplayManifest version");
	}
	if (iManifestVersion != kiReplayManifestVersion)
	{
		LOG(kDefault, kError, "Replay manifest version {} != {}", iManifestVersion, kiReplayManifestVersion);
		return false;
	}

	if (!ReadReplayManifestValue(manifestStream, rManifest.iInitialTick))
	{
		throw std::ios_base::failure("ReplayManifest initial tick");
	}
	int64_t iCoordinateCount = 0;
	if (!ReadReplayManifestValue(manifestStream, iCoordinateCount))
	{
		throw std::ios_base::failure("ReplayManifest activation count");
	}
	common::ValidateDeserializedCount(iCoordinateCount, sizeof(ReplayManifestRecord::iActivationTick) + sizeof(GridCoord::iX) + sizeof(GridCoord::iY), manifestStream, "ReplayManifest records");
	rManifest.records.reserve(iCoordinateCount);
	for (int64_t i = 0; i < iCoordinateCount; ++i)
	{
		ReplayManifestRecord record {};
		if (!ReadReplayManifestValue(manifestStream, record.iActivationTick) || !ReadReplayManifestValue(manifestStream, record.coordinate.iX) || !ReadReplayManifestValue(manifestStream, record.coordinate.iY))
		{
			throw std::ios_base::failure("ReplayManifest activation record");
		}
		rManifest.records.push_back(record);
	}
	uint8_t uiHasFullFrames = 0;
	if (!ReadReplayManifestValue(manifestStream, uiHasFullFrames) || uiHasFullFrames > 1)
	{
		throw std::ios_base::failure("ReplayManifest fullframes flag");
	}
	rManifest.bHasFullFrames = uiHasFullFrames != 0;
	if (rManifest.bHasFullFrames != kbReplayFullFrames)
	{
		throw std::ios_base::failure("ReplayManifest fullframes build mismatch");
	}
	std::array<uint8_t, 32> generationDigest {};
	manifestStream.read(reinterpret_cast<char*>(generationDigest.data()), static_cast<std::streamsize>(generationDigest.size()));
	if (!manifestStream || manifestStream.peek() != std::char_traits<char>::eof())
	{
		throw std::ios_base::failure("ReplayManifest trailing data");
	}

	std::array<uint8_t, 32> expectedGenerationDigest {};
	if (!ComputeReplayGenerationDigest(rManifest, expectedGenerationDigest))
	{
		throw std::ios_base::failure("ReplayManifest inventory file");
	}
	if (expectedGenerationDigest != generationDigest)
	{
		throw std::ios_base::failure("ReplayManifest generation digest");
	}
	return true;
}

Replay::Replay()
: common::Singleton<Replay>(gpReplay, this)
{
	ReplayHarnessRigs::Attach(*this);
}

Replay::~Replay()
{
	ReplayHarnessRigs::Detach(*this);
}

void Replay::ClearReplayTransientState()
{
	// Manifest validation can fail while the live game is still running; only discard state owned exclusively by replay.
	mReplayReaders.clear();
	mPendingReplayReaders.clear();
	game::gpGame->mbReplaying = !mReplayReaders.empty() || !mPendingReplayReaders.empty();
}

void Replay::ClearReplayAbortState()
{
	// Live simulation resumes from the replayed frames, so relink clients and their owned entities to them as a load
	// does. That also frees every subscription, so the next active-set rebuild retires every non-origin cell holding no
	// owned entity until clients resubscribe.
	ClearReplayTransientState();
	game::gpServerSession->mpTransferManager->mTransfers.clear();
	game::gpServerSession->mpBroadcaster->mBroadcastStatusChanges.clear();
	game::OnStateReplaced();
}

void Replay::ResetStreams()
{
	mReplayWriters.clear();
	ClearReplayTransientState();
	miReplayInitialTick = 0;
	ReplayHarnessRigs::Reset(*this);
}

void Replay::InvalidateReplayRecording()
{
	mReplayWriters.clear();
	// A pending stop request outliving the writers would read as a record start at the next SyncReplayTick.
	game::gpGame->mGameFlags.Set(engine::GameFlags::kSaveReplay, false);
	miReplayInitialTick = 0;
	ReplayHarnessRigs::Reset(*this);
}

void Replay::UpdateTerminalReplayWriter(GridCoord coordinate, const ReplayWriterState& rWriterState, const game::Frame& rEndFrame)
{
	game::FrameInput emptyInput {};
	auto it = game::gpGame->mFrameInputs.find(coordinate);
	const game::FrameInput& rLiveInput = it != game::gpGame->mFrameInputs.end() ? it->second : emptyInput;
	rWriterState.pWriter->Update(rEndFrame.interpolate.iTick + 1, rLiveInput, rEndFrame);
}

bool Replay::CaptureAcceptedTransfers(GridCoord destination, std::span<const game::StatusChange> sortedTransfers, const game::Frame& rPreTransferFrame)
{
	if (mReplayWriters.empty())
	{
		return true;
	}
	if (ReplayHarnessRigs::ConsumePersistenceFailure(*this, ReplayHarnessRigs::PersistenceFailurePoint::kTransferCapture))
	{
		InvalidateReplayRecording();
		return false;
	}
	if (sortedTransfers.empty())
	{
		return true;
	}

	std::vector<ReplayWriterState>& rWriterGenerations = mReplayWriters.try_emplace(destination).first->second;
	if (rWriterGenerations.empty() || rWriterGenerations.back().bTerminal)
	{
		game::FrameInput emptyInput {};
		rWriterGenerations.push_back({
			.pWriter = std::make_unique<engine::DifferenceStreamWriter>(rPreTransferFrame, emptyInput, /*bRecordInitialChecksum=*/false),
			.iActivationTick = rPreTransferFrame.interpolate.iTick,
		});
	}
	ReplayWriterState& rWriterState = rWriterGenerations.back();
	game::FrameInput postDispatchInput {};
	postDispatchInput.statusChanges.assign(sortedTransfers.begin(), sortedTransfers.end());
	rWriterState.pWriter->mPostDispatchRecords.emplace_back(rPreTransferFrame.interpolate.iTick, std::move(postDispatchInput));
	ReplayHarnessRigs::ObserveAcceptedTransfers(*this, rPreTransferFrame.interpolate.iTick, sortedTransfers);
	return true;
}

void Replay::ActivateReplayReader(GridCoord coordinate, PendingReplayReader&& rPendingReader)
{
	if (!game::gpGame->mCells.contains(coordinate))
	{
		game::gpGame->CreateCellAtCoordinate(coordinate);
	}
	engine::Cell& rCell = game::gpGame->mCells.at(coordinate);
	rCell.pCurrent = std::move(rPendingReader.pSavedStart);
	engine::TransferViaStream(*rCell.pCurrent, *rCell.pNext);
	game::gpGame->mFrameInputs.insert_or_assign(coordinate, std::move(rPendingReader.initialInput));
	mReplayReaders.emplace(coordinate, std::move(rPendingReader.pReader));
	game::gpGame->mbReplaying = !mReplayReaders.empty() || !mPendingReplayReaders.empty();
}

void Replay::RetireCoordinate(GridCoord coordinate, std::unique_ptr<game::Frame> pLastCompleteFrame)
{
	auto it = mReplayWriters.find(coordinate);
	if (it == mReplayWriters.end())
	{
		return;
	}
	if (it->second.empty())
	{
		return;
	}
	if (it->second.back().bTerminal)
	{
		return;
	}

	ReplayWriterState& rWriterState = it->second.back();
	UpdateTerminalReplayWriter(coordinate, rWriterState, *pLastCompleteFrame);
	rWriterState.pRetainedEndFrame = std::move(pLastCompleteFrame);
	rWriterState.bTerminal = true;
}

void Replay::SaveLoadReplay()
{
	if constexpr (kbDebugInput)
	{
		if (game::gpGame->mGameFlags & engine::GameFlags::kLoadReplay)
		{
			// Heap: DifferenceStream reader + Frame deserialization + ReplayMeta file I/O
			ScopedSuppressAllocationTracking suppress;
			gpProfileManager->LatchRawCpuTimers(false);

			// Outside the try: its catch clears the replay harness rig and transfer-capture state that a live recording still uses.
			ASSERT(!(!mReplayWriters.empty() || (game::gpGame->mGameFlags & engine::GameFlags::kSaveReplay)));
			game::gpGame->mGameFlags.Set(engine::GameFlags::kLoadReplay, false);

			try
			{
				if (!mReplayReaders.empty() || !mPendingReplayReaders.empty())
				{
					ClearReplayAbortState();
					ReplayHarnessRigs::Reset(*this);
					return;
				}
				ClearReplayTransientState();

				// The valid manifest is the generation commit marker. Validate it before parsing any component.
				ReplayManifest manifest;
				if (!ReadReplayManifest(manifest))
				{
					ReplayHarnessRigs::Reset(*this);
					return;
				}
				int64_t iInitialTick = manifest.iInitialTick;
				const std::vector<ReplayManifestRecord>& rRecordedRecords = manifest.records;

				game::ReplayStagedMeta stagedMeta {};
				if (!game::ReadReplayMetadata({engine::FileFlags::kAppDataDirectory, engine::FileFlags::kRead}, ReplayArtifactFilename(ReplayArtifactKind::kMeta, 0, -1), stagedMeta))
				{
					LOG(kDefault, kError, "Failed to read replay metadata");
					ReplayHarnessRigs::Reset(*this);
					return;
				}

				struct StagedReplayReader
				{
					ReplayManifestRecord record;
					PendingReplayReader pendingReader;
				};
				std::vector<StagedReplayReader> stagedReaders;
				stagedReaders.reserve(static_cast<size_t>(std::ssize(rRecordedRecords)));
				for (const ReplayManifestRecord& rRecord : rRecordedRecords)
				{
					StagedReplayReader staged {.record = rRecord};
					staged.pendingReader.coordinate = rRecord.coordinate;
					staged.pendingReader.iActivationTick = rRecord.iActivationTick;
					staged.pendingReader.pSavedStart = std::make_unique<game::Frame>();
					std::filesystem::path coordinateReplayPath = ReplayArtifactFilename(ReplayArtifactKind::kCoordHeader, rRecord.coordinate.ToKey(), rRecord.iActivationTick);
					bool bLoaded = false;
					bool bVersionMismatch = false;
					int64_t iFileVersion = 0;
					int64_t iExpectedVersion = 0;
					staged.pendingReader.pReader = std::make_unique<engine::DifferenceStreamReader>(engine::FileFlags_t {engine::FileFlags::kAppDataDirectory, engine::FileFlags::kRead}, coordinateReplayPath, *staged.pendingReader.pSavedStart, staged.pendingReader.initialInput, bLoaded, bVersionMismatch, iFileVersion, iExpectedVersion, rRecord.iActivationTick == iInitialTick);
					if (bVersionMismatch)
					{
						// An older build recorded this replay: an expected refusal, not damaged data.
						LOG(kDefault, kError, "SaveLoadReplay aborted: replay version {} != expected version {}", iFileVersion, iExpectedVersion);
						ReplayHarnessRigs::Reset(*this);
						return;
					}
					if (!bLoaded)
					{
						throw std::ios_base::failure("ReplayManifest stream bounds");
					}
					if (staged.pendingReader.pReader->miStartTick != rRecord.iActivationTick)
					{
						throw std::ios_base::failure("ReplayManifest stream bounds");
					}
					if (staged.pendingReader.pReader->mSavedEnd.interpolate.iTick < rRecord.iActivationTick)
					{
						throw std::ios_base::failure("ReplayManifest stream bounds");
					}
					if (staged.pendingReader.pReader->mSavedEnd.interpolate.iTick > std::numeric_limits<int64_t>::max() - 1)
					{
						throw std::ios_base::failure("ReplayManifest stream bounds");
					}
					stagedReaders.push_back(std::move(staged));
				}
				std::unordered_map<GridCoord, int64_t> previousSavedEndTicks;
				previousSavedEndTicks.reserve(static_cast<size_t>(std::ssize(stagedReaders)));
				for (const StagedReplayReader& rStagedReader : stagedReaders)
				{
					int64_t iSavedEndTick = rStagedReader.pendingReader.pReader->mSavedEnd.interpolate.iTick;
					auto [it, bInserted] = previousSavedEndTicks.try_emplace(rStagedReader.record.coordinate, iSavedEndTick);
					if (!bInserted)
					{
						if (rStagedReader.record.iActivationTick <= it->second)
						{
							throw std::ios_base::failure("ReplayManifest overlapping coordinate generations");
						}
						it->second = iSavedEndTick;
					}
				}

				if (rRecordedRecords.front().iActivationTick != iInitialTick)
				{
					throw std::ios_base::failure("ReplayManifest missing initial record");
				}

				// Parse the grid into isolated state. Its membership must correlate with the manifest before ResetSession
				// and adoption, because those operations notify clients and replace the live game state.
				engine::StagedGridSave stagedGrid;
				if (!engine::ReadGridSave({engine::FileFlags::kAppDataDirectory, engine::FileFlags::kRead}, std::filesystem::path("F7.replay.grid"), stagedGrid))
				{
					LOG(kDefault, kError, "Failed to read replay grid");
					ReplayHarnessRigs::Reset(*this);
					return;
				}

				std::unordered_set<GridCoord> initialCoordinates;
				initialCoordinates.reserve(static_cast<size_t>(std::ssize(stagedReaders)));
				if (stagedGrid.iTick != iInitialTick)
				{
					throw std::ios_base::failure("ReplayManifest grid initial tick mismatch");
				}
				for (const StagedReplayReader& rStagedReader : stagedReaders)
				{
					if (rStagedReader.record.iActivationTick == iInitialTick)
					{
						initialCoordinates.insert(rStagedReader.record.coordinate);
						auto it = stagedGrid.cells.find(rStagedReader.record.coordinate);
						if (it == stagedGrid.cells.end())
						{
							throw std::ios_base::failure("ReplayManifest initial coord absent from grid");
						}
						const game::Frame& rGridFrame = *it->second.pCurrent;
						const game::Frame& rSavedStart = *rStagedReader.pendingReader.pSavedStart;
						if (rSavedStart.interpolate.iTick != iInitialTick || rSavedStart.Crc() != rGridFrame.Crc())
						{
							throw std::ios_base::failure("ReplayManifest initial stream does not match grid");
						}
					}
				}
				if (initialCoordinates.size() != stagedGrid.cells.size())
				{
					throw std::ios_base::failure("ReplayManifest initial coords do not match grid");
				}

				ReplayHarnessRigs::TransferCaptureSnapshot recordingCaptureInfo = ReplayHarnessRigs::CaptureSnapshot(*this);
				game::gpGame->ResetSession();
				engine::AdoptGridSave(std::move(stagedGrid));

				for (StagedReplayReader& rStagedReader : stagedReaders)
				{
					if (rStagedReader.record.iActivationTick == iInitialTick)
					{
						ActivateReplayReader(rStagedReader.record.coordinate, std::move(rStagedReader.pendingReader));
					}
					else
					{
						mPendingReplayReaders.push_back(std::move(rStagedReader.pendingReader));
					}
				}
				game::gpGame->mbReplaying = !mReplayReaders.empty() || !mPendingReplayReaders.empty();
				int64_t iTickCounter = iInitialTick;
				ASSERT(iTickCounter >= 0);
				game::gpGame->miTickCounter = iTickCounter;

				game::AdoptReplayMetadata(std::move(stagedMeta));
				game::OnStateReplaced();

				// Replay owns each active generation until its reader reaches the recorded end. Rebuild directly from
				// the successfully loaded readers so normal subscription/owned-entity pruning cannot erase an empty coord.
				game::gpGame->mActiveCoordinates.clear();
				game::gpGame->mActiveCoordinates.reserve(mReplayReaders.size());
				for (const auto& [rCoordinate, rpReader] : mReplayReaders)
				{
					game::gpGame->mActiveCoordinates.push_back(rCoordinate);
				}

				// GameBase::ResetSession clears stream-owned diagnostic state. Restore recording evidence so each replay loop
				// relatches playback.
				ReplayHarnessRigs::PlaybackAdopted(*this, recordingCaptureInfo);
				// Replay I/O is outside sim time; do not carry its wall time or pre-load debt into the new loop.
				game::gpGame->mTimeStep.mTickRemainderNanoseconds = 0ns;
				game::gpGame->mTimeStep.mRealTime.Reset();
				game::gpGame->mfLastDeltaTime = 0.0f;
			}
			catch (const std::exception& rException)
			{
				LOG(kDefault, kError, "SaveLoadReplay aborted: corrupt replay data: {}", rException.what());
				if (game::gpGame->mbReplaying)
				{
					ClearReplayAbortState();
				}
				else
				{
					ClearReplayTransientState();
				}
				ReplayHarnessRigs::Reset(*this);
				return;
			}
		}

		if ((game::gpGame->mGameFlags & engine::GameFlags::kSaveReplay) && !mReplayWriters.empty())
		{
			// Heap: manifest records, DifferenceStream writer saves, ReplayMeta + manifest file I/O
			ScopedSuppressAllocationTracking suppress;
			game::gpGame->mGameFlags.Set(engine::GameFlags::kSaveReplay, false);

			// Preserve the valid manifest's deterministic coord ordering after writer state is cleared.
			std::vector<ReplayManifestRecord> recordedRecords;
			for (const auto& [rCoordinate, rWriterGenerations] : mReplayWriters)
			{
				for (const ReplayWriterState& rWriterState : rWriterGenerations)
				{
					recordedRecords.push_back({.iActivationTick = rWriterState.iActivationTick, .coordinate = rCoordinate});
				}
			}
			std::ranges::sort(recordedRecords, ReplayManifestRecordLess);

			bool bReplayWritten = true;

			for (const auto& [rCoordinate, rWriterGenerations] : mReplayWriters)
			{
				for (const ReplayWriterState& rWriterState : rWriterGenerations)
				{
					std::filesystem::path coordinateReplayPath = ReplayArtifactFilename(ReplayArtifactKind::kCoordHeader, rCoordinate.ToKey(), rWriterState.iActivationTick);
					engine::FileFlags_t fileFlags {engine::FileFlags::kAppDataDirectory, engine::FileFlags::kWrite, engine::FileFlags::kBackup};
					const game::Frame* pEndFrame = nullptr;
					if (rWriterState.bTerminal)
					{
						pEndFrame = rWriterState.pRetainedEndFrame.get();
					}
					else
					{
						auto it = game::gpGame->mCells.find(rCoordinate);
						if (it != game::gpGame->mCells.end())
						{
							pEndFrame = it->second.pCurrent.get();
						}
					}

					bool bWriterSaved = false;
					if (pEndFrame != nullptr)
					{
						if (!rWriterState.bTerminal)
						{
							UpdateTerminalReplayWriter(rCoordinate, rWriterState, *pEndFrame);
						}
						bWriterSaved = rWriterState.pWriter->Save(fileFlags, coordinateReplayPath, *pEndFrame);
						if (ReplayHarnessRigs::ConsumePersistenceFailure(*this, ReplayHarnessRigs::PersistenceFailurePoint::kCoordinateWriter, rCoordinate, rWriterState.iActivationTick))
						{
							LOG(kDefault, kError, "Injected replay writer failure for coord ({},{}) activation {}; deleting replay sibling set", rCoordinate.iX, rCoordinate.iY, rWriterState.iActivationTick);
							rWriterState.pWriter->CleanupFiles(fileFlags, coordinateReplayPath);
							bWriterSaved = false;
						}
					}
					else
					{
						LOG(kDefault, kError, "Replay writer for coord ({},{}) activation {} has no terminal frame; deleting partial replay set", rCoordinate.iX, rCoordinate.iY, rWriterState.iActivationTick);
						rWriterState.pWriter->CleanupFiles(fileFlags, coordinateReplayPath);
					}
					if (bWriterSaved && !ReplayHarnessRigs::InjectMalformedFullFrames(*this, rCoordinate, rWriterState.iActivationTick, fileFlags, coordinateReplayPath))
					{
						rWriterState.pWriter->CleanupFiles(fileFlags, coordinateReplayPath);
						bWriterSaved = false;
					}
					bReplayWritten = bWriterSaved && bReplayWritten;
				}
			}

			mReplayWriters.clear();
			miReplayInitialTick = 0;

			// Write replay metadata for F8 load
			bool bMetadataWritten = !ReplayHarnessRigs::ConsumePersistenceFailure(*this, ReplayHarnessRigs::PersistenceFailurePoint::kMetadata) && game::WriteReplayMetadata({engine::FileFlags::kAppDataDirectory, engine::FileFlags::kWrite}, ReplayArtifactFilename(ReplayArtifactKind::kMeta, 0, -1));
			bReplayWritten = bMetadataWritten && bReplayWritten;

			if (bReplayWritten)
			{
				ReplayManifest manifest
				{
					.iInitialTick = recordedRecords.empty() ? 0 : recordedRecords.front().iActivationTick,
					.records = std::move(recordedRecords),
					.bHasFullFrames = kbReplayFullFrames,
				};
				std::array<uint8_t, 32> generationDigest {};
				bReplayWritten = !ReplayHarnessRigs::ConsumePersistenceFailure(*this, ReplayHarnessRigs::PersistenceFailurePoint::kInventory) && ComputeReplayGenerationDigest(manifest, generationDigest) && !ReplayHarnessRigs::ConsumePersistenceFailure(*this, ReplayHarnessRigs::PersistenceFailurePoint::kFinalManifest) && PublishReplayManifest(manifest, generationDigest);
			}
			ReplayHarnessRigs::RecordingStopped(*this, bReplayWritten);

			if (bReplayWritten)
			{
				LOG(kDefault, kDebug, "Recording stopped");
			}
			else
			{
				LOG(kDefault, kError, "Replay persistence failed; recording stopped without a complete replay");
			}
		}
	}
}

Replay::ReplayTickDecision Replay::SyncReplayTick()
{
	// Heap: DifferenceStream reader/writer persist across frames, growing vectors for diffs and checksums.
	//   Workbuffer is popped each frame so can't hold cross-frame state; size depends on recording length
	ScopedSuppressAllocationTracking suppress;

	if constexpr (kbDebugInput)
	{
		if ((game::gpGame->mGameFlags & engine::GameFlags::kSaveReplay) && mReplayWriters.empty())
		{
			ASSERT(!(game::gpGame->mbReplaying || (game::gpGame->mGameFlags & engine::GameFlags::kLoadReplay)));
			game::gpGame->mGameFlags.Set(engine::GameFlags::kSaveReplay, false);

			if (ReplayHarnessRigs::ConsumePersistenceFailure(*this, ReplayHarnessRigs::PersistenceFailurePoint::kManifestInvalidation))
			{
				LOG(kDefault, kError, "Injected replay manifest invalidation failure; recording not started");
				ReplayHarnessRigs::Reset(*this);
				return ReplayTickDecision::kDispatch;
			}
			if (!InvalidateReplayManifest())
			{
				LOG(kDefault, kError, "Replay manifest invalidation failed; recording not started");
				ReplayHarnessRigs::Reset(*this);
				return ReplayTickDecision::kDispatch;
			}

			mReplayReaders.clear();
			mPendingReplayReaders.clear();
			game::gpGame->mbReplaying = !mReplayReaders.empty() || !mPendingReplayReaders.empty();

			bool bGridWritten = !ReplayHarnessRigs::ConsumePersistenceFailure(*this, ReplayHarnessRigs::PersistenceFailurePoint::kGrid) && engine::WriteGridSave({engine::FileFlags::kAppDataDirectory, engine::FileFlags::kWrite}, std::filesystem::path("F7.replay.grid"), game::gpGame->mClientGridCoordinate);
			if (!bGridWritten)
			{
				LOG(kDefault, kError, "Replay grid write failed; recording not started");
				ReplayHarnessRigs::Reset(*this);
				return ReplayTickDecision::kDispatch;
			}

			miReplayInitialTick = 0;
			bool bHaveInitialTick = false;
			for (const auto& [rCoordinate, rCell] : game::gpGame->mCells)
			{
				if (!bHaveInitialTick)
				{
					miReplayInitialTick = rCell.pCurrent->interpolate.iTick;
					bHaveInitialTick = true;
				}
				else if (rCell.pCurrent->interpolate.iTick != miReplayInitialTick)
				{
					LOG(kDefault, kError, "Replay recording start has inconsistent coord ticks");
					mReplayWriters.clear();
					ReplayHarnessRigs::Reset(*this);
					return ReplayTickDecision::kDispatch;
				}

				game::FrameInput& rFrameInput = game::gpGame->mFrameInputs.try_emplace(rCoordinate).first->second;
				std::vector<ReplayWriterState>& rWriterGenerations = mReplayWriters.try_emplace(rCoordinate).first->second;
				rWriterGenerations.push_back({
					.pWriter = std::make_unique<engine::DifferenceStreamWriter>(*rCell.pCurrent, rFrameInput),
					.iActivationTick = miReplayInitialTick,
				});
			}

			ReplayHarnessRigs::RecordingStarted(*this);
			LOG(kDefault, kDebug, "Recording started for {} coords", mReplayWriters.size());
			return ReplayTickDecision::kDispatch;
		}

		if (!mReplayWriters.empty()) [[unlikely]]
		{
			bool bWriterUpdated = false;
			for (const auto& [rCoordinate, rWriterGenerations] : mReplayWriters)
			{
				const ReplayWriterState& rWriterState = rWriterGenerations.back();
				if (rWriterState.bTerminal)
				{
					continue;
				}
				if (!game::gpGame->mCells.contains(rCoordinate))
				{
					continue;
				}

				game::FrameInput& rFrameInput = game::gpGame->mFrameInputs.try_emplace(rCoordinate).first->second;
				rWriterState.pWriter->Update(game::gpGame->miTickCounter, rFrameInput, (*game::gpGame->mCells.at(rCoordinate).pCurrent));
				bWriterUpdated = true;
			}
			if (bWriterUpdated)
			{
				if (ReplayHarnessRigs::ObserveWriterInput(*this, game::gpGame->miTickCounter))
				{
					game::gpGame->mGameFlags.Set(engine::GameFlags::kPaused);
				}
			}
		}

		// Replay publication state belongs to one fixed tick. HarvestTransfers is intentionally disabled during
		// playback, so clear the staged maps here before this tick's post-dispatch records are loaded below.
		if (game::gpGame->mbReplaying)
		{
			game::gpServerSession->mpTransferManager->mTransfers.clear();
			game::gpServerSession->mpBroadcaster->mBroadcastStatusChanges.clear();
		}

		auto AbortReplay = [&]()
		{
			ClearReplayAbortState();
			ReplayHarnessRigs::Reset(*this);
			return ReplayTickDecision::kStopBeforeDispatch;
		};

		// Terminal readers retire first. Their terminal input is consumed and its saved end frame is checksum
		// validated before the coord is removed; the terminal input is not dispatched or published a second time.
		if (game::gpGame->mbReplaying) [[unlikely]]
		{
			for (auto it = mReplayReaders.begin(); it != mReplayReaders.end();)
			{
				engine::GridCoord coordinate = it->first;
				std::unique_ptr<engine::DifferenceStreamReader>& rpReader = it->second;
				bool bTerminalTick = rpReader->IsTerminalTick(game::gpGame->miTickCounter);
				if (!bTerminalTick)
				{
					++it;
					continue;
				}

				game::FrameInput& rFrameInput = game::gpGame->mFrameInputs.try_emplace(coordinate).first->second;
				if (!rpReader->LoadDifference(game::gpGame->miTickCounter, rFrameInput))
				{
					LOG(kDefault, kError, "Replay reader advanced beyond terminal tick for coord ({},{})", coordinate.iX, coordinate.iY);
					return AbortReplay();
				}
				rpReader->ValidateChecksum(game::gpGame->miTickCounter, (*game::gpGame->mCells.at(coordinate).pCurrent));
				if (!rpReader->TerminalConsumed())
				{
					LOG(kDefault, kError, "Replay reader terminal data was not fully consumed for coord ({},{})", coordinate.iX, coordinate.iY);
					return AbortReplay();
				}
				rFrameInput.statusChanges.clear();
				game::gpGame->mCells.erase(coordinate);
				game::gpGame->mFrameInputs.erase(coordinate);
				std::erase(game::gpGame->mActiveCoordinates, coordinate);
				it = mReplayReaders.erase(it);
			}
			game::gpGame->mbReplaying = !mReplayReaders.empty() || !mPendingReplayReaders.empty();

			while (!mPendingReplayReaders.empty() && mPendingReplayReaders.front().iActivationTick <= game::gpGame->miTickCounter)
			{
				PendingReplayReader pendingReader = std::move(mPendingReplayReaders.front());
				mPendingReplayReaders.erase(mPendingReplayReaders.begin());
				if (pendingReader.iActivationTick < game::gpGame->miTickCounter)
				{
					LOG(kDefault, kError, "Replay reader activation tick was skipped for coord ({},{})", pendingReader.coordinate.iX, pendingReader.coordinate.iY);
					return AbortReplay();
				}
				if (mReplayReaders.contains(pendingReader.coordinate))
				{
					LOG(kDefault, kError, "Replay reader activation overlaps live reader for coord ({},{})", pendingReader.coordinate.iX, pendingReader.coordinate.iY);
					return AbortReplay();
				}
				GridCoord coordinate = pendingReader.coordinate;
				ActivateReplayReader(coordinate, std::move(pendingReader));
			}

			// Load each current difference, then the post-dispatch channel record for this exact tick. A transfer
			// harvested after dispatch at event tick E is recorded at E and staged here for publication at E.
			for (const auto& [rCoordinate, rpReader] : mReplayReaders)
			{
				game::FrameInput& rFrameInput = game::gpGame->mFrameInputs.try_emplace(rCoordinate).first->second;
				if (!rpReader->LoadDifference(game::gpGame->miTickCounter, rFrameInput))
				{
					LOG(kDefault, kError, "Replay reader advanced beyond terminal tick for coord ({},{})", rCoordinate.iX, rCoordinate.iY);
					return AbortReplay();
				}

				// Spawns and transfers belong only to the post-dispatch channel, so a recorded
				// input carrying one would publish an entry a connected client applies against authoritative state.
				if (std::ranges::any_of(rFrameInput.statusChanges, [](const game::StatusChange& rStatusChange)
				{
					return game::IsPostDispatchType(rStatusChange.eType);
				}))
				{
					LOG(kDefault, kError, "Replay input carries a spawn or transfer status change for coord ({},{})", rCoordinate.iX, rCoordinate.iY);
					return AbortReplay();
				}

				game::FrameInput postDispatchInput {};
				if (rpReader->LoadPostDispatch(game::gpGame->miTickCounter, postDispatchInput))
				{
					// The generic channel cannot know StatusChange semantics, and a
					// valid tick-input entry would reach ApplySpawnStatusChange, which accepts only spawn and transfer tags.
					if (postDispatchInput.statusChanges.empty() || !std::ranges::all_of(postDispatchInput.statusChanges, [](const game::StatusChange& rStatusChange)
					{
						return game::IsPostDispatchType(rStatusChange.eType);
					}))
					{
						LOG(kDefault, kError, "Replay post-dispatch record is not a post-tick batch for coord ({},{})", rCoordinate.iX, rCoordinate.iY);
						return AbortReplay();
					}
					game::gpServerSession->mpTransferManager->PrepareReplayTransfers(rCoordinate, postDispatchInput.statusChanges);
					ReplayHarnessRigs::ObservePlaybackEvent(*this, game::gpGame->miTickCounter);
				}

				if (!rFrameInput.statusChanges.empty())
				{
					game::gpServerSession->mpBroadcaster->mBroadcastStatusChanges.insert_or_assign(rCoordinate, rFrameInput.statusChanges);
				}
				rpReader->ValidateChecksum(game::gpGame->miTickCounter, (*game::gpGame->mCells.at(rCoordinate).pCurrent));
			}

			if (mReplayReaders.empty() && mPendingReplayReaders.empty())
			{
				LOG(kDefault, kDebug, "End replay {}, looping", game::gpGame->miTickCounter);
				ClearReplayTransientState();
				game::gpGame->mGameFlags.Set(engine::GameFlags::kLoadReplay);
				return ReplayTickDecision::kStopBeforeDispatch;
			}
		}
	}

	return ReplayTickDecision::kDispatch;
}

} // namespace engine

#endif // BT_SERVER
