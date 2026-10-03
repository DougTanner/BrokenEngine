#pragma once

namespace audiorepair
{

// Pack-time output sample rate. Every source .wav is resampled to this so source rate == mastering
// rate holds at runtime, letting XAudio2 bypass its per-voice SRC. The engine pins its mastering
// voice to the same value — see kiMasteringSampleRate in Engine/Source/Audio/AudioUtility.h
// (kept in lockstep). Changing this requires an ExportAudio::kiVersion bump to force re-export.
inline constexpr int64_t kiAudioExportSampleRate = 48'000;

// Policy toggles. Music/ assets are loudness-mastered — flat-topping there is intentional limiting,
// so declip defaults off for them. Flipping either toggle requires an ExportAudio::kiVersion bump
// to force re-export.
inline constexpr bool kbDeclipEnabled = true;
inline constexpr bool kbDeclipMusic = false;

// Emits one LOG(kWarning) line per applied fix with relativeFile; stays silent when the file is clean.
// bLoopAsset: whole-buffer loop — skip edge fades, validate seam continuity instead.
// bAllowDeclip: reconstruct short clipped runs (else clipping is warn-only).
// bAllowEdgeFades: fade non-zero onsets/tails (music passes false — the runtime crossfade always
// ramps music in from zero and transitions out before track end, so edge defects are warn-only).
void RepairAudio(std::vector<float>& rfSamples, int64_t iChannels, int64_t iSamplesPerSecond, std::string_view relativeFile, bool bLoopAsset, bool bAllowDeclip, bool bAllowEdgeFades);

// The caller supplies one or two channels. Offline, the Kaiser-windowed sinc uses ~32 taps per output sample at unity rate for quality over speed and preserves duration; matching rates are a no-op, while conversions log one kInfo line.
void Resample(std::vector<float>& rfSamples, int64_t iChannels, int64_t iSourceRate, int64_t iTargetRate, std::string_view relativeFile);

} // namespace audiorepair
