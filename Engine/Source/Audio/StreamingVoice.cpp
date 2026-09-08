#include "StreamingVoice.h"

#if defined(BT_CLIENT)

#include "AudioUtility.h"
#if defined(BT_DEBUG)
#include "Agent/Commands/AudioStreamingFixture.h"
#endif
#include "File/FileManager.h"
#include "Ui/SoundSettingsWrappersBase.h"

namespace engine
{

using enum StreamingVoiceFlags;

StreamingVoice::StreamingVoice(AudioEngine* pAudioEngine, IXAudio2SourceVoice* pVoice, const LazyChunk* pLazyChunk)
: mpLazyChunk(pLazyChunk)
, mpVoice(pVoice)
, mpAudioEngine(pAudioEngine)
{
	LOG(kAudio, kDebug, "Music streaming: Initializing stream for CRC {}, data size: {} bytes, buffer size: {} bytes", mpLazyChunk->location.crc, mpLazyChunk->header.iSize, kiBufferSize);

	CHECK_HRESULT(mpVoice->SetVolume(0.0f));
}

StreamingVoice::~StreamingVoice()
{
	CancelPendingReads();
	DestroyXAudio2SourceVoice(mpAudioEngine, mpVoice);
#if defined(BT_DEBUG)
	AudioStreamingFixture::RetireVoiceControl(mpAudioStreamingControl);
#endif
}

float StreamingVoice::GetRemainingTime() const
{
	int64_t iRemainingBytes = mpLazyChunk->header.iSize - miNextReadOffset;
	if (iRemainingBytes <= 0)
	{
		return 0.0f;
	}

	return static_cast<float>(iRemainingBytes) / static_cast<float>(mpLazyChunk->header.audioHeader.waveFormat.nAvgBytesPerSec);
}

bool StreamingVoice::ShouldTransition() const
{
	return GetRemainingTime() <= kfCrossfadeDuration || miNextReadOffset >= mpLazyChunk->header.iSize || (mFlags & kLastBufferSubmitted);
}

void StreamingVoice::UpdateRequests(bool bAllowRequests)
{
	for (int64_t iSlot = 0; iSlot < kiBufferCount; ++iSlot)
	{
		if (mSlotStates[iSlot] != SlotState::kPending)
		{
			continue;
		}

		std::span<std::byte> destination(reinterpret_cast<std::byte*>(mBuffers[iSlot]), mSlotBytesRead[iSlot]);
		ChunkReadResult eResult = gpFileManager->TryReadChunkData(mReadRequests[iSlot], mpLazyChunk->location.crc, static_cast<uint64_t>(mSlotOffsets[iSlot]), destination);
		if (eResult == ChunkReadResult::kReady)
		{
			mSlotStates[iSlot] = SlotState::kReady;
		}
		else if (eResult == ChunkReadResult::kFailed)
		{
			mSlotBytesRead[iSlot] = 0;
			mbSlotLastBuffer[iSlot] = true;
			mbReadDone = true;
			mSlotStates[iSlot] = SlotState::kReady;
			LOG(kAudio, kWarning, "Music streaming: Failed to read chunk data at position {}", mSlotOffsets[iSlot]);
		}
	}

	if (!bAllowRequests)
	{
		return;
	}
#if defined(BT_DEBUG)
	if (mpAudioStreamingControl != nullptr && mpAudioStreamingControl->uiPublicationAllowance == 0)
	{
		return;
	}
#endif
	if (mbReadDone)
	{
		return;
	}

	while (mSlotStates[miNextRequest] == SlotState::kEmpty)
	{
		int64_t iSlot = miNextRequest;
		int64_t iRemainingData = mpLazyChunk->header.iSize - miNextReadOffset;
		if (iRemainingData == 0)
		{
			mSlotBytesRead[iSlot] = 0;
			mSlotOffsets[iSlot] = miNextReadOffset;
			mbSlotLastBuffer[iSlot] = true;
			mbReadDone = true;
			mSlotStates[iSlot] = SlotState::kReady;
			LOG(kAudio, kDebug, "Music streaming: No remaining data to read, position: {}/{}", miNextReadOffset, mpLazyChunk->header.iSize);
			return;
		}

		int64_t iBytesToRead = std::min(iRemainingData, kiBufferSize);
		std::span<std::byte> destination(reinterpret_cast<std::byte*>(mBuffers[iSlot]), iBytesToRead);
		ChunkReadResult eResult = gpFileManager->TryReadChunkData(mReadRequests[iSlot], mpLazyChunk->location.crc, static_cast<uint64_t>(miNextReadOffset), destination);
		if (eResult == ChunkReadResult::kRetry)
		{
			return;
		}
#if defined(BT_DEBUG)
		if (mpAudioStreamingControl != nullptr && eResult != ChunkReadResult::kFailed)
		{
			--mpAudioStreamingControl->uiPublicationAllowance;
		}
#endif
		if (eResult == ChunkReadResult::kFailed)
		{
			mSlotBytesRead[iSlot] = 0;
			mSlotOffsets[iSlot] = miNextReadOffset;
			mbSlotLastBuffer[iSlot] = true;
			mbReadDone = true;
			mSlotStates[iSlot] = SlotState::kReady;
			LOG(kAudio, kWarning, "Music streaming: Failed to read chunk data at position {}", miNextReadOffset);
			return;
		}

		mSlotBytesRead[iSlot] = iBytesToRead;
		mSlotOffsets[iSlot] = miNextReadOffset;
		miNextReadOffset += iBytesToRead;
		mbSlotLastBuffer[iSlot] = miNextReadOffset >= mpLazyChunk->header.iSize;
		mSlotStates[iSlot] = eResult == ChunkReadResult::kReady ? SlotState::kReady : SlotState::kPending;
		miNextRequest = (miNextRequest + 1) % kiBufferCount;
		if (mbSlotLastBuffer[iSlot])
		{
			mbReadDone = true;
			LOG(kAudio, kDebug, "Music streaming last buffer: Read {} bytes at position {}/{}", iBytesToRead, miNextReadOffset, mpLazyChunk->header.iSize);
			return;
		}
	}
}

void StreamingVoice::DrainConsumedAndSubmitReady()
{
	int64_t iConsumed = miBuffersConsumed.exchange(0, std::memory_order_acquire);
	for (int64_t i = 0; i < iConsumed; ++i)
	{
		mSlotStates[miNextConsume] = SlotState::kEmpty;
		miNextConsume = (miNextConsume + 1) % kiBufferCount;
	}

	while (!(mFlags & kLastBufferSubmitted))
	{
		if (mSlotStates[miNextSubmit] != SlotState::kReady)
		{
			break;
		}

		int64_t iBytesRead = mSlotBytesRead[miNextSubmit];
		bool bLastBuffer = mbSlotLastBuffer[miNextSubmit];

		if (iBytesRead == 0)
		{
			// EOF or read failure has nothing to submit; mark done and recycle the slot.
			mFlags.Set(kLastBufferSubmitted);
			mSlotStates[miNextSubmit] = SlotState::kEmpty;
			miNextSubmit = (miNextSubmit + 1) % kiBufferCount;
			break;
		}

		XAUDIO2_BUFFER xaudio2Buffer
		{
			.Flags = bLastBuffer ? XAUDIO2_END_OF_STREAM : 0u,
			.AudioBytes = static_cast<UINT32>(iBytesRead),
			.pAudioData = mBuffers[miNextSubmit],
			.PlayBegin = 0,
			.PlayLength = 0,
			.LoopBegin = 0,
			.LoopLength = 0,
			.LoopCount = 0,
			.pContext = this,
		};
		HRESULT hResult = mpVoice->SubmitSourceBuffer(&xaudio2Buffer);
		if (FAILED(hResult))
		{
			char pcHex[20] {};
			LOG(kAudio, kWarning, "SubmitSourceBuffer failed, HRESULT: {}", common::ToHex(std::span(pcHex), static_cast<uint32_t>(hResult)));
			mFlags.Set(kLastBufferSubmitted);
			break;
		}

		mSlotStates[miNextSubmit] = SlotState::kSubmitted;
		miNextSubmit = (miNextSubmit + 1) % kiBufferCount;

		if (!mbStarted)
		{
			CHECK_HRESULT(mpVoice->Start());
			mbStarted = true;
		}

		if (bLastBuffer)
		{
			mFlags.Set(kLastBufferSubmitted);
			break;
		}
	}
}

void StreamingVoice::CancelPendingReads()
{
	for (int64_t iSlot = 0; iSlot < kiBufferCount; ++iSlot)
	{
		mReadRequests[iSlot].Reset();
		if (mSlotStates[iSlot] == SlotState::kPending)
		{
			mSlotStates[iSlot] = SlotState::kEmpty;
		}
	}
}

void StreamingVoice::BeginFadeOut()
{
	mFlags.Clear(kFadingIn);
	mFlags.Set(kFadingOut);
}

void StreamingVoice::DetachXAudio2Voice()
{
	mpVoice = nullptr;
}

bool StreamingVoice::UpdateVolume(float fDeltaTime)
{
	if (mFlags & kFadingIn)
	{
		mfCurrentVolume += fDeltaTime / kfCrossfadeDuration;
		if (mfCurrentVolume >= 1.0f)
		{
			mFlags.Clear(kFadingIn);
		}
	}
	else if (mFlags & kFadingOut)
	{
		mfCurrentVolume -= fDeltaTime / kfCrossfadeDuration;
		if (mfCurrentVolume <= 0.0f)
		{
			mFlags.Clear(kFadingOut);
		}
	}
	mfCurrentVolume = std::clamp(mfCurrentVolume, 0.0f, 1.0f);

	CHECK_HRESULT(mpVoice->SetVolume(VolumeToPower(gMasterVolume.Get(), gMusicVolume.Get(), mfCurrentVolume)));

	return mfCurrentVolume <= 0.0f;
}

void StreamingVoice::OnBufferEnd()
{
	miBuffersConsumed.fetch_add(1, std::memory_order_release);
}

} // namespace engine

#endif // BT_CLIENT
