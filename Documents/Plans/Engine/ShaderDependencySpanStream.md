<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T18:21:31.462Z","dependsOn":[]} -->
# Borrow shader dependency input with C++23 span-backed streams

## Context

C++23 inventory F051, span-backed streams, passed the real-value review for one local input parser. `DataPacker/Source/ExportJobs/ExportShaderDependencies.cpp:186` defines `ParseWhitespaceDependencies(const std::filesystem::path&, const std::string&)`. At line 191 it constructs `std::istringstream stream(rContent)`, copying an already owned string into the stream buffer solely for `while (stream >> token)` extraction. This is an unnecessary ownership layer; replacing it removes that copy and any allocation required for the copied buffer. This offline fallback parser has no measured end-to-end performance claim, and token/path allocations remain.

`ParseDependencyFile` at line 205 owns local `content` and calls the fallback at line 217. The owner remains alive throughout parsing; no intervening code changes its storage, and the stream does not escape. The fallback is used only when canonical input-root prefix parsing does not apply. Validation still rejects tokens that do not independently name existing dependencies under an input root.

The installed MSVC 14.51.36231 `<spanstream>` contains the const borrowed-range constructor at lines 221-226, and `DataPacker/Platforms/VisualStudio2026/DataPacker.vcxproj:177,213` selects `stdcpp23`. No compatibility path or toolchain upgrade is needed. These observations are evidence for the recommendation, not a new approved binding decision.

## Design

The author's recommendation is the following two local edits:

1. Add `#include <spanstream>` adjacent to `<span>` in the standard header group of `Common/ExternalHeaders.h`. `DataPacker/Source/Pch.h` already consumes this aggregation header. Preserve unrelated header content and ordering.
2. In `ParseWhitespaceDependencies`, replace `std::istringstream stream(rContent);` with `std::ispanstream stream(rContent);`. Retain the const-reference signature and all extraction, validation, diagnostics, result construction, and ordering statements verbatim. The const lvalue string directly satisfies the borrowed-range constructor; no explicit span adapter, cast, moved string, helper, or handwritten tokenizer is needed.

Both stream types use `basic_istream` formatted string extraction, default locale, and default whitespace skipping. Their input buffers cover the string's full size, including embedded NUL characters; neither treats the input as a null-terminated C string. This function does not use seeking, putback, buffer mutation, or ownership APIs that would distinguish the buffers. Thus whitespace tokenization and downstream validation remain the same while buffer storage becomes borrowed.

Existing style rule 21 covers borrowing guidance, and rule 47 plus `Common/AGENTS.md` owns external-header placement. Those numbered rules suffice; no new style rule or AGENTS.md prose is recommended. Perform the normal documentation review against `Common/AGENTS.md`, `DataPacker/Source/AGENTS.md`, and `DataPacker/Source/ExportJobs/AGENTS.md` (`Shader Dependencies`); the contracts remain unchanged.

## Critical files

- `DataPacker/Source/ExportJobs/ExportShaderDependencies.cpp`: `ParseWhitespaceDependencies` stream declaration; `ParseDependencyFile` supplies the lifetime proof.
- `Common/ExternalHeaders.h`: standard-library include group.
- `DataPacker/Source/Pch.h` and `DataPacker/Platforms/VisualStudio2026/DataPacker.vcxproj`: verification-only PCH and language-standard evidence.

## In scope

- The one `ParseWhitespaceDependencies` local stream type substitution.
- The required `<spanstream>` aggregation include.
- Review and verification of equivalent extraction and owner-to-borrower lifetime, with no anticipated documentation edits.

## Out of scope

- Other input streams, output/bidirectional streams, tokenizer rewrites, parser signatures, validation changes, diagnostics, and dependency-root matching.
- Style-guide expansion, unrelated include sorting, new helpers, tests, benchmarks, feature gates, compatibility fallbacks, and toolchain changes.
- Shader/cache/export formats or versions, packed data generation, asset regeneration, client/server behavior, and runtime launch.

## Risk and invariants

Future implementation tier: **Tier 1**, local behavior-preserving work with no public signature or invariant exposure. Adding a standard aggregation include does not introduce a cross-subsystem behavioral change.

- `rContent` remains immutable and alive for the entire borrowed stream lifetime; no stream or view escapes the function.
- Dependency sequence, whitespace handling, empty-result behavior, invalid-token rejection, and error text remain identical.
- No new allocation or traversal is introduced; the owned stream-buffer copy is removed, while existing token/path and stream/locale costs remain.
- No simulation math, deterministic CRC state, threading, wire/trust boundary, serialization, `.pack`, export version, or replay compatibility changes occur. Input validation and exported bytes remain unchanged, so no version bump is justified.

## Acceptance criteria

1. The source diff contains only the required aggregation include and the single local stream declaration substitution; all parser statements and its signature remain unchanged.
2. Inspection confirms that `ParseDependencyFile` owns the string throughout the call and neither it nor the parser mutates/reallocates that storage during extraction.
3. DataPacker Release|x64 compiles and links successfully with the installed C++23 library, proving that the const-lvalue constructor is supported in that configuration.
4. Review confirms the same formatted extraction semantics for whitespace, empty input, embedded NUL bytes, token order, and validation rejection; no buffer-specific APIs are used.

## Verification

- Inspect the exact diff against acceptance criteria 1 and 2 and the unchanged validation/diagnostic paths.
- Build DataPacker Release|x64 through `/compile`; do not invoke asset regeneration for this type substitution.
- Settle criterion 4 by the `basic_istream` extraction contract and the full-size buffer construction. Reuse existing narrowly runnable parser verification if available, covering whitespace-separated valid and rejected entries; do not add unit tests, a test suite, or new verification infrastructure.
- Run the applicable C++ correctness, comment, style, affected-code, and documentation reviews through the Change Workflow. Complete its acceptance and finalization gates when implementing this Plan. No `/agent-harness` run is needed because there is no runtime-observable change.

## Notes

No prerequisite Plans or mandatory Coordination constraints were identified. Live-Plan searches for `ParseWhitespaceDependencies`, `ExportShaderDependencies`, `ispanstream`, `spanstream`, and span-backed-stream adoption found no duplicate at authoring time.

Durable primary references:

- [WG21 P0448R4, sections 2 and 6.4](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2021/p0448r4.pdf): externally supplied stream storage and const input support.
- [C++ draft ispanstream constructors](https://eel.is/c++draft/ispanstream.cons): borrowed-range const-view constructor.
- [Microsoft STL spanstream implementation](https://raw.githubusercontent.com/microsoft/STL/main/stl/inc/spanstream): the implementation corresponding to the installed constructor inspected above.

This Plan contains the evidence and boundaries needed to implement independently of the original C++23 inventory or temporary review receipts.
