#include "StreamingVoices.h"

#if defined(BT_CLIENT)

#include "AudioManager.h"
#include "StreamingVoice.h"
#if defined(BT_DEBUG)
#include "Agent/Commands/AudioStreamingFixture.h"
#endif

#include "File/FileManager.h"
#include "File/PackChunks.h"

namespace engine
{

StreamingVoices::StreamingVoices() = default;
StreamingVoices::~StreamingVoices() = default;

void StreamingVoices::Play(common::crc_t uiAudioCrc)
{
	// Heap: make_unique<StreamingVoice> (with triple buffers) and vector push_back for crossfade list.
	// These outlive the call (persist until fade-out completes), so workbuffer/pre-alloc won't work.
	ScopedSuppressAllocationTracking suppress;

	// An explicit track supersedes any pending transition retry.
	muiRetryTrackCrc = 0;

	if (mpCurrentStream != nullptr)
	{
		TransitionCurrentToPrevious();
	}

	if (mpAudioEngine == nullptr) [[unlikely]]
	{
		return;
	}

	if (!mpAudioEngine->IsAudioDevicePresent()) [[unlikely]]
	{
		return;
	}

	CreateStream(uiAudioCrc);
}

void StreamingVoices::CheckTrackTransition()
{
	if (mGetNextTrack && (mpCurrentStream == nullptr || mpCurrentStream->ShouldTransition()))
	{
		// A failed CreateStream (voice allocation) must not consume another playlist entry: retry the
		// same track next call instead of advancing past it.
		common::crc_t uiNextTrackCrc = muiRetryTrackCrc != 0 ? muiRetryTrackCrc : mGetNextTrack();
		if (mpCurrentStream != nullptr)
		{
			TransitionCurrentToPrevious();
		}
		CreateStream(uiNextTrackCrc);
		muiRetryTrackCrc = mpCurrentStream == nullptr ? uiNextTrackCrc : 0;
	}
}

void StreamingVoices::Update(std::chrono::duration<float> deltaTime)
{
	// Heap: Member vector collects faded-out streams for deferred destruction after iteration.
	// Allocation reused across frames. Suppression covers potential growth and destructor calls.
	ScopedSuppressAllocationTracking suppress;

	if (mpCurrentStream != nullptr)
	{
#if defined(BT_DEBUG)
		AudioStreamingFixture* pFixture = gpAttachedAudioStreamingFixture.load(std::memory_order_acquire);
		mpCurrentStream->UpdateRequests(pFixture == nullptr || !(pFixture->mFlags & AudioStreamingFixture::Flags::kPublicationStopped));
#else
		mpCurrentStream->UpdateRequests(true);
#endif
	}
	if (!mPreviousStreams.empty())
	{
#if defined(BT_DEBUG)
		AudioStreamingFixture* pFixture = gpAttachedAudioStreamingFixture.load(std::memory_order_acquire);
		mPreviousStreams.back()->UpdateRequests(pFixture == nullptr || !(pFixture->mFlags & AudioStreamingFixture::Flags::kControlledPublication));
#else
		mPreviousStreams.back()->UpdateRequests(true);
#endif
	}
	for (int64_t i = 0; i + 1 < std::ssize(mPreviousStreams); ++i)
	{
#if defined(BT_DEBUG)
		mPreviousStreams.at(i)->UpdateRequests(AudioStreamingFixture::AllowOlderFadeRequests());
#else
		mPreviousStreams.at(i)->UpdateRequests(false);
#endif
	}

	if (mpCurrentStream != nullptr)
	{
		mpCurrentStream->DrainConsumedAndSubmitReady();
		mpCurrentStream->UpdateVolume(deltaTime);
	}
	for (const std::unique_ptr<StreamingVoice>& rpStream : mPreviousStreams)
	{
		rpStream->DrainConsumedAndSubmitReady();
	}

	for (auto it = mPreviousStreams.begin(); it != mPreviousStreams.end();)
	{
		if ((*it)->UpdateVolume(deltaTime))
		{
			(*it)->CancelPendingReads();
			mStreamsToDestroy.push_back(std::move(*it));
			it = mPreviousStreams.erase(it);
		}
		else
		{
			++it;
		}
	}
	mStreamsToDestroy.clear();
}

void StreamingVoices::CancelPendingReads()
{
	if (mpCurrentStream != nullptr)
	{
		mpCurrentStream->CancelPendingReads();
	}
	for (const std::unique_ptr<StreamingVoice>& rpStream : mPreviousStreams)
	{
		rpStream->CancelPendingReads();
	}
}

void StreamingVoices::Clear(bool bNullVoicesBeforeDestroy)
{
	CancelPendingReads();
	for (const std::unique_ptr<StreamingVoice>& rpStream : mStreamsToDestroy)
	{
		rpStream->CancelPendingReads();
	}

	if (bNullVoicesBeforeDestroy)
	{
		if (mpCurrentStream != nullptr)
		{
			mpCurrentStream->mpVoice = nullptr;
		}

		for (const std::unique_ptr<StreamingVoice>& rpStream : mPreviousStreams)
		{
			if (rpStream != nullptr)
			{
				rpStream->mpVoice = nullptr;
			}
		}

		for (const std::unique_ptr<StreamingVoice>& rpStream : mStreamsToDestroy)
		{
			if (rpStream != nullptr)
			{
				rpStream->mpVoice = nullptr;
			}
		}
	}

	mpCurrentStream.reset();
	mPreviousStreams.clear();
	mStreamsToDestroy.clear();
	// Post-clear playback restarts from the next-track callback, not a stale retry.
	muiRetryTrackCrc = 0;
}

int64_t StreamingVoices::GetStreamCount() const
{
	ASSERT(common::gpMultithreading->IsMainThread());
	return (mpCurrentStream != nullptr ? 1 : 0) + std::ssize(mPreviousStreams) + std::ssize(mStreamsToDestroy);
}

void StreamingVoices::CreateStream(common::crc_t uiAudioCrc)
{
	const LazyChunk& rLazyChunk = gpFileManager->mpPackChunks->mLazyChunkMap.at(uiAudioCrc);
	AssertValidPackedAudio(rLazyChunk.header.audioHeader.waveFormat, rLazyChunk.header.iSize, rLazyChunk.iDataSize);

	IXAudio2SourceVoice* pVoice = nullptr;
	mpAudioEngine->AllocateVoice(&rLazyChunk.header.audioHeader.waveFormat, SoundEffectInstance_Default, false, &pVoice);
	if (pVoice != nullptr)
	{
		mpCurrentStream = std::make_unique<StreamingVoice>(mpAudioEngine, pVoice, &rLazyChunk);
#if defined(BT_DEBUG)
		mpCurrentStream->mpAudioStreamingControl = AudioStreamingFixture::CreateVoiceControl(*mpCurrentStream);
		AudioStreamingFixture::Record(AudioStreamingFixturePartition::kMain, AudioStreamingFixturePhase::kStartup, std::numeric_limits<uint32_t>::max(), uiAudioCrc, 0, rLazyChunk.header.iSize, AudioStreamingFixtureQueueState::kFree, 0, false);
#endif
	}
	else
	{
		char pcHex[20] {};
		LOG(kAudio, kWarning, "CreateStream AllocateVoice failed for CRC {}", common::ToHex(std::span(pcHex), uiAudioCrc));
	}
}

void StreamingVoices::TransitionCurrentToPrevious()
{
	for (const std::unique_ptr<StreamingVoice>& rpStream : mPreviousStreams)
	{
		rpStream->CancelPendingReads();
	}
	mpCurrentStream->BeginFadeOut();
	mPreviousStreams.push_back(std::move(mpCurrentStream));
}

} // namespace engine

#endif // defined(BT_CLIENT)
