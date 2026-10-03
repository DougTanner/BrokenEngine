<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T18:02:17.652Z","dependsOn":[]} -->
# Spell Windows path prefixes with wide raw string literals

## Context

`Tools/ToolCommon/ToolCliCommon.cpp:355` implements `ExtendedLengthPath`, which normalizes absolute drive paths before adding the extended-length prefix. `Tools/ToolCommon/CoordinationStore.cpp:196` implements `CanonicalizeDirectoryPath`, which rejects an extended UNC final path before removing an ordinary extended-length prefix. The three prefix literals currently double every backslash, obscuring the actual Windows spellings. Wide raw string literals display those spellings directly without changing their representation or introducing runtime work.

The source evidence was inspected at baseline `d29fed456d3ede935c5e672f95f13d6733f0660c`. The complete rationale and edits are here; no investigation document is required. Existing executable Plans were searched for the target files, functions, and raw-string adoption; no duplicate owner was found.

## Design

Make exactly these three literal substitutions, keeping each enclosing expression unchanged:

| Function and expression | Existing literal | Replacement literal |
|---|---|---|
| `ExtendedLengthPath`, return expression adding the prefix to `path.lexically_normal().native()` | `L"\\\\?\\"` | `LR"(\\?\)"` |
| `CanonicalizeDirectoryPath`, first `finalPath.starts_with` (UNC rejection) | `L"\\\\?\\UNC\\"` | `LR"(\\?\UNC\)"` |
| `CanonicalizeDirectoryPath`, second `finalPath.starts_with` (prefix removal) | `L"\\\\?\\"` | `LR"(\\?\)"` |

Use the empty raw-string delimiter because neither payload contains the closing delimiter sequence. Retain the `L` encoding prefix. No helper, named constant, new include, comment, or interface is needed. No style-guide or AGENTS.md amendment is warranted: this adds no interface, invariant, or adoption policy, and the current path contracts remain accurate.

## Critical files

- `Tools/ToolCommon/ToolCliCommon.cpp` — `ExtendedLengthPath` prefix producer, currently line 368.
- `Tools/ToolCommon/CoordinationStore.cpp` — `CanonicalizeDirectoryPath` prefix consumers, currently lines 226 and 231.
- `Tools/ToolCommon/AGENTS.md` — existing normalization and UNC-rejection contracts; read-only reference.

## In scope

Only the three literal-token replacements specified in Design, inside `ExtendedLengthPath` and `CanonicalizeDirectoryPath`.

## Out of scope

- Other strings, character literals, regexes, escape sequences, and repository-wide conversion.
- Changes to normalization, drive-path eligibility, UNC diagnostics or rejection, prefix removal length, logical keys, hashing, persistence, allocation, or control flow.
- Changes to interfaces, project membership, documentation, style rules, or tool bootstrap/promotion mechanisms.
- New tests and game runtime scenarios.

## Risk and invariants

Future implementation is Tier 1: a local mechanical representation change with no public signature or invariant exposure. Although these functions serve coordination, changing only equivalent literal tokens does not change coordination behavior or lock policy.

- Both short literals remain arrays of five `const wchar_t` elements: backslash, backslash, question mark, backslash, terminating null.
- The UNC literal remains an array of nine `const wchar_t` elements: backslash, backslash, question mark, backslash, `U`, `N`, `C`, backslash, terminating null.
- Expression types, overload resolution, concatenation and comparison behavior, allocation, and runtime cost remain identical.
- Normalization still precedes prefix addition. UNC rejection still precedes prefix stripping. `erase(0, 4)` and all other expressions remain unchanged.
- Prefixed paths remain OS arguments only; logical keys retain their existing local-drive representation.

## Acceptance criteria and verification

| Criterion | Required future evidence |
|---|---|
| All three target literals show the exact Windows prefix spelling | Focused diff confirming the three replacements from Design and no other C++ edits. |
| The compiled literal values and behavior are unchanged | Review character-by-character expansion against the element sequences above, including encoding prefix, array extent, and terminating null; confirm unchanged enclosing expressions. |
| Both consumers compile with the new spelling | `/compile` for WorktreeCli Release and AgentHarness Release, the two executables compiling ToolCommon; record successful compile/link results. |
| Existing contracts and scope remain intact | Required C++ correctness, style, comment, affected-code, and documentation review under the current workflow, with no propagation or documentation edits expected. |

No unit tests or `/agent-harness` runtime scenarios are required: the diff and literal-value equivalence settle the behavior-preserving criteria, and both tool builds settle syntax and overload compatibility. No game build or asset generation is needed. If this implementation is later landed, use the normal `/finalize-changes` AgentTools promotion policy for its ToolCommon source changes. These are future requirements; no implementation builds or runtime checks were run while authoring this Plan.
