#include "Pch.h"

#include "Agent/Commands/AudioStreamingFixture.h"

#if defined(BT_CLIENT) && defined(BT_DEBUG)

#include "Audio/AudioManager.h"
#include "Audio/StreamingVoice.h"
#include "Audio/StreamingVoices.h"
#include "File/PackChunks.h"

namespace engine
{

namespace
{

std::atomic<AudioStreamingFixture*> gpAttachedAudioStreamingFixture = nullptr;

constexpr uint64_t kuiAudioReadStateMask = 0x3;
constexpr uint64_t kuiHoldStateMask = 0x3;
constexpr uint64_t kuiHoldOwnerMask = 0xc;
constexpr uint64_t kuiHoldGenerationShift = 4;

constexpr AudioChunkReadState AudioReadState(uint64_t uiOwnership)
{
	return static_cast<AudioChunkReadState>(uiOwnership & kuiAudioReadStateMask);
}

constexpr uint64_t AudioReadGeneration(uint64_t uiOwnership)
{
	return uiOwnership >> 2;
}

constexpr uint64_t PackHoldToken(AudioStreamingFixtureHoldOwner eOwner, AudioStreamingFixtureHoldState eState, uint64_t uiGeneration)
{
	return (uiGeneration << kuiHoldGenerationShift)
		| (static_cast<uint64_t>(eOwner) << 2)
		| static_cast<uint64_t>(eState);
}

constexpr AudioStreamingFixtureHoldOwner HoldOwner(uint64_t uiToken)
{
	return static_cast<AudioStreamingFixtureHoldOwner>((uiToken & kuiHoldOwnerMask) >> 2);
}

constexpr AudioStreamingFixtureHoldState HoldState(uint64_t uiToken)
{
	return static_cast<AudioStreamingFixtureHoldState>(uiToken & kuiHoldStateMask);
}

constexpr uint64_t HoldGeneration(uint64_t uiToken)
{
	return uiToken >> kuiHoldGenerationShift;
}

uint64_t NextHoldGeneration(uint64_t uiGeneration)
{
	ASSERT(uiGeneration != (std::numeric_limits<uint64_t>::max() >> kuiHoldGenerationShift));
	return uiGeneration + 1;
}

AudioStreamingFixtureQueueState ToFixtureState(AudioChunkReadState eState)
{
	return static_cast<AudioStreamingFixtureQueueState>(eState);
}

AudioStreamingFixtureVoiceSummary InspectVoice(const StreamingVoice& rVoice)
{
	AudioStreamingFixtureVoiceSummary summary
	{
		.crc = rVoice.mpLazyChunk->location.crc,
		.fVolume = rVoice.mfCurrentVolume,
	};
	summary.flags.Set(AudioStreamingFixtureVoiceFlags::kPresent);
	summary.flags.Set(AudioStreamingFixtureVoiceFlags::kFadingIn, rVoice.mFlags & StreamingVoiceFlags::kFadingIn);
	summary.flags.Set(AudioStreamingFixtureVoiceFlags::kFadingOut, rVoice.mFlags & StreamingVoiceFlags::kFadingOut);
	for (SlotState eState : rVoice.mSlotStates)
	{
		summary.iPendingSlots += eState == SlotState::kPending ? 1 : 0;
	}

	XAUDIO2_VOICE_STATE voiceState {};
	if (rVoice.mpVoice != nullptr)
	{
		rVoice.mpVoice->GetState(&voiceState, XAUDIO2_VOICE_NOSAMPLESPLAYED);
	}
	summary.iBuffersQueued = static_cast<int32_t>(voiceState.BuffersQueued);
	summary.flags.Set(AudioStreamingFixtureVoiceFlags::kUnderrunning, !(rVoice.mFlags & StreamingVoiceFlags::kLastBufferSubmitted) && voiceState.BuffersQueued == 0 && rVoice.mSlotStates[rVoice.miNextSubmit] != SlotState::kReady);
	return summary;
}

} // namespace

AudioStreamingFixture::~AudioStreamingFixture()
{
	Shutdown();
}

void AudioStreamingFixture::Attach(AudioStreamingFixture& rFixture)
{
	AudioStreamingFixture* pExpected = nullptr;
	VERIFY_SUCCESS(gpAttachedAudioStreamingFixture.compare_exchange_strong(pExpected, &rFixture, std::memory_order_release, std::memory_order_relaxed));
}

void AudioStreamingFixture::Detach(AudioStreamingFixture& rFixture)
{
	PackChunks* pPackChunks = rFixture.GetPackChunks();
	if (pPackChunks != nullptr)
	{
		// Shutdown drained active jobs; this excludes the remaining idle-loader lookup while detachment publishes null.
		std::unique_lock lock(pPackChunks->mLoader.mQueueMutex);
		AudioStreamingFixture* pExpected = &rFixture;
		VERIFY_SUCCESS(gpAttachedAudioStreamingFixture.compare_exchange_strong(pExpected, nullptr, std::memory_order_release, std::memory_order_relaxed));
		return;
	}

	AudioStreamingFixture* pExpected = &rFixture;
	VERIFY_SUCCESS(gpAttachedAudioStreamingFixture.compare_exchange_strong(pExpected, nullptr, std::memory_order_release, std::memory_order_relaxed));
}

void AudioStreamingFixture::Shutdown()
{
	if (mbShutdown)
	{
		return;
	}
	ReleaseInvalid();
	ReleaseHold(AudioStreamingFixtureHoldOwner::kControlled);
	ReleaseCoexistence();
	ReleaseSaturation();
	ReleaseControls();
	if (gpAudioManager != nullptr)
	{
		gpAudioManager->mpStreamingVoices->Clear(false);
	}
	if (gpFileManager != nullptr)
	{
		gpFileManager->WaitForLoadersIdle();
	}
	uint64_t uiGate = muiScenarioGate.load(std::memory_order_seq_cst);
	if ((uiGate & 1) != 0)
	{
		muiScenarioGate.store(uiGate + 1, std::memory_order_seq_cst);
	}
	while (muiActiveWriters.load(std::memory_order_seq_cst) != 0)
	{
		SwitchToThread();
	}
	mbShutdown = true;
}

AudioStreamingFixture* AudioStreamingFixture::TryGet()
{
	return gpAttachedAudioStreamingFixture.load(std::memory_order_acquire);
}

PackChunks* AudioStreamingFixture::GetPackChunks() const
{
	return gpFileManager != nullptr ? gpFileManager->mpPackChunks.get() : nullptr;
}

bool AudioStreamingFixture::RequestHold()
{
	if (gpAudioManager == nullptr)
	{
		return false;
	}
	if (gpAudioManager->mbSuspended.load(std::memory_order_acquire))
	{
		return false;
	}
	if (mFlags & Flags::kHistoryStarted)
	{
		return false;
	}
	if (mFlags & Flags::kHoldPending)
	{
		return false;
	}
	mFlags.Set(Flags::kHoldPending);
	return true;
}

bool AudioStreamingFixture::Begin(common::crc_t crc, uint64_t uiOffset, uint64_t uiLength)
{
	if (gpAudioManager == nullptr)
	{
		return false;
	}
	if (!(mFlags & Flags::kMode))
	{
		mFlags.Set(Flags::kMode);
		gpAudioManager->mpStreamingVoices->Clear(false);
	}
	if (mFlags & Flags::kHistoryStarted)
	{
		return true;
	}
	if (!BeginHistory())
	{
		return false;
	}
	if (mFlags & Flags::kHoldPending)
	{
		if (!ArmHold(crc, uiOffset, uiLength))
		{
			return false;
		}
		mFlags.Clear(Flags::kHoldPending);
		mFlags.Set(Flags::kControlledPublication);
	}
	mFlags.Set(Flags::kHistoryStarted);
	return true;
}

bool AudioStreamingFixture::Play(common::crc_t crc)
{
	if (gpAudioManager == nullptr)
	{
		return false;
	}
	if (gpAudioManager->mbSuspended.load(std::memory_order_acquire))
	{
		return false;
	}
	gpAudioManager->mpStreamingVoices->Play(crc);
	const std::unique_ptr<StreamingVoice>& pCurrent = gpAudioManager->mpStreamingVoices->mpCurrentStream;
	return pCurrent != nullptr && pCurrent->mpLazyChunk->location.crc == crc;
}

void AudioStreamingFixture::Clear()
{
	if (gpAudioManager != nullptr)
	{
		gpAudioManager->mpStreamingVoices->Clear(false);
	}
}

void AudioStreamingFixture::StopPublication()
{
	mFlags.Set(Flags::kPublicationStopped);
}

void AudioStreamingFixture::ReleaseControls()
{
	mFlags.Clear(Flags::kHoldPending);
	mFlags.Clear(Flags::kControlledPublication);
	mFlags.Clear(Flags::kPublicationStopped);
	mCommandFlags.Clear(AudioStreamingFixtureCommandFlags::kSaturationActive);
	for (const std::unique_ptr<AudioStreamingVoiceControl>& pControl : mVoiceControls)
	{
		pControl->pVoice->mpAudioStreamingControl = nullptr;
	}
	mVoiceControls.clear();
}

AudioStreamingFixtureAudioSnapshot AudioStreamingFixture::InspectAudio() const
{
	ASSERT(common::gpMultithreading->IsMainThread());
	AudioStreamingFixtureAudioSnapshot snapshot;
	if (gpAudioManager == nullptr)
	{
		return snapshot;
	}
	const StreamingVoices& rVoices = *gpAudioManager->mpStreamingVoices;
	if (rVoices.mpCurrentStream != nullptr)
	{
		snapshot.current = InspectVoice(*rVoices.mpCurrentStream);
		if (rVoices.mpCurrentStream->mpAudioStreamingControl != nullptr)
		{
			snapshot.uiPublicationAllowance = rVoices.mpCurrentStream->mpAudioStreamingControl->uiPublicationAllowance;
		}
	}
	if (!rVoices.mPreviousStreams.empty())
	{
		snapshot.newestFade = InspectVoice(*rVoices.mPreviousStreams.back());
	}
	for (size_t i = 0; i + 1 < rVoices.mPreviousStreams.size(); ++i)
	{
		const AudioStreamingFixtureVoiceSummary older = InspectVoice(*rVoices.mPreviousStreams.at(i));
		++snapshot.olderFades.iCount;
		snapshot.olderFades.iPendingSlots += older.iPendingSlots;
		snapshot.olderFades.iUnderrunningCount += older.flags & AudioStreamingFixtureVoiceFlags::kUnderrunning ? 1 : 0;
	}
	snapshot.flags.Set(AudioStreamingFixtureAudioSnapshotFlags::kControlledPublication, mFlags & Flags::kControlledPublication);
	snapshot.flags.Set(AudioStreamingFixtureAudioSnapshotFlags::kSaturationActive, mCommandFlags & AudioStreamingFixtureCommandFlags::kSaturationActive);
	if (!(snapshot.flags & AudioStreamingFixtureAudioSnapshotFlags::kControlledPublication))
	{
		snapshot.uiPublicationAllowance = 0;
	}
	return snapshot;
}

void AudioStreamingFixture::ResetHistory()
{
	auto reset = [](auto& rHistory)
	{
		rHistory.uiHead.store(0, std::memory_order_relaxed);
		rHistory.uiDropped.store(0, std::memory_order_relaxed);
		for (HistoryEntry& rEntry : rHistory.entries)
		{
			rEntry.uiPublishedSequence.store(0, std::memory_order_relaxed);
			rEntry.record = {};
		}
	};
	reset(mMainHistory);
	reset(mLoader0History);
	reset(mLoader1History);
	muiRetryCount.store(0, std::memory_order_seq_cst);
	muiReservationStart = muiNextSequence.load(std::memory_order_seq_cst);
	mbHistoryInitialized = true;
}

bool AudioStreamingFixture::BeginHistory()
{
	PackChunks* pPackChunks = GetPackChunks();
	if (pPackChunks == nullptr)
	{
		return false;
	}
	if (mbHistoryInitialized)
	{
		return true;
	}
	if (!mbResetPending)
	{
		uint64_t uiGate = muiScenarioGate.load(std::memory_order_seq_cst);
		muiScenarioGate.store((uiGate & 1) != 0 ? uiGate + 1 : uiGate + 2, std::memory_order_seq_cst);
		ReleaseInvalid();
		ReleaseHold(AudioStreamingFixtureHoldOwner::kControlled);
		ReleaseCoexistence();
		ReleaseSaturation();
		mbResetPending = true;
	}
	if (muiActiveWriters.load(std::memory_order_seq_cst) != 0)
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

bool AudioStreamingFixture::ArmHold(AudioStreamingFixtureHoldOwner eOwner, common::crc_t crc, uint64_t uiOffset, uint64_t uiLength)
{
	if (muiHeldIndex.load(std::memory_order_seq_cst) != std::numeric_limits<uint32_t>::max())
	{
		return false;
	}
	uint64_t uiToken = muiHoldToken.load(std::memory_order_seq_cst);
	if (HoldState(uiToken) != AudioStreamingFixtureHoldState::kIdle)
	{
		return false;
	}
	const uint64_t uiGeneration = NextHoldGeneration(HoldGeneration(uiToken));
	mHoldCrc.store(crc, std::memory_order_seq_cst);
	muiHoldOffset.store(uiOffset, std::memory_order_seq_cst);
	muiHoldLength.store(uiLength, std::memory_order_seq_cst);
	const uint64_t uiArmedToken = PackHoldToken(eOwner, AudioStreamingFixtureHoldState::kArmed, uiGeneration);
	return muiHoldToken.compare_exchange_strong(uiToken, uiArmedToken, std::memory_order_seq_cst);
}

bool AudioStreamingFixture::ArmHold(common::crc_t crc, uint64_t uiOffset, uint64_t uiLength)
{
	return ArmHold(AudioStreamingFixtureHoldOwner::kControlled, crc, uiOffset, uiLength);
}

void AudioStreamingFixture::ReleaseHold(AudioStreamingFixtureHoldOwner eOwner)
{
	uint64_t uiToken = muiHoldToken.load(std::memory_order_seq_cst);
	while (HoldOwner(uiToken) == eOwner && HoldState(uiToken) != AudioStreamingFixtureHoldState::kIdle)
	{
		const uint64_t uiIdleToken = PackHoldToken(AudioStreamingFixtureHoldOwner::kNone, AudioStreamingFixtureHoldState::kIdle, NextHoldGeneration(HoldGeneration(uiToken)));
		if (muiHoldToken.compare_exchange_weak(uiToken, uiIdleToken, std::memory_order_seq_cst))
		{
			muiHoldToken.notify_all();
			return;
		}
	}
}

void AudioStreamingFixture::ReleaseHold()
{
	ReleaseHold(AudioStreamingFixtureHoldOwner::kControlled);
}

void AudioStreamingFixture::StartCoexistence(common::crc_t realtimeCrc, common::crc_t normalCrc)
{
	PackChunks* pPackChunks = GetPackChunks();
	ASSERT(pPackChunks != nullptr);
	{
		std::unique_lock lock(pPackChunks->mLoader.mQueueMutex);
		ASSERT(meStagingOwner.load(std::memory_order_seq_cst) == StagingOwner::kNone);
		meStagingOwner.store(StagingOwner::kCoexistence, std::memory_order_seq_cst);
	}
	pPackChunks->mLoader.RequestChunkLoad(std::span(&realtimeCrc, 1), LoadPriority::kRealtime);
	pPackChunks->mLoader.RequestChunkLoad(std::span(&normalCrc, 1), LoadPriority::kNormal);
}

bool AudioStreamingFixture::FinishCoexistence()
{
	PackChunks* pPackChunks = GetPackChunks();
	if (pPackChunks == nullptr)
	{
		return true;
	}
	if (meStagingOwner.load(std::memory_order_seq_cst) == StagingOwner::kNone)
	{
		return true;
	}
	if (!pPackChunks->HasQueuedAudioRead())
	{
		return false;
	}
	ReleaseCoexistence();
	return true;
}

void AudioStreamingFixture::ReleaseCoexistence()
{
	PackChunks* pPackChunks = GetPackChunks();
	if (pPackChunks == nullptr)
	{
		return;
	}
	bool bReleased = false;
	{
		std::unique_lock lock(pPackChunks->mLoader.mQueueMutex);
		if (meStagingOwner.load(std::memory_order_seq_cst) == StagingOwner::kCoexistence)
		{
			meStagingOwner.store(StagingOwner::kNone, std::memory_order_seq_cst);
			bReleased = true;
		}
	}
	if (bReleased)
	{
		pPackChunks->mLoader.PublishWake();
	}
}

bool AudioStreamingFixture::BeginSaturation()
{
	PackChunks* pPackChunks = GetPackChunks();
	if (pPackChunks == nullptr)
	{
		return false;
	}
	std::unique_lock lock(pPackChunks->mLoader.mQueueMutex);
	if (meStagingOwner.load(std::memory_order_seq_cst) != StagingOwner::kNone)
	{
		return false;
	}
	meStagingOwner.store(StagingOwner::kSaturation, std::memory_order_seq_cst);
	return true;
}

void AudioStreamingFixture::ReleaseSaturation()
{
	PackChunks* pPackChunks = GetPackChunks();
	if (pPackChunks == nullptr)
	{
		return;
	}
	bool bReleased = false;
	{
		std::unique_lock lock(pPackChunks->mLoader.mQueueMutex);
		if (meStagingOwner.load(std::memory_order_seq_cst) == StagingOwner::kSaturation)
		{
			meStagingOwner.store(StagingOwner::kNone, std::memory_order_seq_cst);
			bReleased = true;
		}
	}
	if (bReleased)
	{
		pPackChunks->mLoader.PublishWake();
	}
}

void AudioStreamingFixture::Record(AudioStreamingFixturePartition ePartition, AudioStreamingFixturePhase ePhase, uint32_t uiPoolIndex, common::crc_t crc, uint64_t uiOffset, uint64_t uiLength, AudioStreamingFixtureQueueState eState, uint64_t uiGeneration, bool bCancelAcknowledged)
{
	if (AudioStreamingFixture* pFixture = TryGet())
	{
		pFixture->RecordEntry(ePartition, ePhase, uiPoolIndex, crc, uiOffset, uiLength, eState, uiGeneration, bCancelAcknowledged);
	}
}

void AudioStreamingFixture::RecordMain(AudioStreamingFixturePhase ePhase, uint32_t uiPoolIndex, common::crc_t crc, uint64_t uiOffset, uint64_t uiLength, AudioStreamingFixtureQueueState eState, uint64_t uiGeneration, bool bCancelAcknowledged)
{
	Record(AudioStreamingFixturePartition::kMain, ePhase, uiPoolIndex, crc, uiOffset, uiLength, eState, uiGeneration, bCancelAcknowledged);
}

void AudioStreamingFixture::RecordEntry(AudioStreamingFixturePartition ePartition, AudioStreamingFixturePhase ePhase, uint32_t uiPoolIndex, common::crc_t crc, uint64_t uiOffset, uint64_t uiLength, AudioStreamingFixtureQueueState eState, uint64_t uiGeneration, bool bCancelAcknowledged)
{
	const uint64_t uiGate = muiScenarioGate.load(std::memory_order_seq_cst);
	if ((uiGate & 1) == 0)
	{
		return;
	}
	muiActiveWriters.fetch_add(1, std::memory_order_seq_cst);
	if (muiScenarioGate.load(std::memory_order_seq_cst) != uiGate)
	{
		muiActiveWriters.fetch_sub(1, std::memory_order_seq_cst);
		return;
	}
	auto write = [&](auto& rHistory)
	{
		const uint32_t uiIndex = rHistory.uiHead.fetch_add(1, std::memory_order_seq_cst);
		if (uiIndex >= rHistory.entries.size())
		{
			rHistory.uiDropped.fetch_add(1, std::memory_order_seq_cst);
			return;
		}
		const uint64_t uiSequence = muiNextSequence.fetch_add(1, std::memory_order_seq_cst) + 1;
		HistoryEntry& rEntry = rHistory.entries[uiIndex];
		rEntry.record =
		{
			.uiSequence = uiSequence,
			.uiScenarioGeneration = uiGate >> 1,
			.uiGeneration = uiGeneration,
			.uiThreadId = GetCurrentThreadId(),
			.iThreadPriority = GetThreadPriority(GetCurrentThread()),
			.uiPoolIndex = uiPoolIndex,
			.crc = crc,
			.uiOffset = uiOffset,
			.uiLength = uiLength,
			.ePartition = ePartition,
			.ePhase = ePhase,
			.eState = eState,
			.bCancelAcknowledged = bCancelAcknowledged,
		};
		rEntry.uiPublishedSequence.store(uiSequence, std::memory_order_release);
	};
	switch (ePartition)
	{
	case AudioStreamingFixturePartition::kMain: write(mMainHistory); break;
	case AudioStreamingFixturePartition::kLoader0: write(mLoader0History); break;
	case AudioStreamingFixturePartition::kLoader1: write(mLoader1History); break;
	}
	muiActiveWriters.fetch_sub(1, std::memory_order_seq_cst);
}

AudioStreamingFixtureSnapshot AudioStreamingFixture::InspectFile() const
{
	AudioStreamingFixtureSnapshot snapshot;
	const PackChunks* pPackChunks = GetPackChunks();
	if (pPackChunks == nullptr)
	{
		return snapshot;
	}
	for (int64_t iAttempt = 0; iAttempt < 8; ++iAttempt)
	{
		snapshot = {};
		std::array<uint64_t, PackChunks::kiAudioReadEntryCount> poolOwnerships {};
		bool bPoolCopyChanged = false;
		snapshot.uiScenarioGate = muiScenarioGate.load(std::memory_order_acquire);
		snapshot.uiScenarioGeneration = snapshot.uiScenarioGate >> 1;
		snapshot.uiActiveWriters = muiActiveWriters.load(std::memory_order_seq_cst);
		snapshot.uiReservationStart = muiReservationStart;
		snapshot.uiReservationBoundary = muiNextSequence.load(std::memory_order_acquire);
		snapshot.uiRetryCount = muiRetryCount.load(std::memory_order_seq_cst);
		const uint64_t uiHoldToken = muiHoldToken.load(std::memory_order_seq_cst);
		snapshot.eHoldOwner = HoldOwner(uiHoldToken);
		snapshot.eHoldState = HoldState(uiHoldToken);
		snapshot.flags.Set(AudioStreamingFixtureSnapshotFlags::kLoaderStaged, meStagingOwner.load(std::memory_order_seq_cst) != StagingOwner::kNone);
		snapshot.uiHeldIndex = muiHeldIndex.load(std::memory_order_seq_cst);
		snapshot.uiHeldGeneration = muiHeldGeneration.load(std::memory_order_seq_cst);
		for (uint32_t uiIndex = 0; uiIndex < pPackChunks->mAudioReadEntries.size(); ++uiIndex)
		{
			const AudioChunkReadEntry& rEntry = pPackChunks->mAudioReadEntries[uiIndex];
			const uint64_t uiOwnership = rEntry.uiOwnership.load(std::memory_order_acquire);
			poolOwnerships[uiIndex] = uiOwnership;
			AudioStreamingFixturePoolEntry& rPoolEntry = snapshot.poolEntries[uiIndex];
			rPoolEntry.uiIndex = uiIndex;
			rPoolEntry.eState = ToFixtureState(AudioReadState(uiOwnership));
			rPoolEntry.uiGeneration = AudioReadGeneration(uiOwnership);
			if (rPoolEntry.eState != AudioStreamingFixtureQueueState::kFree)
			{
				rPoolEntry.crc = rEntry.crc;
				rPoolEntry.uiOffset = rEntry.uiOffset;
				rPoolEntry.uiLength = rEntry.uiLength;
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
		if (snapshot.uiHeldIndex < snapshot.poolEntries.size())
		{
			snapshot.eHeldState = snapshot.poolEntries[snapshot.uiHeldIndex].eState;
		}
		auto copy = [&](const auto& rHistory, uint32_t& ruiHead, uint32_t& ruiDropped)
		{
			ruiHead = rHistory.uiHead.load(std::memory_order_acquire);
			ruiDropped = rHistory.uiDropped.load(std::memory_order_acquire);
			const uint32_t uiCount = std::min<uint32_t>(ruiHead, static_cast<uint32_t>(rHistory.entries.size()));
			for (uint32_t i = 0; i < uiCount; ++i)
			{
				const uint64_t uiPublished = rHistory.entries[i].uiPublishedSequence.load(std::memory_order_acquire);
				if (uiPublished != 0 && snapshot.iRecordCount < static_cast<int64_t>(snapshot.records.size()))
				{
					snapshot.records[snapshot.iRecordCount++] = rHistory.entries[i].record;
				}
			}
		};
		copy(mMainHistory, snapshot.uiMainHead, snapshot.uiMainDropped);
		copy(mLoader0History, snapshot.uiLoader0Head, snapshot.uiLoader0Dropped);
		copy(mLoader1History, snapshot.uiLoader1Head, snapshot.uiLoader1Dropped);
		if (snapshot.uiActiveWriters != 0)
		{
			continue;
		}
		if (muiActiveWriters.load(std::memory_order_seq_cst) != 0)
		{
			continue;
		}
		if (snapshot.uiScenarioGate != muiScenarioGate.load(std::memory_order_acquire))
		{
			continue;
		}
		if (snapshot.uiReservationBoundary != muiNextSequence.load(std::memory_order_acquire))
		{
			continue;
		}
		if (snapshot.uiRetryCount != muiRetryCount.load(std::memory_order_seq_cst))
		{
			continue;
		}
		if (uiHoldToken != muiHoldToken.load(std::memory_order_seq_cst))
		{
			continue;
		}
		if (snapshot.uiHeldIndex != muiHeldIndex.load(std::memory_order_seq_cst))
		{
			continue;
		}
		if (snapshot.uiHeldGeneration != muiHeldGeneration.load(std::memory_order_seq_cst))
		{
			continue;
		}
		if (static_cast<bool>(snapshot.flags & AudioStreamingFixtureSnapshotFlags::kLoaderStaged) != (meStagingOwner.load(std::memory_order_seq_cst) != StagingOwner::kNone))
		{
			continue;
		}
		bool bPoolChanged = false;
		for (uint32_t uiIndex = 0; uiIndex < pPackChunks->mAudioReadEntries.size(); ++uiIndex)
		{
			bPoolChanged |= pPackChunks->mAudioReadEntries[uiIndex].uiOwnership.load(std::memory_order_acquire) != poolOwnerships[uiIndex];
		}
		if (bPoolChanged)
		{
			continue;
		}
		if (snapshot.uiMainHead != mMainHistory.uiHead.load(std::memory_order_acquire))
		{
			continue;
		}
		if (snapshot.uiLoader0Head != mLoader0History.uiHead.load(std::memory_order_acquire))
		{
			continue;
		}
		if (snapshot.uiLoader1Head != mLoader1History.uiHead.load(std::memory_order_acquire))
		{
			continue;
		}
		std::sort(snapshot.records.begin(), snapshot.records.begin() + snapshot.iRecordCount, [](const AudioStreamingFixtureRecord& rLeft, const AudioStreamingFixtureRecord& rRight)
		{
			return rLeft.uiSequence < rRight.uiSequence;
		});
		snapshot.flags.Set(AudioStreamingFixtureSnapshotFlags::kGapFree, snapshot.iRecordCount == static_cast<int64_t>(snapshot.uiReservationBoundary - snapshot.uiReservationStart));
		for (int64_t i = 0; (snapshot.flags & AudioStreamingFixtureSnapshotFlags::kGapFree) && i < snapshot.iRecordCount; ++i)
		{
			snapshot.flags.Set(AudioStreamingFixtureSnapshotFlags::kGapFree, snapshot.records[i].uiSequence == snapshot.uiReservationStart + static_cast<uint64_t>(i) + 1);
		}
		if (!(snapshot.flags & AudioStreamingFixtureSnapshotFlags::kGapFree) && snapshot.uiMainDropped == 0 && snapshot.uiLoader0Dropped == 0 && snapshot.uiLoader1Dropped == 0)
		{
			continue;
		}
		snapshot.flags.Set(AudioStreamingFixtureSnapshotFlags::kCoherent, snapshot.uiMainDropped == 0 && snapshot.uiLoader0Dropped == 0 && snapshot.uiLoader1Dropped == 0 && (snapshot.flags & AudioStreamingFixtureSnapshotFlags::kGapFree));
		return snapshot;
	}
	return snapshot;
}

AudioStreamingFixtureInvalidResult AudioStreamingFixture::RunInvalid()
{
	AudioStreamingFixtureInvalidResult result;
	PackChunks* pPackChunks = GetPackChunks();
	if (pPackChunks == nullptr)
	{
		return result;
	}
	common::crc_t crc = 0;
	const LazyChunk* pAudioChunk = nullptr;
	for (const auto& [candidateCrc, rChunk] : pPackChunks->mLazyChunkMap)
	{
		if ((rChunk.header.flags & common::ChunkFlags::kChunkAudio) && rChunk.eState.load(std::memory_order_acquire) < ChunkState::kDiskLoaded)
		{
			crc = candidateCrc;
			pAudioChunk = &rChunk;
			break;
		}
	}
	if (pAudioChunk == nullptr)
	{
		ReleaseInvalid();
		return result;
	}
	std::array<std::byte, 16 * 1024 + 1> bytes {};
	ChunkReadRequest request;
	common::crc_t missingCrc = std::numeric_limits<common::crc_t>::max();
	while (pPackChunks->mLazyChunkMap.contains(missingCrc))
	{
		--missingCrc;
	}
	common::crc_t wrongCrc = missingCrc - 1;
	while (pPackChunks->mLazyChunkMap.contains(wrongCrc))
	{
		--wrongCrc;
	}
	result.bMissingCrcFailed = pPackChunks->TryReadChunkData(request, missingCrc, 0, std::span(bytes).first(1)) == ChunkReadResult::kFailed;
	static_cast<void>(pPackChunks->TryReadChunkData(request, crc, 0, std::span(bytes).first(1)));
	result.bWrongCrcFailed = pPackChunks->TryReadChunkData(request, wrongCrc, 0, std::span(bytes).first(1)) == ChunkReadResult::kFailed;
	request.Reset();
	result.bOverflowFailed = pPackChunks->TryReadChunkData(request, crc, std::numeric_limits<uint64_t>::max(), std::span(bytes).first(1)) == ChunkReadResult::kFailed;
	result.bOutOfRangeFailed = pPackChunks->TryReadChunkData(request, crc, static_cast<uint64_t>(pAudioChunk->iDataSize), std::span(bytes).first(1)) == ChunkReadResult::kFailed;
	result.bZeroLengthFailed = pPackChunks->TryReadChunkData(request, crc, 0, std::span<std::byte>()) == ChunkReadResult::kFailed;
	result.bOversizeFailed = pPackChunks->TryReadChunkData(request, crc, 0, std::span(bytes)) == ChunkReadResult::kFailed;
	const ChunkReadResult eShort = pPackChunks->TryReadChunkData(request, crc, 0, std::span(bytes).first(1));
	result.bShortAccepted = eShort == ChunkReadResult::kPending || eShort == ChunkReadResult::kReady;
	request.Reset();
	if (mInvalidRequest.mpPackChunks == nullptr)
	{
		mInvalidCrc = crc;
		muiInvalidOffset = 0;
		muiInvalidLength = std::min<uint64_t>(static_cast<uint64_t>(pAudioChunk->iDataSize), mInvalidBuffer.size());
		if (!ArmHold(AudioStreamingFixtureHoldOwner::kInvalid, crc, muiInvalidOffset, muiInvalidLength))
		{
			result.bPending = true;
			return result;
		}
		const ChunkReadResult eResult = pPackChunks->TryReadChunkData(mInvalidRequest, crc, muiInvalidOffset, std::span(mInvalidBuffer).first(muiInvalidLength));
		result.bPending = eResult == ChunkReadResult::kPending;
		if (!result.bPending)
		{
			ReleaseInvalid();
		}
		return result;
	}
	const uint32_t uiIndex = mInvalidRequest.uiEntryIndex;
	const uint64_t uiGeneration = mInvalidRequest.uiGeneration;
	uint64_t uiOwnership = pPackChunks->mAudioReadEntries[uiIndex].uiOwnership.load(std::memory_order_acquire);
	if (AudioReadState(uiOwnership) != AudioChunkReadState::kLoading)
	{
		result.bPending = true;
		return result;
	}
	std::fill(mInvalidBuffer.begin(), mInvalidBuffer.end(), std::byte {0x5a});
	const ChunkReadResult ePoll = pPackChunks->TryReadChunkData(mInvalidRequest, mInvalidCrc, muiInvalidOffset, std::span(mInvalidBuffer).first(muiInvalidLength - 1));
	result.bTooSmallPollFailed = ePoll == ChunkReadResult::kFailed;
	result.bTooSmallPollNoWrite = std::ranges::all_of(mInvalidBuffer, [](std::byte uiValue)
	{
		return uiValue == std::byte {0x5a};
	});
	uiOwnership = pPackChunks->mAudioReadEntries[uiIndex].uiOwnership.load(std::memory_order_acquire);
	result.bLoadingCancelled = AudioReadGeneration(uiOwnership) != uiGeneration;
	result.bLoadingEntryNotReused = AudioReadState(uiOwnership) == AudioChunkReadState::kLoading;
	ReleaseInvalid();
	return result;
}

void AudioStreamingFixture::ReleaseInvalid()
{
	mInvalidRequest.Reset();
	ReleaseHold(AudioStreamingFixtureHoldOwner::kInvalid);
}

bool AudioStreamingFixture::HoldAudioRead(common::crc_t crc, uint64_t uiOffset, uint64_t uiLength, uint32_t uiIndex, uint64_t uiGeneration)
{
	AudioStreamingFixture* pFixture = TryGet();
	if (pFixture == nullptr)
	{
		return false;
	}
	uint64_t uiHoldToken = pFixture->muiHoldToken.load(std::memory_order_seq_cst);
	if (HoldState(uiHoldToken) != AudioStreamingFixtureHoldState::kArmed)
	{
		return false;
	}
	if (pFixture->mHoldCrc.load(std::memory_order_seq_cst) != crc)
	{
		return false;
	}
	if (pFixture->muiHoldOffset.load(std::memory_order_seq_cst) != uiOffset)
	{
		return false;
	}
	if (pFixture->muiHoldLength.load(std::memory_order_seq_cst) != uiLength)
	{
		return false;
	}
	if (pFixture->muiHoldToken.load(std::memory_order_seq_cst) != uiHoldToken)
	{
		return false;
	}
	pFixture->muiHeldGeneration.store(uiGeneration, std::memory_order_seq_cst);
	pFixture->muiHeldIndex.store(uiIndex, std::memory_order_seq_cst);
	const uint64_t uiConsumedToken = PackHoldToken(HoldOwner(uiHoldToken), AudioStreamingFixtureHoldState::kConsumed, HoldGeneration(uiHoldToken));
	if (!pFixture->muiHoldToken.compare_exchange_strong(uiHoldToken, uiConsumedToken, std::memory_order_seq_cst))
	{
		pFixture->muiHeldGeneration.store(0, std::memory_order_seq_cst);
		pFixture->muiHeldIndex.store(std::numeric_limits<uint32_t>::max(), std::memory_order_seq_cst);
		return false;
	}
	uiHoldToken = uiConsumedToken;
	while (pFixture->muiHoldToken.load(std::memory_order_seq_cst) == uiHoldToken)
	{
		pFixture->muiHoldToken.wait(uiHoldToken, std::memory_order_seq_cst);
	}
	return true;
}

void AudioStreamingFixture::CompleteAudioRead(bool bHeld)
{
	if (!bHeld)
	{
		return;
	}
	if (AudioStreamingFixture* pFixture = TryGet())
	{
		pFixture->muiHeldGeneration.store(0, std::memory_order_seq_cst);
		pFixture->muiHeldIndex.store(std::numeric_limits<uint32_t>::max(), std::memory_order_seq_cst);
	}
}

void AudioStreamingFixture::CountRetry()
{
	if (AudioStreamingFixture* pFixture = TryGet(); pFixture != nullptr && (pFixture->muiScenarioGate.load(std::memory_order_seq_cst) & 1) != 0)
	{
		pFixture->muiRetryCount.fetch_add(1, std::memory_order_seq_cst);
	}
}

bool AudioStreamingFixture::LoadersStaged()
{
	AudioStreamingFixture* pFixture = TryGet();
	return pFixture != nullptr && pFixture->meStagingOwner.load(std::memory_order_seq_cst) != StagingOwner::kNone;
}

void AudioStreamingFixture::PrepareLoaderDrain()
{
	if (AudioStreamingFixture* pFixture = TryGet())
	{
		pFixture->ReleaseInvalid();
		pFixture->ReleaseHold(AudioStreamingFixtureHoldOwner::kControlled);
		pFixture->ReleaseCoexistence();
		pFixture->ReleaseSaturation();
	}
}

bool AudioStreamingFixture::SuppressTrackTransition()
{
	AudioStreamingFixture* pFixture = TryGet();
	return pFixture != nullptr && (pFixture->mFlags & Flags::kMode);
}

bool AudioStreamingFixture::AllowCurrentRequests()
{
	AudioStreamingFixture* pFixture = TryGet();
	return pFixture == nullptr || !(pFixture->mFlags & Flags::kPublicationStopped);
}

bool AudioStreamingFixture::AllowNewestFadeRequests()
{
	AudioStreamingFixture* pFixture = TryGet();
	return pFixture == nullptr || !(pFixture->mFlags & Flags::kControlledPublication);
}

bool AudioStreamingFixture::AllowOlderFadeRequests()
{
	return false;
}

AudioStreamingVoiceControl* AudioStreamingFixture::CreateVoiceControl(StreamingVoice& rVoice)
{
	AudioStreamingFixture* pFixture = TryGet();
	if (pFixture == nullptr)
	{
		return nullptr;
	}
	if (!(pFixture->mFlags & Flags::kControlledPublication))
	{
		return nullptr;
	}
	auto pControl = std::make_unique<AudioStreamingVoiceControl>();
	pControl->pVoice = &rVoice;
	pControl->uiPublicationAllowance = static_cast<uint32_t>(kiBufferCount);
	AudioStreamingVoiceControl* pResult = pControl.get();
	pFixture->mVoiceControls.push_back(std::move(pControl));
	return pResult;
}

void AudioStreamingFixture::RetireVoiceControl(AudioStreamingVoiceControl* pControl)
{
	if (pControl == nullptr)
	{
		return;
	}
	AudioStreamingFixture* pFixture = TryGet();
	if (pFixture == nullptr)
	{
		return;
	}
	auto it = std::ranges::find_if(pFixture->mVoiceControls, [pControl](const std::unique_ptr<AudioStreamingVoiceControl>& pCandidate)
	{
		return pCandidate.get() == pControl;
	});
	if (it != pFixture->mVoiceControls.end())
	{
		pFixture->mVoiceControls.erase(it);
	}
}

} // namespace engine

#endif // BT_CLIENT && BT_DEBUG
