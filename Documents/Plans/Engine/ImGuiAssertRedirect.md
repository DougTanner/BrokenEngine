<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-19T22:17:16.339Z","dependsOn":[]} -->
# Route ImGui asserts through common::Assert

## Context
`ThirdParty/Prebuilts/Source/Engine/ImGui.cpp:20-21` redefines `assert` as
`common::Assert(a, #a)` (declared at `:3`) before including the imgui, backend,
and implot sources, so imgui asserts were meant to reach the engine's assert
handler. The redirect never takes effect: `imgui.cpp` includes `imgui.h`, whose
`ThirdParty/imgui/imgui.h:94-96` includes `<assert.h>` and defines
`IM_ASSERT(_EXPR)` as `assert(_EXPR)` when `IM_ASSERT` is undefined, and the
UCRT `assert.h` has no include guard: it `#undef`s `assert` and redefines it
from `NDEBUG` on every include. So every `IM_ASSERT` in the library (imgui,
the Win32 and Vulkan backends, implot) is the CRT `assert`: `_wassert` in
Debug, nothing in Profile and Release (the ThirdParty project defines `NDEBUG`
outside Debug). Engine units see the same CRT `assert` through
`Common/ExternalHeaders.h:222-225`.

`Engine/Source/Graphics/Managers/ImGuiManager.cpp:145-146` documents the
current behavior: `ImGui_ImplVulkan_Init` reports failures only through
`IM_ASSERT`s, "which compile to nothing outside Debug". Any fix changes that
statement.

## Design
Recommended (the author's choice; the claiming session confirms it with the
user at approval): define `IM_ASSERT(_EXPR)` in the shape of `ASSERT`
(`Common/ErrorUtils.h:20`) — evaluate, and only on failure call
`common::Assert` with the stringified expression — so imgui asserts behave like
engine `ASSERT`s in every configuration (log, `DEBUG_BREAK`, throw;
`Common/ErrorUtils.cpp:14-21`). Rationale: it is what the existing redirect
expresses, and the repository's own `ASSERT` never compiles out.

- `ImGui.cpp`: replace the inert `#undef assert`/`#define assert` pair with the
  `IM_ASSERT` definition, before the first include.
- `Common/ExternalHeaders.h`: define the same `IM_ASSERT` before
  `#include "imgui.h"` (`:222`). `imgui.h` templates and inline functions
  (for example `ImVector::operator[]`) expand `IM_ASSERT`, and engine units and
  the library must compile them identically, or the linker keeps an arbitrary
  one of two differing definitions.
- Constraint: `ExternalHeaders.h` precedes `Common.h` (and its `ErrorUtils.h`)
  in every `Pch.h`, so a forward declaration there must not repeat the default
  argument `ErrorUtils.h:6` declares (redefining a default argument is an
  error); the macro passes `std::source_location::current()` explicitly.
- Rewrite the `ImGuiManager.cpp:145-146` comment to the new behavior.

Alternative, not recommended: delete the inert `ImGui.cpp:19-21` lines and keep
the CRT behavior; the `ImGuiManager.cpp` comment then stays correct.

## Critical files
- `ThirdParty/Prebuilts/Source/Engine/ImGui.cpp`
- `Common/ExternalHeaders.h`
- `Engine/Source/Graphics/Managers/ImGuiManager.cpp`

## In scope
- `ImGui.cpp:19-21` (the redirect) and its forward declaration at `:3`.
- An `IM_ASSERT` definition in `Common/ExternalHeaders.h` immediately before
  the imgui includes at `:222-225`.
- The comment at `ImGuiManager.cpp:145-146`.

## Out of scope
- Every file inside `ThirdParty/imgui` and `ThirdParty/implot`, including
  `imconfig.h`.
- Fixing any imgui assert the new routing exposes; report each as a residual.
- Other ThirdParty wrappers' assert handling.

## Risk tier and invariants
Tier 3 — build/bootstrap coordination that can block other sessions: the
`ThirdParty/Prebuilts` source change feeds the primary wrapper bootstrap's
ThirdParty rebuild that every session's setup runs; the session builds and
links a ThirdParty candidate through `/compile -Target ThirdParty`. It also
changes client runtime behavior across the ThirdParty library and Engine (an
imgui assert that is silent today throws in Profile and Release). No
determinism, CRC, wire, save, or `.pack` exposure: ImGui is UI only.

## Acceptance criteria
- `/compile` ThirdParty candidate builds in Debug, Profile, and Release, then
  client and server in all three configurations and ModelViewer Debug and
  Release, all succeed.
- A temporary deliberately failing `IM_ASSERT` placed in an imgui code path the
  client reaches (removed before review) logs `Assert failed:` with the imgui
  expression in a Release client `/agent-harness` run's log, where it is
  silent today.
- An `/agent-harness` Release client and server run reaching the game screen
  logs no `Assert failed:` line.
