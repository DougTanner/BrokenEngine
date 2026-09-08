#pragma once

#if defined(BT_CLIENT)

namespace DirectX
{
class AudioEngine;
}

namespace engine
{

class StreamingVoice;
struct LazyChunk;

class StreamingVoices
{
public:

	StreamingVoices();
	~StreamingVoices();

	void Init(AudioEngine* pAudioEngine);

	void Play(common::crc_t uiAudioCrc);
	void SetNextTrackCallback(std::function<common::crc_t()> callback);

	void CheckTrackTransition();
	void Update(float fDeltaTime);
	void CancelPendingReads();
	void Clear(bool bNullVoicesBeforeDestroy);

	int64_t GetStreamCount() const;

private:

	void CreateStream(common::crc_t uiAudioCrc);
	void TransitionCurrentToPrevious();

	AudioEngine* mpAudioEngine = nullptr;

#if defined(BT_DEBUG)
public:
#endif
	std::unique_ptr<StreamingVoice> mpCurrentStream;
	std::vector<std::unique_ptr<StreamingVoice>> mPreviousStreams;
private:
	std::vector<std::unique_ptr<StreamingVoice>> mStreamsToDestroy;
	std::function<common::crc_t()> mGetNextTrack;
	// Track whose CreateStream failed during a transition; retried before asking mGetNextTrack again. 0 = none.
	common::crc_t muiRetryTrackCrc = 0;
};

} // namespace engine

#endif // BT_CLIENT
