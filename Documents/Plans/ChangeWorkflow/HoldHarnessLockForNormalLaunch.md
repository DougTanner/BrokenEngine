<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-08T21:53:02.517Z","dependsOn":[]} -->
# Fix: agent-harness Normal launch — an agent-started game holds no harness lock, so another session's harness server exits on the single-instance guard

## Context

The user decided, in their own words: "it should still take a harness lock if launched by an agent". An agent that performs the `Projects/BrokenEngineSandbox/Documents/AgentHarness/launch.md` `## Normal launch` recipe (a launch for the user's own play or testing, routed there by `.agents/skills/agent-harness/SKILL.md` `## When to use`) must therefore hold the same harness lock a harness run holds, so a concurrent harness run from another session waits on the lock instead of starting a server that exits.

Verified root cause, from the current tree:

- `launch.md` `## Normal launch` (:205) states the recipe has "no claim, process check, readiness wait, or release". It checks only for a running server before its own launch (:207-211) and then leaves both processes running with no lock. Its closing sentence (:227) says "a later harness run waits until the user closes it", but nothing makes a harness run wait: `.agents/skills/agent-harness/scripts/Invoke-HarnessClaim.ps1` waits only on a held harness lock (header comment :3-7, claim loop :345-366), and none is held.
- The server is single-instance per machine: `Engine/Source/Main.cpp` (:847-858) creates the `BrokenEngineSandboxServer` mutex, and an `--agent-port` launch that finds it already held logs "Another instance is already running; --agent-port launch aborting" and returns 0.

Observed in this session's `/next-plan` harness run: `Invoke-HarnessClaim.ps1` passed (no lock held), the `--agent-port` server (PID 3744) then exited at once with that single-instance log line, and the immediate process check reported `check.unexpected-exit` for role `server` with no crash report. The machine was running a normal launch started by another session's agent (worktree `e429d0aa-6793-4682-b4eb-82c90942f2d8`: server PID 15536 and client PID 31296, neither with `--agent-port`), which held no harness lock.

Settled from the existing lock mechanism, it is not enough for the normal launch to claim the lock and do nothing else. The lock holder's freshness is its heartbeat (`Tools/AgentHarness/AGENTS.md` :9). `Invoke-HarnessClaim.ps1` waits on a holder only until its `heartbeatAt` is five minutes old (:28-29, :362-364), then returns `claim.foreign-owner`. The worker's takeover steps (`.agents/skills/agent-harness/references/worker.md` steps 11-15) then validate and quit only `--agent-port` listeners and steal the lock. A normal launch runs no agent port and sends no heartbeat, so five minutes in, another session would take over its lock and hit the same single-instance exit. The lock has to stay fresh for as long as the user's game runs and be released when that game exits. The user's play session has no fixed end, so its process exit, not an agent's release call, ends the hold.

Session provenance (machine-local; not reproducible after cleanup). The Client through Worktree fields name the session that observed the friction — the session `/next-plan-review` must reach — while the `Landing ref` line names a ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 7e1a7312-98f6-437d-ad5c-61651f58a4f3
- Worktree/branch UUID: e61b4640-26bd-4b7a-a2c0-5690beb0de97
- Session branch: claude/e61b4640-26bd-4b7a-a2c0-5690beb0de97
- Worktree: .claude\worktrees\BrokenEngine\e61b4640-26bd-4b7a-a2c0-5690beb0de97
- Landing ref: claude/e61b4640-26bd-4b7a-a2c0-5690beb0de97 (the observing session records and lands this Plan itself).
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/HoldHarnessLockForNormalLaunch.md`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to one session alone
  (its diff limited to that session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design

The root cause above is already proven from the tree, so no transcript review is needed. Make the smallest fix inside `## In scope`. If the fix turns out to need anything outside that boundary, surface it for re-planning instead of expanding scope.

Recommended shape, reusing the existing mechanisms:

1. **Claim.** The `## Normal launch` recipe first runs `Invoke-HarnessClaim.ps1` unchanged, exactly as worker step 5 documents it. Pass `-RepositoryRoot` (the adopted worktree), `-GameDataDirectory` (the same normalized Data path the launch passes to `--data-directory`), `-Configuration` for a non-Debug build, and a `-Session` label that names a normal launch, for example `normal-launch`. Reusing the script brings its executable-existence and pack-version preflights and its foreground wait on a held lock with it, so a normal launch requested while another session's harness run holds the lock waits for that run's release. Harness runs carry a 20-minute default budget, which is why this Plan recommends waiting over reporting. On any non-zero claim exit, report the result to the user and launch nothing. The existing `Get-Process` server check stays after the claim succeeds. It still catches a server started outside any agent, for example from Visual Studio. If that check finds a server, the recipe releases the just-claimed lock with `AgentHarness lock release --key default --owner <owner token>` before it reports the server to the user.
2. **Launch.** Launch both executables exactly as the recipe does today, retaining each PID and process start time the way `## Launch` does.
3. **Hold.** Add one new script under `.agents/skills/agent-harness/scripts/`, for example `Invoke-NormalLaunchLockHold.ps1`. The recipe runs it once in the canonical `pwsh -NoProfile -File` form, passing the AgentHarness path and owner token from the claim, the server and client PIDs, and their start times. It returns at once after starting a detached copy of itself in an internal hold mode. That is the self-spawn pattern `Invoke-AgentHarnessProcessCheck.ps1` already uses for its `Watch` action (header :9-12, `Start-Process pwsh ... -WindowStyle Hidden` :316-323), so the agent never runs a hand-written detached command.
   - While either registered process (PID plus matching start time, the identity rule `Invoke-AgentHarnessProcessCheck.ps1` :6-8 states) is alive, the detached copy runs `lock heartbeat --key default --owner <token>` at an interval well inside the five-minute staleness point. It does this on its own schedule, so no agent command is needed.
   - Once both have exited, it runs `lock release --key default --owner <token>` and exits.
   - If a heartbeat or the release fails because the owner was lost, it stops without retrying.
   - The recommended rule holds the lock while either process lives, not only the server. While a normal client is still open, it could auto-connect to a harness server another session starts.
4. **Docs.** Update `## Normal launch` to state the claim, the hold, and that the user's closing of both processes releases the lock, replacing the "no claim ... or release" wording (:205) and making the closing sentence (:227) true. The recipe still never quits, stops, or releases the user's processes itself. The `.agents/skills/agent-harness/SKILL.md` `## When to use` normal-launch bullet keeps its handoff of the launch command and both PIDs as `Decisive checks` rows; change that bullet only if its wording would otherwise be wrong once the claim is added (it currently says only "run only the ... recipe in place of the worker steps").

Live-verification exposure: no C++, determinism/CRC, wire, serialization, `.pack`, or replay surface changes. The harness lock is shared across sessions, so a hold that never releases would block every other session's harness runs. That is why release-on-exit and stop-on-owner-loss are acceptance criteria below.

## Critical files

- `Projects/BrokenEngineSandbox/Documents/AgentHarness/launch.md` — `## Normal launch`
- `.agents/skills/agent-harness/scripts/Invoke-NormalLaunchLockHold.ps1` (new; name is the recommendation)
- `.agents/skills/agent-harness/SKILL.md` — `## When to use`, normal-launch bullet, only if its wording becomes wrong

## In scope

- `launch.md` `## Normal launch`: add the claim, the `lock release` on the existing running-server branch, the hold-script call, and the corrected prose at :205 and :227
- The new hold script: argument validation, the detached self-spawn, the heartbeat loop keyed to the two registered process identities, release on exit, and stop on owner loss
- The `SKILL.md` `## When to use` normal-launch bullet, only as step 4 of `## Design` bounds it

## Out of scope

- `Invoke-HarnessClaim.ps1`, `Invoke-HarnessRelease.ps1`, `Invoke-AgentHarnessProcessCheck.ps1`, and the AgentHarness `lock` verbs: all reused unchanged
- The worker staleness and takeover rules (`references/worker.md` steps 11-15)
- A harness-launch preflight that detects an already-running server before the `--agent-port` launch and reports it as a block instead of an unexplained `check.unexpected-exit`. That is a separate root cause: a game started outside any agent still holds no lock.
- The engine single-instance guard in `Engine/Source/Main.cpp`
- Any transcript path or transcript text in the repository

## Risk tier and invariants

Tier 2. Trigger: scoped tool behavior, confined to one skill's launch recipe and one new skill script, with no C++, build, or bootstrap change. A reviewer may escalate it to Tier 3 if the hold is judged cross-session coordination that can block other sessions, since a stuck hold blocks every session's harness runs.

Invariants:

- The lock is held only while a registered normal-launch process with a matching start time is alive.
- The hold never quits, stops, or signals the user's processes.
- The hold never touches a lock it does not own.

## Acceptance criteria

- With a normal launch running, `AgentHarness lock status` shows the lock held under the normal-launch session label, and its `heartbeatAt` keeps advancing past five minutes with no agent command issued.
- With that normal launch running, a harness run from another worktree waits inside `Invoke-HarnessClaim.ps1` and never reaches `claim.foreign-owner` or launches an `--agent-port` server.
- After the user closes both normal-launch processes, the hold releases the lock and exits. The waiting harness claim then passes, and its server starts without the single-instance log line.
- A normal launch requested while another session's harness run holds the lock waits in the claim and launches only after that run releases.
- The static-checks runner, invoked as `.agents/references/change-workflow.md` `#### Step 5 — Run targeted pre-review checks` documents it, reports every row the change triggers passing.
