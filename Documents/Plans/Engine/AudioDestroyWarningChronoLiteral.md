<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T18:05:37.011Z","dependsOn":[]} -->
# Use a chrono literal for the audio destruction warning threshold

## Context

`Engine/Source/Audio/AudioUtility.h:46-65` defines the client-only `engine::DestroyXAudio2SourceVoice`. After timing `AudioEngine::DestroyVoice` with `std::chrono::steady_clock`, it warns when `elapsed > std::chrono::milliseconds(100)`. The explicit duration construction is an isolated opportunity to use the shorter duration spelling already prescribed by `Documents/C++StyleGuide.txt` rule 12.

`Common/ExternalHeaders.h:64-65` already includes `<chrono>` and imports `std::chrono_literals` for this PCH-backed runtime code. User-defined literals are a C++11 language facility; the standard chrono literal operators used here were introduced in C++14. The repository builds as C++23, so this proposal introduces no language-version change or custom literal implementation.

## Design

In `engine::DestroyXAudio2SourceVoice`, replace only `std::chrono::milliseconds(100)` in the warning condition with `100ms`, producing `if (elapsed > 100ms)`. Both expressions produce `std::chrono::milliseconds` with an integer count of 100. The replacement adds no allocation or runtime work and leaves the comparison's duration conversion unchanged.

No include, namespace import, helper, constant, literal operator, or template is needed. No documentation or style amendment is warranted: rule 12 already directs this spelling, and the Audio subsystem contracts are unchanged.

## Critical files

- `Engine/Source/Audio/AudioUtility.h` — sole implementation edit, the warning condition in `DestroyXAudio2SourceVoice`.
- `Common/ExternalHeaders.h:64-65` — read-only evidence for the existing header and literal namespace import.
- `Documents/C++StyleGuide.txt`, rule 12 — read-only policy supporting the literal.
- `Engine/Source/Audio/AGENTS.md` — read-only audio lifecycle and presentation boundary contracts.

## In scope

Exactly one expression substitution in `engine::DestroyXAudio2SourceVoice`: change the warning comparison's right operand from `std::chrono::milliseconds(100)` to `100ms`.

## Out of scope

- Other duration expressions, repository-wide adoption, and PCH-less tools.
- Custom literal operators or templates, new declarations, imports, includes, or API changes.
- Threshold value, comparison operator, clocks, log text or severity, audio lifecycle, synchronization, and timing behavior.
- Documentation/style policy changes, new tests, asset generation, and performance benchmarking.

## Risk and invariants

Future implementation is Tier 1: a local, mechanical, behavior-preserving expression change with no public signature or invariant exposure.

- Preserve an integer 100-millisecond threshold and the strict `>` comparison, including the no-warning result at exactly 100 milliseconds.
- Preserve both `steady_clock::now()` calls, the elapsed duration type, and the logged duration cast and count.
- Preserve the null checks, stop/flush/destroy ordering, pointer clearing, and `BT_CLIENT` guard.
- Keep all audio work presentation-only and outside deterministic Frame state and CRCs.
- Add no runtime operations, allocations, or dependencies.

## Acceptance criteria and verification

| Criterion | Future verification |
|-----------|---------------------|
| The warning threshold reads `100ms` | Focused diff shows the single specified expression replacement and no other C++ edits. |
| Type, value, warning boundary, and lifecycle are unchanged | Review the integer chrono literal's `std::chrono::milliseconds` result with count 100 against the old construction; confirm the enclosing comparison and function remain identical. |
| The existing import resolves the literal in the client build | Run `/compile` for BrokenEngineSandbox Client `Debug|x64` using Shared runtime data for this source-only change; require successful compile/link. |
| Existing policy remains sufficient | Confirm rule 12 already covers the spelling and no subsystem contract changed; no documentation amendment. |

No server build is needed for this guarded client-only expression. No `/agent-harness` scenario or unit tests are needed: the focused diff and exact type/value equivalence settle behavior, while the client build checks literal lookup and compilation. No runtime measurement is required for this equivalent constant expression. These checks are future implementation requirements; none is claimed to have passed during Plan authoring.
