#include "AudioManager.h"
#if defined(BT_DEBUG)
#include "Agent/Commands/AudioStreamingFixture.h"
#endif

#if defined(BT_CLIENT)

#include "File/FileManager.h"
#include "StaticVoices.h"
#include "StreamingVoices.h"

#include "Profile/ProfileManager.h"
#include "Game.h"

namespace engine
{

constexpr AUDIO_ENGINE_FLAGS keAudioEngineFlags = AudioEngine_UseMasteringLimiter;

// Silent-start recovery probe interval, in Update frames. A deviceless machine holds the silent engine
// forever, so an unthrottled probe would warn + full-reset every frame; ~2 s at 60 fps latency to pick up
// a newly attached device is imperceptible against that cost.
constexpr int64_t kiSilentRecoveryRetryFrames = 120;

std::wstring AudioManager::GetEndpointIdentifier(IMMDevice* pDevice)
{
	LPWSTR pcDeviceIdentifier = nullptr;
	int64_t iResult = pDevice->GetId(&pcDeviceIdentifier);
	if (FAILED(static_cast<HRESULT>(iResult)))
	{
		LOG(kAudio, kWarning, "  GetId failed: {}", common::HresultToString(static_cast<HRESULT>(iResult)).data());
		return std::wstring();
	}
	common::ScopedLambda freeDeviceIdentifier([=]()
	{
		CoTaskMemFree(pcDeviceIdentifier);
	});
	return pcDeviceIdentifier != nullptr ? std::wstring(pcDeviceIdentifier) : std::wstring();
}

std::wstring AudioManager::InitializeAudioEndpoint()
{
	Microsoft::WRL::ComPtr<IMMDeviceEnumerator> pMultimediaDeviceEnumerator;
	int64_t iResult = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(pMultimediaDeviceEnumerator.GetAddressOf()));
	if (FAILED(static_cast<HRESULT>(iResult)))
	{
		LOG(kAudio, kWarning, "  CoCreateInstance(MMDeviceEnumerator) failed: {}", common::HresultToString(static_cast<HRESULT>(iResult)).data());
	}
	else
	{
		LOG(kAudio, kInfo, "  Got MMDeviceEnumerator");
	}

	Microsoft::WRL::ComPtr<IMMDevice> pDefaultAudioEndpoint;
	std::wstring defaultAudioEndpointIdentifier;
	if (pMultimediaDeviceEnumerator != nullptr && pMultimediaDeviceEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &pDefaultAudioEndpoint) == S_OK)
	{
		LOG(kAudio, kInfo, "  Got DefaultAudioEndpoint");
		defaultAudioEndpointIdentifier = GetEndpointIdentifier(pDefaultAudioEndpoint.Get());
		if (!defaultAudioEndpointIdentifier.empty())
		{
			LOG(kAudio, kInfo, "    pcDeviceId: \"{}\"", defaultAudioEndpointIdentifier);
			mpAudioEngine = std::make_unique<AudioEngine>(keAudioEngineFlags, nullptr, defaultAudioEndpointIdentifier.c_str(), AudioCategory_GameEffects);
		}
		else
		{
			LOG(kAudio, kWarning, "  Default audio endpoint gave no id; falling back to the OS default");
		}
	}
	else
	{
		LOG(kAudio, kWarning, "  No default audio endpoint available; falling back to the OS default");
	}

	// Always yield one stable engine. When no endpoint produced one (no enumerator, a failed default-endpoint
	// query, or an empty default id), construct against the OS default (deviceId == nullptr): with the mastering limiter
	// flag and no AudioEngine_ThrowOnNoAudioHW, DirectXTK keeps a silent-mode shell instead of throwing.
	// The voice subsystems receive this pointer once and it is never recreated — only Reset recovers it.
	if (mpAudioEngine == nullptr)
	{
		LOG(kAudio, kWarning, "  No usable audio endpoint; constructing silent-capable engine against the OS default");
		mpAudioEngine = std::make_unique<AudioEngine>(keAudioEngineFlags, nullptr, nullptr, AudioCategory_GameEffects);
	}
	return defaultAudioEndpointIdentifier;
}

