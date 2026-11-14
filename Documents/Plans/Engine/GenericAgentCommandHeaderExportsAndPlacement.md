<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:41:33.870Z","dependsOn":[]} -->
# Move the generic agent command headers into Engine.h

## Context

Found by a deep analysis of `Engine/Source/Agent` (architecture phase,
dependency and client/server lenses), verified by code reading against commit
`4fa6d2d7`; nothing was built. Line numbers are from the commit that added
this Plan; locate by symbol if they moved. Paths are under
`Engine/Source/Agent/` unless they start at `Engine/` or `Projects/`.

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

Unmet criteria: the client generic command header exports only names another
translation unit uses; no generic agent command header is included from a
game `Pch.h` under an exception whose stated reason does not hold for it.

## Design

1. Make `UiStateName` and `GameFlagNames` `static` and delete
   `AgentCommandsClientGeneric.h:15-16`. This is the enabling step for step
   2: `Engine.h` may aggregate the client header only once it names no
   `GameBase.h` type, because each game's `Pch.h` includes `GameBase.h` after
   `Engine.h`, and those two declarations are the only ones that name one.
   The rest of the header names `std::string_view`, `nlohmann::json`, and
   `engine::Client` (`Engine/Source/Network/Client/Client.h:64`, which
   `Engine.h:66` includes).
2. Move both includes into `Engine.h`: `AgentCommandsClientGeneric.h` into
   the `BT_CLIENT` span after `Network/Client/Client.h` (`:66`), and
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

Risk tier: Tier 2 (the author's classification). Trigger: two exported
declarations are removed from a header a precompiled header aggregates, and
`Engine.h` and each game `Pch.h` change, which Tier 1's "no public signature
exposure" excludes. No runtime behavior changes and there is no determinism,
wire, serialization, or threading exposure. Both precompiled headers rebuild.

## Critical files

- `Engine/Source/Agent/AgentCommandsClientGeneric.h`, `.cpp`
- `Engine/Source/Engine.h`
- `Projects/BrokenEngineSandbox/Source/Pch.h`, and the `Pch.h` of every other
  game project that includes the two headers
- `Engine/Source/AGENTS.md` (the Aggregation bullet)

## In scope

- `UiStateName`, `GameFlagNames`: linkage and their two declarations.
- The include sites of `AgentCommandsClientGeneric.h` and
  `AgentCommandsServerGeneric.h` in `Engine.h` and in each game `Pch.h`, and
  the `Pch.h:29` comment.
- The clause "then the generic agent command headers that name its types" in
  the Aggregation bullet of `Engine/Source/AGENTS.md`.

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

## Acceptance criteria

1. Code read: `UiStateName` and `GameFlagNames` are `static` and undeclared
   in `AgentCommandsClientGeneric.h`; `Engine.h` includes both generic
   command headers in their spans; no game `Pch.h` includes either header or
   carries the `:29` comment; the Aggregation bullet no longer names the
   generic agent command headers and still records `GameBase.h`'s placement
   as the exception.
2. `/compile` builds the BrokenEngineSandbox client and server and
   ModelViewer.
3. The repository static checks pass.
