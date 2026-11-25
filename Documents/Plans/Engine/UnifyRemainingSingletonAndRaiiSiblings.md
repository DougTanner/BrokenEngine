<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-25T00:02:01.145Z","dependsOn":["Documents/Plans/Engine/RemoveFlightPlanes.md"]} -->
# Unify Remaining Singleton and Graphics RAII Siblings

## Context

Across `Common/`, `Engine/`, `DataPacker/`, and
`Projects/BrokenEngineSandbox/`, 31 of 33 `gp*` global managers and services
inherit `common::Singleton<T>` (`Common/Singleton.h`), which enforces
non-copyability, asserts against duplicate construction, publishes the global
pointer, and clears it on destruction. Two engine singletons still wire their
`gp*` globals manually in `Engine/Source/Main.cpp`:

- `engine::AgentCommandServer` (`Engine/Source/Agent/AgentCommandServer.h`,
  `Engine/Source/Main.cpp`) manually deletes copy operations, assigns
  `gpAgentCommandServer = pAgentCommandServer.get()` after construction, and
  clears it with a `common::ScopedLambda` — directly beside `AgentUiRegistry`
  and `AgentInput`, which both inherit `common::Singleton<T>`.
- `engine::Input` (`Engine/Source/Input/Input.h`, `Engine/Source/Main.cpp`)
  assigns `gpInput = pInput.get()` manually, remains copyable, has no
  double-construction assertion, and never clears `gpInput` on destruction.

Separately, in `Engine/Source/Graphics/Objects/`, sibling Vulkan RAII wrappers
(`Buffer`, `CommandBuffers`, `Shader`, `Texture`) all explicitly delete both
copy construction and copy assignment, while `Pipeline` (`Pipeline.h`) deletes
only copy construction and omits `Pipeline& operator=(const Pipeline&) =
delete;`.

Bringing these stragglers onto the established sibling patterns removes
legacy-vs-new template hazards for future engine services and RAII wrappers.

## Design

1. **`AgentCommandServer` inherits `common::Singleton<AgentCommandServer>`**:
   - Inherit `public common::Singleton<AgentCommandServer>` in
     `Engine/Source/Agent/AgentCommandServer.h` and remove the now-redundant
     manual `AgentCommandServer(const AgentCommandServer&) = delete;` and
     `operator=` declarations.
   - In `AgentCommandServer::AgentCommandServer(int64_t iPort)`
     (`Engine/Source/Agent/AgentCommandServer.cpp`), initialize
     `common::Singleton<AgentCommandServer>(gpAgentCommandServer, nullptr)` so
     a throwing bind or listen never publishes a half-constructed global, then
     call `Register(this);` at the end of the constructor body once startup
     succeeds.
   - In `Engine/Source/Main.cpp`, remove `common::ScopedLambda
     clearAgentCommandServer` and the manual `gpAgentCommandServer =
     pAgentCommandServer.get();` assignment. Destruction order remains safe:
     `pGame`, `pAgentInput`, and `pAgentUiRegistry` are destroyed before
     `pAgentCommandServer`, `~AgentCommandServer()` stops and joins the
     listener thread, and `~Singleton()` then clears `gpAgentCommandServer`.

2. **`Input` inherits `common::Singleton<Input>`**:
   - Inherit `public common::Singleton<Input>` in
     `Engine/Source/Input/Input.h` and declare `Input();`.
   - Define `Input::Input() : common::Singleton<Input>(gpInput, this) {}` in
     `Engine/Source/Input/Input.cpp` so `inline Input* gpInput = nullptr;`
     remains at the bottom of `Input.h`, matching sibling manager headers.
   - In `Engine/Source/Main.cpp`, remove the manual `gpInput = pInput.get();`
     assignment. `gpInput` is now asserted null before construction, published
     on construction, and automatically cleared on destruction.

3. **`Pipeline` deletes copy assignment**:
   - Add `Pipeline& operator=(const Pipeline&) = delete;` immediately after
     `Pipeline(const Pipeline&) = delete;` in
     `Engine/Source/Graphics/Objects/Pipeline.h`.

## Critical files

- `Engine/Source/Agent/AgentCommandServer.h`
- `Engine/Source/Agent/AgentCommandServer.cpp`
- `Engine/Source/Input/Input.h`
- `Engine/Source/Input/Input.cpp`
- `Engine/Source/Main.cpp`
- `Engine/Source/Graphics/Objects/Pipeline.h`

## In scope

- Converting `engine::AgentCommandServer` and `engine::Input` to inherit
  `common::Singleton<T>` and removing their manual `gp*` registration and
  `ScopedLambda` cleanup in `Engine/Source/Main.cpp`.
- Adding `Pipeline& operator=(const Pipeline&) = delete;` to
  `Engine/Source/Graphics/Objects/Pipeline.h`.

## Out of scope

- Changing `common::Singleton<T>` semantics or touching any of the 31 classes
  that already inherit it.
- Changing `AgentCommandServer` socket, thread, or deferred-response behavior,
  or `Input` polling behavior.
- Adding or restructuring `AGENTS.md` files in leaf directories whose contracts
  are already owned by parent hubs.

## Risk triggers and invariants

- **Risk tier**: Tier 1 (mechanical, behavior-preserving local cleanup with no
  protocol, serialization, CRC, threading, or trust-boundary change).
- **Invariants**:
  - `AgentCommandServer` must not publish `gpAgentCommandServer` before its
    socket bind/listen succeeds, so a failed bind in `Main.cpp` leaves
    `gpAgentCommandServer == nullptr`.
  - `gpAgentCommandServer` and `gpInput` must be `nullptr` before construction
    and after destruction, with no double-registration.
  - Both `BT_CLIENT` and `BT_SERVER` builds must compile cleanly (`Input` and
    `Pipeline` are `BT_CLIENT`-only; `AgentCommandServer` compiles on both
    builds).

## Acceptance criteria

- Every `gp*` global manager/service class across `Common/`, `Engine/`,
  `DataPacker/`, and `Projects/BrokenEngineSandbox/` inherits
  `common::Singleton<T>`, with no manual `gpAgentCommandServer =` or
  `gpInput =` assignments remaining in `Engine/Source/Main.cpp`.
- `Engine/Source/Graphics/Objects/Pipeline.h` explicitly deletes both copy
  construction and copy assignment, matching `Buffer`, `CommandBuffers`,
  `Shader`, and `Texture`.
- BrokenEngineSandbox client (Debug and Release) and server (Debug and
  Release) compile cleanly through `/compile`.