void AudioManager::CacheMasteringVoiceChannels()
{
	IXAudio2MasteringVoice* pMasterVoice = mpAudioEngine->GetMasterVoice();
	if (pMasterVoice == nullptr)
	{
		return;
	}
	XAUDIO2_VOICE_DETAILS voiceDetails {};
	pMasterVoice->GetVoiceDetails(&voiceDetails);
	WAVEFORMATEXTENSIBLE waveFormat = mpAudioEngine->GetOutputFormat();
	miMasteringVoiceChannels = std::min(static_cast<int64_t>(voiceDetails.InputChannels), static_cast<int64_t>(waveFormat.Format.nChannels));
}

WAVEFORMATEX AudioManager::MakePinnedOutputFormat(int64_t iChannels) const
{
	WAVEFORMATEX format {};
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = static_cast<WORD>(iChannels);
	format.nSamplesPerSec = static_cast<DWORD>(kiMasteringSampleRate);
	format.wBitsPerSample = 16;
	format.nBlockAlign = static_cast<WORD>(format.nChannels * (format.wBitsPerSample / 8));
	format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
	format.cbSize = 0;
	return format;
}

void AudioManager::ConfigureLiveGraph(const wchar_t* pcSelectedDeviceIdentifier)
{
	// A silent-mode engine has no live output graph (GetOutputChannels() == 0). Defer all pinned-format
	// setup — mPinnedOutputFormat stays empty and kPinnedFormatValid false — so Update recovery discovers
	// native channels from the OS default when a device later appears.
	if (mpAudioEngine->GetOutputChannels() == 0)
	{
		LOG(kAudio, kInfo, "  No live audio graph at startup; running silent until a device appears");
		return;
	}

	mPinnedOutputFormat = MakePinnedOutputFormat(mpAudioEngine->GetOutputChannels());
	if (mpAudioEngine->GetOutputSampleRate() != kiMasteringSampleRate)
	{
		LOG(kAudio, kInfo, "  Pinning mastering voice to {} Hz (device native {} Hz)", kiMasteringSampleRate, mpAudioEngine->GetOutputSampleRate());
		// Reset goes silent before migrating (AudioEngine.cpp SetSilentMode), so a failed pin leaves the
		// graph silent — fall back to the device-default format to restore an audible (resampled, unpinned)
		// graph. That fallback graph is never recorded as pinned: kPinnedFormatValid below stays false unless
		// the live rate really is kiMasteringSampleRate. DirectXTK reports failure two ways: false for a
		// missing/busy device, a throw for other failures (e.g. the pinned channel count exceeding the
		// device's); guard both at this trust boundary.
		bool bPinned = false;
		try
		{
			bPinned = mpAudioEngine->Reset(&mPinnedOutputFormat, pcSelectedDeviceIdentifier);
		}
		catch (const std::exception&)
		{
			LOG(kAudio, kWarning, "  Pinned mastering format Reset threw");
		}
		if (!bPinned)
		{
			// The 48 kHz contract this subsystem documents cannot be honored on this device; every source
			// voice pays SRC from here on, so surface it loudly rather than degrading silently.
			LOG(kAudio, kError, "  Mastering-rate pin failed; falling back to device-default format (audio resampled, not pinned)");
			DEBUG_BREAK();
			try
			{
				mpAudioEngine->Reset(nullptr, pcSelectedDeviceIdentifier);
			}
			catch (const std::exception&)
			{
				LOG(kAudio, kWarning, "  Device-default format Reset also failed; audio stays silent until the device recovers");
			}
		}
	}

	// The pin/fallback Resets may have left the graph silent (both failed). Only touch the live interface —
	// debug config, mastering diagnostics, channel cache, pinned-format validity — when one exists.
	if (!mpAudioEngine->IsAudioDevicePresent())
	{
		LOG(kAudio, kWarning, "  Audio graph went silent during mastering-rate pin; no live format available");
		return;
	}

	IXAudio2* pIXAudio2 = mpAudioEngine->GetInterface();
	XAUDIO2_DEBUG_CONFIGURATION debugConfiguration
	{
		.TraceMask = XAUDIO2_LOG_ERRORS | XAUDIO2_LOG_WARNINGS,
		.BreakMask = XAUDIO2_LOG_ERRORS,
		.LogThreadID = TRUE,
		.LogFileline = TRUE,
		.LogFunctionName = TRUE,
		.LogTiming = FALSE,
	};
	pIXAudio2->SetDebugConfiguration(&debugConfiguration);

	IXAudio2MasteringVoice* pMasteringVoice = mpAudioEngine->GetMasterVoice();
	if (pMasteringVoice != nullptr)
	{
		XAUDIO2_VOICE_DETAILS voiceDetails {};
		pMasteringVoice->GetVoiceDetails(&voiceDetails);

		char pcHex[20] {};
		LOG(kAudio, kInfo, "    Audio engine: channels {} channel mask {} rate {}", mpAudioEngine->GetOutputChannels(), common::ToHex(std::span(pcHex), mpAudioEngine->GetChannelMask()), mpAudioEngine->GetOutputSampleRate());
		LOG(kAudio, kInfo, "    Output format: channels {} channel mask {} format {}", mpAudioEngine->GetOutputFormat().Format.nChannels, common::ToHex(std::span(pcHex), mpAudioEngine->GetOutputFormat().dwChannelMask), mpAudioEngine->GetOutputFormat().Format.wFormatTag);
		LOG(kAudio, kInfo, "    MasteringVoice: channels {} sample rate {}", voiceDetails.InputChannels, voiceDetails.InputSampleRate);
	}

	CacheMasteringVoiceChannels();

	// Pinned only when the live graph really runs at the mastering rate — the native-48 kHz skip above or a
	// successful pin. A device-default fallback graph stays unpinned, so the device-loss path in Update will
	// not re-apply mPinnedOutputFormat from it.
	mFlags.Set(AudioManagerFlags::kPinnedFormatValid, mpAudioEngine->GetOutputSampleRate() == kiMasteringSampleRate);
}

