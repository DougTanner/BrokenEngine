<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T18:23:07.233Z","dependsOn":[]} -->
# Express RunGit argument appending with C++23 append_range

## Context

Inventory F078 covers C++23 range container operations. The actionable clarity improvement is confined to `toolcli::RunGit` in `Tools/ToolCommon/ToolCliCommon.cpp:197-204`, verified at baseline `6c6245f4b1dee82d7721ef271ce4925ca754ff47`. Its fresh `std::vector<std::wstring> arguments { L"git.exe" };` currently receives the complete const-reference input through `arguments.insert(arguments.end(), rArguments.begin(), rArguments.end());`. The insertion result is unused. Naming this whole-range append directly removes three iterator expressions without adding an abstraction. This is a local clarity refactor, not a defect or a performance claim.

The C++23 proposal [P1206R7, Containers range constructors and methods, page 5](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2022/p1206r7.pdf) introduces `append_range` and explicitly preserves source order. Both consuming projects already select `stdcpp23` and compile this shared source: `Tools/AgentHarness/Platforms/VisualStudio2026/AgentHarness.vcxproj:11,31,35` and `Tools/WorktreeCli/Platforms/VisualStudio2026/WorktreeCli.vcxproj:15,48,52`.

## Design

The author's recommendation is to replace the one insertion statement with `arguments.append_range(rArguments);`. Preserve the executable prefix, const-reference parameter, process options, call to `RunProcess`, exit-code handling, and return path verbatim.

The source is a const lvalue vector and the destination is freshly created locally, so they cannot overlap. The range elements remain const references and are copied immediately into owned destination strings. Both forms produce the executable prefix followed by every source element in order; neither retains a view or moves strings out of the caller. Empty input appends nothing, and duplicates and empty strings retain their existing meaning. Keep the existing standard allocator and bulk append operation; introduce no temporary owning container, per-element loop, reserve cleanup, or allocation strategy. No measured speedup, exact allocation count, or generated-code equivalence is required or claimed.

Existing `Documents/C++StyleGuide.txt` rule 53 suffices: it addresses repeated `push_back`/`emplace_back` calls, which neither expression uses. No new global adoption rule or substantive AGENTS.md update is recommended for this one local expression. Run the normal documentation synchronization review after implementation and record that the owning ToolCommon constraints remain accurate.

## Critical files

- `Tools/ToolCommon/ToolCliCommon.cpp` — `toolcli::RunGit`, single insertion statement.
- `Tools/ToolCommon/AGENTS.md` — governing shared-tool contracts; review only.
- `Documents/C++StyleGuide.txt`, rule 53 — existing allocation guidance; review only.
- Both consuming `.vcxproj` files cited above — build coverage evidence; no edits.

## In scope

Only the `arguments.insert(...)` expression in `toolcli::RunGit`, changed to `arguments.append_range(rArguments)`.

## Out of scope

Other range-container APIs, `std::ranges::to`, `std::from_range`, move adaptation, `ServerSimulationFixtures` (separate F104 work), other call sites, new helpers, global modernization guidance, headers, public signatures, compiler options, project membership, process behavior, and allocation tuning. No unit tests or benchmark project.

## Risk and invariants

Recommended future Change Workflow classification: Tier 1, because this is a local behavior-preserving substitution with no public signature or invariant exposure. The vector handed to `RunProcess` must retain the same executable, argument order, contents, ownership, and unmodified source. Preserve process launch options and error propagation. Simulation determinism/CRC, serialization, pack versions, save/replay and wire formats, affinity, threading, and trust boundaries are untouched. The shared source requires both AgentTools targets to compile; it introduces no build/bootstrap coordination changes.

## Acceptance criteria

| Criterion | Decisive evidence |
|---|---|
| Whole-source append is expressed directly | The source diff changes only the insertion expression in `RunGit` to `arguments.append_range(rArguments);`. |
| Ownership and process behavior remain equivalent | Review confirms fresh non-aliasing destination, unchanged const source type, unchanged prefix and all code after the append; const elements copy in source order with empty input a no-op. |
| Both compiled consumers accept the C++23 member | Successful AgentHarness and WorktreeCli `Release|x64` builds through `/compile`. |
| Scope remains local | No other source, headers, project files, or substantive policy edits. Existing rule 53 and ToolCommon AGENTS.md remain accurate. |

## Verification

Inspect the one-line diff and the complete `RunGit` body against the acceptance table. Use the normal C++ affected-site, correctness, style, comment, and documentation checks at the applicable Tier-1 workflow steps. Build AgentHarness and WorktreeCli `Release|x64` through `/compile`; the eventual source change follows `/finalize-changes` for AgentTools promotion. No gameplay/runtime scenario is required because argument construction equivalence is settled by the unchanged types and local expression; no `/agent-harness` scenario is included. Do not add unit tests.

## Notes

No dependency or mandatory coordination is needed: F104's fixture move-append work is in a different function and file. Searches of live Plans for `RunGit`, `append_range`, and `ToolCliCommon` found no existing owner for this implementation boundary. This Plan contains the evidence needed for execution and does not rely on temporary review receipts or a feature inventory document.
