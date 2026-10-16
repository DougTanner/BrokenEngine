#include "Pch.h"

#include "Agent/Commands/AudioStreamingHarnessRig.h"

#if defined(BT_CLIENT) && defined(BT_DEBUG)

#include "Audio/AudioManager.h"
#include "Audio/StreamingVoice.h"
#include "Audio/StreamingVoices.h"
#include "File/PackChunks.h"

namespace engine
{

constexpr uint64_t kuiAudioReadStateMask = 0x3;
constexpr uint64_t kuiHoldStateMask = 0x3;
constexpr uint64_t kuiHoldOwnerMask = 0xc;
constexpr uint64_t kuiHoldGenerationShift = 4;

static constexpr AudioChunkReadState AudioReadState(uint64_t uiOwnership)
{
	return static_cast<AudioChunkReadState>(uiOwnership & kuiAudioReadStateMask);
}

static constexpr uint64_t AudioReadGeneration(uint64_t uiOwnership)
{
	return uiOwnership >> 2;
}

static constexpr uint64_t PackHoldToken(AudioStreamingHarnessRigHoldOwner eOwner, AudioStreamingHarnessRigHoldState eState, uint64_t uiGeneration)
{
	return (uiGeneration << kuiHoldGenerationShift)
		| (static_cast<uint64_t>(eOwner) << 2)
		| static_cast<uint64_t>(eState);
}

static constexpr AudioStreamingHarnessRigHoldOwner HoldOwner(uint64_t uiToken)
{
	return static_cast<AudioStreamingHarnessRigHoldOwner>((uiToken & kuiHoldOwnerMask) >> 2);
}

static constexpr AudioStreamingHarnessRigHoldState HoldState(uint64_t uiToken)
{
	return static_cast<AudioStreamingHarnessRigHoldState>(uiToken & kuiHoldStateMask);
}

static constexpr uint64_t HoldGeneration(uint64_t uiToken)
{
	return uiToken >> kuiHoldGenerationShift;
}

static uint64_t NextHoldGeneration(uint64_t uiGeneration)
{
	ASSERT(uiGeneration != (std::numeric_limits<uint64_t>::max() >> kuiHoldGenerationShift));
	return uiGeneration + 1;
}

static AudioStreamingHarnessRigQueueState ToHarnessRigState(AudioChunkReadState eState)
{
	return static_cast<AudioStreamingHarnessRigQueueState>(eState);
}

static AudioStreamingHarnessRigVoiceSummary InspectVoice(const StreamingVoice& rVoice)
{
	AudioStreamingHarnessRigVoiceSummary summary
	{
		.crc = rVoice.mpLazyChunk->location.crc,
		.fVolume = rVoice.mfCurrentVolume,
	};
	summary.flags.Set(AudioStreamingHarnessRigVoiceFlags::kPresent);
	summary.flags.Set(AudioStreamingHarnessRigVoiceFlags::kFadingIn, rVoice.mFlags & StreamingVoiceFlags::kFadingIn);
	summary.flags.Set(AudioStreamingHarnessRigVoiceFlags::kFadingOut, rVoice.mFlags & StreamingVoiceFlags::kFadingOut);
	for (SlotState eState : rVoice.mSlotStates)
	{
		summary.iPendingSlots += eState == SlotState::kPending ? 1 : 0;
	}

	XAUDIO2_VOICE_STATE voiceState {};
	if (rVoice.mpVoice != nullptr)
	{
		rVoice.mpVoice->GetState(&voiceState, XAUDIO2_VOICE_NOSAMPLESPLAYED);
	}
	summary.iBuffersQueued = static_cast<int64_t>(voiceState.BuffersQueued);
	summary.flags.Set(AudioStreamingHarnessRigVoiceFlags::kUnderrunning, !(rVoice.mFlags & StreamingVoiceFlags::kLastBufferSubmitted) && voiceState.BuffersQueued == 0 && rVoice.mSlotStates[rVoice.miNextSubmit] != SlotState::kReady);
	return summary;
}

void AudioStreamingHarnessRig::Attach(AudioStreamingHarnessRig& rHarnessRig)
{
	AudioStreamingHarnessRig* pExpected = nullptr;
	VERIFY_SUCCESS(gpAttachedAudioStreamingHarnessRig.compare_exchange_strong(pExpected, &rHarnessRig, std::memory_order_release, std::memory_order_relaxed));
}

void AudioStreamingHarnessRig::Detach(AudioStreamingHarnessRig& rHarnessRig)
{
	// Shutdown drained active jobs; this excludes the remaining idle-loader lookup while detachment publishes null.
	std::unique_lock lock(gpFileManager->mpPackChunks->mLoader.mQueueMutex);
	AudioStreamingHarnessRig* pExpected = &rHarnessRig;
	VERIFY_SUCCESS(gpAttachedAudioStreamingHarnessRig.compare_exchange_strong(pExpected, nullptr, std::memory_order_release, std::memory_order_relaxed));
}

void AudioStreamingHarnessRig::Shutdown()
{
	ReleaseHold(AudioStreamingHarnessRigHoldOwner::kControlled);
	ReleaseLoaders();
	gpAudioManager->mpStreamingVoices->Clear(false);
	gpFileManager->mpPackChunks->mLoader.WaitForLoadersIdle();
	uint64_t uiGate = muiScenarioGate.load(std::memory_order_seq_cst);
	if ((uiGate & 1) != 0)
	{
		muiScenarioGate.store(uiGate + 1, std::memory_order_seq_cst);
	}
	while (miActiveWriters.load(std::memory_order_seq_cst) != 0)
	{
		SwitchToThread();
	}
}

bool AudioStreamingHarnessRig::Play(common::crc_t crc)
{
	if (gpAudioManager->mbSuspended.load(std::memory_order_acquire))
	{
		return false;
	}
	if (!(mFlags & Flags::kMode))
	{
		mFlags.Set(Flags::kMode);
		gpAudioManager->mpStreamingVoices->Clear(false);
	}
	gpAudioManager->mpStreamingVoices->Play(crc);
	const std::unique_ptr<StreamingVoice>& pCurrent = gpAudioManager->mpStreamingVoices->mpCurrentStream;
	return pCurrent != nullptr && pCurrent->mpLazyChunk->location.crc == crc;
}

void AudioStreamingHarnessRig::Clear()
{
	gpAudioManager->mpStreamingVoices->Clear(false);
}

AudioStreamingHarnessRigAudioSnapshot AudioStreamingHarnessRig::InspectAudio() const
{
	ASSERT(common::gpMultithreading->IsMainThread());
	AudioStreamingHarnessRigAudioSnapshot snapshot;
	const StreamingVoices& rVoices = *gpAudioManager->mpStreamingVoices;
	if (rVoices.mpCurrentStream != nullptr)
	{
		snapshot.current = InspectVoice(*rVoices.mpCurrentStream);
	}
	if (!rVoices.mPreviousStreams.empty())
	{
		snapshot.newestFade = InspectVoice(*rVoices.mPreviousStreams.back());
	}
	for (int64_t i = 0; i + 1 < std::ssize(rVoices.mPreviousStreams); ++i)
	{
		AudioStreamingHarnessRigVoiceSummary older = InspectVoice(*rVoices.mPreviousStreams.at(i));
		++snapshot.olderFades.iCount;
		snapshot.olderFades.iPendingSlots += older.iPendingSlots;
		snapshot.olderFades.iUnderrunningCount += older.flags & AudioStreamingHarnessRigVoiceFlags::kUnderrunning ? 1 : 0;
	}
	snapshot.flags.Set(AudioStreamingHarnessRigAudioSnapshotFlags::kPublicationStopped, mFlags & Flags::kPublicationStopped);
	return snapshot;
}

void AudioStreamingHarnessRig::ResetHistory()
{
	auto Reset = [](auto& rHistory)
	{
		rHistory.iHead.store(0, std::memory_order_relaxed);
		rHistory.iDropped.store(0, std::memory_order_relaxed);
		for (HistoryEntry& rEntry : rHistory.entries)
		{
			rEntry.iPublishedSequence.store(0, std::memory_order_relaxed);
			rEntry.record = {};
		}
	};
	Reset(mMainHistory);
	Reset(mLoader0History);
	Reset(mLoader1History);
	miRetryCount.store(0, std::memory_order_seq_cst);
	miReservationStart = miNextSequence.load(std::memory_order_seq_cst);
}

bool AudioStreamingHarnessRig::BeginHistory()
{
	PackChunks* pPackChunks = gpFileManager->mpPackChunks.get();
	if (!mbResetPending)
	{
		uint64_t uiGate = muiScenarioGate.load(std::memory_order_seq_cst);
		muiScenarioGate.store((uiGate & 1) != 0 ? uiGate + 1 : uiGate + 2, std::memory_order_seq_cst);
		ReleaseHold(AudioStreamingHarnessRigHoldOwner::kControlled);
		ReleaseLoaders();
		mbResetPending = true;
	}
	if (miActiveWriters.load(std::memory_order_seq_cst) != 0)
	{
		return false;
	}
	if (pPackChunks->HasOccupiedAudioRead())
	{
		return false;
	}
	ResetHistory();
	uint64_t uiDisabledGate = muiScenarioGate.load(std::memory_order_seq_cst);
	ASSERT((uiDisabledGate & 1) == 0);
	ASSERT(uiDisabledGate != std::numeric_limits<uint64_t>::max());
	mbResetPending = false;
	muiScenarioGate.store(uiDisabledGate + 1, std::memory_order_seq_cst);
	return true;
}

bool AudioStreamingHarnessRig::ArmHold(AudioStreamingHarnessRigHoldOwner eOwner, common::crc_t crc, int64_t iOffset, int64_t iLength)
{
	if (miHeldIndex.load(std::memory_order_seq_cst) != 4'294'967'295i64)
	{
		return false;
	}
	uint64_t uiToken = muiHoldToken.load(std::memory_order_seq_cst);
	if (HoldState(uiToken) != AudioStreamingHarnessRigHoldState::kIdle)
	{
		return false;
	}
	uint64_t uiGeneration = NextHoldGeneration(HoldGeneration(uiToken));
	mHoldCrc.store(crc, std::memory_order_seq_cst);
	miHoldOffset.store(iOffset, std::memory_order_seq_cst);
	miHoldLength.store(iLength, std::memory_order_seq_cst);
	uint64_t uiArmedToken = PackHoldToken(eOwner, AudioStreamingHarnessRigHoldState::kArmed, uiGeneration);
	return muiHoldToken.compare_exchange_strong(uiToken, uiArmedToken, std::memory_order_seq_cst);
}

void AudioStreamingHarnessRig::ReleaseHold(AudioStreamingHarnessRigHoldOwner eOwner)
{
	uint64_t uiToken = muiHoldToken.load(std::memory_order_seq_cst);
	while (HoldOwner(uiToken) == eOwner && HoldState(uiToken) != AudioStreamingHarnessRigHoldState::kIdle)
	{
		uint64_t uiIdleToken = PackHoldToken(AudioStreamingHarnessRigHoldOwner::kNone, AudioStreamingHarnessRigHoldState::kIdle, NextHoldGeneration(HoldGeneration(uiToken)));
		if (muiHoldToken.compare_exchange_weak(uiToken, uiIdleToken, std::memory_order_seq_cst))
		{
			muiHoldToken.notify_all();
			return;
		}
	}
}

void AudioStreamingHarnessRig::StallLoaders()
{
	std::unique_lock lock(gpFileManager->mpPackChunks->mLoader.mQueueMutex);
	mbLoadersStalled.store(true, std::memory_order_seq_cst);
}

void AudioStreamingHarnessRig::ReleaseLoaders()
{
	PackChunks* pPackChunks = gpFileManager->mpPackChunks.get();
	bool bReleased = false;
	{
		std::unique_lock lock(pPackChunks->mLoader.mQueueMutex);
		bReleased = mbLoadersStalled.exchange(false, std::memory_order_seq_cst);
	}
	if (bReleased)
	{
		pPackChunks->mLoader.PublishWake();
	}
}

void AudioStreamingHarnessRig::LoadChunk(common::crc_t crc, LoadPriority ePriority)
{
	gpFileManager->mpPackChunks->mLoader.RequestChunkLoad(std::span(&crc, 1), ePriority);
}

void AudioStreamingHarnessRig::Record(AudioStreamingHarnessRigPartition ePartition, AudioStreamingHarnessRigPhase ePhase, int64_t iPoolIndex, common::crc_t crc, int64_t iOffset, int64_t iLength, AudioStreamingHarnessRigQueueState eState, int64_t iGeneration, bool bCancelAcknowledged)
{
	if (AudioStreamingHarnessRig* pHarnessRig = gpAttachedAudioStreamingHarnessRig.load(std::memory_order_acquire); pHarnessRig != nullptr)
	{
		pHarnessRig->RecordEntry(ePartition, ePhase, iPoolIndex, crc, iOffset, iLength, eState, iGeneration, bCancelAcknowledged);
	}
}

void AudioStreamingHarnessRig::RecordEntry(AudioStreamingHarnessRigPartition ePartition, AudioStreamingHarnessRigPhase ePhase, int64_t iPoolIndex, common::crc_t crc, int64_t iOffset, int64_t iLength, AudioStreamingHarnessRigQueueState eState, int64_t iGeneration, bool bCancelAcknowledged)
{
	uint64_t uiGate = muiScenarioGate.load(std::memory_order_seq_cst);
	if ((uiGate & 1) == 0)
	{
		return;
	}
	miActiveWriters.fetch_add(1, std::memory_order_seq_cst);
	if (muiScenarioGate.load(std::memory_order_seq_cst) != uiGate)
	{
		miActiveWriters.fetch_sub(1, std::memory_order_seq_cst);
		return;
	}
	auto Write = [&](auto& rHistory)
	{
		int64_t iIndex = rHistory.iHead.fetch_add(1, std::memory_order_seq_cst);
		if (iIndex >= std::ssize(rHistory.entries))
		{
			rHistory.iDropped.fetch_add(1, std::memory_order_seq_cst);
			return;
		}
		int64_t iSequence = miNextSequence.fetch_add(1, std::memory_order_seq_cst) + 1;
		HistoryEntry& rEntry = rHistory.entries[iIndex];
		rEntry.record =
		{
			.iSequence = iSequence,
			.iScenarioGeneration = static_cast<int64_t>(uiGate >> 1),
			.iGeneration = iGeneration,
			.iThreadId = static_cast<int64_t>(GetCurrentThreadId()),
			.iThreadPriority = GetThreadPriority(GetCurrentThread()),
			.iPoolIndex = iPoolIndex,
			.crc = crc,
			.iOffset = iOffset,
			.iLength = iLength,
			.ePartition = ePartition,
			.ePhase = ePhase,
			.eState = eState,
			.bCancelAcknowledged = bCancelAcknowledged,
		};
		rEntry.iPublishedSequence.store(iSequence, std::memory_order_release);
	};
	switch (ePartition)
	{
		case AudioStreamingHarnessRigPartition::kMain: Write(mMainHistory); break;
		case AudioStreamingHarnessRigPartition::kLoader0: Write(mLoader0History); break;
		case AudioStreamingHarnessRigPartition::kLoader1: Write(mLoader1History); break;
	}
	miActiveWriters.fetch_sub(1, std::memory_order_seq_cst);
}

AudioStreamingHarnessRigSnapshot AudioStreamingHarnessRig::InspectFile() const
{
	AudioStreamingHarnessRigSnapshot snapshot;
	const PackChunks* pPackChunks = gpFileManager->mpPackChunks.get();
	for (int64_t k = 0; k < 8; ++k)
	{
		snapshot = {};
		std::array<uint64_t, PackChunks::kiAudioReadEntryCount> poolOwnerships {};
		bool bPoolCopyChanged = false;
		snapshot.uiScenarioGate = muiScenarioGate.load(std::memory_order_acquire);
		snapshot.iScenarioGeneration = static_cast<int64_t>(snapshot.uiScenarioGate >> 1);
		snapshot.iActiveWriters = miActiveWriters.load(std::memory_order_seq_cst);
		snapshot.iReservationStart = miReservationStart;
		snapshot.iReservationBoundary = miNextSequence.load(std::memory_order_acquire);
		snapshot.iRetryCount = miRetryCount.load(std::memory_order_seq_cst);
		uint64_t uiHoldToken = muiHoldToken.load(std::memory_order_seq_cst);
		snapshot.eHoldOwner = HoldOwner(uiHoldToken);
		snapshot.eHoldState = HoldState(uiHoldToken);
		snapshot.flags.Set(AudioStreamingHarnessRigSnapshotFlags::kLoaderStaged, mbLoadersStalled.load(std::memory_order_seq_cst));
		snapshot.iHeldIndex = miHeldIndex.load(std::memory_order_seq_cst);
		snapshot.iHeldGeneration = miHeldGeneration.load(std::memory_order_seq_cst);
		for (int64_t i = 0; i < std::ssize(pPackChunks->mAudioReadEntries); ++i)
		{
			const AudioChunkReadEntry& rEntry = pPackChunks->mAudioReadEntries[i];
			uint64_t uiOwnership = rEntry.uiOwnership.load(std::memory_order_acquire);
			poolOwnerships[i] = uiOwnership;
			AudioStreamingHarnessRigPoolEntry& rPoolEntry = snapshot.poolEntries[i];
			rPoolEntry.iIndex = i;
			rPoolEntry.eState = ToHarnessRigState(AudioReadState(uiOwnership));
			rPoolEntry.iGeneration = static_cast<int64_t>(AudioReadGeneration(uiOwnership));
			if (rPoolEntry.eState != AudioStreamingHarnessRigQueueState::kFree)
			{
				rPoolEntry.crc = rEntry.crc;
				rPoolEntry.iOffset = rEntry.iOffset;
				rPoolEntry.iLength = rEntry.iLength;
				if (rEntry.uiOwnership.load(std::memory_order_acquire) != uiOwnership)
				{
					bPoolCopyChanged = true;
					break;
				}
			}
		}
		if (bPoolCopyChanged)
		{
			continue;
		}
		if (snapshot.iHeldIndex < std::ssize(snapshot.poolEntries))
		{
			snapshot.eHeldState = snapshot.poolEntries[snapshot.iHeldIndex].eState;
		}
		auto Copy = [&](const auto& rHistory, int64_t& riHead, int64_t& riDropped)
		{
			riHead = rHistory.iHead.load(std::memory_order_acquire);
			riDropped = rHistory.iDropped.load(std::memory_order_acquire);
			int64_t iCount = std::min<int64_t>(riHead, std::ssize(rHistory.entries));
			for (int64_t i = 0; i < iCount; ++i)
			{
				int64_t iPublished = rHistory.entries[i].iPublishedSequence.load(std::memory_order_acquire);
				if (iPublished != 0 && snapshot.iRecordCount < static_cast<int64_t>(snapshot.records.size()))
				{
					snapshot.records[snapshot.iRecordCount++] = rHistory.entries[i].record;
				}
			}
		};
		Copy(mMainHistory, snapshot.iMainHead, snapshot.iMainDropped);
		Copy(mLoader0History, snapshot.iLoader0Head, snapshot.iLoader0Dropped);
		Copy(mLoader1History, snapshot.iLoader1Head, snapshot.iLoader1Dropped);
		if (snapshot.iActiveWriters != 0)
		{
			continue;
		}
		if (miActiveWriters.load(std::memory_order_seq_cst) != 0)
		{
			continue;
		}
		if (snapshot.uiScenarioGate != muiScenarioGate.load(std::memory_order_acquire))
		{
			continue;
		}
		if (snapshot.iReservationBoundary != miNextSequence.load(std::memory_order_acquire))
		{
			continue;
		}
		if (snapshot.iRetryCount != miRetryCount.load(std::memory_order_seq_cst))
		{
			continue;
		}
		if (uiHoldToken != muiHoldToken.load(std::memory_order_seq_cst))
		{
			continue;
		}
		if (snapshot.iHeldIndex != miHeldIndex.load(std::memory_order_seq_cst))
		{
			continue;
		}
		if (snapshot.iHeldGeneration != miHeldGeneration.load(std::memory_order_seq_cst))
		{
			continue;
		}
		if (static_cast<bool>(snapshot.flags & AudioStreamingHarnessRigSnapshotFlags::kLoaderStaged) != mbLoadersStalled.load(std::memory_order_seq_cst))
		{
			continue;
		}
		bool bPoolChanged = false;
		for (int64_t i = 0; i < std::ssize(pPackChunks->mAudioReadEntries); ++i)
		{
			bPoolChanged |= pPackChunks->mAudioReadEntries[i].uiOwnership.load(std::memory_order_acquire) != poolOwnerships[i];
		}
		if (bPoolChanged)
		{
			continue;
		}
		if (snapshot.iMainHead != mMainHistory.iHead.load(std::memory_order_acquire))
		{
			continue;
		}
		if (snapshot.iLoader0Head != mLoader0History.iHead.load(std::memory_order_acquire))
		{
			continue;
		}
		if (snapshot.iLoader1Head != mLoader1History.iHead.load(std::memory_order_acquire))
		{
			continue;
		}
		std::sort(snapshot.records.begin(), snapshot.records.begin() + snapshot.iRecordCount, [](const AudioStreamingHarnessRigRecord& rLeft, const AudioStreamingHarnessRigRecord& rRight)
		{
			return rLeft.iSequence < rRight.iSequence;
		});
		snapshot.flags.Set(AudioStreamingHarnessRigSnapshotFlags::kGapFree, snapshot.iRecordCount == snapshot.iReservationBoundary - snapshot.iReservationStart);
		for (int64_t i = 0; (snapshot.flags & AudioStreamingHarnessRigSnapshotFlags::kGapFree) && i < snapshot.iRecordCount; ++i)
		{
			snapshot.flags.Set(AudioStreamingHarnessRigSnapshotFlags::kGapFree, snapshot.records[i].iSequence == snapshot.iReservationStart + i + 1);
		}
		if (!(snapshot.flags & AudioStreamingHarnessRigSnapshotFlags::kGapFree) && snapshot.iMainDropped == 0 && snapshot.iLoader0Dropped == 0 && snapshot.iLoader1Dropped == 0)
		{
			continue;
		}
		snapshot.flags.Set(AudioStreamingHarnessRigSnapshotFlags::kCoherent, snapshot.iMainDropped == 0 && snapshot.iLoader0Dropped == 0 && snapshot.iLoader1Dropped == 0 && (snapshot.flags & AudioStreamingHarnessRigSnapshotFlags::kGapFree));
		return snapshot;
	}
	return snapshot;
}

bool AudioStreamingHarnessRig::HoldAudioRead(common::crc_t crc, int64_t iOffset, int64_t iLength, int64_t iIndex, int64_t iGeneration)
{
	AudioStreamingHarnessRig* pHarnessRig = gpAttachedAudioStreamingHarnessRig.load(std::memory_order_acquire);
	if (pHarnessRig == nullptr)
	{
		return false;
	}
	uint64_t uiHoldToken = pHarnessRig->muiHoldToken.load(std::memory_order_seq_cst);
	if (HoldState(uiHoldToken) != AudioStreamingHarnessRigHoldState::kArmed)
	{
		return false;
	}
	if (pHarnessRig->mHoldCrc.load(std::memory_order_seq_cst) != crc)
	{
		return false;
	}
	if (pHarnessRig->miHoldOffset.load(std::memory_order_seq_cst) != iOffset)
	{
		return false;
	}
	if (pHarnessRig->miHoldLength.load(std::memory_order_seq_cst) != iLength)
	{
		return false;
	}
	if (pHarnessRig->muiHoldToken.load(std::memory_order_seq_cst) != uiHoldToken)
	{
		return false;
	}
	// Claiming the held index before consuming the token means only its claimant ever resets it, so a loader that loses the token to a re-armed hold cannot clear the winner's index.
	int64_t iExpectedIndex = 4'294'967'295i64;
	if (!pHarnessRig->miHeldIndex.compare_exchange_strong(iExpectedIndex, iIndex, std::memory_order_seq_cst))
	{
		return false;
	}
	pHarnessRig->miHeldGeneration.store(iGeneration, std::memory_order_seq_cst);
	uint64_t uiConsumedToken = PackHoldToken(HoldOwner(uiHoldToken), AudioStreamingHarnessRigHoldState::kConsumed, HoldGeneration(uiHoldToken));
	if (!pHarnessRig->muiHoldToken.compare_exchange_strong(uiHoldToken, uiConsumedToken, std::memory_order_seq_cst))
	{
		pHarnessRig->miHeldGeneration.store(0, std::memory_order_seq_cst);
		pHarnessRig->miHeldIndex.store(4'294'967'295i64, std::memory_order_seq_cst);
		return false;
	}
	uiHoldToken = uiConsumedToken;
	while (pHarnessRig->muiHoldToken.load(std::memory_order_seq_cst) == uiHoldToken)
	{
		pHarnessRig->muiHoldToken.wait(uiHoldToken, std::memory_order_seq_cst);
	}
	return true;
}

void AudioStreamingHarnessRig::CompleteAudioRead(bool bHeld)
{
	if (!bHeld)
	{
		return;
	}
	if (AudioStreamingHarnessRig* pHarnessRig = gpAttachedAudioStreamingHarnessRig.load(std::memory_order_acquire); pHarnessRig != nullptr)
	{
		pHarnessRig->miHeldGeneration.store(0, std::memory_order_seq_cst);
		pHarnessRig->miHeldIndex.store(4'294'967'295i64, std::memory_order_seq_cst);
	}
}

void AudioStreamingHarnessRig::CountRetry()
{
	if (AudioStreamingHarnessRig* pHarnessRig = gpAttachedAudioStreamingHarnessRig.load(std::memory_order_acquire); pHarnessRig != nullptr && (pHarnessRig->muiScenarioGate.load(std::memory_order_seq_cst) & 1) != 0)
	{
		pHarnessRig->miRetryCount.fetch_add(1, std::memory_order_seq_cst);
	}
}

void AudioStreamingHarnessRig::PrepareLoaderDrain()
{
	if (AudioStreamingHarnessRig* pHarnessRig = gpAttachedAudioStreamingHarnessRig.load(std::memory_order_acquire); pHarnessRig != nullptr)
	{
		pHarnessRig->ReleaseHold(AudioStreamingHarnessRigHoldOwner::kControlled);
		pHarnessRig->ReleaseLoaders();
	}
}

} // namespace engine

#endif // BT_CLIENT && BT_DEBUG
