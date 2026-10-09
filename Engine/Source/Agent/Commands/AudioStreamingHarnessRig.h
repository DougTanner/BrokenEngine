#pragma once

#if defined(BT_CLIENT) && defined(BT_DEBUG)

#include "File/FileManager.h"

namespace engine
{

class AudioStreamingHarnessRig;
inline std::atomic<AudioStreamingHarnessRig*> gpAttachedAudioStreamingHarnessRig = nullptr;

class PackChunks;
class StreamingVoice;

enum class AudioStreamingHarnessRigPartition : uint8_t
{
	kMain,
	kLoader0,
	kLoader1,
};

enum class AudioStreamingHarnessRigPhase : uint8_t
{
	kStartup,
	kRefillQueued,
	kRefillLoading,
	kRefillReady,
	kRefillCancelled,
	kExistingQueued,
	kExistingComplete,
};

enum class AudioStreamingHarnessRigQueueState : uint8_t
{
	kFree,
	kQueued,
	kLoading,
	kReady,
};

enum class AudioStreamingHarnessRigHoldOwner : uint8_t
{
	kNone,
	kControlled,
	kInvalid,
};

enum class AudioStreamingHarnessRigHoldState : uint8_t
{
	kIdle,
	kArmed,
	kConsumed,
};

enum class AudioStreamingHarnessRigSnapshotFlags : uint8_t
{
	kLoaderStaged = 0x01,
	kCoherent = 0x02,
	kGapFree = 0x04,
};
using AudioStreamingHarnessRigSnapshotFlags_t = common::Flags<AudioStreamingHarnessRigSnapshotFlags>;

enum class AudioStreamingHarnessRigVoiceFlags : uint8_t
{
	kPresent = 0x01,
	kFadingIn = 0x02,
	kFadingOut = 0x04,
	kUnderrunning = 0x08,
};
using AudioStreamingHarnessRigVoiceFlags_t = common::Flags<AudioStreamingHarnessRigVoiceFlags>;

enum class AudioStreamingHarnessRigAudioSnapshotFlags : uint8_t
{
	kControlledPublication = 0x01,
	kSaturationActive = 0x02,
};
using AudioStreamingHarnessRigAudioSnapshotFlags_t = common::Flags<AudioStreamingHarnessRigAudioSnapshotFlags>;

enum class AudioStreamingHarnessRigCommandFlags : uint8_t
{
	kSaturationActive = 0x01,
};
using AudioStreamingHarnessRigCommandFlags_t = common::Flags<AudioStreamingHarnessRigCommandFlags>;

struct AudioStreamingHarnessRigRecord
{
	uint64_t uiSequence = 0;
	int64_t iScenarioGeneration = 0;
	int64_t iGeneration = 0;
	int64_t iThreadId = 0;
	int64_t iThreadPriority = 0;
	int64_t iPoolIndex = 4'294'967'295i64;
	common::crc_t crc = 0;
	int64_t iOffset = 0;
	int64_t iLength = 0;
	AudioStreamingHarnessRigPartition ePartition = AudioStreamingHarnessRigPartition::kMain;
	AudioStreamingHarnessRigPhase ePhase = AudioStreamingHarnessRigPhase::kStartup;
	AudioStreamingHarnessRigQueueState eState = AudioStreamingHarnessRigQueueState::kFree;
	bool bCancelAcknowledged = false;
};

struct AudioStreamingHarnessRigPoolEntry
{
	int64_t iIndex = 0;
	AudioStreamingHarnessRigQueueState eState = AudioStreamingHarnessRigQueueState::kFree;
	int64_t iGeneration = 0;
	common::crc_t crc = 0;
	int64_t iOffset = 0;
	int64_t iLength = 0;
};

struct AudioStreamingHarnessRigSnapshot
{
	uint64_t uiScenarioGate = 0;
	int64_t iScenarioGeneration = 0;
	int64_t iActiveWriters = 0;
	uint64_t uiReservationStart = 0;
	uint64_t uiReservationBoundary = 0;
	uint64_t uiRetryCount = 0;
	AudioStreamingHarnessRigHoldOwner eHoldOwner = AudioStreamingHarnessRigHoldOwner::kNone;
	AudioStreamingHarnessRigHoldState eHoldState = AudioStreamingHarnessRigHoldState::kIdle;
	AudioStreamingHarnessRigSnapshotFlags_t flags;
	int64_t iHeldIndex = 4'294'967'295i64;
	int64_t iHeldGeneration = 0;
	AudioStreamingHarnessRigQueueState eHeldState = AudioStreamingHarnessRigQueueState::kFree;
	int64_t iMainHead = 0;
	int64_t iLoader0Head = 0;
	int64_t iLoader1Head = 0;
	int64_t iMainDropped = 0;
	int64_t iLoader0Dropped = 0;
	int64_t iLoader1Dropped = 0;
	int64_t iRecordCount = 0;
	std::array<AudioStreamingHarnessRigPoolEntry, 6> poolEntries {};
	std::array<AudioStreamingHarnessRigRecord, 80> records {};
};

struct AudioStreamingHarnessRigInvalidResult
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

struct AudioStreamingHarnessRigVoiceSummary
{
	common::crc_t crc = 0;
	float fVolume = 0.0f;
	int64_t iPendingSlots = 0;
	int64_t iBuffersQueued = 0;
	AudioStreamingHarnessRigVoiceFlags_t flags;
};

struct AudioStreamingHarnessRigOlderFades
{
	int64_t iCount = 0;
	int64_t iPendingSlots = 0;
	int64_t iUnderrunningCount = 0;
};

struct AudioStreamingHarnessRigAudioSnapshot
{
	AudioStreamingHarnessRigVoiceSummary current;
	AudioStreamingHarnessRigVoiceSummary newestFade;
	AudioStreamingHarnessRigOlderFades olderFades;
	int64_t iPublicationAllowance = 0;
	AudioStreamingHarnessRigAudioSnapshotFlags_t flags;
};

struct AudioStreamingVoiceControl
{
	StreamingVoice* pVoice = nullptr;
	uint32_t uiPublicationAllowance = 0;
};

class AudioStreamingHarnessRig
{
public:
	AudioStreamingHarnessRig() = default;
	~AudioStreamingHarnessRig();

