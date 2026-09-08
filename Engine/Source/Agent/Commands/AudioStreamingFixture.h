#pragma once

#if defined(BT_CLIENT) && defined(BT_DEBUG)

#include "File/FileManager.h"

namespace engine
{

class PackChunks;
class StreamingVoice;

enum class AudioStreamingFixturePartition : uint8_t
{
	kMain,
	kLoader0,
	kLoader1,
};

enum class AudioStreamingFixturePhase : uint8_t
{
	kStartup,
	kRefillQueued,
	kRefillLoading,
	kRefillReady,
	kRefillCancelled,
	kExistingQueued,
	kExistingComplete,
};

enum class AudioStreamingFixtureQueueState : uint8_t
{
	kFree,
	kQueued,
	kLoading,
	kReady,
};

enum class AudioStreamingFixtureHoldOwner : uint8_t
{
	kNone,
	kControlled,
	kInvalid,
};

enum class AudioStreamingFixtureHoldState : uint8_t
{
	kIdle,
	kArmed,
	kConsumed,
};

enum class AudioStreamingFixtureSnapshotFlags : uint8_t
{
	kLoaderStaged = 0x01,
	kCoherent = 0x02,
	kGapFree = 0x04,
};
using AudioStreamingFixtureSnapshotFlags_t = common::Flags<AudioStreamingFixtureSnapshotFlags>;

enum class AudioStreamingFixtureVoiceFlags : uint8_t
{
	kPresent = 0x01,
	kFadingIn = 0x02,
	kFadingOut = 0x04,
	kUnderrunning = 0x08,
};
using AudioStreamingFixtureVoiceFlags_t = common::Flags<AudioStreamingFixtureVoiceFlags>;

enum class AudioStreamingFixtureAudioSnapshotFlags : uint8_t
{
	kControlledPublication = 0x01,
	kSaturationActive = 0x02,
};
using AudioStreamingFixtureAudioSnapshotFlags_t = common::Flags<AudioStreamingFixtureAudioSnapshotFlags>;

enum class AudioStreamingFixtureCommandFlags : uint8_t
{
	kSaturationActive = 0x01,
};
using AudioStreamingFixtureCommandFlags_t = common::Flags<AudioStreamingFixtureCommandFlags>;

struct AudioStreamingFixtureRecord
{
	uint64_t uiSequence = 0;
	uint64_t uiScenarioGeneration = 0;
	uint64_t uiGeneration = 0;
	uint32_t uiThreadId = 0;
	int32_t iThreadPriority = 0;
	uint32_t uiPoolIndex = std::numeric_limits<uint32_t>::max();
	common::crc_t crc = 0;
	uint64_t uiOffset = 0;
	uint64_t uiLength = 0;
	AudioStreamingFixturePartition ePartition = AudioStreamingFixturePartition::kMain;
	AudioStreamingFixturePhase ePhase = AudioStreamingFixturePhase::kStartup;
	AudioStreamingFixtureQueueState eState = AudioStreamingFixtureQueueState::kFree;
	bool bCancelAcknowledged = false;
};

struct AudioStreamingFixturePoolEntry
{
	uint32_t uiIndex = 0;
	AudioStreamingFixtureQueueState eState = AudioStreamingFixtureQueueState::kFree;
	uint64_t uiGeneration = 0;
	common::crc_t crc = 0;
	uint64_t uiOffset = 0;
	uint64_t uiLength = 0;
};

struct AudioStreamingFixtureSnapshot
{
	uint64_t uiScenarioGate = 0;
	uint64_t uiScenarioGeneration = 0;
	uint64_t uiActiveWriters = 0;
	uint64_t uiReservationStart = 0;
	uint64_t uiReservationBoundary = 0;
	uint64_t uiRetryCount = 0;
	AudioStreamingFixtureHoldOwner eHoldOwner = AudioStreamingFixtureHoldOwner::kNone;
	AudioStreamingFixtureHoldState eHoldState = AudioStreamingFixtureHoldState::kIdle;
	AudioStreamingFixtureSnapshotFlags_t flags;
	uint32_t uiHeldIndex = std::numeric_limits<uint32_t>::max();
	uint64_t uiHeldGeneration = 0;
	AudioStreamingFixtureQueueState eHeldState = AudioStreamingFixtureQueueState::kFree;
	uint32_t uiMainHead = 0;
	uint32_t uiLoader0Head = 0;
	uint32_t uiLoader1Head = 0;
	uint32_t uiMainDropped = 0;
	uint32_t uiLoader0Dropped = 0;
	uint32_t uiLoader1Dropped = 0;
	int64_t iRecordCount = 0;
	std::array<AudioStreamingFixturePoolEntry, 6> poolEntries {};
	std::array<AudioStreamingFixtureRecord, 80> records {};
};

struct AudioStreamingFixtureInvalidResult
{
	bool bMissingCrcFailed = false;
	bool bWrongCrcFailed = false;
	bool bOverflowFailed = false;
	bool bOutOfRangeFailed = false;
	bool bZeroLengthFailed = false;
	bool bOversizeFailed = false;
	bool bShortAccepted = false;
	bool bTooSmallPollFailed = false;
	bool bTooSmallPollNoWrite = false;
	bool bLoadingCancelled = false;
	bool bLoadingEntryNotReused = false;
	bool bPending = false;
};

struct AudioStreamingFixtureVoiceSummary
{
	common::crc_t crc = 0;
	float fVolume = 0.0f;
	int32_t iPendingSlots = 0;
	int32_t iBuffersQueued = 0;
	AudioStreamingFixtureVoiceFlags_t flags;
};

struct AudioStreamingFixtureOlderFades
{
	int32_t iCount = 0;
	int32_t iPendingSlots = 0;
	int32_t iUnderrunningCount = 0;
};

struct AudioStreamingFixtureAudioSnapshot
{
	AudioStreamingFixtureVoiceSummary current;
	AudioStreamingFixtureVoiceSummary newestFade;
	AudioStreamingFixtureOlderFades olderFades;
	uint32_t uiPublicationAllowance = 0;
	AudioStreamingFixtureAudioSnapshotFlags_t flags;
};

struct AudioStreamingVoiceControl
{
	StreamingVoice* pVoice = nullptr;
	uint32_t uiPublicationAllowance = 0;
};

class AudioStreamingFixture
{
public:
	AudioStreamingFixture() = default;
	~AudioStreamingFixture();