void AudioManager::InitializeAudioSubsystems(const wchar_t* pcSelectedDeviceIdentifier)
{
	// Live-graph configuration (skips itself in silent mode); runs before ownership attach so its pin Reset
	// is not observed as a notification.
	ConfigureLiveGraph(pcSelectedDeviceIdentifier);

	// One-time ownership attach — exactly once against the stable engine, even when silent. RegisterNotify
	// deliberately follows ConfigureLiveGraph so the initial pin attempt cannot deliver an OnReset here.
	mpAudioEngine->RegisterNotify(this, false);
	mpStaticVoices->Initialize(mpAudioEngine.get(), &miMasteringVoiceChannels);
	mpStreamingVoices->mpAudioEngine = mpAudioEngine.get();
}

AudioManager::AudioManager()
: mpStaticVoices(std::make_unique<StaticVoices>())
, mpStreamingVoices(std::make_unique<StreamingVoices>())
{
	ASSERT(gpAudioManager == nullptr);

	gpAudioManager = this;

	LOG(kAudio, kInfo, "\nAudioManager");

	try
	{
		std::wstring selectedDeviceIdentifier = InitializeAudioEndpoint();

		if (mpAudioEngine != nullptr)
		{
			InitializeAudioSubsystems(selectedDeviceIdentifier.empty() ? nullptr : selectedDeviceIdentifier.c_str());
		}
	}
	catch ([[maybe_unused]] const std::exception& rException)
	{
		LOG(kDefault, kError, "Failed to create AudioManager: {}", rException.what());
		return;
	}
}

AudioManager::~AudioManager()
{
	if (mpAudioEngine != nullptr)
	{
		mpAudioEngine->UnregisterNotify(this, false, false);
	}

	ClearVoices(false);

	if (mpAudioEngine != nullptr)
	{
		mpAudioEngine->Update();
	}

	if (gpAudioManager == this)
	{
		gpAudioManager = nullptr;
	}
}



void AudioManager::ClearVoices(bool bNullVoicesBeforeDestroy)
{
	mpStaticVoices->Clear(bNullVoicesBeforeDestroy);
	mpStreamingVoices->Clear(bNullVoicesBeforeDestroy);
}