	AudioStreamingHarnessRig(const AudioStreamingHarnessRig&) = delete;
	AudioStreamingHarnessRig& operator=(const AudioStreamingHarnessRig&) = delete;
	static void Attach(AudioStreamingHarnessRig& rHarnessRig);
	static void Detach(AudioStreamingHarnessRig& rHarnessRig);
	void Shutdown();

	bool RequestHold();
	bool Begin(common::crc_t crc, int64_t iOffset, int64_t iLength);
	bool Play(common::crc_t crc);
	void Clear();
	void ReleaseControls();
	AudioStreamingHarnessRigAudioSnapshot InspectAudio() const;

	bool BeginHistory();
	AudioStreamingHarnessRigSnapshot InspectFile() const;
	bool ArmHold(AudioStreamingHarnessRigHoldOwner eOwner, common::crc_t crc, int64_t iOffset, int64_t iLength);
	void ReleaseHold(AudioStreamingHarnessRigHoldOwner eOwner);
	void StartCoexistence(common::crc_t realtimeCrc, common::crc_t normalCrc);
	bool FinishCoexistence();
	void ReleaseCoexistence();
	bool BeginSaturation();
	void ReleaseSaturation();
	AudioStreamingHarnessRigInvalidResult RunInvalid();
	void ReleaseInvalid();

	AudioStreamingHarnessRigCommandFlags_t mCommandFlags;

	static void Record(AudioStreamingHarnessRigPartition ePartition, AudioStreamingHarnessRigPhase ePhase, int64_t iPoolIndex, common::crc_t crc, int64_t iOffset, int64_t iLength, AudioStreamingHarnessRigQueueState eState, int64_t iGeneration, bool bCancelAcknowledged);
	static bool HoldAudioRead(common::crc_t crc, int64_t iOffset, int64_t iLength, int64_t iIndex, int64_t iGeneration);
	static void CompleteAudioRead(bool bHeld);
	static void CountRetry();
	static void PrepareLoaderDrain();
	static bool AllowOlderFadeRequests();
	static AudioStreamingVoiceControl* CreateVoiceControl(StreamingVoice& rVoice);
	static void RetireVoiceControl(const AudioStreamingVoiceControl* pControl);

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

	Flags_t mFlags;
	std::atomic<StagingOwner> meStagingOwner {StagingOwner::kNone};

private:
	struct HistoryEntry
	{
		AudioStreamingHarnessRigRecord record {};
		std::atomic<int64_t> iPublishedSequence {0};
	};

	template <int64_t SIZE>
	struct History
	{
		std::array<HistoryEntry, static_cast<size_t>(SIZE)> entries {};
		std::atomic<uint32_t> uiHead {0};
		std::atomic<uint32_t> uiDropped {0};
	};

	void ResetHistory();
	void RecordEntry(AudioStreamingHarnessRigPartition ePartition, AudioStreamingHarnessRigPhase ePhase, int64_t iPoolIndex, common::crc_t crc, int64_t iOffset, int64_t iLength, AudioStreamingHarnessRigQueueState eState, int64_t iGeneration, bool bCancelAcknowledged);

	std::vector<std::unique_ptr<AudioStreamingVoiceControl>> mVoiceControls;
	std::atomic<uint64_t> muiScenarioGate {0};
	std::atomic<int64_t> miActiveWriters {0};
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
	std::atomic<int64_t> miHoldOffset {0};
	std::atomic<int64_t> miHoldLength {0};
	std::atomic<int64_t> miHeldIndex {4'294'967'295i64};
	std::atomic<int64_t> miHeldGeneration {0};
	ChunkReadRequest mInvalidRequest;
	std::array<std::byte, 16 * 1'024> mInvalidBuffer {};
	common::crc_t mInvalidCrc = 0;
	int64_t miInvalidOffset = 0;
	int64_t miInvalidLength = 0;
	bool mbShutdown = false;
};

} // namespace engine

#endif // BT_CLIENT && BT_DEBUG
