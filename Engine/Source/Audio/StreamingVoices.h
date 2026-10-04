#pragma once

#if defined(BT_CLIENT)

namespace DirectX
{
class AudioEngine;
} // namespace DirectX

namespace engine
{

class StreamingVoice;
struct LazyChunk;

class StreamingVoices
{
public:

	StreamingVoices();
	~StreamingVoices();


	void Play(common::crc_t uiAudioCrc);

	void CheckTrackTransition();
	void Update(std::chrono::duration<float> deltaTime);
	void CancelPendingReads();
	void Clear(bool bNullVoicesBeforeDestroy);

	int64_t GetStreamCount() const;

private:

	void CreateStream(common::crc_t uiAudioCrc);
	void TransitionCurrentToPrevious();

public:
	AudioEngine* mpAudioEngine = nullptr;
private:

#if defined(BT_DEBUG)
public:
#endif
	std::unique_ptr<StreamingVoice> mpCurrentStream;
	std::vector<std::unique_ptr<StreamingVoice>> mPreviousStreams;
private:
	std::vector<std::unique_ptr<StreamingVoice>> mStreamsToDestroy;
public:
	std::function<common::crc_t()> mGetNextTrack;
private:
	// Track whose CreateStream failed during a transition; retried before asking mGetNextTrack again. 0 = none.
	common::crc_t muiRetryTrackCrc = 0;
};

} // namespace engine

#endif // BT_CLIENT
