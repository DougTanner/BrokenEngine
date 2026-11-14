<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:41:56.672Z","dependsOn":[]} -->
# Forward-declare the audio harness rig in AgentCommandServer.h

## Context

Found by a deep analysis of `Engine/Source/Agent` (architecture phase,
dependency lens), verified by code reading against commit `4fa6d2d7`; nothing
was built. Line numbers are from that commit. Paths are under
`Engine/Source/Agent/` unless they start at `Engine/`.

`AgentCommandServer.h:3-5` includes `Agent/Commands/AudioStreamingHarnessRig.h`
(229 lines), and `:48` initializes the member in class with
`std::make_unique<AudioStreamingHarnessRig>()`. The header reaches both
precompiled headers through `Engine/Source/Engine.h:20`. Every other
translation unit that names a rig type includes the rig header itself; only
`AgentCommandServer.cpp` (uses at `:92` and `:100-102`) relies on the
aggregated include. The constructor and destructor are already out of line.

Unmet criterion: `Engine/Source/AGENTS.md` Internals bullet (`:46-53`) — heavy
private state in a header `Engine.h` aggregates sits behind a
`std::unique_ptr` and a forward declaration, its definition header included
only by the owner's translation units.

Low priority. The same bullet says this is "worth it when the internals'
header changes often", and the rig header has three commits (2026-10-08, -09,
-15) and none since. Against that, the cost the bullet weighs is already
paid, because the member is already a `std::unique_ptr`. The gain is that
client Debug precompiled headers stop parsing the rig header and stop
rebuilding when it changes.

## Design

1. `AgentCommandServer.h`: replace the include at `:3-5` with a forward
   declaration of `AudioStreamingHarnessRig` under the same guard, and drop
   the in-class initializer at `:48`.
2. `AgentCommandServer.cpp`: include
   `Agent/Commands/AudioStreamingHarnessRig.h` under the same guard and
   construct the member in the constructor, before the `Attach` call at `:92`.

Risk tier: Tier 2 (the author's classification). Trigger: an include is
removed from a header `Engine.h` aggregates, which changes what every client
Debug translation unit sees and is more than Tier 1's "no public signature
exposure" allows. No runtime behavior changes: the rig is still constructed
before `Attach` and destroyed where it is today. No threading, determinism,
or serialization exposure.

## Critical files

- `Engine/Source/Agent/AgentCommandServer.h`
- `Engine/Source/Agent/AgentCommandServer.cpp`

## In scope

- The rig include and the `mpAudioStreamingHarnessRig` declaration in
  `AgentCommandServer.h`.
- The include list and the constructor of `AgentCommandServer.cpp`.
- A direct rig include in any translation unit the client Debug build shows
  was relying on the aggregated one.

## Out of scope

- `AudioStreamingHarnessRig` itself and its `Attach`, `Shutdown`, and `Detach`
  order.
- `ListenerLoop`, `Drain`, and the rest of `AgentCommandServer`.
- The Internals bullet text.

## Acceptance criteria

1. Code read: `AgentCommandServer.h` names the rig only through a forward
   declaration.
2. `/compile` builds the BrokenEngineSandbox client in Debug and Release, the
   server, and ModelViewer.
