<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:41:56.672Z","dependsOn":[]} -->
# Agent headers in the precompiled headers

## Context

Found by a deep analysis of `Engine/Source/Agent` (architecture phase,
dependency and client/server lenses), verified by code reading against commit
`4fa6d2d7`; nothing was built. Line numbers in items 1 and 2 are from the
commit that added that finding's Plan, and in item 3 from `4fa6d2d7`; locate
by symbol if they moved. Paths are under `Engine/Source/Agent/` unless they start
at `Engine/` or `Projects/`. Both changes touch headers that `Engine.h` or a
game `Pch.h` aggregates, so one session pays one precompiled-header rebuild.

1. Unused exports in the client header. `AgentCommandsClientGeneric.h:15-16`
   declares `UiStateName` and `GameFlagNames`. Their definitions are at
   `AgentCommandsClientGeneric.cpp:1122` and `:1136`, and their only uses are
   at `:1173` and `:1175`, after the definitions. Nothing else in the
   repository names them. They are the only declarations in that header that
   name `GameBase.h` types (`UiState`, `GameFlags_t`).
2. Aggregation placement does not match its recorded exception.
   One sentence at `Engine/Source/AGENTS.md:37-39` records two things: each
   game's `Pch.h` includes `GameBase.h` right after `Engine.h`, and after
   `GameBase.h` come "the generic agent command headers that name its types";
   it calls that placement "a recorded exception, not license to add another".
   `AgentCommandsServerGeneric.h:8-28` names no `GameBase.h` type, yet
   `Projects/BrokenEngineSandbox/Source/Pch.h:33-35` includes it there and not
   in the `BT_SERVER` span of `Engine/Source/Engine.h:81-86`. After item 1 the
   client header names no `GameBase.h` type either, which also makes the
   comment at `Pch.h:29` false.
3. The audio harness rig header reaches both precompiled headers.
   `AgentCommandServer.h:3-5` includes
   `Agent/Commands/AudioStreamingHarnessRig.h` (229 lines), and `:48`
   initializes the member in class with
   `std::make_unique<AudioStreamingHarnessRig>()`. The header reaches both
   precompiled headers through `Engine/Source/Engine.h:20`. Every other
   translation unit that names a rig type includes the rig header itself; only
   `AgentCommandServer.cpp` (uses at `:92` and `:100-102`) relies on the
   aggregated include. The constructor and destructor are already out of line.

   Low priority on its own. The `Engine/Source/AGENTS.md` Internals bullet
   says this is "worth it when the internals' header changes often", and the
   rig header has three commits (2026-10-08, -09, -15) and none since. Against
   that, the cost the bullet weighs is already paid, because the member is
   already a `std::unique_ptr`. The gain is that client Debug precompiled
   headers stop parsing the rig header and stop rebuilding when it changes.

Unmet criteria: the client generic command header exports only names another
translation unit uses; no generic agent command header is included from a game
`Pch.h` under an exception whose stated reason does not hold for it; and, per
the `Engine/Source/AGENTS.md` Internals bullet (`:46-53`), heavy private state
in a header `Engine.h` aggregates sits behind a `std::unique_ptr` and a forward
declaration, its definition header included only by the owner's translation
units.

## Design

1. Make `UiStateName` and `GameFlagNames` `static` and delete
   `AgentCommandsClientGeneric.h:15-16`. This is the enabling step for step
   2: `Engine.h` may aggregate the client header only once it names no
   `GameBase.h` type, because each game's `Pch.h` includes `GameBase.h` after
   `Engine.h`, and those two declarations are the only ones that name one.
   The rest of the header names `std::string_view`, `nlohmann::json`, and
   `engine::Client` (`Engine/Source/Network/Client/Client.h:64`, which
   `Engine.h:66` includes).
