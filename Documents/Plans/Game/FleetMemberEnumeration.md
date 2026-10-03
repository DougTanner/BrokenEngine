<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T18:29:01.708Z","dependsOn":[]} -->
# Fleet Member Enumeration

## Context

C++23 inventory item F120, `std::views::enumerate`, has one demonstrated clarity application in `game::HudScreen::RenderFleetPanel`. At `Projects/BrokenEngineSandbox/Source/Ui/Screens/HudScreen.cpp:272`, the member loop declares `for (int64_t i = 0; const FleetMember& rMember : pFleet->members)` and separately increments `i` at line 314. The counter supplies ImGui identity and one-based ship labels across a substantial loop body. Coupling ordinal and element in the declaration removes that distant bookkeeping. This is a clarity refactor, with no measured speedup claim.

The pointer obtained at line 245 is `const Fleet*`; `Projects/BrokenEngineSandbox/Source/Fleet.h:43` declares its member storage as `std::vector<FleetMember>`. The loop neither mutates nor outlives that vector. The [enumerate iterator specification](https://eel.is/c++draft/range.enumerate.iterator) defines dereference as a tuple of the range difference type and range reference type, and advances the underlying iterator and ordinal together. The [adaptor specification](https://eel.is/c++draft/range.enumerate.overview) adapts its argument through `views::all`. Applied to this const lvalue vector, the element binding remains `const FleetMember&`; the per-iteration tuple does not copy the member. No allocation or additional traversal is introduced.

The configured VS 2026 toolchain selects `stdcpp23` in `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj:125,179`. Installed MSVC 14.51.36231 provides `enumerate_view` and its adaptor in `<ranges>`; its `vcruntime.h` defines x64 `ptrdiff_t` as `__int64`. Thus this vector's signed difference type preserves the existing `%lld` argument contract for `i + 1` on the supported x64 target.

Rules 8 and 15 of `Documents/C++StyleGuide.txt` currently require counter initialization in range-for and broadly prohibit range-loop `auto`; rule 38 already requires associative-container structured bindings. A narrow policy amendment is needed alongside this adoption. Existing behavior is correct; the requested improvement is removal of independent ordinal maintenance, not a defect claim.

## Design

The author's recommendation is this bounded change:

1. Replace only the outer fleet-member declaration with `for (auto [i, rMember] : std::views::enumerate(pFleet->members))` and delete its terminal `++i`. Retain every other body statement, including the inner coordinate-search loop, format strings, casts, calls, and ordering.
2. Add `<ranges>` once between `<random>` and `<ratio>` in `Common/ExternalHeaders.h` if it is absent when implementation starts. The game's `Pch.h:3` already includes that aggregation header. Other independently executable range-adoption plans may have supplied this include first; no ordering dependency is needed.
3. In style rule 8, retain `i/j/k` naming, range-for preference, and iterator naming. Recommend `std::views::enumerate` when a counter is the zero-based ordinal of every visited element; retain the C++20 initializer guidance for other counters. Include this loop as the narrow example. Explain there that element reference/constness comes from the underlying range, so read-only enumeration should use a const range. This is the owning adoption guidance, not a general ranges migration rule.
4. Replace rule 15's blanket range-loop `auto` prohibition with a prohibition excepting structured bindings for rule 8 enumeration and rule 38 associative-container iteration. Leave its other exceptions and rule 38 unchanged. Rules 17 and 18 already govern index and read-only reference use and require no edits.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Ui/Screens/HudScreen.cpp` — `HudScreen::RenderFleetPanel` outer member loop.
- `Common/ExternalHeaders.h` — standard-library include group only.
- `Documents/C++StyleGuide.txt` — rules 8 and 15 only.
- `Projects/BrokenEngineSandbox/Source/Fleet.h` and `Projects/BrokenEngineSandbox/Source/Pch.h` — read-only reference/lifetime and include evidence.

## In scope

The fleet-member range-for declaration and terminal ordinal increment in `HudScreen::RenderFleetPanel`; one conditional-on-absence `<ranges>` include; the precisely described rule 8 guidance and rule 15 exception. No other enumeration sites are included.

## Out of scope

Fleet data changes, selection or subscription behavior, network requests, labels, UI geometry, formatting casts, the inner coordinate search, new helpers or pipelines, portability shims, whole-repository modernization, toolchain upgrades, unrelated style changes, architecture documentation, and unit tests.

## Acceptance criteria

1. The outer member loop obtains ordinal and const member reference from one enumerate declaration; no separately maintained outer `i` remains.
2. Every other loop-body statement is unchanged. ImGui IDs remain zero-based; live/dead ship labels remain one-based and byte-for-byte unchanged in their format strings. Iteration and callback ordering are identical, including selection, desired-cell subscription updates, and respawn requests.
3. The vector remains borrowed for the loop lifetime with no element copies, new allocation, structural mutation, escaping reference, or second traversal. Empty-vector behavior remains an empty member list.
4. Rules 8 and 15 agree with this spelling while preserving rule 38's associative-container guidance and the default range-loop `auto` restriction.
5. `<ranges>` appears once in the central include group, and the client builds successfully in Debug and Release x64.

## Verification

- Inspect the final diff against criteria 1, 2, and 4; the bounded statement-preserving substitution makes UI equivalence checkable without runtime driving.
- Inspect the configured toolchain's enumerate dereference tuple and vector difference type to confirm `const FleetMember&` binding and signed 64-bit `%lld` compatibility. Check that the source vector stays const and live for the entire loop; this settles criterion 3 together with the referenced adaptor semantics.
- Use `/compile` for BrokenEngineSandbox client Debug|x64 and Release|x64, which checks the actual C++23 adaptor and structured binding on both configured client builds. No unit tests or benchmark are needed. Any subsequently requested live HUD verification uses `/agent-harness`.
- Complete applicable Change Workflow propagation, C++ correctness/style/comment reviews, and documentation synchronization review. No new AGENTS.md facts are proposed because the style guide owns the adoption guidance.

## Notes

Future implementation is Tier 1: local behavior-preserving spelling and its necessary style policy, with no public signature or invariant exposure. No simulation/CRC, serialization, pack or save/replay version, wire, threading, trust, allocation-policy, or client/server affinity change is proposed. The common-header edit only makes a standard header available and introduces no independently owned subsystem behavior. Dependencies: none; no mandatory cross-plan Coordination constraint. The plan is self-contained and does not require an inventory report or temporary review receipt.
