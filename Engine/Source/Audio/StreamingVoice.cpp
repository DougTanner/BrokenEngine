#include "StreamingVoice.h"

#if defined(BT_CLIENT)

#include "File/FileManager.h"
#include "File/PackChunks.h"
#include "Ui/SoundSettingsWrappersBase.h"
#include "AudioUtility.h"

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
}

std::chrono::duration<float> StreamingVoice::GetRemainingTime() const
{
	int64_t iRemainingBytes = mpLazyChunk->header.iSize - miNextReadOffset;
	if (iRemainingBytes <= 0)
	{
		return std::chrono::duration<float>::zero();
	}

	return std::chrono::duration<float>(static_cast<float>(iRemainingBytes) / static_cast<float>(mpLazyChunk->header.audioHeader.waveFormat.nAvgBytesPerSec));
}

bool StreamingVoice::ShouldTransition() const
{
	return GetRemainingTime() <= kCrossfadeDuration || (mFlags & kLastBufferSubmitted);
}

void StreamingVoice::UpdateRequests(bool bAllowRequests)
{
	for (int64_t i = 0; i < kiBufferCount; ++i)
	{
		if (mpeSlotStates[i] != SlotState::kPending)
		{
			continue;
		}

		std::span<std::byte> destination(reinterpret_cast<std::byte*>(mpuiBuffers[i]), mpiSlotBytesRead[i]);
		ChunkReadResult eResult = gpFileManager->mpPackChunks->TryReadChunkData(mpReadRequests[i], mpLazyChunk->location.crc, mpiSlotOffsets[i], destination);
		if (eResult == ChunkReadResult::kReady)
		{
			mpeSlotStates[i] = SlotState::kReady;
		}
		else if (eResult == ChunkReadResult::kFailed)
		{
			mpiSlotBytesRead[i] = 0;
			mpbSlotLastBuffer[i] = true;
			mbReadDone = true;
			mpeSlotStates[i] = SlotState::kReady;
			LOG(kAudio, kWarning, "Music streaming: Failed to read chunk data at position {}", mpiSlotOffsets[i]);
		}
	}

	if (!bAllowRequests)
	{
		return;
	}
	if (mbReadDone)
	{
		return;
	}

	while (mpeSlotStates[miNextRequest] == SlotState::kEmpty)
	{
		int64_t iSlot = miNextRequest;
		int64_t iRemainingData = mpLazyChunk->header.iSize - miNextReadOffset;
		if (iRemainingData == 0)
		{
			mpiSlotBytesRead[iSlot] = 0;
			mpiSlotOffsets[iSlot] = miNextReadOffset;
			mpbSlotLastBuffer[iSlot] = true;
			mbReadDone = true;
			mpeSlotStates[iSlot] = SlotState::kReady;
			LOG(kAudio, kDebug, "Music streaming: No remaining data to read, position: {}/{}", miNextReadOffset, mpLazyChunk->header.iSize);
			return;
		}

		int64_t iBytesToRead = std::min(iRemainingData, kiBufferSize);
		std::span<std::byte> destination(reinterpret_cast<std::byte*>(mpuiBuffers[iSlot]), iBytesToRead);
		ChunkReadResult eResult = gpFileManager->mpPackChunks->TryReadChunkData(mpReadRequests[iSlot], mpLazyChunk->location.crc, miNextReadOffset, destination);
		if (eResult == ChunkReadResult::kRetry)
		{
			return;
		}
		if (eResult == ChunkReadResult::kFailed)
		{
			mpiSlotBytesRead[iSlot] = 0;
			mpiSlotOffsets[iSlot] = miNextReadOffset;
			mpbSlotLastBuffer[iSlot] = true;
			mbReadDone = true;
			mpeSlotStates[iSlot] = SlotState::kReady;
			LOG(kAudio, kWarning, "Music streaming: Failed to read chunk data at position {}", miNextReadOffset);
			return;
		}

		mpiSlotBytesRead[iSlot] = iBytesToRead;
		mpiSlotOffsets[iSlot] = miNextReadOffset;
		miNextReadOffset += iBytesToRead;
		mpbSlotLastBuffer[iSlot] = miNextReadOffset >= mpLazyChunk->header.iSize;
		mpeSlotStates[iSlot] = eResult == ChunkReadResult::kReady ? SlotState::kReady : SlotState::kPending;
		miNextRequest = (miNextRequest + 1) % kiBufferCount;
		if (mpbSlotLastBuffer[iSlot])
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
		mpeSlotStates[miNextConsume] = SlotState::kEmpty;
		miNextConsume = (miNextConsume + 1) % kiBufferCount;
	}

	while (!(mFlags & kLastBufferSubmitted))
	{
		if (mpeSlotStates[miNextSubmit] != SlotState::kReady)
		{
			break;
		}

		int64_t iBytesRead = mpiSlotBytesRead[miNextSubmit];
		bool bLastBuffer = mpbSlotLastBuffer[miNextSubmit];

		if (iBytesRead == 0)
		{
			// An empty slot represents end of stream or a read failure.
			mFlags.Set(kLastBufferSubmitted);
			mpeSlotStates[miNextSubmit] = SlotState::kEmpty;
			miNextSubmit = (miNextSubmit + 1) % kiBufferCount;
			break;
		}

		XAUDIO2_BUFFER xaudio2Buffer
		{
			.Flags = bLastBuffer ? XAUDIO2_END_OF_STREAM : 0ui32,
			.AudioBytes = static_cast<UINT32>(iBytesRead),
			.pAudioData = mpuiBuffers[miNextSubmit],
			.PlayBegin = 0,
			.PlayLength = 0,
			.LoopBegin = 0,
			.LoopLength = 0,
			.LoopCount = 0,
			.pContext = this,
		};
		int64_t iResult = mpVoice->SubmitSourceBuffer(&xaudio2Buffer);
		if (FAILED(static_cast<HRESULT>(iResult)))
		{
			char pcHex[20] {};
			LOG(kAudio, kWarning, "SubmitSourceBuffer failed, HRESULT: {}", common::ToHex(std::span(pcHex), static_cast<uint32_t>(iResult)));
			mFlags.Set(kLastBufferSubmitted);
			break;
		}

		mpeSlotStates[miNextSubmit] = SlotState::kSubmitted;
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
	for (int64_t i = 0; i < kiBufferCount; ++i)
	{
		mpReadRequests[i].Reset();
		if (mpeSlotStates[i] == SlotState::kPending)
		{
			mpeSlotStates[i] = SlotState::kEmpty;
		}
	}
}

void StreamingVoice::BeginFadeOut()
{
	mFlags.Set(kFadingIn, false);
	mFlags.Set(kFadingOut);
}

bool StreamingVoice::UpdateVolume(std::chrono::duration<float> deltaTime)
{
	if (mFlags & kFadingIn)
	{
		mfCurrentVolume += deltaTime / kCrossfadeDuration;
		if (mfCurrentVolume >= 1.0f)
		{
			mFlags.Set(kFadingIn, false);
		}
	}
	else if (mFlags & kFadingOut)
	{
		mfCurrentVolume -= deltaTime / kCrossfadeDuration;
		if (mfCurrentVolume <= 0.0f)
		{
			mFlags.Set(kFadingOut, false);
		}
	}
	mfCurrentVolume = std::clamp(mfCurrentVolume, 0.0f, 1.0f);

	CHECK_HRESULT(mpVoice->SetVolume(VolumeToPower(gMasterVolume.mfCurrent, gMusicVolume.mfCurrent, mfCurrentVolume)));

	return mfCurrentVolume <= 0.0f;
}

void StreamingVoice::OnBufferEnd()
{
	miBuffersConsumed.fetch_add(1, std::memory_order_release);
}

} // namespace engine

#endif // BT_CLIENT
