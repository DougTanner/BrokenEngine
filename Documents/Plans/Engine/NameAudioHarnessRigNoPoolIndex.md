<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T23:20:59.837Z","dependsOn":[]} -->
# Name the audio harness rig's "no pool index" value

## Context

Found by a deep analysis of `Engine/Source/Agent`, verified by code reading
against commit `4fa6d2d7`; nothing was built. Line numbers are from the
commit that added this Plan; locate by symbol if they moved. `Rig.h` and
`Rig.cpp` below are `Engine/Source/Agent/Commands/AudioStreamingHarnessRig.h`
and `.cpp`.

The rig marks "no pool index" with a value that has no name. The literal
`4'294'967'295i64` is written at ten sites: `Rig.h:84`, `:115`, `:223`;
`Rig.cpp:217`, `:503`, `:513`, `:533`;
`Engine/Source/Agent/Commands/AgentCommandsAudioStreaming.cpp:142`, `:192`,
`:347`. Three producers pass `std::numeric_limits<uint32_t>::max()`, the same
number, to `AudioStreamingHarnessRig::Record` for the same meaning
(`Engine/Source/Audio/StreamingVoices.cpp:168`,
`Engine/Source/File/PackChunkLoader.cpp:94`, `:243`).

A change to one of the thirteen sites compiles clean and makes the rig
mistake a held index or report a pool index that does not exist. The damage
is limited to Debug harness output.

The rig's value is its own, not File's "no entry" value
(`PackChunks::kiInvalidAudioReadEntry`, `Engine/Source/File/PackChunks.h:84`,
and the `ChunkReadRequest::miEntryIndex` initializer,
`Engine/Source/File/FileManager.h:131`). The two are equal numbers, and the
File value never reaches the rig: the two `Record` calls that pass
`miEntryIndex` (`Engine/Source/File/PackChunks.cpp:622`, `:711`) run only
after the request is known to hold an entry (`:596`, `:686-689`). File's
value belongs to `Documents/Plans/Engine/DeclareAudioReadIdentityOnce.md`.

Unmet criterion: the rig's "no pool index" value has one name that its
producers and consumers use.

## Design

One named `constexpr int64_t` in `Rig.h`, holding `4'294'967'295`, used at
the ten sites and passed by the three producers, which already include
`Rig.h` (`StreamingVoices.cpp:8`, `PackChunkLoader.cpp:8`).

Risk tier: Tier 3 (the author's classification). Trigger: a change spanning
independently owned subsystems. The constant is added to `Rig.h` (Agent) and
the edit replaces arguments in File and Audio; each of the three directories
has its own `AGENTS.md`. The producers already include `Rig.h` and call
`Record`, so no new dependency is added. No runtime behavior changes in any
build: every edited line swaps a literal for a name of the same value, and
every edited line sits under `BT_CLIENT` and `BT_DEBUG`, so there is no
determinism, wire, serialization, or threading exposure. The
BrokenEngineSandbox client Debug build compiles all thirteen sites. The
ModelViewer Debug build compiles the three `Rig.h` sites and the two
`PackChunkLoader.cpp` sites, the latter in discarded
`if constexpr (kbAudioStreamingHarnessRig)` branches that never run; it does
not compile `Rig.cpp`, `AgentCommandsAudioStreaming.cpp`, or
`StreamingVoices.cpp`.

## Critical files

- `Engine/Source/Agent/Commands/AudioStreamingHarnessRig.h`
- `Engine/Source/Agent/Commands/AudioStreamingHarnessRig.cpp`
- `Engine/Source/Agent/Commands/AgentCommandsAudioStreaming.cpp`
- `Engine/Source/File/PackChunkLoader.cpp`
- `Engine/Source/Audio/StreamingVoices.cpp`

## In scope

- One new constant in `Rig.h`.
- The ten `4'294'967'295i64` sites and the three
  `std::numeric_limits<uint32_t>::max()` arguments to
  `AudioStreamingHarnessRig::Record` listed in `## Context`.

## Out of scope

- `PackChunks::kiInvalidAudioReadEntry` and the
  `ChunkReadRequest::miEntryIndex` initializer:
  `Documents/Plans/Engine/DeclareAudioReadIdentityOnce.md` owns them. This
  Plan edits no line that Plan names, and neither needs the other first.
- `PackChunks.h`, `PackChunks.cpp`, and `FileManager.h`: this Plan changes
  none of them.
- The `static_assert` lines in `Rig.cpp` that tie `AudioChunkReadState` to
  `AudioStreamingHarnessRigQueueState`.
- `AudioStreamingHarnessRigHoldOwner`, the `poolEntries` shape, and any other
  rig behavior or JSON field.
- The action dispatch in `CommandAudioStreamingHarnessRig`.

## Acceptance criteria

1. Code read: `Rig.h` declares one `constexpr int64_t` equal to
   `4'294'967'295`; each of the ten former literal sites names it, so no bare
   `4'294'967'295i64` remains in the three Agent files outside that
   declaration; the three producers pass it to `Record`; the change edits no
   other line. Together these show the value at every site is unchanged.
2. `/compile` builds the BrokenEngineSandbox client in Debug and ModelViewer
   in Debug. All thirteen sites sit under `BT_CLIENT` and `BT_DEBUG`: the
   first build compiles every site, and the second compiles the three `Rig.h`
   sites and the two `PackChunkLoader.cpp` sites.

## Coordination

- `Documents/Plans/Engine/AssignAudioHarnessRigSnapshotOnce.md` rewrites the
  action chain of `CommandAudioStreamingHarnessRig` around the sentinel site
  at `AgentCommandsAudioStreaming.cpp:347`. Neither Plan needs the other
  first; whichever lands second applies its edit to that function as the
  other left it.
