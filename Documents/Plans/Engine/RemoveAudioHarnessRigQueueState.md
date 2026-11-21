<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-20T23:51:24.091Z","dependsOn":[]} -->
# Remove the Mirrored Audio Harness Rig Queue State

## Context

`AudioStreamingHarnessRigQueueState`
(`Engine/Source/Agent/Commands/AudioStreamingHarnessRig.h:33-39`) copies
`AudioChunkReadState` (`Engine/Source/File/PackChunks.h:11-17`, inside
`#if defined(BT_CLIENT)`): the same four enumerators (`kFree`, `kQueued`,
`kLoading`, `kReady`) in the same order on the same `uint8_t` base. Keeping the
two in step costs four `static_assert`s and a converter `ToHarnessRigState`
(`AudioStreamingHarnessRig.cpp:47-54`), plus a value cast at
`Engine/Source/File/PackChunks.cpp:618`. One concept has two names.

The rig header is whole-file `BT_CLIENT && BT_DEBUG` (`:3`), already includes
`File/FileManager.h` (`:5`), and is not in `Engine.h`; the server build sees
neither type. `PackChunks.h` includes `FileManager.h` and `PackChunkLoader.h`,
neither of which includes the rig header, so including `PackChunks.h` from the
rig header forms no cycle. The rig may inspect Pack state in client Debug
builds (`Engine/Source/File/AGENTS.md` `## Packed Assets`).

Nothing serializes the enum: no wire, save, or CRC use. Its only output is
harness JSON through `AudioStreamingHarnessRigQueueStateName`
(`Engine/Source/Agent/Commands/AgentCommandsAudioStreaming.cpp:53-67`, used at
`:144`, `:159`, `:196`), whose strings stay the same.

## Design

Delete `AudioStreamingHarnessRigQueueState` and use `AudioChunkReadState`
wherever it appeared. The author recommends making the full enum visible to the
rig header with `#include "File/PackChunks.h"` rather than an opaque
`enum class AudioChunkReadState : uint8_t;` declaration: the include keeps the
`= ...::kFree` default member initializers, while an opaque declaration would
force `{}` initializers and `AgentCommandsAudioStreaming.cpp` would need the
include anyway for its `switch`.

Memory layout, atomics, and memory orders do not change; the snapshot fields
keep a one-byte enum with the same values.

## Critical files

- `Engine/Source/Agent/Commands/AudioStreamingHarnessRig.h`
- `Engine/Source/Agent/Commands/AudioStreamingHarnessRig.cpp`
- `Engine/Source/Agent/Commands/AgentCommandsAudioStreaming.cpp`
- `Engine/Source/File/PackChunks.cpp`
- `Engine/Source/File/PackChunkLoader.cpp`
- `Engine/Source/Audio/StreamingVoices.cpp`

## In scope

- `AudioStreamingHarnessRig.h`: delete the `AudioStreamingHarnessRigQueueState`
  enum; change the `eState` and `eHeldState` member types of the rig's
  snapshot, pool-entry, and record types to `AudioChunkReadState`; change the
  `eState` parameter type of `AudioStreamingHarnessRig::Record` and
  `RecordEntry`; add `#include "File/PackChunks.h"`.
- `AudioStreamingHarnessRig.cpp`: delete the four `static_assert`s and
  `ToHarnessRigState`; change the `Record` and `RecordEntry` definitions'
  parameter types; in the pool-entry snapshot loop assign
  `AudioReadState(uiOwnership)` directly and compare against
  `AudioChunkReadState::kFree`.
- `PackChunks.cpp`: pass `eState` with no cast in the `kRefillCancelled`
  `Record` call, and spell `AudioChunkReadState::` in every other `Record`
  call.
- `PackChunkLoader.cpp` and `StreamingVoices.cpp`: spell
  `AudioChunkReadState::` in each `Record` call.
- `AgentCommandsAudioStreaming.cpp`: the parameter type and `case` labels of
  `AudioStreamingHarnessRigQueueStateName`.

## Out of scope

- The other rig enums (`AudioStreamingHarnessRigPartition`,
  `AudioStreamingHarnessRigPhase`, and the hold owner and hold state enums).
- The harness JSON field names and string values, including the name of
  `AudioStreamingHarnessRigQueueStateName`.
- Pack audio-read logic and memory orders, and the `AudioChunkReadState`
  definition.

## Acceptance criteria

- A search of `Engine/` and `Projects/` for `AudioStreamingHarnessRigQueueState`
  outside the name of `AudioStreamingHarnessRigQueueStateName`, and for
  `ToHarnessRigState`, returns no hits.
- Clean `/compile` builds of the BrokenEngineSandbox client in Debug (the only
  configuration that compiles the rig) and Release, and of ModelViewer in
  Debug (it compiles `PackChunks.cpp` and `PackChunkLoader.cpp`).
- No harness check: the JSON output is unchanged by construction.

## Notes

- Change Workflow tier: Tier 2. Trigger: it changes the public static
  `AudioStreamingHarnessRig::Record` signature and the snapshot field types
  that File and Audio call sites use, so it is not Tier 1. It touches no
  determinism, wire, save, threading, or trust surface, and behavior is
  unchanged.
- Client Debug only; no `kiVersion`, `.pack`, replay, or shader exposure.
