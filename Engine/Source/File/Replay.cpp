#include "Pch.h"

#if defined(BT_SERVER)

#include "File/Replay.h"

#include "Agent/Commands/ReplayFixtures.h"
#include "File/GridSave.h"
#include "Network/Server/ServerBroadcaster.h"
#include "Network/Server/ServerTransferManager.h"
#include "GameBase.h"

#include "Network/Server/ServerSession.h"
#include "Profile/ProfileManager.h"
#include "Game.h"

namespace engine
{

// On-disk version of the F7.replay.manifest generation inventory. Old manifests are rejected on read.
constexpr int64_t kiReplayManifestVersion = 4;
constexpr int64_t kiInvalidReplayManifestVersion = 0;

template <std::integral TYPE>
static void WriteReplayManifestValue(std::ostream& rStream, TYPE value)
{
	using UnsignedType = std::make_unsigned_t<TYPE>;
	UnsignedType uiValue = std::bit_cast<UnsignedType>(value);
	for (int64_t i = 0; i < static_cast<int64_t>(sizeof(TYPE)); ++i)
	{
		rStream.put(static_cast<char>((uiValue >> (i * 8)) & 0xFFu));
	}
}

template <std::integral TYPE>
static bool ReadReplayManifestValue(std::istream& rStream, TYPE& rValue)
{
	using UnsignedType = std::make_unsigned_t<TYPE>;
	UnsignedType uiValue = 0;
	for (int64_t i = 0; i < static_cast<int64_t>(sizeof(TYPE)); ++i)
	{
		int iByte = rStream.get();
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

struct ReplayManifestInventoryEntry
{
	ReplayArtifactKind eKind {};
	uint64_t uiCoordinateKey = 0;
	int64_t iActivationTick = -1;
	engine::FileContentDigest digest;
};

struct ReplayManifest
{
	int64_t iInitialTick = 0;
	std::vector<ReplayManifestRecord> records;
	bool bHasFullFrames = false;
	std::vector<ReplayManifestInventoryEntry> inventory;
};

constexpr std::string_view kReplayManifestGenerationDomain = "broken-engine/replay-manifest-generation/v4";
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

static bool ReplayInventoryEntryLess(const ReplayManifestInventoryEntry& rLeft, const ReplayManifestInventoryEntry& rRight)
{
	return std::tie(rLeft.eKind, rLeft.uiCoordinateKey, rLeft.iActivationTick) < std::tie(rRight.eKind, rRight.uiCoordinateKey, rRight.iActivationTick);
}

static bool AppendReplayManifestPayload(const ReplayManifest& rManifest, std::vector<std::byte>& rPayload)
{
	if (rManifest.records.size() > static_cast<size_t>(std::numeric_limits<int64_t>::max()) || rManifest.inventory.size() > static_cast<size_t>(std::numeric_limits<int64_t>::max()))
	{
		return false;
	}
	static constexpr size_t kuiFixedBytes = sizeof(int64_t) * 4 + 1;
	if (rManifest.records.size() > (std::numeric_limits<size_t>::max() - kuiFixedBytes) / 16
	 || rManifest.inventory.size() > (std::numeric_limits<size_t>::max() - kuiFixedBytes - rManifest.records.size() * 16) / 57)
	{
		return false;
	}

	rPayload.clear();
	rPayload.reserve(kuiFixedBytes + rManifest.records.size() * 16 + rManifest.inventory.size() * 57);
	auto AppendValue = [&rPayload]<std::integral TYPE>(TYPE value)
	{
		using UnsignedType = std::make_unsigned_t<TYPE>;
		UnsignedType uiValue = std::bit_cast<UnsignedType>(value);
		for (int64_t i = 0; i < static_cast<int64_t>(sizeof(TYPE)); ++i)
		{
			rPayload.push_back(static_cast<std::byte>((uiValue >> (i * 8)) & 0xFFu));
		}
	};

	AppendValue(kiReplayManifestVersion);
	AppendValue(rManifest.iInitialTick);
	AppendValue(static_cast<int64_t>(rManifest.records.size()));
	for (const ReplayManifestRecord& rRecord : rManifest.records)
	{
		AppendValue(rRecord.iActivationTick);
		AppendValue(rRecord.coordinate.iX);
		AppendValue(rRecord.coordinate.iY);
	}
	AppendValue(static_cast<uint8_t>(rManifest.bHasFullFrames ? 1 : 0));
	AppendValue(static_cast<int64_t>(rManifest.inventory.size()));
	for (const ReplayManifestInventoryEntry& rEntry : rManifest.inventory)
	{
		AppendValue(static_cast<uint8_t>(rEntry.eKind));
		AppendValue(rEntry.uiCoordinateKey);
		AppendValue(rEntry.iActivationTick);
		AppendValue(rEntry.digest.iByteCount);
		rPayload.insert(rPayload.end(), reinterpret_cast<const std::byte*>(rEntry.digest.sha256.data()), reinterpret_cast<const std::byte*>(rEntry.digest.sha256.data() + rEntry.digest.sha256.size()));
	}
	return true;
}

static bool ComputeReplayGenerationDigest(const ReplayManifest& rManifest, std::array<uint8_t, 32>& rDigest)
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
	for (int64_t i = 0; i < static_cast<int64_t>(sizeof(uint32_t)); ++i)
	{
		rootPreimage.push_back(static_cast<std::byte>((static_cast<uint32_t>(kReplayManifestGenerationDomain.size()) >> (i * 8)) & 0xFFu));
	}
	rootPreimage.insert(rootPreimage.end(), reinterpret_cast<const std::byte*>(kReplayManifestGenerationDomain.data()), reinterpret_cast<const std::byte*>(kReplayManifestGenerationDomain.data() + kReplayManifestGenerationDomain.size()));
	rootPreimage.insert(rootPreimage.end(), payload.begin(), payload.end());
	return engine::gpFileManager->ComputeSha256(rootPreimage, rDigest);
}

static bool BuildExpectedReplayInventory(ReplayManifest& rManifest, bool bHashFiles)
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
			rManifest.inventory.push_back({.eKind = eKind, .uiCoordinateKey = rRecord.coordinate.ToKey(), .iActivationTick = rRecord.iActivationTick});
		}
	}
	std::ranges::sort(rManifest.inventory, ReplayInventoryEntryLess);
	for (int64_t i = 1; i < std::ssize(rManifest.inventory); ++i)
	{
		if (rManifest.inventory.at(i - 1).eKind == rManifest.inventory.at(i).eKind
		 && rManifest.inventory.at(i - 1).uiCoordinateKey == rManifest.inventory.at(i).uiCoordinateKey
		 && rManifest.inventory.at(i - 1).iActivationTick == rManifest.inventory.at(i).iActivationTick)
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
		if (!engine::gpFileManager->ComputeOrdinaryFileSha256({engine::FileFlags::kAppDataDirectory, engine::FileFlags::kRead}, ReplayArtifactFilename(rEntry.eKind, rEntry.uiCoordinateKey, rEntry.iActivationTick), rEntry.digest))
		{
			return false;
		}
	}
	return true;
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

