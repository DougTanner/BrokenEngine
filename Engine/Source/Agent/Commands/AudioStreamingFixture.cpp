#include "Pch.h"

#include "Agent/Commands/AudioStreamingFixture.h"

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

static constexpr uint64_t PackHoldToken(AudioStreamingFixtureHoldOwner eOwner, AudioStreamingFixtureHoldState eState, uint64_t uiGeneration)
{
	return (uiGeneration << kuiHoldGenerationShift)
		| (static_cast<uint64_t>(eOwner) << 2)
		| static_cast<uint64_t>(eState);
}

static constexpr AudioStreamingFixtureHoldOwner HoldOwner(uint64_t uiToken)
{
	return static_cast<AudioStreamingFixtureHoldOwner>((uiToken & kuiHoldOwnerMask) >> 2);
}

static constexpr AudioStreamingFixtureHoldState HoldState(uint64_t uiToken)
{
	return static_cast<AudioStreamingFixtureHoldState>(uiToken & kuiHoldStateMask);
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

static AudioStreamingFixtureQueueState ToFixtureState(AudioChunkReadState eState)
{
	return static_cast<AudioStreamingFixtureQueueState>(eState);
}

static AudioStreamingFixtureVoiceSummary InspectVoice(const StreamingVoice& rVoice)
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
	summary.iBuffersQueued = static_cast<int64_t>(voiceState.BuffersQueued);
	summary.flags.Set(AudioStreamingFixtureVoiceFlags::kUnderrunning, !(rVoice.mFlags & StreamingVoiceFlags::kLastBufferSubmitted) && voiceState.BuffersQueued == 0 && rVoice.mSlotStates[rVoice.miNextSubmit] != SlotState::kReady);
	return summary;
}

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
	PackChunks* pPackChunks = gpFileManager != nullptr ? gpFileManager->mpPackChunks.get() : nullptr;
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
		gpFileManager->mpPackChunks->mLoader.WaitForLoadersIdle();
	}
	uint64_t uiGate = muiScenarioGate.load(std::memory_order_seq_cst);
	if ((uiGate & 1) != 0)
	{
		muiScenarioGate.store(uiGate + 1, std::memory_order_seq_cst);
	}
	while (miActiveWriters.load(std::memory_order_seq_cst) != 0)
	{
		SwitchToThread();
	}
	mbShutdown = true;
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

bool AudioStreamingFixture::Begin(common::crc_t crc, int64_t iOffset, int64_t iLength)
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
		if (!ArmHold(AudioStreamingFixtureHoldOwner::kControlled, crc, iOffset, iLength))
		{
			return false;
		}
		mFlags.Set(Flags::kHoldPending, false);
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

void AudioStreamingFixture::ReleaseControls()
{
	mFlags.Set(Flags::kHoldPending, false);
	mFlags.Set(Flags::kControlledPublication, false);
	mFlags.Set(Flags::kPublicationStopped, false);
	mCommandFlags.Set(AudioStreamingFixtureCommandFlags::kSaturationActive, false);
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
			snapshot.iPublicationAllowance = rVoices.mpCurrentStream->mpAudioStreamingControl->uiPublicationAllowance;
		}
	}
	if (!rVoices.mPreviousStreams.empty())
	{
		snapshot.newestFade = InspectVoice(*rVoices.mPreviousStreams.back());
	}
	for (int64_t i = 0; i + 1 < std::ssize(rVoices.mPreviousStreams); ++i)
	{
		AudioStreamingFixtureVoiceSummary older = InspectVoice(*rVoices.mPreviousStreams.at(i));
		++snapshot.olderFades.iCount;
		snapshot.olderFades.iPendingSlots += older.iPendingSlots;
		snapshot.olderFades.iUnderrunningCount += older.flags & AudioStreamingFixtureVoiceFlags::kUnderrunning ? 1 : 0;
	}
	snapshot.flags.Set(AudioStreamingFixtureAudioSnapshotFlags::kControlledPublication, mFlags & Flags::kControlledPublication);
	snapshot.flags.Set(AudioStreamingFixtureAudioSnapshotFlags::kSaturationActive, mCommandFlags & AudioStreamingFixtureCommandFlags::kSaturationActive);
	if (!(snapshot.flags & AudioStreamingFixtureAudioSnapshotFlags::kControlledPublication))
	{
		snapshot.iPublicationAllowance = 0;
	}
	return snapshot;
}

