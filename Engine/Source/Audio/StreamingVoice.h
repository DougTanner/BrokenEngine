#pragma once

#if defined(BT_CLIENT)

#include "File/FileManager.h"

namespace DirectX
{
class AudioEngine;
}

namespace engine
{

struct LazyChunk;
#if defined(BT_DEBUG)
struct AudioStreamingVoiceControl;
#endif

inline constexpr int64_t kiBufferCount = 3;
inline constexpr int64_t kiBufferSize = 16 * 1024;
inline constexpr float kfCrossfadeDuration = 1.0f;

enum class StreamingVoiceFlags : uint8_t
{
	kFadingIn            = 0x01,
	kFadingOut           = 0x02,
	kLastBufferSubmitted = 0x04,
};
using StreamingVoiceFlags_t = common::Flags<StreamingVoiceFlags>;

enum class SlotState : uint8_t
{
	kEmpty,
	kPending,
	kReady,
	kSubmitted,
};

class StreamingVoice : public IVoiceNotify
{
public:

	StreamingVoice() = delete;
	StreamingVoice(AudioEngine* pAudioEngine, IXAudio2SourceVoice* pVoice, const LazyChunk* pLazyChunk);

	~StreamingVoice();

	StreamingVoice(const StreamingVoice&) = delete;
	StreamingVoice& operator=(const StreamingVoice&) = delete;

	StreamingVoice(StreamingVoice&&) = delete;
	StreamingVoice& operator=(StreamingVoice&&) = delete;

	float GetRemainingTime() const;
	bool ShouldTransition() const;
	void UpdateRequests(bool bAllowRequests);
	void DrainConsumedAndSubmitReady();
	void CancelPendingReads();
	void BeginFadeOut();
	void DetachXAudio2Voice();
	bool UpdateVolume(float fDeltaTime);

	// IVoiceNotify
	void OnBufferEnd() override;
	void OnCriticalError() override {}
	void OnReset() override {}
	void OnUpdate() override {}
	void OnDestroyEngine() noexcept override {}
	void OnTrim() override {}
	void GatherStatistics([[maybe_unused]] AudioStatistics& rStats) const override {}
	void OnDestroyParent() noexcept override {}
private:
#if defined(BT_DEBUG)
public:
#endif

	StreamingVoiceFlags_t mFlags = StreamingVoiceFlags::kFadingIn;
	const LazyChunk* mpLazyChunk = nullptr;
private:
	int64_t miNextReadOffset = 0;
#if defined(BT_DEBUG)
public:
#endif
	float mfCurrentVolume = 0.0f;
private:
	int64_t miNextRequest = 0;
#if defined(BT_DEBUG)
public:
#endif
	int64_t miNextSubmit = 0;
private:
	int64_t miNextConsume = 0;
	bool mbStarted = false;
	bool mbReadDone = false;
	std::atomic<int64_t> miBuffersConsumed = 0;
#if defined(BT_DEBUG)
public:
#endif
	SlotState mSlotStates[kiBufferCount] {};
private:
	ChunkReadRequest mReadRequests[kiBufferCount];
	int64_t mSlotOffsets[kiBufferCount] {};
	int64_t mSlotBytesRead[kiBufferCount] {};
	bool mbSlotLastBuffer[kiBufferCount] {};
	uint8_t mBuffers[kiBufferCount][kiBufferSize] {};
#if defined(BT_DEBUG)
public:
#endif
	IXAudio2SourceVoice* mpVoice = nullptr;
private:
	AudioEngine* mpAudioEngine = nullptr;
#if defined(BT_DEBUG)
public:
	AudioStreamingVoiceControl* mpAudioStreamingControl = nullptr;
#endif
};

} // namespace engine

#endif // BT_CLIENT
