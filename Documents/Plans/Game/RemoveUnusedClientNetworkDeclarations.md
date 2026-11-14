<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:43:12.185Z","dependsOn":[]} -->
# Remove unused declarations and includes from the client game Network files

## Context

Found by a deep analysis of `Projects/BrokenEngineSandbox/Source/Network`
(architecture phase), verified by code read against commit `4cf9d607`; nothing
was compiled. Paths are under
`Projects/BrokenEngineSandbox/Source/Network/Client/` unless they start at
`Engine/` or `Projects/`.

Two client headers declare names nothing uses, and two `.cpp` files include a
header they do not use:

- `ClientReconciler.h:10` `struct FrameInput;` — not used in the header;
  `game::FrameInput` is already complete through the PCH (`Pch.h` includes
  `GameBase.h`, which stores it by value).
- `ClientReconciler.h:12` `using engine::SnapshotIndex;` — every unqualified
  `SnapshotIndex` use in the repository is inside `namespace engine`.
- `ClientSession.h:13` `class Client;` and `:15` `struct ReceivedDebugFrame;`
  (in its `namespace engine` block) — neither is named in the header; both are
  defined in `Engine/Source/Network/Client/Client.h`, which `Engine.h`
  aggregates into the PCH.
- `ClientSession.h:24` `enum class GamePacketType : uint8_t;` — not named in
  the header; every user includes `Network/GamePacketType.h` itself.
- `ClientSession.h:33` `SubscriptionChangeReason::kPollTick` — only its
  `ToString` case (`ClientSession.cpp:31`) names it. The engine passes the
  string literal instead (`Engine/Source/GameBase.cpp:299`,
  `Engine/Source/Agent/AgentCommandsClientGeneric.cpp:1107`).
- `#include "Profile/ProfileRows.h"` at `ClientReconciler.cpp:8` and
  `ClientSession.cpp:12` — the header declares only `GameCpuCounters`,
  `GameCpuTimers`, and their two name tables, and no file under
  `Projects/BrokenEngineSandbox/Source/Network/` names any of them.
  `ReconcileReplayClientState.cpp` includes the same `ClientReconciler.h` and
  `Game.h` without it.

Every declaration deleted below sits in a game-local header, and no file
under `Engine/Source` depends on any of them.

## Design

Delete exactly these lines:

1. `ClientReconciler.h`: `struct FrameInput;` and
   `using engine::SnapshotIndex;`.
2. `ClientSession.h`: `class Client;` and `struct ReceivedDebugFrame;` (keep
   `class ClientSessionRuntime;`, which the `friend` declaration needs), the
   `GamePacketType` forward declaration, and the `kPollTick` enumerator.
3. `ClientSession.cpp`: the `kPollTick` case in `ToString`, and the
   `Profile/ProfileRows.h` include.
4. `ClientReconciler.cpp`: the `Profile/ProfileRows.h` include.

`SubscriptionChangeReason` values are passed only to `ToString`, so removing
an enumerator changes no stored or sent value.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Network/Client/ClientReconciler.h`
- `Projects/BrokenEngineSandbox/Source/Network/Client/ClientReconciler.cpp`
- `Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.h`
- `Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.cpp`

## In scope

- The five declaration lines, the `kPollTick` enumerator and its `ToString`
  case, and the two include lines named in `## Design`.

## Out of scope

- Replacing `SubscriptionChangeReason` and `ToString` with string literals:
  it reaches `Ui/Screens/HudScreen.cpp` and trades compile-checked names for
  literals, a style preference the analysis did not accept as debt.
- The engine's `"kPollTick"` literals, and the two reason strings that differ
  from their enumerator names.
- Every other include and declaration in these files, and the whole-file
  guard, which already sits directly after the first include.

## Risk tier

Tier 2 — scoped behavior. Trigger: an enumerator is removed from an enum
declared in a header that engine translation units include, which Tier 1's
"no public signature exposure" excludes. No runtime behavior changes and no
determinism, wire, serialization, or threading exposure.

## Acceptance criteria

- Code read: none of the deleted names or include lines remains in the four
  files.
- `/compile` builds the BrokenEngineSandbox client. That build also compiles
  the engine translation units that include these headers
  (`Engine/Source/GameBase.cpp`, `Network/Client/ClientSessionBase.cpp`,
  `ClientSessionRuntime.cpp`, `ClientDesyncCore.cpp`,
  `Profile/ProfileNetworkScreen.cpp`, `Agent/AgentCommandsClientGeneric.cpp`,
  `Agent/Commands/ClientDesyncProbe.cpp`), and it is the proof that the
  include and the declarations are unused.
- The repository static checks pass.
