<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:27:56.775Z","dependsOn":[]} -->
# Reclaim inactive persistent records from the static voice budget

## Context

The accepted finding `CAI/shard-0011/002` identifies a persistent-audio
capacity leak. `PriorityPass` rejects a new ID when
`mVoices.size() - miFadeOutCount` reaches `kiMaxStaticVoices`
(`Engine/Source/Audio/StaticVoices.cpp:393-414`). A voice that fades out is
marked `kInactive` and its XAudio2 pointer is returned to the pool, but its
`mVoices` record remains (`StaticVoices.cpp:448-494`). `InvalidationPass` keeps
that record while its owner ID is still present, so after 128 live owners have
become inactive, a newly audible ID is deferred forever even though no active
voice is using the slots. This contradicts the attenuation-priority and
hysteresis contract in `Engine/Source/Audio/AGENTS.md`.

The source shard's frozen/live target hashes match audit baseline
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`; current status contains only the
six pre-existing `Documents/Plans/Agents/Cpp*Audit.md` edits. The admission
failure is therefore pre-existing, unresolved, and outside the approved audit
work.

## Design

The author's recommendation is to reclaim one valid `kInactive` record from
`mVoices` when a new candidate has crossed the activation threshold and the
primary cap would otherwise reject it. Erase the record using the existing
swap-and-pop lifecycle, then admit the candidate through the current
`AcquireOrLoadVoice` path. Keep active/fading records, the separate fade-out
pool, the existing priority order, and same-ID reactivation behavior unchanged.
Using the existing vector order for the inactive-record scan avoids inventing a
second ranking policy for records that hold no source voice.

## Critical files

- `Engine/Source/Audio/StaticVoices.cpp:189-251,393-494` — invalidation, priority, deactivation, and inactive lifecycle.
- `Engine/Source/Audio/StaticVoices.h:22-35,54-100` — capacity and flag semantics.
- `Engine/Source/Audio/AGENTS.md` — attenuation, hysteresis, and presentation-only contracts (read-only authority).

## In scope

- The new-ID admission and inactive-record lifecycle in `StaticVoices.cpp`.
- Reclaiming a valid inactive record before the primary-cap rejection, without
  changing fade-out accounting or same-ID reacquisition.
- Any local bookkeeping needed so the reclaimed record cannot be invalidated or
  destroyed a second time.

## Out of scope

- Changing `kiMaxStaticVoices`, the fade-out pool size, attenuation thresholds,
  priority ordering, or XAudio2 pool behavior.
- Global mute culling, corrupt audio-header handling, device reset, streaming
  voices, and all deterministic Frame/CRC state.
- New compatibility modes, runtime toggles, or unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 2: this is scoped client audio presentation
behavior, outside simulation CRC, wire/serialization, and trust-boundary
handling.

Preserve these invariants:

- A candidate at or above `kfCullVolume` can obtain a primary slot when an
  inactive record is available.
- Active voices still win by attenuated priority, and fade-out transitions
  remain graceful.
- An inactive record's XAudio2 pointer is null before its storage is reclaimed,
  and the same ID can still reactivate through the existing path.
- Audio remains presentation-only and replay ticks remain skipped.

## Acceptance criteria

- A deliberate run with 128 persistent owners that become inactive followed by
  a new audible owner produces a source voice for the new owner instead of
  repeating `Max static voices reached` indefinitely.
- Reacquiring an old owner during its inactive window still follows the
  existing fade-in path, and active/fading priority behavior is unchanged.
- Client `Debug|x64` builds clean through `/compile`.

## Notes

The consolidated index records no duplicate-family hint for this candidate.
