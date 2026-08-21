<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-19T01:46:42.616Z","dependsOn":[]} -->
# Detect agent process crashes and retain report evidence

## Context

The agent harness has an existing lifecycle ownership gap. Launch on Codex
retains the exact PIDs returned by hidden `Start-Process`, and
`Invoke-HarnessRelease.ps1` quits, waits for, and if necessary force-stops
only those exact PIDs before releasing the lock. There is no mandatory check
that distinguishes an intentional exit from an unexpected exit or discovers
that a crash report was newly written. A handled exception can therefore
terminate with exit code `0` while the active command or poll sees only
transport loss. The process-check script is currently absent. A report
baseline taken as part of post-launch registration would also be too late: an
immediate startup exit can write a report before registration and then have
that new report incorrectly treated as stale.

This Plan depends on
`Documents/Plans/Engine/AgentModeCrashReportSilentSave.md`, which supplies the
non-modal `crash_report_fixture` and preserves the existing configured-
AppData, roaming-AppData, and Desktop-child report candidates.

## Design

Add `.agents/skills/agent-harness/scripts/Invoke-AgentHarnessProcessCheck.ps1`
with four actions: `Baseline`, `Register`, `Check`, and `MarkIntentional`.
Each run has one absolute ignored state path under its retained evidence
directory; state is never placed in tracked repository content. Before each
role's `Start-Process`, `Baseline` records the role plus every configured-
AppData, roaming-AppData, and Desktop-child crash-report candidate with its
pre-launch existence, size, and last-write identity. The project launch block
captures the exact PID and process-start identity from the
`Start-Process -PassThru` result. `Register` then runs immediately after that
launch, binding the caller-captured PID/start identity to the role's existing
pre-launch baseline. An immediate `Check` follows registration so a process
that already exited during startup, including one that wrote a report before
readiness, is evaluated against the true pre-launch baseline. The launch
record and later checks use this state rather than process-name searches.

`Check` evaluates every registered role. An unmarked disappearance (or loss
of its registered exact process identity) is unexpected regardless of exit
code. A changed candidate reports the role, report path, exception headline,
and retained evidence location. If no candidate changed, it reports the
unexpected exit without inventing crash-report evidence. Unchanged reports
from before launch are stale and ignored. `MarkIntentional` binds an exact
role and PID (and its recorded process-start identity) immediately before a
clean quit or forced cleanup.

The result and exit behavior is typed at the behavior level: healthy or
correctly intentional is success; an unexpected exit/crash is a distinct
blocked/failure result; invalid state or setup is an error. The checker never
promises an out-of-band push while no command, poll, or explicit check is
active.

Integrate the checks into the documented launch record, direct command
handling (especially immediately after transport failure and before retry),
every iteration of `Wait-HarnessPing.ps1` and
`Wait-IslandSceneReady.ps1`, and `Invoke-HarnessRelease.ps1`. Extend, rather
than replace, exact-PID ownership, quit, wait, stop, and lock-release behavior.
Never delete a crash report or suppress a crash that was observed before
cleanup; retain the report and its evidence directory.

## Critical files

- `.agents/skills/agent-harness/scripts/Invoke-AgentHarnessProcessCheck.ps1` —
  the new Baseline/Register/Check/MarkIntentional state machine and typed
  outcomes.
- `.agents/skills/agent-harness/SKILL.md` — launch-record, direct-command,
  transport-failure, polling, notification, and lifecycle contracts.
- `Projects/BrokenEngineSandbox/Documents/AgentHarness.md` — the concrete
  `Start-Process -PassThru` launch block and configured crash-report
  identities owned by the project documentation.
- `.agents/skills/agent-harness/scripts/Wait-HarnessPing.ps1` — check each
  registered role before retry/sleep and terminate early on unexpected exit.
- `.agents/skills/agent-harness/scripts/Wait-IslandSceneReady.ps1` — check
  each role during every readiness iteration and terminate early on failure.
- `.agents/skills/agent-harness/scripts/Invoke-HarnessRelease.ps1` — mark
  exact owned roles intentional before quit/forced cleanup, preserve reports,
  and keep existing exact-PID and lock-release ordering.

## In scope

- The new process-check script's four actions, one absolute ignored state path
  per run, exact role/PID/process-start identity, and candidate baselines for
  all existing crash-report locations.
- The project launch block invokes `Baseline` before each `Start-Process`,
  captures the exact PID and process-start identity from `-PassThru`, then
  invokes `Register` immediately followed by `Check`; its configured report
  identities remain the candidates being baselined.
