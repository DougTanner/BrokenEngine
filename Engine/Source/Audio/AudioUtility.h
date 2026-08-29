#pragma once

#if defined(BT_CLIENT)

namespace engine
{

// Pinned mastering-voice sample rate. Must match DataPacker's audiorepair::kiAudioExportSampleRate
// (packed audio is resampled to this) so source rate == mastering rate and XAudio2 bypasses per-voice
// SRC. Windows shared-mode does any final device-rate conversion once at the mastering output.
inline constexpr int kiMasteringSampleRate = 48000;

inline constexpr float VolumeToPower(float fMasterVolume, float fSoundVolume, float fLocalVolume = 1.0f)
{
	// More natural-feeling volume controls
	float fVolume = fMasterVolume * fSoundVolume * fLocalVolume;
	return fVolume * fVolume;
}

// XAudio2 bypasses its per-voice sample-rate converter only when the frequency ratio is exactly 1.0
// (and source rate == mastering rate — both pinned to 48 kHz). Slow-Doppler and non-randomized-pitch
// voices compute ratios like 0.9997 that force the resampler for no audible reason; snap those to 1.0.
inline constexpr float kfFrequencyRatioSnapEpsilon = 0.003f; // ~5 cents, inaudible

inline constexpr float SnapFrequencyRatio(float fRatio)
{
	return (fRatio > 1.0f - kfFrequencyRatioSnapEpsilon && fRatio < 1.0f + kfFrequencyRatioSnapEpsilon) ? 1.0f : fRatio;
}

// Packed audio is opaque file data, and DataPacker's ExportAudio is the format authority for it. Without this
// gate XAudio2 either throws an opaque error from AllocateVoice or reads past the end of the lazy chunk.
// Order is load-bearing: the block-align asserts run before the modulo below, which would divide by zero.
inline void AssertValidPackedAudio(const WAVEFORMATEX& rWaveFormat, int64_t iAudioBytes, int64_t iChunkCapacityBytes)
{
	ASSERT(rWaveFormat.wFormatTag == WAVE_FORMAT_PCM);
	ASSERT(rWaveFormat.wBitsPerSample == 16);
	ASSERT(rWaveFormat.nChannels == 1 || rWaveFormat.nChannels == 2);
	ASSERT(rWaveFormat.nSamplesPerSec == static_cast<DWORD>(kiMasteringSampleRate));
	ASSERT(rWaveFormat.nBlockAlign == rWaveFormat.nChannels * 2);
	ASSERT(rWaveFormat.nAvgBytesPerSec == rWaveFormat.nSamplesPerSec * static_cast<DWORD>(rWaveFormat.nBlockAlign));
	ASSERT(iAudioBytes > 0);
	ASSERT(iAudioBytes % static_cast<int64_t>(rWaveFormat.nBlockAlign) == 0);
	ASSERT(iAudioBytes <= iChunkCapacityBytes);
}

inline void DestroyXAudio2SourceVoice(AudioEngine* pAudioEngine, IXAudio2SourceVoice*& rpVoice)
{
	if (rpVoice != nullptr)
	{
		rpVoice->Stop(0, XAUDIO2_COMMIT_NOW);
		rpVoice->FlushSourceBuffers();
		if (pAudioEngine != nullptr)
		{
			std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
			pAudioEngine->DestroyVoice(rpVoice);
			std::chrono::steady_clock::duration elapsed = std::chrono::steady_clock::now() - start;
			if (elapsed > std::chrono::milliseconds(100))
			{
				LOG(kAudio, kWarning, "DestroyXAudio2SourceVoice took {}ms", std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
			}
		}
		rpVoice = nullptr;
	}
}

} // namespace engine

#endif // BT_CLIENT
