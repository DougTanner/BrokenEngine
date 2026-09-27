<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-27T16:16:42.845Z","dependsOn":[]} -->
# Fix: Report RunExecutable launch failure without exceptions

## Context

`Documents/C++StyleGuide.txt:68` (rule 9) reserves exceptions for fatal
errors. `common::RunExecutable` (`Common/WindowsUtils.cpp:142-221`) reports
every process-launch failure (`CreatePipe`, `SetHandleInformation`,
`InitializeProcThreadAttributeList`, `UpdateProcThreadAttribute`,
`CreateProcessW`) through `VERIFY_SUCCESS`, which throws
`std::runtime_error` in every configuration (`Common/ErrorUtils.h:22`,
`Common/ErrorUtils.cpp:14-22`). DataPacker's `FileManager.cpp` recovers two of
those throws as normal control flow:

- `RunGit` (`DataPacker/Source/FileManager.cpp:53-75`) catches at :61 and
  returns `std::nullopt`.
- `DiscoverLinkedWorktreeIdentity` catches at :218 around
  `git worktree list` (:214-224), calls `rReject()`, logs that output linking
  is disabled, and returns `std::nullopt`.

That is a rule 9 finding under
`.agents/skills/repo-code-review/references/checks.md` `### Style guide contracts`.
Both functions already return an optional, but the throw comes from a Common
callee with no non-throwing variant, so the rule sweep Plan that found it left
it out of scope.

The only other caller, `ExportShader::RunVulkanTool`
(`DataPacker/Source/ExportJobs/ExportShader.cpp:106-131`), does not catch;
its throw becomes a job failure through the documented DataPacker per-job
failure channel (`DataPacker/Source/AGENTS.md`).

## Design

Author's recommendation: change `RunExecutable` to return
`std::optional<ExecutableResult>`, `std::nullopt` when the child process
could not be started, in place of each launch `VERIFY_SUCCESS`. The existing
RAII handle guards already clean up on early return. Then:

- `RunGit` and `DiscoverLinkedWorktreeIdentity` drop their try/catch and take
  their existing `std::nullopt` / reject paths when the result is empty,
  keeping today's log text.
- `ExportShader::RunVulkanTool` throws its existing `std::runtime_error` with
  the tool name when the result is empty, so the job-failure channel is
  unchanged.
- Update the `RunExecutable` declaration comment in `Common/WindowsUtils.h`
  to state the empty result.

This keeps one function and one format (no parallel throwing variant).

## Critical files

- `Common/WindowsUtils.h:13-21`, `Common/WindowsUtils.cpp:142-221` —
  `ExecutableResult` and `RunExecutable`.
- `DataPacker/Source/FileManager.cpp:53-75`, `:195-238` — `RunGit` and
  `DiscoverLinkedWorktreeIdentity`.
- `DataPacker/Source/ExportJobs/ExportShader.cpp:106-131` —
  `RunVulkanTool`.

## In scope

- `common::RunExecutable` signature, launch-failure return, its header
  comment, and the body's RAII comment (`Common/WindowsUtils.cpp:144-145`),
  which names throwing `VERIFY_SUCCESS` unwinding as the reason for the guards
  and must name early return instead.
- The try/catch in `RunGit` and in `DiscoverLinkedWorktreeIdentity`'s
  `git worktree list` call, replaced by an empty-result check.
- The one `RunExecutable` call in `ExportShader::RunVulkanTool`, adapted to
  the optional result with unchanged failure behavior.

## Out of scope

- `common::RunExecutableInNewConsole` and its caller
  (`DataPacker/Source/ExportJobs/Island/BakeRoute.cpp:289`); none of its
  throws is caught.
- Output reading, exit-code handling, the handle-inheritance whitelist, and
  every other Common function.
- Worktree output-linking policy, reject behavior and log text.
- Every other rule 9 site, and every other style guide rule.

## Risk tier and invariants

Change Workflow Tier 2. Trigger: scoped offline tool behavior (DataPacker
external-process launch failure). `RunExecutable`'s only callers are in
DataPacker, but `Common/WindowsUtils.cpp` is also compiled into the
`BrokenEngineSandbox` client and `BrokenEngineSandboxServer` projects, so the
changed signature builds into all three executables. No
determinism/CRC, wire, `.pack`/manifest format, exporter version, replay,
threading or trust-boundary surface changes.

Invariants:

- A successful launch returns the same output and exit code as today.
- A launch failure in shader export still fails that job with an error naming
  the tool.
- A launch failure during worktree discovery still rejects and disables
  output linking with the same log line.
- No first-party throw is caught in `FileManager.cpp`'s Git calls.

## Acceptance criteria

- `rg -n "catch" DataPacker/Source/FileManager.cpp` shows no catch around a
  `RunExecutable` call.
- DataPacker Debug and Release build through `/compile`.
- Client and Server Debug build through `/compile`. A tracked change under
  `DataPacker/**` makes Local data mode mandatory, so this criterion needs one
  Local generation build through `/compile`, authorized by this approved Plan
  (`.agents/skills/compile/references/runtime-data-mode.md` `## Mode selection`).

## Notes

Found by the rule 9 candidate sweep of
`Documents/Plans/Engine/StyleGuideRepoReviewRuleSweep.md` at baseline
`46241779`; the sites are pre-existing there.
