<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:27:51.646Z","dependsOn":[]} -->
# Preserve the 48 kHz pin across audio device-reset fallback

## Context

The accepted finding `CAI/shard-0011/001` identifies a required audio contract
that the handled device-reset fallback violates. `AudioManager::Update`
first retries `Reset(&mPinnedOutputFormat, nullptr)` and, when the old channel
layout is rejected by the new endpoint, succeeds with
`Reset(nullptr, nullptr)` (`Engine/Source/Audio/AudioManager.cpp:491-512`).
DirectXTK treats the null format as a request for the device default, while
`FinishDeviceReset` only recaches channels and clears voices
(`AudioManager.cpp:384-395`). The stale `kPinnedFormatValid` state therefore
can describe a live graph whose mastering rate is not 48 kHz, even though
`Engine/Source/Audio/AGENTS.md` requires that rate to survive device resets.
The equivalent startup fallback marks the stale format valid as well
(`AudioManager.cpp:187-206,244-246`).

The source shard's frozen/live target hashes match the audit baseline
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`, and the current session has only
the six pre-existing `Documents/Plans/Agents/Cpp*Audit.md` edits. The source
evidence is therefore unchanged and the fallback remains unresolved and
outside the approved audit work.

## Design

The author's recommendation is to keep the existing `AudioEngine` instance,
but rebuild `mPinnedOutputFormat` from the newly selected endpoint whenever a
requested channel layout is rejected. Retry `Reset` with the rebuilt 48 kHz
format before accepting the graph as live. If that retry fails, keep the
engine in its existing silent/retry state instead of treating the device
default-rate reset as a successful pinned recovery. Update the valid-format
state only for a graph known to use the rebuilt 48 kHz format, and preserve
the current stale-voice clear ordering.

This recommendation covers both the device-loss branch and the startup
fallback so a successful graph has one invariant regardless of entry point.
It does not replace the stable engine or add a new audio thread.

## Critical files

- `Engine/Source/Audio/AudioManager.cpp:187-206,244-246,384-395,485-522` — startup and device-loss reset paths.
- `Engine/Source/Audio/AudioManager.h:93-95` — cached pinned-format lifetime.
- `Engine/Source/Audio/AGENTS.md` — 48 kHz and stable-engine contracts (read-only authority).

## In scope

- Recomputing the 48 kHz pinned output format for the newly selected endpoint
  when the stale channel layout is rejected.
- Retrying the pinned reset, setting `kPinnedFormatValid` only for that pinned
  graph, and retaining the existing silent/retry behavior when pinning fails.
- The startup fallback and device-loss recovery regions named above.

## Out of scope

- Replacing `AudioEngine`, changing XAudio2 voice ownership, or changing the
  fill-worker/callback synchronization contract.
- Changing DataPacker audio formats, source-voice resampling policy, or the
  user-facing audio settings.
- Any other audio-capacity or corrupt-chunk finding, including static voice
  admission and lazy wave metadata.

## Risk tier and invariants

Expected Change Workflow Tier 2 (downgraded from Tier 3). The changed code
handles an external audio endpoint and XAudio2 reset results at a trust
boundary, though audio is presentation-only and outside the Frame CRC.

Tier rationale: the fix is a fully specified local recovery ordering change
inside one subsystem's two fallback branches — rebuild the pinned format for
the new endpoint, retry, and mark the format valid only on that success — with
no format, threading, or CRC surface touched.

Preserve these invariants:

- Every accepted live graph uses a valid 48 kHz mastering format.
- A missing or incompatible endpoint leaves a usable silent/retry state rather
  than a live graph with an unintended device-rate conversion.
- The existing stable engine, voice-subsystem pointers, stale-voice clearing,
  main-thread affinity, and no-CRC audio boundary remain unchanged.

## Acceptance criteria

- Source inspection shows both startup and device-loss success paths establish
  a 48 kHz `mPinnedOutputFormat`; the null-format reset is never reported as a
  pinned success.
- Client `Debug|x64` builds clean through `/compile`.
- A controlled endpoint-loss/recovery run (or the available audio recovery
  fixture) shows recovery either at 48 kHz or in the silent/retry state, with
  stale voices cleared before playback resumes.

## Notes

The consolidated index records this as a single accepted candidate with no
duplicate-family hint or external verification dependency.