	AudioStreamingFixture(const AudioStreamingFixture&) = delete;
	AudioStreamingFixture& operator=(const AudioStreamingFixture&) = delete;
	static void Attach(AudioStreamingFixture& rFixture);
	static void Detach(AudioStreamingFixture& rFixture);
	void Shutdown();

	bool RequestHold();
	bool Begin(common::crc_t crc, uint64_t uiOffset, uint64_t uiLength);
	bool Play(common::crc_t crc);
	void Clear();
	void StopPublication();
	void ReleaseControls();
	AudioStreamingFixtureAudioSnapshot InspectAudio() const;

	bool BeginHistory();
	AudioStreamingFixtureSnapshot InspectFile() const;
	bool ArmHold(common::crc_t crc, uint64_t uiOffset, uint64_t uiLength);
	void ReleaseHold();
	void StartCoexistence(common::crc_t realtimeCrc, common::crc_t normalCrc);
	bool FinishCoexistence();
	void ReleaseCoexistence();
	bool BeginSaturation();
	void ReleaseSaturation();
	AudioStreamingFixtureInvalidResult RunInvalid();
	void ReleaseInvalid();

	AudioStreamingFixtureCommandFlags_t mCommandFlags;

	static void Record(AudioStreamingFixturePartition ePartition, AudioStreamingFixturePhase ePhase, uint32_t uiPoolIndex, common::crc_t crc, uint64_t uiOffset, uint64_t uiLength, AudioStreamingFixtureQueueState eState, uint64_t uiGeneration, bool bCancelAcknowledged);
	static void RecordMain(AudioStreamingFixturePhase ePhase, uint32_t uiPoolIndex, common::crc_t crc, uint64_t uiOffset, uint64_t uiLength, AudioStreamingFixtureQueueState eState, uint64_t uiGeneration, bool bCancelAcknowledged);
	static bool HoldAudioRead(common::crc_t crc, uint64_t uiOffset, uint64_t uiLength, uint32_t uiIndex, uint64_t uiGeneration);
	static void CompleteAudioRead(bool bHeld);
	static void CountRetry();
	static bool LoadersStaged();
	static void PrepareLoaderDrain();
	static bool SuppressTrackTransition();
	static bool AllowCurrentRequests();
	static bool AllowNewestFadeRequests();
	static bool AllowOlderFadeRequests();
	static AudioStreamingVoiceControl* CreateVoiceControl(StreamingVoice& rVoice);
	static void RetireVoiceControl(AudioStreamingVoiceControl* pControl);

private:
	static AudioStreamingFixture* TryGet();

