<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:28:01.582Z","dependsOn":[]} -->
# Reject corrupt lazy audio metadata before voice allocation

## Context

The accepted finding `CAI/shard-0011/003` identifies a packed-audio trust
boundary gap. `PackChunks` marks a lazy chunk ready after reading its header
without semantic wave-format validation (`Engine/Source/File/PackChunks.cpp:213-226,737-739`).
`StaticVoice::LoadXAudio2SourceVoice` then asserts only the 3D mono condition
and passes the unvalidated `WAVEFORMATEX` and `header.iSize` to XAudio2
(`Engine/Source/Audio/StaticVoice.cpp:23-56`). `StreamingVoices::CreateStream`
uses the same metadata (`Engine/Source/Audio/StreamingVoices.cpp:197-210`).
Malformed metadata can therefore throw through a simulation worker or the
main audio update instead of taking the per-chunk soft-failure path required by
`Engine/Source/File/AGENTS.md`.

The shard's frozen/live target hashes match audit baseline
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`, while current status contains only
the six pre-existing `Documents/Plans/Agents/Cpp*Audit.md` edits. The missing
validation is unresolved and pre-existing at the approved audit boundary.

## Design

The author's recommendation is to validate the audio tag, channel/rate/block
fields, and payload extent at the lazy audio publication boundary, before a
ready chunk can reach a voice allocator. A failed validation should publish
the existing classified failed/non-ready result and notify waiters without
throwing. Static and streaming callers should treat that result as their
existing skip/retry outcome; they must never call `AllocateVoice` or submit a
buffer for invalid metadata. Keep resident-chunk ownership, worker wakeups,
and valid 48 kHz playback unchanged.

## Critical files

- `Engine/Source/File/PackChunks.cpp:213-226,700-739` — lazy header/read and terminal-state publication.
- `Engine/Source/Audio/StaticVoice.cpp:23-56` — static source-voice admission.
- `Engine/Source/Audio/StreamingVoices.cpp:197-210` — music stream admission.
- `Common/DataFile.h` and `Engine/Source/File/AGENTS.md` — packed audio layout and corruption contract (read-only authority).

## In scope

- Semantic validation of lazy `AudioHeader` fields and the resident payload
  extent before ready-state publication or XAudio2 allocation.
- Non-throwing propagation of a failed optional audio chunk through both static
  and streaming voice callers.
- The exact load, state-publication, and caller regions named above.

## Out of scope

- General pack row/offset validation, path termination, cross-pack generation
  policy, or any unrelated asset type.
- DataPacker's valid-audio producer output, XAudio2 library changes, voice
  capacity policy, device reset, and replay/CRC state.
- Blanket allocation-tracking suppression around malformed input.

## Risk tier and invariants

Expected Change Workflow Tier 2 (downgraded from Tier 3). The change validates
opaque packed data and crosses background-loader, simulation-worker, and
main-thread trust boundaries.

Tier rationale: the Design fully specifies the header fields and payload extent
to check at one publication point, reusing the existing classified
failed/non-ready chunk result that both voice callers already handle. No pack
layout, threading structure, or valid-audio playback behavior changes.

Preserve these invariants:

- A malformed optional audio chunk cannot throw through a worker dispatch or
  main-loop audio update.
- Failed chunks publish completion and leave workers/waiters live; valid chunks
  retain release/acquire visibility and resident ownership.
- No source voice receives a format or byte extent outside the validated audio
  header/payload, and the 48 kHz source/mastering contract remains intact.

## Acceptance criteria

- A pack with a readable but invalid audio wave header reaches a classified
  failed/skip result in both static and streaming requests, with no exception
  escaping the worker or main update.
- A valid packed sound and music stream still become ready and play through the
  existing paths.
- Client `Debug|x64` builds clean through `/compile`; server compilation remains
  clean for the shared File sources.

## Coordination

`Documents/Plans/Engine/PackedPathTermination.md` and
`Documents/Plans/Engine/CrossPackReferenceResolution.md` also inspect
`PackChunks.cpp`, but own path-string safety and missing cross-pack references.
Keep those regions separate and re-derive line numbers from current source
before implementation; no dependency is required.

## Notes

The consolidated index records this candidate without a duplicate-family hint
or external verification request. The broader pack-extent candidate in another
shard is a separate root and is not included here.