Replay::Replay()
{
	ASSERT(gpReplay == nullptr);

	gpReplay = this;
	ReplayFixtures::Attach(*this);
}

Replay::~Replay()
{
	ReplayFixtures::Detach(*this);
	if (gpReplay == this)
	{
		gpReplay = nullptr;
	}
}

void Replay::ClearReplayTransientState()
{
	// Manifest validation can fail while the live game is still running; only discard state owned exclusively by replay.
	mReplayReaders.clear();
	mPendingReplayReaders.clear();
	game::gpGame->mbReplaying = !mReplayReaders.empty() || !mPendingReplayReaders.empty();
	game::OnReplayStreamsInvalidated();
}

void Replay::ClearReplayAbortState()
{
	// Live simulation resumes from the replayed frames, so relink clients and fleets to them as a load does. That also
	// frees every subscription, so the next active-set rebuild retires every non-origin cell holding no Player until
	// clients resubscribe.
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
	ReplayFixtures::Reset(*this);
}

void Replay::InvalidateReplayRecording()
{
	mReplayWriters.clear();
	miReplayInitialTick = 0;
	ReplayFixtures::Reset(*this);
	game::OnReplayStreamsInvalidated();
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
		return false;
	}
	if (ReplayFixtures::ConsumePersistenceFailure(*this, ReplayFixtures::PersistenceFailurePoint::kTransferCapture))
	{
		InvalidateReplayRecording();
		return true;
	}
	if (sortedTransfers.empty())
	{
		return false;
	}

	std::vector<ReplayWriterState>& rWriterGenerations = mReplayWriters.try_emplace(destination).first->second;
	if (rWriterGenerations.empty() || rWriterGenerations.back().bTerminal)
	{
		game::FrameInput emptyInput {};
		rWriterGenerations.push_back({
			.pWriter = std::make_unique<engine::DifferenceStreamWriter<game::Frame, game::FrameInput>>(rPreTransferFrame, emptyInput, /*bRecordInitialChecksum=*/false),
			.iActivationTick = rPreTransferFrame.interpolate.iTick,
		});
	}
	ReplayWriterState& rWriterState = rWriterGenerations.back();
	game::FrameInput postDispatchInput {};
	postDispatchInput.statusChanges.assign(sortedTransfers.begin(), sortedTransfers.end());
	rWriterState.pWriter->mPostDispatchRecords.emplace_back(rPreTransferFrame.interpolate.iTick, postDispatchInput);
	ReplayFixtures::ObserveAcceptedTransfers(*this, rPreTransferFrame.interpolate.iTick, sortedTransfers);
	return false;
}

