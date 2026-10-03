<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T18:23:46.398Z","dependsOn":[]} -->
# Mark the unreachable texture-size fallback with std::unreachable

## Context

C++23 inventory F074 is `std::unreachable()`. In `Common/TextureFormat.cpp:6`, `common::SizeInBytes` maps supported Vulkan formats to byte counts. Its default branch at lines 40-42 executes `ASSERT(false)` followed by `return 4 * iPixels;`. That dead return misleadingly resembles support for unknown formats as four-byte pixels. The accepted improvement is clarity, with no claimed performance gain.

The exact control-flow proof is local: `Common/ErrorUtils.h:20` defines `ASSERT` unconditionally, including in Release; `ASSERT(false)` passes false to `common::Assert`. In `Common/ErrorUtils.cpp:14-21`, that false branch logs, performs the configured debug break, and throws `std::runtime_error`. If logging or exception construction itself throws, control also exits exceptionally. There is no normal return to the fallback. This proof does not assume that every producer supplies a supported format.

[WG21 P0627R6, proposed utility.unreachable wording](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2021/p0627r6.pdf) specifies a noreturn function whose execution is undefined behavior; the guaranteed preceding throw is essential. [Microsoft's C++ conformance table](https://learn.microsoft.com/en-gb/cpp/overview/visual-cpp-language-conformance?view=msvc-170) records implementation support for P0627R6. `Common/ExternalHeaders.h:113` already includes `<utility>` and the repository targets Visual Studio 2026/C++23.

## Design

The author recommends replacing only the default branch's `return 4 * iPixels;` with `std::unreachable();`, retaining `ASSERT(false);` immediately before it. All supported cases and arithmetic stay byte-for-byte unchanged, including the legitimate four-byte-format return earlier in the switch.

The marker expresses that execution cannot continue after the throwing assertion. It does not validate a format or replace a check. Both old and new continuations are unreachable under the existing assertion contract, so supported results and unsupported-format diagnostics/exception behavior remain equivalent. No new include, helper, comment, policy, or attribute on the conditionally returning `common::Assert` is needed.

## Critical files

- `Common/TextureFormat.cpp`, `common::SizeInBytes`: sole implementation edit.
- `Common/ErrorUtils.h`, `ASSERT`, and `Common/ErrorUtils.cpp`, `common::Assert`: read-only proof of unconditional failure and throw behavior.
- `Common/ExternalHeaders.h`: read-only `<utility>` availability.
- `Common/AGENTS.md`, Headers, Validation, and Platform; `Documents/C++StyleGuide.txt` rule 59: existing validation and switch-formatting contracts suffice; neither needs an edit or duplicated adoption guidance.
- `DataPacker/Platforms/VisualStudio2026/DataPacker.vcxproj`, `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj`, and `BrokenEngineSandboxServer.vcxproj` in that same directory: existing direct compilation owners for `Common/TextureFormat.cpp`; membership stays unchanged.

## In scope

Only the unreachable fallback statement following `ASSERT(false)` in `common::SizeInBytes` in `Common/TextureFormat.cpp`.

## Out of scope

Changing assertion macros/helpers, error reporting, format validation, supported format cases, arithmetic, `ComputeImageByteSize`, signatures, includes, project membership, or other default branches. No repository-wide migration, compatibility wrapper, documentation/style-guide edit, unit test, runtime benchmark, or optimization claim.

## Risk and invariants

Recommended future Change Workflow classification: Tier 1, a local behavior-preserving substitution with no public signature or invariant change. Common is compiled by several targets, but no independently owned integration contract changes.

Preserve unconditional `ASSERT(false)` and its non-returning false path, including the existing diagnostic/break/throw sequence and `_Analysis_assume_` tail. Recheck this proof at implementation time; a returning or elided assertion invalidates this plan's equivalence and requires resolution before applying it. Do not use `std::unreachable()` as validation.

All supported format byte counts remain identical. The marker adds no executed work, allocations, resource lifetime changes, or threading changes. CRC/determinism, serialization, `.pack` versioning, wire protocol, and replay behavior have no changed computation or representation.

## Acceptance criteria

1. The implementation diff replaces exactly the default fallback return in `SizeInBytes` with `std::unreachable()` and preserves the preceding assertion and every supported return.
2. Inspection of the current macro and helper proves that the exact assertion at this site cannot return normally in any build configuration; unsupported formats retain their diagnostic/exception path.
3. All three direct compilation owners build successfully through `/compile`; `<utility>` resolves the C++23 symbol without a compatibility shim or additional includes.
4. Review confirms no public contract, format arithmetic, allocation, lifetime, serialization, or deterministic-state change, and existing documentation/style contracts still accurately describe the result.

## Verification

Review the one-line source diff against criterion 1; expand `ASSERT(false)` using the actual `Common/ErrorUtils.h` definition and follow `common::Assert(false, ...)` through `Common/ErrorUtils.cpp` for criterion 2. Retain that proof in the implementation acceptance evidence rather than inferring safety from the switch's list of cases.

Use `/compile` for DataPacker Release|x64 and BrokenEngineSandbox client/server Debug|x64 and Release|x64, following the skill's configuration and runtime-data rules. These builds exercise direct source ownership and symbol availability; the macro/helper inspection proves the unreachable path. Complete the standard triggered C++ review, affected-code, and documentation checks without expanding the one-line scope. No unit tests or `/agent-harness` run are needed because no reachable runtime behavior changes.

## Notes

This plan is self-contained and does not rely on an inventory audit or temporary review receipt. A live-Plan search for `SizeInBytes`, `TextureFormat`, `std::unreachable`, and post-assert/fallback-return outcomes found no duplicate. No directional prerequisites or mandatory coordination constraints were identified.
