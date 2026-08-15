# Client stops responding during a non-adjacent fleet move

Findings record from active-set baseline harness work (2026-08-15). Not
decision-complete: the root cause is unknown and no fix is decided, so this is
not an executable Plan. Route it through `/external-diagnose-bug`; promote it to
`Documents/Plans/<area>/` once a proven root cause and a decided fix exist.

## Observation

Build and binaries: Debug client at commit
`45f2d1d04c37e7fc5a779be133273ebafdba2028` (pre-change binaries, before the
active-set skeleton move).

After the scripted baseline movement samples completed (fleet at `[0,1]`, server
`activeCoords` `[0,0] [0,1] [1,1]`), one extra non-adjacent move was injected on
the server agent port:

```
{"cmd":"inject_status_changes","params":{"changes":[{"coord":[0,1],"type":"UpdateFleet","playerUuid":1688849860263937,"fleetWantedCoord":[-2,0]}]}}
```

It was accepted (`injected:1`, `deferred:false`). During the resulting two-cell
traverse the client process stopped responding:

- `describe_scene` on the client agent port timed out at 60 s.
- The client agent listener then disappeared entirely (subsequent `connect`
  attempts failed).
- `BrokenEngineSandbox.Debug.exe` remained alive with `Responding:False`.
- No crash report was written under `%APPDATA%\Broken Engine Sandbox`.
- The client had to be force-stopped during harness release.

The server stayed healthy throughout. With the client gone it reported server
tick 12925, `clientCount:0`, `activeCoords` `[-2,0] [0,0]` — consistent with the
documented union rule (player-held coord plus the always-added origin).

## What is not established

- Whether the non-adjacent (two-cell) `fleetWantedCoord` jump is the trigger at
  all. Every earlier move in the same session was single-cell and none hung.
- Whether the hang is in traversal/cell-transfer handling, in the agent listener
  thread, or elsewhere. The process was alive but not pumping, and no crash
  report exists, so a deadlock and a runaway loop are both open.
- Whether it reproduces. It was observed once, outside the scripted scenario.

## Boundary evidence

Pre-existing and outside the active change: it was observed on baseline
`45f2d1d0` binaries, before the
`Documents/Plans/Network/ActiveSetSkeletonToEngine.md` change existed, and that
Plan's `## In scope` covers only the server-side active-set skeleton move.

## Suggested next step

Re-run the baseline scenario on a Debug client and inject a non-adjacent
`UpdateFleet fleetWantedCoord` while the fleet is mid-traverse; if it reproduces,
attach a debugger and capture the stalled thread stacks (the process stays alive,
so a live attach or a manual dump is possible). Contemporary run artifacts lived
in `Temp/activeset-baseline.md` (final section), `Temp/server-agent.log`, and
`Temp/client-agent.log`; `Temp/` is untracked and may already be gone, so the
details above are the durable record.
