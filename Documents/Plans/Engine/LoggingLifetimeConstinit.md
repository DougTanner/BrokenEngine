<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T18:16:38.252Z","dependsOn":[]} -->
# Enforce constant initialization of logging lifetime atomics

## Context

`Common/Log/Log.cpp:15` defines `common::giMyOutputDebugString` with value zero, and `Common/Log/Log.cpp:36` defines `common::sbLogAlive` with value false. Their constant initialization is currently an implicit property of the initializers. The comments at lines 35 and 150–151 explicitly rely on it: `LogWrite` reads the flag and uses the debug-string counter even when dynamic logging objects are unavailable. `LogAliveGuard` at lines 44–57 changes the flag around the stream lifetime. `Common/Determinism.cpp:105` reads the counter to avoid recursive logging of debug-output exceptions.

The accepted adoption opportunity is to make this existing initialization requirement compiler-enforced with two declaration specifiers. This is preventive enforcement and clarity, not a demonstrated runtime failure or optimization. `Common/Log/AGENTS.md` under `Failure and Concurrency Rules` already owns the logging lifetime contract.

## Design

Recommend these exact replacements in `Common/Log/Log.cpp`:

```cpp
constinit std::atomic<int64_t> giMyOutputDebugString = 0;
static constinit std::atomic<bool> sbLogAlive = false;
```

The [C++ constinit rules](https://eel.is/c++draft/dcl.constinit) reject dynamic initialization for an initializing declaration carrying the specifier. The [atomic constructor specification](https://eel.is/c++draft/atomics.types.generic) provides constexpr value constructors for the existing scalar initializers. The extern declaration in `Common/Log/Log.h:15` can remain unchanged: this guarantee belongs on the initializing definition.

Preserve all types, linkage, values, atomic accesses and memory orders, declaration order, `init_seg(lib)`, guard placement, and fallback branches. `constinit` adds no const qualification and no runtime operation. It does not extend object lifetimes or replace the guard. There is no new allocation, locking, branch, or other performance cost in this declaration-only change; no speedup is claimed.

## Critical files

- `Common/Log/Log.cpp`: definitions of `giMyOutputDebugString` and `sbLogAlive`; read-only lifetime context in `LogAliveGuard` and `LogWrite`.
- `Common/Log/Log.h:15`: read-only extern declaration consistency check.
- `Common/Determinism.cpp:105`: read-only debug-output marker consumer.
- `Common/Log/AGENTS.md` under `Failure and Concurrency Rules`: existing invariant owner, no text change proposed.

## In scope

Add `constinit` to only the two definitions above. Preserve their existing initializer spelling and local rationale comments.

## Out of scope

No annotations on `gLogRuntimeLevels`, other globals, or extern declarations. No changes to mutexes, streams, ring buffers, guards, logging behavior, atomic memory order, exception handling, documentation, or style policy. No new abstractions, unit tests, or project membership changes.

## Acceptance criteria

1. Source diff contains only the two `constinit` additions to the named definitions. Types, initial values and runtime statements remain identical.
2. Future `/compile` builds of BrokenEngineSandbox client, BrokenEngineSandbox server, and DataPacker succeed with the supported toolchain, proving both initializers meet the language requirement in their consuming targets.
3. Review confirms the existing lifetime guard and fallback remain intact. No simulation/CRC, serialization, pack version, replay, wire, trust boundary, or layout changes occur.

## Verification

Use declaration-level diff inspection plus the three target builds through `/compile`; follow that skill's supported configuration requirements. No runtime acceptance criterion changes, so no `/agent-harness` run or performance benchmark is needed. Run the required C++ correctness, style, comment, affected-code, and documentation reviews through the implementation workflow; no related source updates are anticipated.

## Notes

Risk: Tier 1, local behavior-preserving declaration enforcement with no public signature or runtime invariant change. Although these are atomics used at a lifetime boundary, the change does not alter threading behavior, lifetime ordering, or what the boundary computes or trusts.

Documentation/style treatment: `Documents/C++StyleGuide.txt` rule 36 already requires initialization, and rule 20's initializer syntax remains satisfied. No new general constinit rule is needed for these two existing contracts. Preserve the non-obvious lifetime rationale comments under rule 64; the existing `Common/Log/AGENTS.md` lifetime rule stays accurate, so `/update-claude-docs` should record a no-change result.

Searches of current Plans by the target names, `Log.cpp`, constinit, constant initialization, logging lifetime, and Coordination found no duplicate root cause or implementation boundary. No dependencies or mandatory Coordination constraints are required. Planning only: no builds or runtime verification have been performed.
