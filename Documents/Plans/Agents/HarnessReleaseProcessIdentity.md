<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-21T12:53:55.542Z","dependsOn":[]} -->
# Gate harness release wait and force-stop on process identity, not PID alone

## Context

`.agents/skills/agent-harness/scripts/Invoke-HarnessRelease.ps1` makes every
process decision from the supplied `-ServerPid`/`-ClientPid` numbers alone. Its
liveness test is `Test-ProcessAlive` (`Invoke-HarnessRelease.ps1:69-71`), which
returns true for whatever process currently holds that number; the bounded wait
loop polls it (`:140-150`) and the force-stop path runs
`Stop-Process -Id $process.ProcessId -Force` on a still-"alive" number
(`:152-164`). Windows recycles process IDs, so if the launched client or server
exits and its number is reissued to an unrelated process before or during
release, this script waits on that unrelated process for the full
`-TimeoutSeconds` budget and then force-kills it, while the released run is
reported as `stopped` rather than as the exit it actually was.

This is pre-existing behavior: the same three regions are byte-identical at
commit `48cebb3f42bde7b9c0bdd00dff5541520011b578`. The identity data the fix
needs already exists elsewhere:
`.agents/skills/agent-harness/scripts/Invoke-AgentHarnessProcessCheck.ps1`
persists each role's PID plus its process-start time (`Register`, `:232-251`)
and already compares both before treating a process as alive
(`Test-RegisteredProcessAlive`, `:153-166`, which compares the two UTC start
times within a small tolerance and treats an unreadable start time as "not ours"). Release already receives the path
to that state through its optional `-ProcessCheckStatePath` parameter
(`Invoke-HarnessRelease.ps1:14`), but uses it only for the advisory crash check.

The plan that added that checker deliberately left release PID-only ("Retention
of exact-PID ownership, quit/wait/stop/lock-release behavior" was in its
`## In scope`), and the user decided during that change to record the recycling
risk here instead of widening it.

## Design

Gate release's liveness decisions on PID plus process-start time, reusing the
identity the process-check state already holds.

- When `-ProcessCheckStatePath` names a readable state file, read each role's
  persisted `processId`/`startTimeUtc` and use that start time as the identity
  for the matching `-ServerPid`/`-ClientPid`.
- Replace `Test-ProcessAlive` with a check that treats the number as alive only
  when a process with that ID exists and its `StartTime`, converted to UTC,
  matches the recorded start time within the same small tolerance the checker
  already uses. An existing process whose start time differs
  is a recycled number: the launched process is already gone, so the role's
  state becomes `already-absent`/`exited-after-quit` and the current holder is
  never waited on and never stopped.
- When no identity is available for a role — no state path supplied, the file is
  unreadable, or the role has no registered process — keep today's PID-only
  behavior for that role so existing callers that pass only PIDs keep working
  unchanged.
- Reading process start time can throw for a process the session cannot open;
  treat an unreadable start time as "not our process" rather than as a match, so
  release never force-stops a process it cannot identify.

Everything else stays as it is: quit ordering, the bounded wait, the
stop-confirm watchdog, the surviving-PID blocked result that keeps the harness
claim held, and the advisory crash check that already runs before the first
quit.

If the documented invocation in `.agents/skills/agent-harness/SKILL.md` gains no
new parameter, that document needs no change beyond describing that release now
uses the recorded process identity when the state path is supplied.

## Critical files

- `.agents/skills/agent-harness/scripts/Invoke-HarnessRelease.ps1` —
  `Test-ProcessAlive`, the `$tracked` role build-up, the wait loop, and the
  force-stop loop.
- `.agents/skills/agent-harness/scripts/Invoke-AgentHarnessProcessCheck.ps1` —
  read-only source of the persisted `processId`/`startTimeUtc` state shape.
- `.agents/skills/agent-harness/SKILL.md` — the release contract text, if the
  described behavior changes what a caller must supply or expect.

## In scope

- Reading the persisted per-role PID and start time from the file named by the
  existing `-ProcessCheckStatePath` parameter.
- Identity-gated liveness inside `Invoke-HarnessRelease.ps1`: the liveness
  helper, the initial role states, the wait loop, and the force-stop loop.
- The documented fallback to PID-only behavior when no identity is available.
- Release contract wording in the harness SKILL.md when the behavior above
  changes what callers supply or observe, plus `/validate-skill` on it.

## Out of scope

- Any change to `Invoke-AgentHarnessProcessCheck.ps1` behavior, its state file
  format, or its crash-report handling.
- Quit ordering, timeouts, the surviving-process blocked result, lock-release
  ordering, or crash-report retention.
- Process-name searches, broad kills, or a monitor daemon.
- Engine, game, simulation, CRC, replay, wire, or gameplay behavior.
- Unit tests.

## Risk tier and invariants

Expected future Change Workflow Tier 3: the change reads OS process state across
a trust boundary and decides a forced termination from it. The work stays inside
one script plus its contract text.

Preserve these invariants:

- Release never force-stops a process it has not positively identified as the
  one this run launched.
- A role with no available identity keeps today's exact PID-only behavior.
- Release still leaves the harness claim held when an owned process survives.
- No process-name search or broad kill is introduced.

## Acceptance criteria

- With a state file registering a role whose recorded PID is live but whose
  start time differs from the recorded one, release reports that role as absent,
  does not wait out the timeout, and issues no `Stop-Process` for it.
- With a state file whose recorded PID and start time both match a live owned
  process that ignores quit, release still force-stops it and confirms the stop
  as it does today.
- With no `-ProcessCheckStatePath` supplied, release behaves exactly as it does
  today for both roles.
- A normal harness run releases with `status: pass`, no surviving PIDs, and the
  lock released.
- `/validate-skill` passes for a changed SKILL.md; `plan validate` exits 0.

## Notes

The identity comparison must be done in UTC on both sides: the state file stores
a round-trip UTC timestamp, while `Get-Process` returns local `StartTime`.
