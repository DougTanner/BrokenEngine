<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-13T23:06:18.784Z","dependsOn":[]} -->
# Require the operation deadline in AgentHarness RefreshHeartbeatIfDue

## Context

`Tools/AgentHarness/AgentHarness.cpp` declares the file-static helper
`RefreshHeartbeatIfDue` with a third parameter `optionalOperationDeadline`, a
`std::optional` of `std::chrono::steady_clock::time_point` that defaults to
`std::nullopt`. The body branches on it: with a deadline it calls
`RefreshHarnessHeartbeat(owner, <remaining milliseconds, floored at 0>)`;
without one it falls to `else if (!RefreshHarnessHeartbeat(owner))`, which uses
the default wait declared in `Tools/AgentHarness/HarnessLockCommands.h`.

Verified root cause: the helper has five callers, all in the same file, and
every one passes a deadline, so the `std::nullopt` default and the `else if`
branch can never run:

- `CheckDeadlineAndRefreshHeartbeatAfterNoProgress` passes `rOperationDeadline`.
- `WaitForSocketReadiness` passes `rOperationDeadline` at two call sites.
- The two transport loops that hold a local `deadline` pass `deadline` (the
  calls with `rArguments.owner`).

No other file names `RefreshHeartbeatIfDue`. The two deadline-less heartbeat
refreshes in `main` (before connect and after the response) call
`RefreshHarnessHeartbeat(arguments.owner)` directly and do not go through this
helper.

Originating gap: a refactor observation logged against this function as
pre-existing dead code. The optional parameter also misleads a reader into
thinking a transport-phase refresh without a deadline is a supported path.

## Design

Recommended, because it is the smallest change that removes the dead path and
matches the neighbouring functions' parameter form:

1. Change the third parameter of `RefreshHeartbeatIfDue` to a required
   `const std::chrono::steady_clock::time_point& rOperationDeadline` with no
   default, the same form `CheckDeadlineAndRefreshHeartbeatAfterNoProgress` and
   `WaitForSocketReadiness` already use.
2. In the body, remove the `if (optionalOperationDeadline)` test and the
   `else if (!RefreshHarnessHeartbeat(owner))` branch, keeping the
   remaining-milliseconds computation and the
   `RefreshHarnessHeartbeat(owner, ...)` call with its floor at 0 unconditional
   and otherwise unchanged.
3. Leave all five call sites as they are; each already passes a
   `steady_clock::time_point`, which binds to the new parameter.

Risk tier: Tier 1 — mechanical. Trigger: local behavior-preserving work on one
file-static function with no public signature or invariant exposure
(`.agents/references/risk-tiers.md` `### Risk tiers`). The executed path is
unchanged for every existing caller.

Landing: `Tools/AgentHarness/AgentHarness.cpp` is a non-Markdown path under
`Tools/AgentHarness/`, so the change is a shared AgentTools trigger. It lands
only through the landing gate (`/finalize-changes`), which rebuilds both tools
from the landed commit and promotes them as
`.agents/skills/finalize-changes/references/agenttools.md` defines; the
session never builds into or copies over the shared primary Output. The change
alters no CLI contract, lock or claim format, exit code, or timing, so it
cannot affect another live worktree and needs no pause-and-warn under that
reference.

Build verification: the AgentHarness AgentTools Release candidate build through
`/compile`.

## Critical files

- `Tools/AgentHarness/AgentHarness.cpp`

## In scope

- `RefreshHeartbeatIfDue` in `Tools/AgentHarness/AgentHarness.cpp`: its third
  parameter's type, name, and default, and the `if`/`else if` on that parameter
  in its body.

## Out of scope

- The five call sites of `RefreshHeartbeatIfDue`, unless the build proves one
  needs a change.
- `RefreshHarnessHeartbeat`, its default `iMaximumWaitMilliseconds`, and
  `Tools/AgentHarness/HarnessLockCommands.h`/`.cpp`; the two direct calls in
  `main` still use that default.
- The `now >= rNextHeartbeatDue` test in `WaitForSocketReadiness` that precedes
  its call, and any other restructuring of the transport loops.
- Include cleanup, heartbeat interval or deadline values, and
  `Tools/AgentHarness/AGENTS.md`, whose described behavior does not change.

## Acceptance criteria

- `RefreshHeartbeatIfDue` takes a required operation deadline: no default
  argument, no `std::optional`, and no call to `RefreshHarnessHeartbeat` without
  a wait argument inside it.
- The AgentHarness AgentTools Release candidate build succeeds with no new
  warning.
- The diff touches only `RefreshHeartbeatIfDue` in
  `Tools/AgentHarness/AgentHarness.cpp`.

## Notes

- Invariants to preserve (`Tools/AgentHarness/AGENTS.md`): ownership is
  refreshed on the command thread whenever due during transport, each refresh
  wait is bounded by the phase's absolute deadline, and ownership loss exits
  `1`. No wire, framing, lock-record, threading, or exit-code exposure.
- No live `/agent-harness` run is needed to settle acceptance; the diff and the
  build are decisive.