	enum class Flags : uint8_t
	{
		kMode = 0x01,
		kHoldPending = 0x02,
		kHistoryStarted = 0x04,
		kControlledPublication = 0x08,
		kPublicationStopped = 0x10,
	};
	using Flags_t = common::Flags<Flags>;

	enum class StagingOwner : uint8_t
	{
		kNone,
		kCoexistence,
		kSaturation,
	};

	struct HistoryEntry
	{
		AudioStreamingFixtureRecord record {};
		std::atomic<uint64_t> uiPublishedSequence {0};
	};

	template <size_t SIZE>
	struct History
	{
		std::array<HistoryEntry, SIZE> entries {};
		std::atomic<uint32_t> uiHead {0};
		std::atomic<uint32_t> uiDropped {0};
	};

	bool ArmHold(AudioStreamingFixtureHoldOwner eOwner, common::crc_t crc, uint64_t uiOffset, uint64_t uiLength);
	void ReleaseHold(AudioStreamingFixtureHoldOwner eOwner);
	void ResetHistory();
	void RecordEntry(AudioStreamingFixturePartition ePartition, AudioStreamingFixturePhase ePhase, uint32_t uiPoolIndex, common::crc_t crc, uint64_t uiOffset, uint64_t uiLength, AudioStreamingFixtureQueueState eState, uint64_t uiGeneration, bool bCancelAcknowledged);
	PackChunks* GetPackChunks() const;

	Flags_t mFlags;
	std::vector<std::unique_ptr<AudioStreamingVoiceControl>> mVoiceControls;
	std::atomic<uint64_t> muiScenarioGate {0};
	std::atomic<uint64_t> muiActiveWriters {0};
	std::atomic<uint64_t> muiNextSequence {0};
	std::atomic<uint64_t> muiRetryCount {0};
	uint64_t muiReservationStart = 0;
	bool mbResetPending = false;
	bool mbHistoryInitialized = false;
	History<32> mMainHistory;
	History<24> mLoader0History;
	History<24> mLoader1History;
	std::atomic<uint64_t> muiHoldToken {0};
	std::atomic<common::crc_t> mHoldCrc {0};
	std::atomic<uint64_t> muiHoldOffset {0};
	std::atomic<uint64_t> muiHoldLength {0};
	std::atomic<uint32_t> muiHeldIndex {std::numeric_limits<uint32_t>::max()};
	std::atomic<uint64_t> muiHeldGeneration {0};
	std::atomic<StagingOwner> meStagingOwner {StagingOwner::kNone};
	ChunkReadRequest mInvalidRequest;
	std::array<std::byte, 16 * 1024> mInvalidBuffer {};
	common::crc_t mInvalidCrc = 0;
	uint64_t muiInvalidOffset = 0;
	uint64_t muiInvalidLength = 0;
	bool mbShutdown = false;
};

} // namespace engine

#endif // BT_CLIENT && BT_DEBUG