void AudioManager::Suspend()
{
	if (mpAudioEngine == nullptr)
	{
		return;
	}

	mbSuspended.store(true, std::memory_order_release);
	mpStreamingVoices->CancelPendingReads();

	mpAudioEngine->Suspend();

	// Processing thread is now stopped — DestroyVoice returns instantly
	LOG(kAudio, kInfo, "Suspend: destroying {} static voices, {} streams", std::ssize(mpStaticVoices->mVoices), mpStreamingVoices->GetStreamCount());
	ClearVoices(false);
}

void AudioManager::Resume()
{
	if (mpAudioEngine == nullptr)
	{
		return;
	}

	mbSuspended.store(false, std::memory_order_release);
	mpAudioEngine->Resume();
	LOG(kAudio, kInfo, "Resume");
}

void AudioManager::PlayMusic(common::crc_t uiAudioCrc)
{
	if (mbSuspended.load(std::memory_order_acquire))
	{
		return;
	}
	mpStreamingVoices->Play(uiAudioCrc);
}

void AudioManager::PlayOneShot(const game::Frame& rFrame, common::crc_t uiAudioCrc, bool b3d, float fVolume, float fPitch, float fPitchRange)
{
	if (mbSuspended.load(std::memory_order_acquire))
	{
		return;
	}
	mpStaticVoices->PlayOneShot(rFrame, uiAudioCrc, b3d, fVolume, fPitch, fPitchRange);
}

void XM_CALLCONV AudioManager::PlayOneShot3d(const game::Frame& rFrame, common::crc_t uiAudioCrc, GridCoord emitterCoordinate, FXMVECTOR vecLocalPosition, float fVolume, float fPitch, float fPitchRange)
{
	if (mbSuspended.load(std::memory_order_acquire))
	{
		return;
	}
	mpStaticVoices->PlayOneShotThreeDimensional(rFrame, uiAudioCrc, emitterCoordinate, vecLocalPosition, fVolume, fPitch, fPitchRange);
}

void AudioManager::FinishDeviceReset()
{
	// A successful DirectXTK Reset synchronously calls OnReset. This explicit clear owns that reset; consume
	// its deferred request so it cannot clear a newly restarted track next frame. A genuinely later callback
	// store after this exchange remains armed for the next Update. Harmless when Reset failed (nothing set it).
	mbClearVoicesRequested.exchange(false, std::memory_order_acquire);

	CacheMasteringVoiceChannels();

	// After Reset() is called, all XAudio2SourceVoices are destroyed internally to AudioEngine and their pointers must be set to nullptr
	ClearVoices(true);
}

