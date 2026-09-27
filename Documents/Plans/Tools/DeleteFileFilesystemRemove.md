<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-27T19:34:50.983Z","dependsOn":[]} -->
# Cleanup: Tools — replace DeleteFileW with std::filesystem::remove (style guide rule 35)

## Context
Style guide rule 35 requires the `std::filesystem` library for file system
work. The whole-file scanner sweep
(`Documents/Plans/Engine/StyleGuideScannerRuleSweepCommonDataPackerTools.md`)
left every Win32 file API call in `Tools/` as a residual, because a swap
changes error handling (the `/code-style-review` worker step 12 bound). The
`::DeleteFileW` calls have a `std::filesystem::remove(path, std::error_code&)`
form that keeps their outcomes, provided each site maps the two differences:
`remove` returns `false` with the error code cleared when the file does not
exist (where `DeleteFileW` fails with `ERROR_FILE_NOT_FOUND`), and it reports
failure through the `std::error_code` (whose `value()` is the Win32 error on
MSVC) instead of `GetLastError`.

Sites (final-tree lines), by how the result is used:

- Failure reported through `FailWindows` (`Tools/ToolCommon/ToolCliCommon.cpp:234`,
  which reads `GetLastError`):
  `Tools/AgentHarness/HarnessLockCommands.cpp:229` and
  `Tools/WorktreeCli/LandingLockCommands.cpp:242` (lock release)
- Not-found tolerated, any other error reported with its code:
  `Tools/WorktreeCli/BuildCommand.cpp:646` (tolerates
  `ERROR_FILE_NOT_FOUND` and `ERROR_PATH_NOT_FOUND`)
- Result only tested for failure:
  `Tools/WorktreeCli/PlanScheduler.cpp:205,335,931,1113`
- Result ignored (best-effort cleanup):
  `Tools/ToolCommon/CoordinationStore.cpp:88,334,347` and
  `Tools/WorktreeCli/PlanScheduler.cpp:1080`

## Design
The author's recommendation:
1. At every site, call `std::filesystem::remove` with the same (extended-length
   where it is today) path and a local `std::error_code`.
2. Treat `remove` returning `false` as the `DeleteFileW` failure at the sites
   that test for failure, so a missing file stays a failure exactly as today.
3. Add a `FailWindows(std::string_view operation, DWORD uiError)` overload in
   `ToolCliCommon` that formats the existing message from the given code, and
   make the current `FailWindows(operation)` call it with `GetLastError()`. The
   two lock-release sites pass `ec.value()`, or `ERROR_FILE_NOT_FOUND` when the
   code is clear, so the printed message is unchanged.
4. At `BuildCommand.cpp:646`, tolerate a clear code and the two not-found codes
   and report any other `ec.value()` in the existing message.

Rationale: the call sites keep their current success, failure and message
behavior; the one shared overload serves the two sites that need a code other
than `GetLastError`.

## Critical files
- `Tools/AgentHarness/HarnessLockCommands.cpp`
- `Tools/WorktreeCli/LandingLockCommands.cpp`
- `Tools/WorktreeCli/BuildCommand.cpp`
- `Tools/WorktreeCli/PlanScheduler.cpp`
- `Tools/ToolCommon/CoordinationStore.cpp`
- `Tools/ToolCommon/ToolCliCommon.h`, `Tools/ToolCommon/ToolCliCommon.cpp`

## In scope
- The eleven `::DeleteFileW` calls listed in `## Context` and the failure
  handling immediately around each
- The `FailWindows` overload in `ToolCliCommon.h`/`.cpp`

## Out of scope
- The `GetFileAttributesW` residuals, not planned because `std::filesystem`
  has no query for a generic reparse point or for the hidden and temporary
  attributes: `DataPacker/Source/Attribution.cpp:26,36`,
  `DataPacker/Source/FileManager.cpp:87,93,264,441`,
  `Tools/WorktreeCli/PlanScheduler.cpp:200`
- Every other Win32 call, and any change to lock, claim or build-object
  semantics
- Rule 18 and 32 residuals in the same files (their own follow-up Plan)

## Risk tier and invariants
Tier 3 (invariant/integration): trigger is build/bootstrap coordination that
can block other sessions — the landing lock, the harness lock, Plan claim
deletion and the build driver's object invalidation all run through these
calls, and WorktreeCli and AgentHarness ship as the prebuilt AgentTools. Every
exit code and printed message must stay identical, including the missing-file
cases.

## Acceptance criteria
- `/compile` passes for WorktreeCli and AgentHarness (Release)
- An `/agent-harness` session that acquires and releases the harness lock
  completes with unchanged output
- `pwsh -NoProfile -File .agents/scripts/Test-PlanSchedulerState.ps1` reports
  `status: valid`

## Notes
Originating record: the scanner sweep's rule 35 residuals.
