#pragma once

#if defined(BT_CLIENT)

#include "Frame/GridCoord.h"
#include "StaticVoice.h"

namespace DirectX
{
class AudioEngine;
} // namespace DirectX

namespace game
{

struct Frame;

} // namespace game

namespace engine
{

inline constexpr int64_t kiMaxStaticVoices = 128;

// FadeOutPool: bonus capacity above kiMaxStaticVoices for voices ramping out
// gracefully. Lets a budget-eviction free its primary slot to the new sound
// immediately while the displaced voice keeps playing through a fade. When at
// cap, mark attempts DEBUG_BREAK and skip; the deactivation pass retries the
// next frame once a slot frees.
inline constexpr int64_t kiMaxFadeOutPool = 128;

// Below this attenuated-volume threshold a sound is considered inaudible and is
// culled (one-shots: never spawned; persistent: voice released, entry kept).
// Hysteresis: candidacy floor is 0.8 * kfCullVolume so a sound right at the
// boundary doesn't thrash between active and inactive each frame.
inline constexpr float kfCullVolume = 0.01f;

class StaticVoices
{
public:

	void Initialize(AudioEngine* pAudioEngine, const int64_t* piMasteringVoiceChannels);

	void PlayOneShot(const game::Frame& rFrame, common::crc_t uiAudioCrc, bool bThreeDimensional, float fVolume, float fPitch = 1.0f, float fPitchRange = 0.0f);
	// vecLocalPosition is local to emitterCoordinate; the cull and mix convert it against mListenerCoordinate.
	void XM_CALLCONV PlayOneShotThreeDimensional(const game::Frame& rFrame, common::crc_t uiAudioCrc, GridCoord emitterCoordinate, FXMVECTOR vecLocalPosition, float fVolume, float fPitch = 1.0f, float fPitchRange = 0.0f);

	// rFrame is the cell emitterCoordinate names, so every persistent-sound position it carries is local to it.
	void UpdateLifecycle(const game::Frame& rFrame, GridCoord emitterCoordinate, float fDeltaTime);
	void UpdateListenerPosition();
	void UpdateVolumes();

	void Clear(bool bNullVoicesBeforeDestroy);

private:

	// Unlocked one-shot body shared by both public paths; callers hold mOneShotMutex.
	// rfPitch is in/out: the pitch-randomization result flows back so the 3D path can
	// pass the randomized ratio on to ApplyThreeDimensionalVolume.
	IXAudio2SourceVoice* PlayOneShotLocked(common::crc_t uiAudioCrc, bool bThreeDimensional, float fVolume, float& rfPitch, float fPitchRange);

	void XM_CALLCONV ApplyThreeDimensionalVolume(IXAudio2SourceVoice* pVoice, FXMVECTOR vecPosition, FXMVECTOR vecVelocity, float fVolume, float fPitch);

	float ComputeAttenuatedVolume(float fDistance, float fSoundVolume) const;

	// UpdateLifecycle passes, run in fixed order each frame.
	void InvalidationPass(const SoundsInterpolate& rSoundsInterpolate);
	void PriorityPass(const SoundsInterpolate& rSoundsInterpolate, const SoundsPostRender& rSoundsPostRender, GridCoord emitterCoordinate);
	void DeactivationPass();
	void AdvanceFadeOut(float fDeltaTime);
	void AdvanceFadeIn(float fDeltaTime);

	struct PooledVoice
	{
		common::crc_t uiAudioCrc = 0;
		IXAudio2SourceVoice* pVoice = nullptr;
	};
	struct AcquiredVoice
	{
		IXAudio2SourceVoice* pVoice = nullptr;
		bool bFromPool = false;
	};

	void ReturnVoiceToPool(common::crc_t uiAudioCrc, IXAudio2SourceVoice* pVoice);
	IXAudio2SourceVoice* AcquireVoiceFromPool(common::crc_t uiAudioCrc);
	AcquiredVoice AcquireOrLoadVoice(common::crc_t uiAudioCrc);
	void ClearPool();
	void BeginFadeOut(StaticVoice& rVoice);
	void EndFadeOut(StaticVoice& rVoice);
	void ActivateVoice(StaticVoice& rVoice, IXAudio2SourceVoice* pVoice);
	void RetireVoice(StaticVoice& rVoice);

	AudioEngine* mpAudioEngine = nullptr;
	const int64_t* mpiMasteringVoiceChannels = nullptr;

	std::mutex mOneShotMutex; // 3D path locks once and calls the unlocked PlayOneShotLocked helper (no re-entry)
	common::RandomEngine mRandomEngine;

public:
	bool mbSkipNextInvalidation = false;

	std::vector<StaticVoice> mVoices;
private:
	std::vector<PooledVoice> mPooledVoices;
	int64_t miFadeOutCount = 0; // Maintained count of mVoices entries flagged kFadingOut; kept in sync at every Set/Clear so the budget checks stay O(1). Reset in Clear().

	// UpdateListenerPosition writes listener/fade state through mfEffectiveFadeEnd on the main thread.
	// Main.cpp sequences ClientUpdate (all Dispatch workers join), Render, then AudioManager::Update.
	// Worker one-shots read this state through PlayOneShotThreeDimensional, ComputeAttenuatedVolume and ApplyThreeDimensionalVolume without locking; that ordering excludes concurrent writes.
	// The same ordering permits UpdateLifecycle, UpdateVolumes and Clear to access mVoices/mPooledVoices without mOneShotMutex.
	// mListenerCoordinate is the cell for mVecListenerPosition; cull, priority and mix rebase each emitter's local position into it.
	GridCoord mListenerCoordinate {};
	XMVECTOR mVecListenerPosition {};
	X3DAUDIO_LISTENER mX3dAudioListener
	{
		.OrientFront = {0.0f, 0.0f, -1.0f},
		.OrientTop = {0.0f, -1.0f, 0.0f},
		.Position = {0.0f, 0.0f, 0.0f},
		.Velocity = {0.0f, 0.0f, 0.0f},
	};

	float mfCurveDistanceScaler = 10.0f;
	float mfManualFadeVolume = 0.15f;

	float mfEffectiveFadeStart = 0.0f;
	float mfEffectiveFadeEnd = 150.0f;
};

} // namespace engine

#endif // BT_CLIENT