void AudioManager::AttemptSilentEngineRecovery()
{
	// The engine is silent with no format known to be pinned: no device at construction, a startup pin that
	// left it silent, or a device-loss pin failure whose stored channel count is now stale. In every case the
	// current endpoint's channel count is unknown, so reset to the OS default to bring up a live graph and
	// learn it, then derive + pin the 48 kHz format.
	if (!(mFlags & AudioManagerFlags::kSilentRecoveryLogged))
	{
		LOG(kAudio, kWarning, "Audio device absent; probing for a device to recover");
		mFlags.Set(AudioManagerFlags::kSilentRecoveryLogged);
	}

	bool bDefaultReset = false;
	mpStreamingVoices->CancelPendingReads();
	try
	{
		bDefaultReset = mpAudioEngine->Reset(nullptr, nullptr);
	}
	catch (const std::exception&)
	{
		// Still silent; the throttled retry re-enters next interval.
	}

	if (!bDefaultReset || !mpAudioEngine->IsAudioDevicePresent())
	{
		return;
	}

	// A live graph exists now. Derive the pinned format from the discovered native channels and pin to
	// 48 kHz — unless the device is already native 48 kHz (mirror startup's skip to avoid a needless graph
	// teardown/rebuild). A failed pin goes silent, so restore the device-default graph on false/throw; that
	// restored graph is audible but unpinned and is not recorded as pinned below.
	mPinnedOutputFormat = MakePinnedOutputFormat(mpAudioEngine->GetOutputChannels());
	if (mpAudioEngine->GetOutputSampleRate() != kiMasteringSampleRate)
	{
		// The default Reset above brought the graph live, so these pin/fallback Resets run against a live
		// xaudio2 — DirectXTK's Reset synchronously delivers the migration OnCriticalError (SetSilentMode).
		// Suppress it across this window (main-thread synchronous, so only the expected notification arrives)
		// so recovery is not logged as a false critical error with a redundant voice clear.
		mFlags.Set(AudioManagerFlags::kExpectedResetInProgress);
		bool bPinned = false;
		try
		{
			bPinned = mpAudioEngine->Reset(&mPinnedOutputFormat, nullptr);
		}
		catch (const std::exception&)
		{
			LOG(kAudio, kWarning, "Pinned mastering format Reset threw during silent recovery");
		}
		if (!bPinned)
		{
			// Same contract loss as the startup pin failure: the recovered graph will resample every source
			// voice, so report it loudly instead of recovering silently into an unpinned graph.
			LOG(kAudio, kError, "Mastering-rate pin failed during silent recovery; restoring device-default graph (audio resampled, not pinned)");
			DEBUG_BREAK();
			try
			{
				mpAudioEngine->Reset(nullptr, nullptr);
			}
			catch (const std::exception&)
			{
				LOG(kAudio, kWarning, "Device-default Reset failed after pin failure during silent recovery; staying silent");
			}
		}
		mFlags.Set(AudioManagerFlags::kExpectedResetInProgress, false);
	}

	// Only claim recovery — re-armed warning, voice reset — when the graph is live, and claim the pin only
	// when that live graph runs at the mastering rate. A device-default fallback graph is audible but
	// unpinned, so Update's device-loss path must not re-apply mPinnedOutputFormat from it.
	if (mpAudioEngine->IsAudioDevicePresent())
	{
		mFlags.Set(AudioManagerFlags::kPinnedFormatValid, mpAudioEngine->GetOutputSampleRate() == kiMasteringSampleRate);
		mFlags.Set(AudioManagerFlags::kSilentRecoveryLogged, false);
		LOG(kAudio, kInfo, "Audio device recovered; live graph established");
		FinishDeviceReset();
	}
}

