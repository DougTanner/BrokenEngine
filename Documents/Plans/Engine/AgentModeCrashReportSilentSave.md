<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-19T01:46:31.065Z","dependsOn":[]} -->
# Make agent-mode crash reports silent and deterministic

## Context

This is an existing-contract defect. Agent launches are intended to be
non-modal: `gLaunchOptions.iAgentPort != 0` is already the agent-mode
authority, and the startup path minimizes an agent client without activation.
`Engine/Source/CrashReport.cpp::HandleException` still unconditionally calls
`MessageBox` at line 42, so an agent-launched client or server can wait for
user input instead of saving its report. The fixed-buffer
`SetCrashReportAppDataDirectory` routing already exists in
`Engine/Source/CrashReport.cpp:8-36`, and `Engine/Source/Main.cpp:781-784`
already copies the `--app-data-directory` override before the main loop.

The current `HandleException` IDNO branch already selects the configured
AppData directory, then the existing roaming-AppData path and Desktop-child
fallback when the configured route is unavailable. It writes the existing
role-specific report, including the exception text, callstack, DxDiag, and
log buffers. `wWinMain` catches the exception, calls the handler, and returns
zero (`Engine/Source/Main.cpp:830-850`). The fix must make only agent mode
take that existing non-modal branch.

## Design

In `HandleException`, when `gLaunchOptions.iAgentPort != 0`, skip
`MessageBox` and select the existing IDNO/AppData branch. Ordinary launches
continue to show the current Yes/No prompt and keep its current Desktop-versus-
AppData behavior. Do not add a report path, filename, allocation, directory
traversal, network operation, or fallback policy.

Add one permanent Debug-only shared `crash_report_fixture` command to
`Engine/Source/Agent/AgentCommandsShared.cpp` through the existing
`ExecuteSharedAgentCommand` dispatch. It is available on both client and
server endpoints because each project dispatcher tries this engine-owned path
first. It accepts only an empty JSON params object. The build guard runs first:
outside `kbDebugInput`, the command returns the exact normal failure
`crash_report_fixture requires kbDebugInput build`. In a Debug build,
malformed or non-empty params use the normal synchronous agent command failure
and must not terminate the process; those malformed and non-Debug failures
throw through the normal `AgentCommandServer::Drain()` failure handling. In
Debug agent mode with the valid empty object it calls `HandleException()` and
then `ExitProcess(0)`, intentionally returning no response because the process
terminates. The existing Agent command socket is loopback-only; this command
does not change that boundary.

Document the shared command in the generic shared command reference. Keep the
dispatch and ownership notes limited to the existing engine-shared command
region and the engine Agent ownership documentation; do not assign the
fixture to a project game-dispatch or project Agent ownership region.

## Critical files

- `Engine/Source/CrashReport.cpp:38-115` — `HandleException`, including the
  prompt branch and the existing fixed-buffer report routes.
- `Engine/Source/Main.cpp:775-784,830-850` — existing agent-option and
  exception-catch authority; read-only confirmation of the launch and return
  contract.
- `Engine/Source/Agent/AgentCommandsShared.cpp` — the existing
  `ExecuteSharedAgentCommand` engine-shared dispatch and the fixture handler.
- `Engine/Source/Agent/AGENTS.md` — the engine-shared command ownership
  statement directly affected by the fixture.
- `.agents/skills/agent-harness/references/command-reference.md` — the
  generic shared command schema and transport-loss behavior.

## In scope

- Make `HandleException` choose its existing IDNO/AppData branch for
  `gLaunchOptions.iAgentPort != 0` while preserving the ordinary prompt,
  fixed-buffer path construction, fallback order, filenames, and report
  contents.
- Add the permanent Debug-only shared client/server `crash_report_fixture`
  dispatch to `ExecuteSharedAgentCommand` with empty-object parameter
  validation, the exact non-Debug failure, and the deliberate Debug
  `HandleException()`/`ExitProcess(0)` sequence.
- Update only the generic shared command reference and the directly affected
  engine Agent ownership documentation; project game-dispatch ownership is
  out of scope.
- Verify the static prompt/branch contract, Debug client/server builds, and
  the deterministic client/server fixture acceptance below.
- No new C++ public signature and no new source file.

## Out of scope

- New crash-report paths, filenames, allocations, directory traversal,
  networking, or fallback behavior.
- Any change to ordinary-launch prompting, configured AppData routing,
  roaming fallback, Desktop-child fallback, report contents, or client/server
  report naming.
- Harness process detection, report baselining, intentional-stop marking,
  polling, or release integration; those belong to
  `Documents/Plans/Agents/AgentHarnessCrashReportDetection.md`.
- Simulation, CRC, replay, save, wire/protocol, input, UI, or gameplay state.
- Unit tests or a second crash trigger/fixture.

## Risk tier and invariants

Expected future Change Workflow Tier 3: the implementation crosses the shared
client/server crash path and places a deliberately process-terminating command
at the loopback JSON trust boundary.

Preserve these invariants:

- `HandleException` remains fixed-buffer/no-new-allocation on the crash path.
- Ordinary launches retain the current Yes/No prompt and both branches.
- Configured AppData, roaming fallback, Desktop-child fallback, report
  contents, and client/server filenames remain unchanged.
- A malformed fixture request never calls `HandleException`; the valid Debug
  request intentionally produces transport loss, exit code zero, and no
  response.
- No simulation, CRC, replay, save, wire, or deterministic state changes.

## Coordination

`AgentHarnessCrashReportDetection.md` depends on this Plan and must consume
the fixture's exact transport-loss/exit behavior and the existing report
identities. It must detect and retain that evidence without adding another
crash path, changing the fixture, or replacing the current exact-PID release
ownership.

## Acceptance criteria

- Debug client and server builds pass through the repository build workflow.
- For each endpoint, launch with a fresh configured AppData root. A malformed
  fixture request fails normally without process exit. The valid empty-object
  fixture then causes transport loss, the exact retained PID to exit within a
  bounded interval with exit code `0`, and no modal stall.
- Each endpoint produces a new complete role-specific report through the
  existing configured-AppData route, with the existing fallback candidates
  unchanged. The report contains `Unknown exception`, a callstack section,
  DxDiag, and log sections.
- Static/build evidence proves the non-Debug branch returns the exact
  `crash_report_fixture requires kbDebugInput build` failure and that ordinary
  launches still retain the current Yes/No prompt behavior.
- Static ownership evidence shows every fixture reference uses the engine
  `ExecuteSharedAgentCommand` path and generic shared command reference, with
  no project game-dispatch ownership.
- No unit tests are added.

## Notes

The fixture passes no exception object so the existing report's
`Unknown exception` text is deterministic. The harness-side process and report
discovery is deliberately a separate Plan so the crash path stays bounded and
the command socket remains a request/response transport.

## Execution card

- Goal: make agent-mode crash handling non-modal and provide one deterministic
  shared Debug fixture for client/server verification.
- Boundary: the existing `HandleException` prompt branch, engine-shared
  `ExecuteSharedAgentCommand` dispatch, generic command reference, and the
  directly affected engine ownership note; project game dispatch remains
  outside the boundary.
- Tier trigger: Tier 3 for shared client/server crash behavior and deliberate
  process termination at a loopback trust boundary.
- Acceptance: Debug client/server builds; malformed-request and valid-fixture
  process/report observations; static ordinary-launch and non-Debug evidence;
  no unit tests.
- No implementation is authorized by this conversion itself; this document
  is the scheduler action record.
