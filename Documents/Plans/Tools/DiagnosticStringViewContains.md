<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T18:20:40.876Z","dependsOn":[]} -->
# Express build diagnostic substring checks with contains

## Context

C++23 inventory item F047 is `std::basic_string::contains` and `std::basic_string_view::contains`. At baseline `6c6245f4b1dee82d7721ef271ce4925ca754ff47`, `DiagnosticParser::ParseLine` in `Tools/WorktreeCli/BuildCommand.cpp:258,281` uses three `std::string_view::find` results solely as Boolean substring predicates. No search offset or returned position is used. The existing code is correct; the requested improvement makes that intent explicit without sentinel comparisons. No performance improvement is claimed.

## Design

The author's recommendation is exactly these replacements in `DiagnosticParser::ParseLine`:

- `line.find("error") == std::string_view::npos && line.find("warning") == std::string_view::npos` becomes `!line.contains("error") && !line.contains("warning")` in the early-return condition.
- `line.find("error") != std::string_view::npos` becomes `line.contains("error")` in unmatched-message handling.

[WG21 P1679R3, section 5.2](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2020/p1679r3.html) defines view containment as the existing find/sentinel predicate. Negation therefore preserves absence checks. Search operands and short-circuit ordering remain identical, including case sensitivity and length-bounded haystack handling. No view escapes or owning allocation is added. The project already selects `stdcpp23` in `Tools/WorktreeCli/Platforms/VisualStudio2026/WorktreeCli.vcxproj:48,52`; retain its existing toolchain configuration.

## Critical files

- `Tools/WorktreeCli/BuildCommand.cpp` — three predicates in `DiagnosticParser::ParseLine`.
- `Tools/WorktreeCli/AGENTS.md` — review its existing diagnostic/build ownership contract; no change expected.
- `Documents/C++StyleGuide.txt` — existing rules 40 and 41 suffice for view parameters and standard qualification; no new adoption rule is warranted for these three local substitutions.

## In scope

Only the three Boolean substring checks in `DiagnosticParser::ParseLine` described above, preserving surrounding expressions and formatting conventions.

## Out of scope

DataPacker and other callers; repository-wide modernization; positional or offset searches; helpers, result caching, regexes, message storage, signatures, includes, project settings, build coordination, immutable AgentTools promotion, documentation/style-rule edits, and new unit tests.

## Risk and invariants

Future implementation is Tier 1: local behavior-preserving spelling with no signature or invariant exposure. Diagnostic acceptance, suffix trimming, line-size limits, regex matching, unmatched-message caps, allocations, ownership and exception behavior remain unchanged. No simulation, determinism/CRC, serialization, replay, wire, threading, affinity or trust policy changes.

## Acceptance criteria

- The diff contains exactly the three specified predicate substitutions and no lost search offsets or consumed positions.
- All surrounding guards, operands, ordering, parsing and storage remain unchanged.
- WorktreeCli candidate compilation succeeds through the existing `/compile` route without writing the shared immutable AgentTools output.

## Verification

Inspect the before/after diff against the exact expressions above and the standard equivalence. Run the applicable C++ correctness, comment, style and affected-code/documentation checks; the unchanged contracts require no documentation edit. Use `/compile` for a WorktreeCli Release x64 candidate build. Runtime fixture changes, game driving, asset export and benchmarks provide no additional acceptance evidence for this spelling-only change and are unnecessary. Do not add unit tests. Future landing uses `/finalize-changes` after normal applicable review and verification.

## Notes

No dependencies or coordination constraints. This bounded tooling change can land independently of the separate DataPacker application of F047. No duplicate live Plan owning `DiagnosticParser::ParseLine` containment was found when authored. This Plan is self-contained and requires no adoption inventory or temporary review receipt.