void AudioManager::Update(const game::Frame* pFrame)
{
	ScopedCpuProfile scopedCpuProfile(kCpuTimerAudio);

	// Heap: Voice map emplace/erase, make_unique<StreamingVoice> for track transitions, and
	// XAudio2 internal allocations (AllocateVoice, Update). Not controllable or pre-allocatable.
	ScopedSuppressAllocationTracking suppress;

	if (mbSuspended.load(std::memory_order_acquire))
	{
		return;
	}

	if (mbClearVoicesRequested.exchange(false, std::memory_order_acquire))
	{
		ClearVoices(true);
	}

	if (mpAudioEngine != nullptr && !mpAudioEngine->IsAudioDevicePresent()) [[unlikely]]
	{
		if (mFlags & AudioManagerFlags::kPinnedFormatValid)
		{
			LOG(kAudio, kWarning, "Music streaming: Audio device not present, resetting audio engine");

			// Re-apply the pinned 48 kHz format (native channels captured at startup) so the mastering voice keeps the
			// SRC bypass; Reset(nullptr) would otherwise recreate it at the new default device's rate. DirectXTK reports
			// failure two ways: Reset returns false for a missing/busy device, but THROWS for any other failure — including
			// CreateMasteringVoice failing when the pinned channel count exceeds the new device's (e.g. a 7.1 -> stereo
			// hot-swap). Guard both at this trust boundary (the ctor guards Reset the same way). The stale channel count
			// is what a hot-swap invalidates, so a failure drops the pin claim and leaves the engine silent: the else
			// branch below then probes the new endpoint, rediscovers its channel count, and re-pins from it.
			bool bMasteringReset = false;
			mpStreamingVoices->CancelPendingReads();
			try
			{
				bMasteringReset = mpAudioEngine->Reset(&mPinnedOutputFormat, nullptr);
			}
			catch (const std::exception&)
			{
				LOG(kAudio, kWarning, "Pinned mastering format Reset threw (device incompatible with the pinned channel count)");
			}
			if (!bMasteringReset)
			{
				LOG(kAudio, kWarning, "Pinned mastering format Reset failed; staying silent until the device probe re-pins");
			}

			// A failed Reset leaves masterRate zero (AudioEngine.cpp), so this records the pin only on success.
			mFlags.Set(AudioManagerFlags::kPinnedFormatValid, mpAudioEngine->IsAudioDevicePresent() && mpAudioEngine->GetOutputSampleRate() == kiMasteringSampleRate);

			FinishDeviceReset();
		}
		else
		{
			// No graph is known to be pinned (kPinnedFormatValid false — a silent start, a startup or recovery
			// fallback graph running at the device rate, or a device-loss pin failure that invalidated the stored
			// channel count), so the pinned-first path above cannot run: the probe below resets to the OS default
			// to rediscover the endpoint's channels and re-pin. Throttle it so a permanently-deviceless machine does not
			// warn + full-reset every frame (a stable silent engine makes this branch true every frame).
			if (++miSilentRecoveryFrameCounter >= kiSilentRecoveryRetryFrames)
			{
				miSilentRecoveryFrameCounter = 0;
				AttemptSilentEngineRecovery();
			}
		}
	}

	if (mpAudioEngine == nullptr || !mpAudioEngine->IsAudioDevicePresent()) [[unlikely]]
	{
		return;
	}

	float fDeltaTime = common::NanosecondsToFloatSeconds<float>(mRealTime.GetDeltaNs(true));

#if defined(BT_DEBUG)
	if (AudioStreamingFixture* pFixture = gpAttachedAudioStreamingFixture.load(std::memory_order_acquire); pFixture == nullptr || !(pFixture->mFlags & AudioStreamingFixture::Flags::kMode))
	{
		mpStreamingVoices->CheckTrackTransition();
	}
#else
	mpStreamingVoices->CheckTrackTransition();
#endif
	mpStreamingVoices->Update(std::chrono::duration<float>(fDeltaTime));

	if (pFrame != nullptr)
	{
		// Listener position must update first — UpdateLifecycle's priority/cull pass
		// reads mVecListenerPosition and mfEffectiveFadeEnd computed here.
		mpStaticVoices->UpdateListenerPosition();
		// pFrame is the client cell's render frame (Main.cpp), so every persistent-sound position it carries is
		// local to that cell.
		mpStaticVoices->UpdateLifecycle(*pFrame, game::gpGame->mClientGridCoordinate, fDeltaTime);
	}

	mpStaticVoices->UpdateVolumes();

	if constexpr (kbProfiling)
	{
		gpProfileManager->GetCpuCounter(kCpuCounterSounds).iCount = std::ssize(mpStaticVoices->mVoices);
	}
	if constexpr (kbProfiling)
	{
		gpProfileManager->GetCpuCounter(kCpuCounterStreams).iCount = mpStreamingVoices->GetStreamCount();
	}

	mpAudioEngine->Update();
}

void AudioManager::OnCriticalError()
{
	// Silent-recovery's pin/fallback Reset synchronously fires this migration notification (SetSilentMode) on
	// the main thread while the graph is intentionally being torn down and rebuilt. It is expected there, not a
	// real critical error, so skip the false error log and the redundant voice clear (recovery clears voices via
	// FinishDeviceReset on success).
	if (mFlags & AudioManagerFlags::kExpectedResetInProgress)
	{
		return;
	}

	LOG(kDefault, kError, "AudioManager::OnCriticalError()");

	// Destroy voices immediately — XAudio2 callback thread is no longer running
	ClearVoices(false);
}

void AudioManager::OnReset()
{
	LOG(kAudio, kDebug, "AudioManager::OnReset()");
	mbClearVoicesRequested.store(true, std::memory_order_release);
}

void AudioManager::OnDestroyEngine() noexcept
{
	LOG(kAudio, kDebug, "AudioManager::OnDestroyEngine()");
	mbClearVoicesRequested.store(true, std::memory_order_release);
}

void AudioManager::OnTrim()
{
	LOG(kAudio, kDebug, "AudioManager::OnTrim()");
}

void AudioManager::OnDestroyParent() noexcept
{
	LOG(kAudio, kDebug, "AudioManager::OnDestroyParent()");
}

} // namespace engine

#endif // defined(BT_CLIENT)
