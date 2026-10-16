#pragma once

#if defined(BT_CLIENT) && defined(BT_DEBUG)

#include "File/FileManager.h"

namespace engine
{

class AudioStreamingHarnessRig;
inline std::atomic<AudioStreamingHarnessRig*> gpAttachedAudioStreamingHarnessRig = nullptr;

class PackChunks;

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
	kPublicationStopped = 0x01,
};
using AudioStreamingHarnessRigAudioSnapshotFlags_t = common::Flags<AudioStreamingHarnessRigAudioSnapshotFlags>;

struct AudioStreamingHarnessRigRecord
{
	int64_t iSequence = 0;
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
	int64_t iReservationStart = 0;
	int64_t iReservationBoundary = 0;
	int64_t iRetryCount = 0;
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
	AudioStreamingHarnessRigAudioSnapshotFlags_t flags;
};

class AudioStreamingHarnessRig
{
public:
	AudioStreamingHarnessRig() = default;

	AudioStreamingHarnessRig(const AudioStreamingHarnessRig&) = delete;
	AudioStreamingHarnessRig& operator=(const AudioStreamingHarnessRig&) = delete;
	static void Attach(AudioStreamingHarnessRig& rHarnessRig);
	static void Detach(AudioStreamingHarnessRig& rHarnessRig);
	void Shutdown();

	bool Play(common::crc_t crc);
	void Clear();
	AudioStreamingHarnessRigAudioSnapshot InspectAudio() const;

	bool BeginHistory();
	AudioStreamingHarnessRigSnapshot InspectFile() const;
	bool ArmHold(AudioStreamingHarnessRigHoldOwner eOwner, common::crc_t crc, int64_t iOffset, int64_t iLength);
	void ReleaseHold(AudioStreamingHarnessRigHoldOwner eOwner);
	void StallLoaders();
	void ReleaseLoaders();
	void LoadChunk(common::crc_t crc, LoadPriority ePriority);

	static void Record(AudioStreamingHarnessRigPartition ePartition, AudioStreamingHarnessRigPhase ePhase, int64_t iPoolIndex, common::crc_t crc, int64_t iOffset, int64_t iLength, AudioStreamingHarnessRigQueueState eState, int64_t iGeneration, bool bCancelAcknowledged);
	static bool HoldAudioRead(common::crc_t crc, int64_t iOffset, int64_t iLength, int64_t iIndex, int64_t iGeneration);
	static void CompleteAudioRead(bool bHeld);
	static void CountRetry();
	static void PrepareLoaderDrain();

	enum class Flags : uint8_t
	{
		kMode = 0x01,
		kPublicationStopped = 0x02,
	};
	using Flags_t = common::Flags<Flags>;

	Flags_t mFlags;
	std::atomic<bool> mbLoadersStalled = false;

private:
	struct HistoryEntry
	{
		AudioStreamingHarnessRigRecord record {};
		std::atomic<int64_t> iPublishedSequence = 0;
	};

	template <int64_t SIZE>
	struct History
	{
		std::array<HistoryEntry, static_cast<size_t>(SIZE)> entries {};
		std::atomic<int64_t> iHead = 0;
		std::atomic<int64_t> iDropped = 0;
	};

	void ResetHistory();
	void RecordEntry(AudioStreamingHarnessRigPartition ePartition, AudioStreamingHarnessRigPhase ePhase, int64_t iPoolIndex, common::crc_t crc, int64_t iOffset, int64_t iLength, AudioStreamingHarnessRigQueueState eState, int64_t iGeneration, bool bCancelAcknowledged);

	std::atomic<uint64_t> muiScenarioGate = 0;
	std::atomic<int64_t> miActiveWriters = 0;
	std::atomic<int64_t> miNextSequence = 0;
	std::atomic<int64_t> miRetryCount = 0;
	int64_t miReservationStart = 0;
	bool mbResetPending = false;
	History<32> mMainHistory;
	History<24> mLoader0History;
	History<24> mLoader1History;
	std::atomic<uint64_t> muiHoldToken = 0;
	std::atomic<common::crc_t> mHoldCrc = 0;
	std::atomic<int64_t> miHoldOffset = 0;
	std::atomic<int64_t> miHoldLength = 0;
	std::atomic<int64_t> miHeldIndex = 4'294'967'295i64;
	std::atomic<int64_t> miHeldGeneration = 0;
};

} // namespace engine

#endif // BT_CLIENT && BT_DEBUG