- Unexpected-exit, changed-report, stale-report, no-report, intentional-stop,
  and invalid-state result behavior described above.
- Mandatory integration into launch records, direct command transport-failure
  handling, both existing wait helpers, and harness release.
- Retention of exact-PID ownership, quit/wait/stop/lock-release behavior and
  already observed report evidence.
- Validation of the changed skill package and the focused script checks.

## Out of scope

- Engine or game crash-path changes, report-path/filename changes, or a second
  crash fixture; the dependency Plan owns those.
- Replacing exact-PID ownership or release with process-name searches, broad
  kills, or a separate monitor daemon.
- An out-of-band push or notification channel when no command, poll, or check
  is active.
- Deleting, moving, or rewriting crash reports during cleanup.
- Simulation, CRC, replay, save, wire/protocol, or gameplay behavior.
- A build when current verified binaries already satisfy the runtime
  prerequisite; route `/compile` only if that prerequisite is missing.
- Unit tests.

## Risk tier and invariants

Expected future Change Workflow Tier 3: the implementation crosses OS process
and file trust boundaries and integrates several harness lifecycle scripts.

Preserve these invariants:

- Every process decision uses the registered exact role/PID/process-start
  identity, never a process-name or broad PID search.
- Exit code alone never turns an unmarked disappearance into an intentional
  stop; the report baseline is captured before launch and compared before
  cleanup.
- Changed reports are retained and surfaced with real role/path/headline/
  evidence data; no report evidence is invented when none changed.
- Pre-existing unchanged reports are ignored, while a report written before
  readiness is detected by the immediate post-registration `Check`; intentional
  quit/forced cleanup is marked immediately before the action.
- Existing quit, exact-PID wait/stop, and lock-release ordering remains the
  owner of process and harness-lock cleanup.

## Coordination

This Plan depends on
`Documents/Plans/Engine/AgentModeCrashReportSilentSave.md` and consumes its
fixture's transport-loss, exit-code-zero, and existing report identity
contract. The process checker must not modify that crash path or add a new
report route. The existing release ownership remains authoritative; this
Plan adds evidence checks around it. The project launch document remains the
owner of the concrete launch ordering and configured report identities, so its
pre-launch baseline and immediate post-launch registration/check are in scope.

## Acceptance criteria

- The dependency fixture is run on both server and client without
  `MarkIntentional`. The process check surfaces its deliberate transport loss
  and newly written report even though the process exit code is `0`, including
  role, path, exception headline, and retained evidence location.
- If either endpoint exits and writes its report before readiness, the
  pre-launch `Baseline`, caller-captured `Start-Process -PassThru` PID/start
  identity, immediate `Register`, and immediate `Check` surface that report
  against the true pre-launch state instead of treating it as stale.
- A pre-existing unchanged report is not announced as a new crash.
- A normal intentional `quit` and an exact-PID forced cleanup are not
  misreported as unexpected exits after `MarkIntentional`.
- An unmarked termination of a registered exact PID with no changed report is
  reported as an unexpected exit without fabricated report evidence.
- `Wait-HarnessPing.ps1` and `Wait-IslandSceneReady.ps1` terminate early when
  the registered process disappears unexpectedly, rather than consuming the
  remaining poll timeout.
- Release leaves no owned process or harness lock and retains any newly
  written crash report and evidence directory.
- The changed skill passes `/validate-skill`. No build is required unless the
  runtime prerequisite lacks current verified binaries. No unit tests are
  added.

## Notes

The checker reports through the command/poll/check that is already active;
transport loss is not a promise of asynchronous notification. The state file
and report files remain ignored evidence so a later diagnosis can inspect the
exact process and file observations. `Baseline` runs before each launch, and
the immediate post-launch `Register`/`Check` sequence preserves a report
written before readiness instead of baselining it away.

## Execution card

- Goal: make every harness lifecycle observe unexpected exact-PID exits and
  retain newly written crash reports.
- Boundary: one process-check script, the project-owned launch block/report
  identities, and the existing direct-command, wait-helper, and release
  integrations.
- Tier trigger: Tier 3 for OS process/file trust boundaries and cross-script
  lifecycle integration.
- Acceptance: fixture crash discovery, stale/intentional/no-report behavior,
  early poll termination, cleanup retention, and skill validation.
- No implementation is authorized by this conversion itself; this document
  is the scheduler action record.