void Replay::ActivateReplayReader(GridCoord coordinate, PendingReplayReader&& rPendingReader)
{
	if (!game::gpGame->mCoordinateFrames.contains(coordinate))
	{
		game::gpGame->CreateFrameAtCoordinate(coordinate);
	}
	engine::CoordFrames& rFrames = game::gpGame->mCoordinateFrames.at(coordinate);
	rFrames.pCurrent = std::move(rPendingReader.pSavedStart);
	rFrames.pNext = std::make_unique<game::Frame>();
	engine::TransferViaStream(*rFrames.pCurrent, *rFrames.pNext);
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
			gpProfileManager->LatchRawCpuTimers(false, game::gpGame->miTickCounter);

			// Outside the try: its catch clears the replay fixture and transfer-capture state that a live recording still uses.
			ASSERT(!(!mReplayWriters.empty() || (game::gpGame->mGameFlags & engine::GameFlags::kSaveReplay)));
			game::gpGame->mGameFlags.Set(engine::GameFlags::kLoadReplay, false);

			try
			{
				if (!mReplayReaders.empty() || !mPendingReplayReaders.empty())
				{
					ClearReplayAbortState();
					ReplayFixtures::Reset(*this);
					return;
				}
				ClearReplayTransientState();

				// The valid manifest is the generation commit marker. Validate it before reading any component.
				std::fstream manifestStream = engine::gpFileManager->OpenFile({engine::FileFlags::kAppDataDirectory, engine::FileFlags::kRead}, std::filesystem::path("F7.replay.manifest"));
				if (!manifestStream)
				{
					LOG(kDefault, kError, "Failed to read replay manifest");
					ReplayFixtures::Reset(*this);
					return;
				}
				int64_t iManifestVersion = 0;
				if (!ReadReplayManifestValue(manifestStream, iManifestVersion))
				{
					throw std::ios_base::failure("ReplayManifest version");
				}
				if (iManifestVersion != kiReplayManifestVersion)
				{
					LOG(kDefault, kError, "Replay manifest version {} != {}", iManifestVersion, kiReplayManifestVersion);
					ReplayFixtures::Reset(*this);
					return;
				}

				ReplayManifest manifest;
				if (!ReadReplayManifestValue(manifestStream, manifest.iInitialTick))
				{
					throw std::ios_base::failure("ReplayManifest initial tick");
				}
				int64_t iCoordinateCount = 0;
				if (!ReadReplayManifestValue(manifestStream, iCoordinateCount))
				{
					throw std::ios_base::failure("ReplayManifest activation count");
				}
				int64_t iInitialTick = manifest.iInitialTick;
				if (iInitialTick < 0 || iInitialTick > std::numeric_limits<int64_t>::max() - 1)
				{
					throw std::ios_base::failure("ReplayManifest initial tick");
				}
				common::ValidateDeserializedCount(iCoordinateCount, 16, manifestStream, "ReplayManifest records");
				if (iCoordinateCount <= 0)
				{
					throw std::ios_base::failure("ReplayManifest empty records");
				}
				manifest.records.reserve(iCoordinateCount);
				for (int64_t i = 0; i < iCoordinateCount; ++i)
				{
					ReplayManifestRecord record {};
					if (!ReadReplayManifestValue(manifestStream, record.iActivationTick) || !ReadReplayManifestValue(manifestStream, record.coordinate.iX) || !ReadReplayManifestValue(manifestStream, record.coordinate.iY))
					{
						throw std::ios_base::failure("ReplayManifest activation record");
					}
					if (record.iActivationTick < iInitialTick)
					{
						throw std::ios_base::failure("ReplayManifest non-canonical record");
					}
					if (record.iActivationTick > std::numeric_limits<int64_t>::max() - 1)
					{
						throw std::ios_base::failure("ReplayManifest non-canonical record");
					}
					if (!manifest.records.empty() && !ReplayManifestRecordLess(manifest.records.back(), record))
					{
						throw std::ios_base::failure("ReplayManifest non-canonical record");
					}
					manifest.records.push_back(record);
				}
				uint8_t uiHasFullFrames = 0;
				if (!ReadReplayManifestValue(manifestStream, uiHasFullFrames) || uiHasFullFrames > 1)
				{
					throw std::ios_base::failure("ReplayManifest fullframes flag");
				}
				manifest.bHasFullFrames = uiHasFullFrames != 0;
				if (manifest.bHasFullFrames != kbReplayFullFrames)
				{
					throw std::ios_base::failure("ReplayManifest fullframes build mismatch");
				}

				int64_t iInventoryCount = 0;
				if (!ReadReplayManifestValue(manifestStream, iInventoryCount))
				{
					throw std::ios_base::failure("ReplayManifest inventory count");
				}
				common::ValidateDeserializedCount(iInventoryCount, 57, manifestStream, "ReplayManifest inventory");
				if (iInventoryCount <= 0)
				{
					throw std::ios_base::failure("ReplayManifest empty inventory");
				}
				manifest.inventory.reserve(iInventoryCount);
				for (int64_t i = 0; i < iInventoryCount; ++i)
				{
					uint8_t uiKind = 0;
					ReplayManifestInventoryEntry entry;
					if (!ReadReplayManifestValue(manifestStream, uiKind))
					{
						throw std::ios_base::failure("ReplayManifest inventory entry");
					}
					if (uiKind > static_cast<uint8_t>(ReplayArtifactKind::kFullFrames))
					{
						throw std::ios_base::failure("ReplayManifest inventory entry");
					}
					if (!ReadReplayManifestValue(manifestStream, entry.uiCoordinateKey))
					{
						throw std::ios_base::failure("ReplayManifest inventory entry");
					}
					if (!ReadReplayManifestValue(manifestStream, entry.iActivationTick))
					{
						throw std::ios_base::failure("ReplayManifest inventory entry");
					}
					if (!ReadReplayManifestValue(manifestStream, entry.digest.iByteCount))
					{
						throw std::ios_base::failure("ReplayManifest inventory entry");
					}
					if (entry.digest.iByteCount < 0)
					{
						throw std::ios_base::failure("ReplayManifest inventory entry");
					}
					entry.eKind = static_cast<ReplayArtifactKind>(uiKind);
					manifestStream.read(reinterpret_cast<char*>(entry.digest.sha256.data()), static_cast<std::streamsize>(entry.digest.sha256.size()));
					if (!manifestStream)
					{
						throw std::ios_base::failure("ReplayManifest inventory digest");
					}
					if (!manifest.inventory.empty() && !ReplayInventoryEntryLess(manifest.inventory.back(), entry))
					{
						throw std::ios_base::failure("ReplayManifest non-canonical inventory");
					}
					manifest.inventory.push_back(entry);
				}
				std::array<uint8_t, 32> generationDigest {};
				manifestStream.read(reinterpret_cast<char*>(generationDigest.data()), static_cast<std::streamsize>(generationDigest.size()));
				if (!manifestStream || manifestStream.peek() != std::char_traits<char>::eof())
				{
					throw std::ios_base::failure("ReplayManifest trailing data");
				}

				ReplayManifest expectedManifest = manifest;
				if (!BuildExpectedReplayInventory(expectedManifest, false) || expectedManifest.inventory.size() != manifest.inventory.size())
				{
					throw std::ios_base::failure("ReplayManifest inventory shape");
				}
				for (int64_t i = 0; i < std::ssize(manifest.inventory); ++i)
				{
					if (expectedManifest.inventory.at(i).eKind != manifest.inventory.at(i).eKind)
					{
						throw std::ios_base::failure("ReplayManifest inventory identity");
					}
					if (expectedManifest.inventory.at(i).uiCoordinateKey != manifest.inventory.at(i).uiCoordinateKey)
					{
						throw std::ios_base::failure("ReplayManifest inventory identity");
					}
					if (expectedManifest.inventory.at(i).iActivationTick != manifest.inventory.at(i).iActivationTick)
					{
						throw std::ios_base::failure("ReplayManifest inventory identity");
					}
				}
				std::array<uint8_t, 32> expectedGenerationDigest {};
				if (!ComputeReplayGenerationDigest(manifest, expectedGenerationDigest) || expectedGenerationDigest != generationDigest)
				{
					throw std::ios_base::failure("ReplayManifest generation digest");
				}
				for (const ReplayManifestInventoryEntry& rEntry : manifest.inventory)
				{
					engine::FileContentDigest actualDigest;
					std::filesystem::path filename = ReplayArtifactFilename(rEntry.eKind, rEntry.uiCoordinateKey, rEntry.iActivationTick);
					if (!engine::gpFileManager->ComputeOrdinaryFileSha256({engine::FileFlags::kAppDataDirectory, engine::FileFlags::kRead}, filename, actualDigest)
					 || actualDigest.iByteCount != rEntry.digest.iByteCount || actualDigest.sha256 != rEntry.digest.sha256)
					{
						throw std::ios_base::failure("ReplayManifest inventory file");
					}
				}
				const std::vector<ReplayManifestRecord>& rRecordedRecords = manifest.records;

				game::ReplayStagedMeta stagedMeta {};
				if (!game::ReadReplayMeta({engine::FileFlags::kAppDataDirectory, engine::FileFlags::kRead}, ReplayArtifactFilename(ReplayArtifactKind::kMeta, 0, -1), stagedMeta))
				{
					LOG(kDefault, kError, "Failed to read replay metadata");
					ReplayFixtures::Reset(*this);
					return;
				}

				struct StagedReplayReader
				{
					ReplayManifestRecord record;
					PendingReplayReader pendingReader;
				};
				std::vector<StagedReplayReader> stagedReaders;
				stagedReaders.reserve(rRecordedRecords.size());
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
					staged.pendingReader.pReader = std::make_unique<engine::DifferenceStreamReader<game::Frame, game::FrameInput>>(engine::FileFlags_t {engine::FileFlags::kAppDataDirectory, engine::FileFlags::kRead}, coordinateReplayPath, *staged.pendingReader.pSavedStart, staged.pendingReader.initialInput, bLoaded, bVersionMismatch, iFileVersion, iExpectedVersion, rRecord.iActivationTick == iInitialTick);
					if (bVersionMismatch)
					{
						// An older build recorded this replay: an expected refusal, not damaged data.
						LOG(kDefault, kError, "SaveLoadReplay aborted: replay version {} != expected version {}", iFileVersion, iExpectedVersion);
						ReplayFixtures::Reset(*this);
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
				previousSavedEndTicks.reserve(stagedReaders.size());
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

				// Parse the grid into isolated state. Its membership must correlate with the manifest before Reset
				// and adoption, because those operations notify clients and replace the live game state.
				engine::StagedGridSave stagedGrid;
				if (!engine::ReadGridSave({engine::FileFlags::kAppDataDirectory, engine::FileFlags::kRead}, std::filesystem::path("F7.replay.grid"), stagedGrid))
				{
					LOG(kDefault, kError, "Failed to read replay grid");
					ReplayFixtures::Reset(*this);
					return;
				}

				std::unordered_set<GridCoord> initialCoordinates;
				initialCoordinates.reserve(stagedReaders.size());
				if (stagedGrid.iTick != iInitialTick)
				{
					throw std::ios_base::failure("ReplayManifest grid initial tick mismatch");
				}
				for (const StagedReplayReader& rStagedReader : stagedReaders)
				{
					if (rStagedReader.record.iActivationTick == iInitialTick)
					{
						initialCoordinates.insert(rStagedReader.record.coordinate);
						auto it = stagedGrid.coordinateFrames.find(rStagedReader.record.coordinate);
						if (it == stagedGrid.coordinateFrames.end())
						{
							throw std::ios_base::failure("ReplayManifest initial coord absent from grid");
						}
						const game::Frame& rGridFrame = *it->second.pCurrent;
						const game::Frame& rSavedStart = *rStagedReader.pendingReader.pSavedStart;
						if (rSavedStart.interpolate.iTick != iInitialTick
						 || std::bit_cast<uint32_t>(rSavedStart.interpolate.fCurrentTime) != std::bit_cast<uint32_t>(stagedGrid.fCurrentTime)
						 || rSavedStart.Crc() != rGridFrame.Crc())
						{
							throw std::ios_base::failure("ReplayManifest initial stream does not match grid");
						}
					}
				}
				if (initialCoordinates.size() != stagedGrid.coordinateFrames.size())
				{
					throw std::ios_base::failure("ReplayManifest initial coords do not match grid");
				}

				ReplayFixtures::TransferCaptureSnapshot recordingCaptureInfo = ReplayFixtures::CaptureSnapshot(*this);
				game::gpGame->Reset();
				engine::AdoptGridSave(std::move(stagedGrid));

				float fInitialTime = stagedReaders.front().pendingReader.pSavedStart->interpolate.fCurrentTime;
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
				game::gpGame->mfCurrentTime = fInitialTime;

				game::AdoptReplayMeta(std::move(stagedMeta));
				game::OnStateReplaced();

				// Replay owns each active generation until its reader reaches the recorded end. Rebuild directly from
				// the successfully loaded readers so normal subscription/player pruning cannot erase an empty coord.
				game::gpGame->mActiveCoordinates.clear();
				game::gpGame->mActiveCoordinates.reserve(mReplayReaders.size());
				for (const auto& [rCoordinate, rpReader] : mReplayReaders)
				{
					game::gpGame->mActiveCoordinates.push_back(rCoordinate);
				}

				// Game::Reset clears stream-owned diagnostic state. Restore recording evidence so each replay loop
				// relatches playback.
				ReplayFixtures::PlaybackAdopted(*this, recordingCaptureInfo);
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
				ReplayFixtures::Reset(*this);
				return;
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

			if (ReplayFixtures::ConsumePersistenceFailure(*this, ReplayFixtures::PersistenceFailurePoint::kManifestInvalidation))
			{
				LOG(kDefault, kError, "Injected replay manifest invalidation failure; recording not started");
				ReplayFixtures::Reset(*this);
				game::OnReplayStreamsInvalidated();
				return ReplayTickDecision::kDispatch;
			}
			if (!InvalidateReplayManifest())
			{
				LOG(kDefault, kError, "Replay manifest invalidation failed; recording not started");
				ReplayFixtures::Reset(*this);
				game::OnReplayStreamsInvalidated();
				return ReplayTickDecision::kDispatch;
			}

			mReplayReaders.clear();
			mPendingReplayReaders.clear();
			game::gpGame->mbReplaying = !mReplayReaders.empty() || !mPendingReplayReaders.empty();

			bool bGridWritten = !ReplayFixtures::ConsumePersistenceFailure(*this, ReplayFixtures::PersistenceFailurePoint::kGrid) && engine::WriteGridSave({engine::FileFlags::kAppDataDirectory, engine::FileFlags::kWrite}, std::filesystem::path("F7.replay.grid"), game::gpGame->mClientGridCoordinate);
			if (!bGridWritten)
			{
				LOG(kDefault, kError, "Replay grid write failed; recording not started");
				ReplayFixtures::Reset(*this);
				game::OnReplayStreamsInvalidated();
				return ReplayTickDecision::kDispatch;
			}

			miReplayInitialTick = 0;
			bool bHaveInitialTick = false;
			for (const auto& [rCoordinate, rFrames] : game::gpGame->mCoordinateFrames)
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
					ReplayFixtures::Reset(*this);
					game::OnReplayStreamsInvalidated();
					return ReplayTickDecision::kDispatch;
				}

				game::FrameInput& rFrameInput = game::gpGame->mFrameInputs.try_emplace(rCoordinate).first->second;
				std::vector<ReplayWriterState>& rWriterGenerations = mReplayWriters.try_emplace(rCoordinate).first->second;
				rWriterGenerations.push_back({
					.pWriter = std::make_unique<engine::DifferenceStreamWriter<game::Frame, game::FrameInput>>(*rFrames.pCurrent, rFrameInput),
					.iActivationTick = miReplayInitialTick,
				});
			}

			ReplayFixtures::RecordingStarted(*this);
			LOG(kDefault, kDebug, "Recording started for {} coords", mReplayWriters.size());
			return ReplayTickDecision::kDispatch;
		}

		if ((game::gpGame->mGameFlags & engine::GameFlags::kSaveReplay) && !mReplayWriters.empty())
		{
			game::gpGame->mGameFlags.Set(engine::GameFlags::kSaveReplay, false);
			game::OnReplayStreamsInvalidated();

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
						auto it = game::gpGame->mCoordinateFrames.find(rCoordinate);
						if (it != game::gpGame->mCoordinateFrames.end())
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
						if (ReplayFixtures::ConsumePersistenceFailure(*this, ReplayFixtures::PersistenceFailurePoint::kCoordinateWriter, rCoordinate, rWriterState.iActivationTick))
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
					if (bWriterSaved && ReplayFixtures::ConsumePersistenceFailure(*this, ReplayFixtures::PersistenceFailurePoint::kFullFramesRecord, rCoordinate, rWriterState.iActivationTick))
					{
						std::filesystem::path fullFramesPath = std::filesystem::path(coordinateReplayPath).concat(".fullframes");
						std::fstream fullFramesStream = engine::gpFileManager->OpenFile({engine::FileFlags::kAppDataDirectory, engine::FileFlags::kRead}, fullFramesPath);
						fullFramesStream.seekg(0, std::ios::end);
						std::streamoff iBeforeBytes = fullFramesStream.tellg();
						bool bFullFramesTruncated = fullFramesStream.is_open() && iBeforeBytes > 0
						                         && iBeforeBytes <= std::numeric_limits<std::streamsize>::max();
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
							LOG(kDefault, kDebug, "Injected malformed replay fullframes for coord ({},{}); bytes {} -> {}", rCoordinate.iX, rCoordinate.iY, iBeforeBytes, prefix.size());
						}
						else
						{
							LOG(kDefault, kError, "Injected malformed replay fullframes failed for coord ({},{}); deleting replay sibling set", rCoordinate.iX, rCoordinate.iY);
							rWriterState.pWriter->CleanupFiles(fileFlags, coordinateReplayPath);
							bWriterSaved = false;
						}
					}
					bReplayWritten = bWriterSaved && bReplayWritten;
				}
			}

			mReplayWriters.clear();
			miReplayInitialTick = 0;

			// Write replay metadata for F8 load
			bool bMetadataWritten = !ReplayFixtures::ConsumePersistenceFailure(*this, ReplayFixtures::PersistenceFailurePoint::kMetadata) && game::WriteReplayMeta({engine::FileFlags::kAppDataDirectory, engine::FileFlags::kWrite}, ReplayArtifactFilename(ReplayArtifactKind::kMeta, 0, -1));
			bReplayWritten = bMetadataWritten && bReplayWritten;

			if (bReplayWritten)
			{
				ReplayManifest manifest
				{
					.iInitialTick = recordedRecords.empty() ? 0 : recordedRecords.front().iActivationTick,
					.records = recordedRecords,
					.bHasFullFrames = kbReplayFullFrames,
				};
				std::array<uint8_t, 32> generationDigest {};
				bReplayWritten = !ReplayFixtures::ConsumePersistenceFailure(*this, ReplayFixtures::PersistenceFailurePoint::kInventory) && BuildExpectedReplayInventory(manifest, true) && ComputeReplayGenerationDigest(manifest, generationDigest) && !ReplayFixtures::ConsumePersistenceFailure(*this, ReplayFixtures::PersistenceFailurePoint::kFinalManifest) && PublishReplayManifest(manifest, generationDigest);
			}
			ReplayFixtures::RecordingStopped(*this, bReplayWritten);

			if (bReplayWritten)
			{
				LOG(kDefault, kDebug, "Recording stopped");
			}
			else
			{
				LOG(kDefault, kError, "Replay persistence failed; recording stopped without a complete replay");
			}
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
				if (!game::gpGame->mCoordinateFrames.contains(rCoordinate))
				{
					continue;
				}

				game::FrameInput& rFrameInput = game::gpGame->mFrameInputs.try_emplace(rCoordinate).first->second;
				rWriterState.pWriter->Update(game::gpGame->miTickCounter, rFrameInput, (*game::gpGame->mCoordinateFrames.at(rCoordinate).pCurrent));
				bWriterUpdated = true;
			}
			if (bWriterUpdated)
			{
				if (ReplayFixtures::ObserveWriterInput(*this, game::gpGame->miTickCounter))
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
			ReplayFixtures::Reset(*this);
			return ReplayTickDecision::kStopBeforeDispatch;
		};

		// Terminal readers retire first. Their terminal input is consumed and its saved end frame is checksum
		// validated before the coord is removed; the terminal input is not dispatched or published a second time.
		if (game::gpGame->mbReplaying) [[unlikely]]
		{
			for (auto it = mReplayReaders.begin(); it != mReplayReaders.end();)
			{
				engine::GridCoord coordinate = it->first;
				std::unique_ptr<engine::DifferenceStreamReader<game::Frame, game::FrameInput>>& rpReader = it->second;
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
				rpReader->ValidateChecksum(game::gpGame->miTickCounter, (*game::gpGame->mCoordinateFrames.at(coordinate).pCurrent));
				if (!rpReader->TerminalConsumed())
				{
					LOG(kDefault, kError, "Replay reader terminal data was not fully consumed for coord ({},{})", coordinate.iX, coordinate.iY);
					return AbortReplay();
				}
				rFrameInput.statusChanges.clear();
				game::gpGame->mCoordinateFrames.erase(coordinate);
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

				// Transfers belong only to the post-dispatch channel, so a recorded
				// input carrying one would publish an entry a connected client applies against authoritative state.
				if (std::ranges::any_of(rFrameInput.statusChanges, [](const game::StatusChange& rStatusChange)
				{
					return game::IsTransferType(rStatusChange.eType);
				}))
				{
					LOG(kDefault, kError, "Replay input carries a transfer status change for coord ({},{})", rCoordinate.iX, rCoordinate.iY);
					return AbortReplay();
				}

				game::FrameInput postDispatchInput {};
				if (rpReader->LoadPostDispatch(game::gpGame->miTickCounter, postDispatchInput))
				{
					// The generic channel cannot know StatusChange semantics, and a
					// valid non-transfer entry would reach std::get<TransferData> in the spawn path.
					if (postDispatchInput.statusChanges.empty() || !std::ranges::all_of(postDispatchInput.statusChanges, [](const game::StatusChange& rStatusChange)
					{
						return game::IsTransferType(rStatusChange.eType);
					}))
					{
						LOG(kDefault, kError, "Replay post-dispatch record is not a transfer batch for coord ({},{})", rCoordinate.iX, rCoordinate.iY);
						return AbortReplay();
					}
					game::gpServerSession->mpTransferManager->PrepareReplayTransfers(rCoordinate, postDispatchInput.statusChanges);
					ReplayFixtures::ObservePlaybackEvent(*this, game::gpGame->miTickCounter);
				}

				if (!rFrameInput.statusChanges.empty())
				{
					game::gpServerSession->mpBroadcaster->mBroadcastStatusChanges.insert_or_assign(rCoordinate, rFrameInput.statusChanges);
				}
				rpReader->ValidateChecksum(game::gpGame->miTickCounter, (*game::gpGame->mCoordinateFrames.at(rCoordinate).pCurrent));
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