void AudioStreamingFixture::ResetHistory()
{
	auto Reset = [](auto& rHistory)
	{
		rHistory.uiHead.store(0, std::memory_order_relaxed);
		rHistory.uiDropped.store(0, std::memory_order_relaxed);
		for (HistoryEntry& rEntry : rHistory.entries)
		{
			rEntry.iPublishedSequence.store(0, std::memory_order_relaxed);
			rEntry.record = {};
		}
	};
	Reset(mMainHistory);
	Reset(mLoader0History);
	Reset(mLoader1History);
	muiRetryCount.store(0, std::memory_order_seq_cst);
	muiReservationStart = muiNextSequence.load(std::memory_order_seq_cst);
	mbHistoryInitialized = true;
}

bool AudioStreamingFixture::BeginHistory()
{
	PackChunks* pPackChunks = gpFileManager != nullptr ? gpFileManager->mpPackChunks.get() : nullptr;
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

bool AudioStreamingFixture::ArmHold(AudioStreamingFixtureHoldOwner eOwner, common::crc_t crc, int64_t iOffset, int64_t iLength)
{
	if (miHeldIndex.load(std::memory_order_seq_cst) != 4'294'967'295i64)
	{
		return false;
	}
	uint64_t uiToken = muiHoldToken.load(std::memory_order_seq_cst);
	if (HoldState(uiToken) != AudioStreamingFixtureHoldState::kIdle)
	{
		return false;
	}
	uint64_t uiGeneration = NextHoldGeneration(HoldGeneration(uiToken));
	mHoldCrc.store(crc, std::memory_order_seq_cst);
	miHoldOffset.store(iOffset, std::memory_order_seq_cst);
	miHoldLength.store(iLength, std::memory_order_seq_cst);
	uint64_t uiArmedToken = PackHoldToken(eOwner, AudioStreamingFixtureHoldState::kArmed, uiGeneration);
	return muiHoldToken.compare_exchange_strong(uiToken, uiArmedToken, std::memory_order_seq_cst);
}

void AudioStreamingFixture::ReleaseHold(AudioStreamingFixtureHoldOwner eOwner)
{
	uint64_t uiToken = muiHoldToken.load(std::memory_order_seq_cst);
	while (HoldOwner(uiToken) == eOwner && HoldState(uiToken) != AudioStreamingFixtureHoldState::kIdle)
	{
		uint64_t uiIdleToken = PackHoldToken(AudioStreamingFixtureHoldOwner::kNone, AudioStreamingFixtureHoldState::kIdle, NextHoldGeneration(HoldGeneration(uiToken)));
		if (muiHoldToken.compare_exchange_weak(uiToken, uiIdleToken, std::memory_order_seq_cst))
		{
			muiHoldToken.notify_all();
			return;
		}
	}
}

void AudioStreamingFixture::StartCoexistence(common::crc_t realtimeCrc, common::crc_t normalCrc)
{
	PackChunks* pPackChunks = gpFileManager != nullptr ? gpFileManager->mpPackChunks.get() : nullptr;
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
	PackChunks* pPackChunks = gpFileManager != nullptr ? gpFileManager->mpPackChunks.get() : nullptr;
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
	PackChunks* pPackChunks = gpFileManager != nullptr ? gpFileManager->mpPackChunks.get() : nullptr;
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
	PackChunks* pPackChunks = gpFileManager != nullptr ? gpFileManager->mpPackChunks.get() : nullptr;
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
	PackChunks* pPackChunks = gpFileManager != nullptr ? gpFileManager->mpPackChunks.get() : nullptr;
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

void AudioStreamingFixture::Record(AudioStreamingFixturePartition ePartition, AudioStreamingFixturePhase ePhase, int64_t iPoolIndex, common::crc_t crc, int64_t iOffset, int64_t iLength, AudioStreamingFixtureQueueState eState, int64_t iGeneration, bool bCancelAcknowledged)
{
	if (AudioStreamingFixture* pFixture = gpAttachedAudioStreamingFixture.load(std::memory_order_acquire); pFixture != nullptr)
	{
		pFixture->RecordEntry(ePartition, ePhase, iPoolIndex, crc, iOffset, iLength, eState, iGeneration, bCancelAcknowledged);
	}
}

void AudioStreamingFixture::RecordEntry(AudioStreamingFixturePartition ePartition, AudioStreamingFixturePhase ePhase, int64_t iPoolIndex, common::crc_t crc, int64_t iOffset, int64_t iLength, AudioStreamingFixtureQueueState eState, int64_t iGeneration, bool bCancelAcknowledged)
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
		int64_t iIndex = rHistory.uiHead.fetch_add(1, std::memory_order_seq_cst);
		if (iIndex >= std::ssize(rHistory.entries))
		{
			rHistory.uiDropped.fetch_add(1, std::memory_order_seq_cst);
			return;
		}
		uint64_t uiSequence = muiNextSequence.fetch_add(1, std::memory_order_seq_cst) + 1;
		HistoryEntry& rEntry = rHistory.entries[iIndex];
		rEntry.record =
		{
			.uiSequence = uiSequence,
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
		rEntry.iPublishedSequence.store(static_cast<int64_t>(uiSequence), std::memory_order_release);
	};
	switch (ePartition)
	{
		case AudioStreamingFixturePartition::kMain: Write(mMainHistory); break;
		case AudioStreamingFixturePartition::kLoader0: Write(mLoader0History); break;
		case AudioStreamingFixturePartition::kLoader1: Write(mLoader1History); break;
	}
	miActiveWriters.fetch_sub(1, std::memory_order_seq_cst);
}

AudioStreamingFixtureSnapshot AudioStreamingFixture::InspectFile() const
{
	AudioStreamingFixtureSnapshot snapshot;
	const PackChunks* pPackChunks = gpFileManager != nullptr ? gpFileManager->mpPackChunks.get() : nullptr;
	if (pPackChunks == nullptr)
	{
		return snapshot;
	}
	for (int64_t k = 0; k < 8; ++k)
	{
		snapshot = {};
		std::array<uint64_t, PackChunks::kiAudioReadEntryCount> poolOwnerships {};
		bool bPoolCopyChanged = false;
		snapshot.uiScenarioGate = muiScenarioGate.load(std::memory_order_acquire);
		snapshot.iScenarioGeneration = static_cast<int64_t>(snapshot.uiScenarioGate >> 1);
		snapshot.iActiveWriters = miActiveWriters.load(std::memory_order_seq_cst);
		snapshot.uiReservationStart = muiReservationStart;
		snapshot.uiReservationBoundary = muiNextSequence.load(std::memory_order_acquire);
		snapshot.uiRetryCount = muiRetryCount.load(std::memory_order_seq_cst);
		uint64_t uiHoldToken = muiHoldToken.load(std::memory_order_seq_cst);
		snapshot.eHoldOwner = HoldOwner(uiHoldToken);
		snapshot.eHoldState = HoldState(uiHoldToken);
		snapshot.flags.Set(AudioStreamingFixtureSnapshotFlags::kLoaderStaged, meStagingOwner.load(std::memory_order_seq_cst) != StagingOwner::kNone);
		snapshot.iHeldIndex = miHeldIndex.load(std::memory_order_seq_cst);
		snapshot.iHeldGeneration = miHeldGeneration.load(std::memory_order_seq_cst);
		for (int64_t i = 0; i < std::ssize(pPackChunks->mAudioReadEntries); ++i)
		{
			const AudioChunkReadEntry& rEntry = pPackChunks->mAudioReadEntries[i];
			uint64_t uiOwnership = rEntry.uiOwnership.load(std::memory_order_acquire);
			poolOwnerships[i] = uiOwnership;
			AudioStreamingFixturePoolEntry& rPoolEntry = snapshot.poolEntries[i];
			rPoolEntry.iIndex = i;
			rPoolEntry.eState = ToFixtureState(AudioReadState(uiOwnership));
			rPoolEntry.iGeneration = static_cast<int64_t>(AudioReadGeneration(uiOwnership));
			if (rPoolEntry.eState != AudioStreamingFixtureQueueState::kFree)
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
			riHead = rHistory.uiHead.load(std::memory_order_acquire);
			riDropped = rHistory.uiDropped.load(std::memory_order_acquire);
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
		if (snapshot.iHeldIndex != miHeldIndex.load(std::memory_order_seq_cst))
		{
			continue;
		}
		if (snapshot.iHeldGeneration != miHeldGeneration.load(std::memory_order_seq_cst))
		{
			continue;
		}
		if (static_cast<bool>(snapshot.flags & AudioStreamingFixtureSnapshotFlags::kLoaderStaged) != (meStagingOwner.load(std::memory_order_seq_cst) != StagingOwner::kNone))
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
		if (snapshot.iMainHead != mMainHistory.uiHead.load(std::memory_order_acquire))
		{
			continue;
		}
		if (snapshot.iLoader0Head != mLoader0History.uiHead.load(std::memory_order_acquire))
		{
			continue;
		}
		if (snapshot.iLoader1Head != mLoader1History.uiHead.load(std::memory_order_acquire))
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
		if (!(snapshot.flags & AudioStreamingFixtureSnapshotFlags::kGapFree) && snapshot.iMainDropped == 0 && snapshot.iLoader0Dropped == 0 && snapshot.iLoader1Dropped == 0)
		{
			continue;
		}
		snapshot.flags.Set(AudioStreamingFixtureSnapshotFlags::kCoherent, snapshot.iMainDropped == 0 && snapshot.iLoader0Dropped == 0 && snapshot.iLoader1Dropped == 0 && (snapshot.flags & AudioStreamingFixtureSnapshotFlags::kGapFree));
		return snapshot;
	}
	return snapshot;
}

AudioStreamingFixtureInvalidResult AudioStreamingFixture::RunInvalid()
{
	AudioStreamingFixtureInvalidResult result;
	PackChunks* pPackChunks = gpFileManager != nullptr ? gpFileManager->mpPackChunks.get() : nullptr;
	if (pPackChunks == nullptr)
	{
		return result;
	}
	common::crc_t crc = 0;
	const LazyChunk* pAudioChunk = nullptr;
	for (const auto& [rCandidateCrc, rChunk] : pPackChunks->mLazyChunkMap)
	{
		if ((rChunk.header.flags & common::ChunkFlags::kChunkAudio) && rChunk.eState.value.load(std::memory_order_acquire) < ChunkState::kDiskLoaded)
		{
			crc = rCandidateCrc;
			pAudioChunk = &rChunk;
			break;
		}
	}
	if (pAudioChunk == nullptr)
	{
		ReleaseInvalid();
		return result;
	}
	std::array<std::byte, 16 * 1'024 + 1> bytes {};
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
	std::ignore = pPackChunks->TryReadChunkData(request, crc, 0, std::span(bytes).first(1));
	result.bWrongCrcFailed = pPackChunks->TryReadChunkData(request, wrongCrc, 0, std::span(bytes).first(1)) == ChunkReadResult::kFailed;
	request.Reset();
	result.bOverflowFailed = pPackChunks->TryReadChunkData(request, crc, static_cast<int64_t>(std::numeric_limits<uint64_t>::max()), std::span(bytes).first(1)) == ChunkReadResult::kFailed;
	result.bOutOfRangeFailed = pPackChunks->TryReadChunkData(request, crc, pAudioChunk->iDataSize, std::span(bytes).first(1)) == ChunkReadResult::kFailed;
	result.bZeroLengthFailed = pPackChunks->TryReadChunkData(request, crc, 0, std::span<std::byte>()) == ChunkReadResult::kFailed;
	result.bOversizeFailed = pPackChunks->TryReadChunkData(request, crc, 0, std::span(bytes)) == ChunkReadResult::kFailed;
	ChunkReadResult eShort = pPackChunks->TryReadChunkData(request, crc, 0, std::span(bytes).first(1));
	result.bShortAccepted = eShort == ChunkReadResult::kPending || eShort == ChunkReadResult::kReady;
	request.Reset();
	if (mInvalidRequest.mpPackChunks == nullptr)
	{
		mInvalidCrc = crc;
		miInvalidOffset = 0;
		miInvalidLength = std::min<int64_t>(pAudioChunk->iDataSize, std::ssize(mInvalidBuffer));
		if (!ArmHold(AudioStreamingFixtureHoldOwner::kInvalid, crc, miInvalidOffset, miInvalidLength))
		{
			result.bPending = true;
			return result;
		}
		ChunkReadResult eResult = pPackChunks->TryReadChunkData(mInvalidRequest, crc, miInvalidOffset, std::span(mInvalidBuffer).first(static_cast<size_t>(miInvalidLength)));
		result.bPending = eResult == ChunkReadResult::kPending;
		if (!result.bPending)
		{
			ReleaseInvalid();
		}
		return result;
	}
	int64_t iIndex = mInvalidRequest.miEntryIndex;
	int64_t iGeneration = static_cast<int64_t>(mInvalidRequest.muiGeneration);
	uint64_t uiOwnership = pPackChunks->mAudioReadEntries[iIndex].uiOwnership.load(std::memory_order_acquire);
	if (AudioReadState(uiOwnership) != AudioChunkReadState::kLoading)
	{
		result.bPending = true;
		return result;
	}
	std::fill(mInvalidBuffer.begin(), mInvalidBuffer.end(), std::byte {0x5a});
	ChunkReadResult ePoll = pPackChunks->TryReadChunkData(mInvalidRequest, mInvalidCrc, miInvalidOffset, std::span(mInvalidBuffer).first(static_cast<size_t>(miInvalidLength - 1)));
	result.bTooSmallPollFailed = ePoll == ChunkReadResult::kFailed;
	result.bTooSmallPollNoWrite = std::ranges::all_of(mInvalidBuffer, [](std::byte uiValue)
	{
		return uiValue == std::byte {0x5a};
	});
	uiOwnership = pPackChunks->mAudioReadEntries[iIndex].uiOwnership.load(std::memory_order_acquire);
	result.bLoadingCancelled = static_cast<int64_t>(AudioReadGeneration(uiOwnership)) != iGeneration;
	result.bLoadingEntryNotReused = AudioReadState(uiOwnership) == AudioChunkReadState::kLoading;
	ReleaseInvalid();
	return result;
}

void AudioStreamingFixture::ReleaseInvalid()
{
	mInvalidRequest.Reset();
	ReleaseHold(AudioStreamingFixtureHoldOwner::kInvalid);
}

bool AudioStreamingFixture::HoldAudioRead(common::crc_t crc, int64_t iOffset, int64_t iLength, int64_t iIndex, int64_t iGeneration)
{
	AudioStreamingFixture* pFixture = gpAttachedAudioStreamingFixture.load(std::memory_order_acquire);
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
	if (pFixture->miHoldOffset.load(std::memory_order_seq_cst) != iOffset)
	{
		return false;
	}
	if (pFixture->miHoldLength.load(std::memory_order_seq_cst) != iLength)
	{
		return false;
	}
	if (pFixture->muiHoldToken.load(std::memory_order_seq_cst) != uiHoldToken)
	{
		return false;
	}
	pFixture->miHeldGeneration.store(iGeneration, std::memory_order_seq_cst);
	pFixture->miHeldIndex.store(iIndex, std::memory_order_seq_cst);
	uint64_t uiConsumedToken = PackHoldToken(HoldOwner(uiHoldToken), AudioStreamingFixtureHoldState::kConsumed, HoldGeneration(uiHoldToken));
	if (!pFixture->muiHoldToken.compare_exchange_strong(uiHoldToken, uiConsumedToken, std::memory_order_seq_cst))
	{
		pFixture->miHeldGeneration.store(0, std::memory_order_seq_cst);
		pFixture->miHeldIndex.store(4'294'967'295i64, std::memory_order_seq_cst);
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
	if (AudioStreamingFixture* pFixture = gpAttachedAudioStreamingFixture.load(std::memory_order_acquire); pFixture != nullptr)
	{
		pFixture->miHeldGeneration.store(0, std::memory_order_seq_cst);
		pFixture->miHeldIndex.store(4'294'967'295i64, std::memory_order_seq_cst);
	}
}

void AudioStreamingFixture::CountRetry()
{
	if (AudioStreamingFixture* pFixture = gpAttachedAudioStreamingFixture.load(std::memory_order_acquire); pFixture != nullptr && (pFixture->muiScenarioGate.load(std::memory_order_seq_cst) & 1) != 0)
	{
		pFixture->muiRetryCount.fetch_add(1, std::memory_order_seq_cst);
	}
}

void AudioStreamingFixture::PrepareLoaderDrain()
{
	if (AudioStreamingFixture* pFixture = gpAttachedAudioStreamingFixture.load(std::memory_order_acquire); pFixture != nullptr)
	{
		pFixture->ReleaseInvalid();
		pFixture->ReleaseHold(AudioStreamingFixtureHoldOwner::kControlled);
		pFixture->ReleaseCoexistence();
		pFixture->ReleaseSaturation();
	}
}

bool AudioStreamingFixture::AllowOlderFadeRequests()
{
	return false;
}

AudioStreamingVoiceControl* AudioStreamingFixture::CreateVoiceControl(StreamingVoice& rVoice)
{
	AudioStreamingFixture* pFixture = gpAttachedAudioStreamingFixture.load(std::memory_order_acquire);
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

void AudioStreamingFixture::RetireVoiceControl(const AudioStreamingVoiceControl* pControl)
{
	if (pControl == nullptr)
	{
		return;
	}
	AudioStreamingFixture* pFixture = gpAttachedAudioStreamingFixture.load(std::memory_order_acquire);
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
