<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-05T17:35:02.843Z","dependsOn":[]} -->
# Harness Heartbeat Refresh Failure

## Context

### Observed symptom

During one live `/agent-harness` run on 2026-11-05 (claim 17:00:57 UTC, release
17:26:50 UTC, Debug BrokenEngineSandbox client and server), AgentHarness exited
`1` with the single stderr line `AgentHarness: harness heartbeat refresh failed`
on four `click` commands. Each named label `Multisampling` in window
`GraphicsMenu` on port 27101, in one client process: one at about 17:01:25 UTC
and three at 17:02:03.823, 17:02:04.135, and 17:02:04.439 UTC.

- The process check ran after each failure and passed, with no crash findings.
- `lock status` showed the lock still owned, so the condition
  `Tools/AgentHarness/AGENTS.md` gives for this exit, ownership loss, was not
  visibly met.
- After the first failure `describe_ui` read `Multisampling` unchecked, and a
  later client process opened with it checked. That fits none of the failed
  clicks landing, but does not prove it.
- A `describe_ui` immediately before the three-failure burst passed. So did
  every later command in the run, including `Multisampling` clicks at 17:04:44
  and 17:04:50 UTC.
- `Multisampling` toggles a swapchain-tier recreation in the client.
- The change under test touched no file under `Tools/`, so the defect predates
  it. The run's acceptance checks passed after retries, and no diagnosis was
  made.
- That run's retry driver ran the process check inside itself, not as the own
  shell call `.agents/skills/agent-harness/references/worker.md` requires. A
  reproduction uses the documented form.

The false condition is that a socket command fails with this message only when
ownership was lost. The cost is a command reported as failed with no way to tell
whether the client received it, which is unsafe for a toggle such as this click.

### Why the root cause is unconfirmed

- `RefreshHarnessHeartbeat` (`Tools/AgentHarness/HarnessLockCommands.cpp:260`)
  returns `false` with no reason for each of: `Guard` acquisition (:270, where
  `failureReason` is discarded, unlike `RunHarnessLockCommand` at :166),
  `ReadMetadata`, `ValidateMetadataEnvelope`, or `HasOwner` (:275), and
  `StampHarnessHeartbeat` (:279). `WriteBytesAtomic`
  (`Tools/ToolCommon/CoordinationStore.cpp:303`) can fail in `CreateFileW`,
  `WriteFile`, or `MoveFileExW`, and no `GetLastError` value is reported.
- Four sites print the same text (`Tools/AgentHarness/AgentHarness.cpp:316`,
  `:544`, `:656`, `:675`). `:656` is the refresh before connect, so nothing was
  sent. `:675` is the refresh after the response, so the click already ran.
  `:316` and `:544` run only when the 60-second interval falls due inside one
  command, which rules them out for the three failures about 300 ms apart.
- `Guard` retries sharing, lock, and access-denied errors for 10 seconds by
  default (`CoordinationStore.cpp:23-76`), so failures 300 ms apart are not a
  guard wait running out. Any other Windows error returns at once.
- The refresh never reads the request, and `heartbeatAt` appears nowhere in
  `Engine/Source` or `Projects`. No code path links the lock file to the click
  or to swapchain recreation, so the `Multisampling` correlation is unexplained
  and may be timing alone.

### Reproduction setup as the observing run recorded it

No deterministic reproduction is recorded. The failing sequence was, in order:

1. Launch the Debug client and server with Local generation data, the client's
   app-data holding a version 22 graphics settings file, which it rejected.
2. Client `window_state {"minimized":false}`, then `resize` 1920x1080.
3. Poll server `status` and client `describe_ui` until `clientCount` is 1 and
   `uiState` is `kNone`.
4. Client `key {"key":"ESC"}`, wait 250 ms, then `click` label `GRAPHICS` in
   window `PauseMenu`, then `describe_ui` reads `kGraphicsSettings`.
5. `describe_ui`, then
   `click {"label":"Multisampling","window":"GraphicsMenu","describeUiAfter":false}`.

## Design

1. Confirm the root cause through `/external-diagnose-bug`. The first question
   is which site and which `RefreshHarnessHeartbeat` branch failed, with its
   Windows error.
2. Make the smallest fix inside the region `## In scope` bounds.
3. Surface a fix outside that region for re-planning instead of widening scope.
4. If the symptom will not reproduce, report that instead of landing a
   speculative fix.

Recommendation: start from a stderr message naming the failed branch and the
site, because that answers the first question on any later occurrence and tells
the caller whether the command was dispatched. Keep it only if it proves useful.

Risk: Change Workflow Tier 2. The trigger is scoped behavior of one tool, the
AgentHarness socket command path. Length-prefixed framing, exit codes, and the
ownership check stay unchanged, and refresh stays on the command thread. A fix
in `Tools/ToolCommon` also reaches WorktreeCli's landing and Plan coordination,
an independently owned subsystem and Tier 3, which is why that code is read-only
here.

No determinism/CRC, serialization, `.pack`, `kiVersion`, replay, wire, affinity,
allocation, or shader exposure. Build AgentHarness through `/compile`, and run
live verification through `/agent-harness`.

## Critical files

- `Tools/AgentHarness/HarnessLockCommands.cpp`
- `Tools/AgentHarness/AgentHarness.cpp`
- `Tools/ToolCommon/CoordinationStore.cpp` (read-only)

## In scope

- Investigating the root cause of the recorded symptom, including temporary
  instrumentation removed before landing.
- The fix region: `RefreshHarnessHeartbeat` and `StampHarnessHeartbeat` in
  `HarnessLockCommands.cpp`; `RefreshHeartbeatIfDue`, `FailSocketOperation`, the
  heartbeat failure branch of `ExchangeFramedSocketCommand`, and the two
  refreshes in `RunSocketCommand` in `AgentHarness.cpp`.

## Out of scope

- Any edit under `Tools/ToolCommon`, including `Guard`, `ReadMetadata`, and
  `WriteBytesAtomic`.
- The client, server, and the `click` command handler.
- `.agents/skills/agent-harness/` scripts and references.
- Changing heartbeat interval, the staleness rule, framing, or exit codes.
- Unit tests.

## Acceptance criteria

- The root cause is stated with evidence naming the failing site and branch.
- The recorded symptom no longer reproduces: the sequence that produced it
  before the fix, repeated on `Multisampling` through `click`, returns exit `0`
  each time, and no click reports `harness heartbeat refresh failed` while
  `lock status` shows the lock owned.
- A socket command with an owner token that does not own the lock still exits
  `1` with no command dispatched.

## Coordination

`Documents/Plans/Engine/CoreOrderedGuardSequences.md` names a candidate at
`HarnessLockCommands.cpp:275`, inside `RefreshHarnessHeartbeat`. Whichever lands
second refreshes its coordinates and keeps the other's meaning. No order is
required.

## Notes

The owning contracts are `Tools/AgentHarness/AGENTS.md` and
`.agents/skills/agent-harness/references/worker.md` step 33.
