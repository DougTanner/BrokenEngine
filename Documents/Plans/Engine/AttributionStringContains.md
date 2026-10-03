<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T18:21:32.493Z","dependsOn":[]} -->
# Express attribution filename substring checks with contains

## Context

C++23 inventory item F047 is `std::basic_string::contains` and `std::basic_string_view::contains`. At baseline `6c6245f4b1dee82d7721ef271ce4925ca754ff47`, `SelectLicenseFiles` in `DataPacker/Source/Attribution.cpp:135-136` creates the existing owning `std::string filenameLower`, then checks `copying` and `readme` using `find != std::string::npos`. Both positions are immediately discarded. The requested clarity improvement expresses the Boolean intent directly; current behavior is correct and no performance improvement is claimed.

## Design

The author's recommendation is to replace the fallback condition with exactly:

```cpp
filenameLower.contains("copying") || filenameLower == "manual.md" || filenameLower.contains("readme")
```

Retain `filenameLower` and its existing lowercasing expression. Retain the exact `manual.md` comparison and operand ordering. These are substring matches anywhere in the filename, not prefixes or exact filename matches.

[WG21 P1679R3, sections 5.1-5.2](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2020/p1679r3.html) specifies string containment through a view of the original data and size, whose containment is the same find/sentinel predicate. Consequently, the two substitutions preserve case sensitivity, length-bounded haystack handling and short-circuit behavior, without another owning allocation or an escaping view. `DataPacker/Platforms/VisualStudio2026/DataPacker.vcxproj:177,213` already selects `stdcpp23`; retain the existing toolchain configuration.

## Critical files

- `DataPacker/Source/Attribution.cpp` — two predicates in `SelectLicenseFiles`.
- `DataPacker/Source/AGENTS.md` — review attribution generation's existing contract; no change expected.
- `Documents/C++StyleGuide.txt` — existing rule 41 suffices for standard qualification; rule 40 remains applicable to existing view parameters. No new adoption rule is warranted for two local substitutions.

## In scope

Only the two `filenameLower.find(...) != std::string::npos` predicates for `copying` and `readme` inside `SelectLicenseFiles`.

## Out of scope

WorktreeCli and other callers; broad string modernization; positional or offset searches; case folding, license priority or selection changes; helper extraction, caching, ownership changes, filesystem effects, generated Attribution data, export work, signatures, includes, project settings, documentation/style-rule edits, and new unit tests.

## Risk and invariants

Future implementation is Tier 1: local behavior-preserving spelling with no public signature or invariant exposure. Preserve primary-license priority and early return, fallback substring semantics, exact `manual.md` matching, file enumeration order, selected paths, allocations and lifetime, and downstream copy behavior. No simulation, determinism/CRC, serialization or pack format, replay, wire, threading, affinity or trust policy changes.

## Acceptance criteria

- Exactly the two specified Boolean predicates use `contains`; their operands and surrounding condition are otherwise unchanged.
- The owning lowercased string, primary-license loop, selection order and downstream filesystem behavior remain unchanged.
- DataPacker Release x64 compiles through `/compile` without executing data export.

## Verification

Inspect the before/after diff against the specified condition and the standard equivalence, confirming no offset or used position was discarded. Run the applicable C++ correctness, comment, style and affected-code/documentation checks; unchanged contracts need no documentation edit. Build DataPacker Release x64 through `/compile`, without asset generation or running DataPacker. No game launch, benchmark, generated-data rewrite or unit test is required. Future landing uses `/finalize-changes` after normal applicable review and verification.

## Notes

No dependencies or coordination constraints. This bounded asset-tool change can land independently of the separate WorktreeCli application of F047. No duplicate live Plan owning `SelectLicenseFiles` containment was found when authored. This Plan is self-contained and requires no adoption inventory or temporary review receipt.