2. Move both generic includes into `Engine.h`: `AgentCommandsClientGeneric.h`
   into the `BT_CLIENT` span after `Network/Client/Client.h` (`:66`), and
   `AgentCommandsServerGeneric.h` into the `BT_SERVER` span (`:81-86`).
   Delete both includes, their two guards, and the `:29` comment from each
   game `Pch.h` (`Projects/BrokenEngineSandbox/Source/Pch.h:28-35`). In the
   sentence at `Engine/Source/AGENTS.md:37-39`, delete only the clause "then
   the generic agent command headers that name its types"; the rest of the
   sentence, which records `GameBase.h`'s own placement as the exception,
   stays.

   This is the author's recommendation, because it removes the part of the
   exception that no longer has a reason instead of rewording it. Both
   headers carry their own whole-file guard, and ModelViewer, which includes
   `Engine.h`, sees client declarations that name engine types only. The
   alternative considered and not taken: keep both includes in each game
   `Pch.h` and reword the `Pch.h:29` comment and the `Engine/Source/AGENTS.md`
   clause so neither says the headers name `GameBase.h` types.
3. `AgentCommandServer.h`: replace the rig include at `:3-5` with a forward
   declaration of `AudioStreamingHarnessRig` under the same guard, and drop
   the in-class initializer at `:48`.
4. `AgentCommandServer.cpp`: include
   `Agent/Commands/AudioStreamingHarnessRig.h` under the same guard and
   construct the member in the constructor, before the `Attach` call at `:92`.

## Risk tier

Tier 2 (the authors' classification). Triggers: two exported declarations are
removed from a header a precompiled header aggregates, `Engine.h` and each game
`Pch.h` change, and an include is removed from a header `Engine.h` aggregates,
which changes what every client Debug translation unit sees; all are more than
Tier 1's "no public signature exposure" allows. No runtime behavior changes:
the rig is still constructed before `Attach` and destroyed where it is today.
No determinism, wire, serialization, or threading exposure. Both precompiled
headers rebuild.

## Critical files

- `Engine/Source/Agent/AgentCommandsClientGeneric.h`, `.cpp`
- `Engine/Source/Agent/AgentCommandServer.h`, `.cpp`
- `Engine/Source/Engine.h`
- `Projects/BrokenEngineSandbox/Source/Pch.h`, and the `Pch.h` of every other
  game project that includes the two generic headers
- `Engine/Source/AGENTS.md` (the Aggregation bullet)

## In scope

- `UiStateName`, `GameFlagNames`: linkage and their two declarations.
- The include sites of `AgentCommandsClientGeneric.h` and
  `AgentCommandsServerGeneric.h` in `Engine.h` and in each game `Pch.h`, and
  the `Pch.h:29` comment.
- The clause "then the generic agent command headers that name its types" in
  the Aggregation bullet of `Engine/Source/AGENTS.md`.
- The rig include and the `mpAudioStreamingHarnessRig` declaration in
  `AgentCommandServer.h`.
- The include list and the constructor of `AgentCommandServer.cpp`.
- A direct rig include in any translation unit the client Debug build shows
  was relying on the aggregated one.

## Out of scope

- `CommandLoad` and the other take-over exports of
  `AgentCommandsServerGeneric.h`, which `Engine/Source/Agent/AGENTS.md`
  records as deliberate.
- Every other export of the generic agent command headers, and
  `AgentCommandsShared.h`.
- Every include in `AgentCommandsClientGeneric.cpp` and
  `AgentCommandsServerGeneric.cpp` other than their own header.
- `GameBase.h`'s own placement and the part of the Aggregation bullet that
  records it.
- `AudioStreamingHarnessRig` itself and its `Attach`, `Shutdown`, and `Detach`
  order.
- `ListenerLoop`, `Drain`, and the rest of `AgentCommandServer`.
- The Internals bullet text of `Engine/Source/AGENTS.md`.

## Acceptance criteria

1. Code read: `UiStateName` and `GameFlagNames` are `static` and undeclared
   in `AgentCommandsClientGeneric.h`; `Engine.h` includes both generic
   command headers in their spans; no game `Pch.h` includes either header or
   carries the `:29` comment; the Aggregation bullet no longer names the
   generic agent command headers and still records `GameBase.h`'s placement
   as the exception.
2. Code read: `AgentCommandServer.h` names the rig only through a forward
   declaration.
3. `/compile` builds the BrokenEngineSandbox client in Debug and Release, the
   server, and ModelViewer.
4. The repository static checks pass.
