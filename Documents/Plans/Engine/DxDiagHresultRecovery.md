<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-27T16:16:40.860Z","dependsOn":[]} -->
# Fix: Stop ReadDxDiag on HRESULT failure without exceptions

## Context

`Documents/C++StyleGuide.txt:68` (rule 9) reserves exceptions for fatal
errors. `engine::ReadDxDiag` (`Engine/Source/CrashReport.cpp:166-241`) is the
best-effort DxDiag capture that `Main.cpp:155` starts on a background thread
through `std::async`. Its body uses `CHECK_HRESULT` (:172, :175, :184, :187,
:190, :193, :198, :200) to abort the capture, and the try's
`catch (const std::exception&)` (:231) and `catch (...)` (:235) arms log
"Failed to read DxDiag" and continue to the completion store at :240.
`CHECK_HRESULT` (`Common/ErrorUtils.h:21`) throws `std::runtime_error` in
every configuration through `common::CheckHresult`
(`Common/ErrorUtils.cpp:24-33`), so a non-fatal abort is carried by an
exception: a rule 9 finding under
`.agents/skills/repo-code-review/references/checks.md` `### Style guide contracts`.
The function returns `void`, so no existing non-throwing result channel can
replace the throw; the rule sweep Plan that found it left it out of scope.

## Design

Author's recommendation: replace each `CHECK_HRESULT` in `ReadDxDiag` with a
plain HRESULT test that logs the failing call and HRESULT through
`common::HresultToString` (`Common/WindowsUtils.h:8`) and stops the capture,
keeping whatever `sDxDiag` already holds. Every exit, early or normal, must
still reach `sbDxDiagComplete.store(true, std::memory_order_release)`, so
either keep a single exit (the capture body in a local function or lambda that
returns early, followed by the one store) or register the store with the
existing `common::ScopedLambda`. Remove the `catch (...)` arm. Keep the try
with its `std::exception` arm: the `sDxDiag +=` appends and `common::ToString`
conversions (:222-225) can still throw `std::bad_alloc`, a standard-library
throw the arm continues to cover.

## Critical files

- `Engine/Source/CrashReport.cpp:166-241` — `ReadDxDiag`.
- `Engine/Source/CrashReport.cpp:8-9`, `:154-161` — `sDxDiag`,
  `sbDxDiagComplete`, and the reader that trusts the release store.
- `Engine/Source/Main.cpp:150-157` — the background launch.
- `Common/ErrorUtils.h:21`, `Common/ErrorUtils.cpp:24-33` — why
  `CHECK_HRESULT` throws.

## In scope

- The `CHECK_HRESULT` calls and the try/catch inside `ReadDxDiag`.
- The control flow that guarantees the single completion store on every exit.

## Out of scope

- `HandleException`, crash-report paths and writing, and the `Main.cpp`
  launch and join.
- The per-property best-effort calls that already ignore their HRESULT
  (:202-229).
- `CHECK_HRESULT` and the rest of Common.
- Every other rule 9 site, and every other style guide rule.

## Risk tier and invariants

Change Workflow Tier 2. Trigger: scoped runtime behavior in one subsystem
(crash-report DxDiag capture). The function runs on its own thread, but the
thread model and the `sDxDiag`/`sbDxDiagComplete` publication are unchanged,
so no threading surface changes. No determinism/CRC, wire, serialization,
replay or trust-boundary surface is touched.

Invariants:

- `sbDxDiagComplete` is stored `true` with release order exactly once, after
  the last write to `sDxDiag`, on every exit path.
- A successful capture produces the same `sDxDiag` text as today.
- No first-party throw is caught inside `ReadDxDiag`.

## Acceptance criteria

- `rg -n "CHECK_HRESULT" Engine/Source/CrashReport.cpp` returns no lines
  inside `ReadDxDiag`.
- Client and server Debug and Release builds succeed through `/compile`.

## Notes

Found by the rule 9 candidate sweep of
`Documents/Plans/Engine/StyleGuideRepoReviewRuleSweep.md` at baseline
`46241779`; the site is pre-existing there.
