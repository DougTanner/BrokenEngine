<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-08T21:55:56.341Z","dependsOn":[]} -->
# Fix: agent-harness Launch — a server already running outside the harness makes the harness server exit, and the run reports an unexplained `check.unexpected-exit`

## Context

A harness launch should notice a game server that is already running before it starts its own, and report the run as blocked by that server. Today it launches anyway, and the run fails with a process-check finding that does not say why.

Verified root cause, from the current tree:

- The server is single-instance per machine. `Engine/Source/Main.cpp` (:847-858) creates the `BrokenEngineSandboxServer` mutex, gated on `kbSingleInstance`, which is `true` only under `BT_SERVER` (`Projects/BrokenEngineSandbox/Source/Pch.h` :22-26). An `--agent-port` launch that finds the mutex already held logs "Another instance is already running; --agent-port launch aborting" and returns 0.
- `Projects/BrokenEngineSandbox/Documents/AgentHarness/launch.md` `## Launch` (:5-101) goes straight from directory setup to the server `Baseline`, `Start-Process`, `Register`, and immediate `Check`. Nothing checks for a running server first. The only running-server check in that document is in `## Normal launch` (:207-213, `Get-Process -Name 'BrokenEngineSandboxServer*'`).
- `.agents/skills/agent-harness/scripts/Invoke-HarnessClaim.ps1` waits only on a held harness lock (header comment :3-7). A server started outside any agent holds no lock, so the claim passes.
- The immediate `Check` (`.agents/skills/agent-harness/references/worker.md` steps 24-25) then reports exit `2` `check.unexpected-exit` for role `server`, with `reportPath` and `headline` `null`, because no crash report was written. Under `.agents/skills/agent-harness/SKILL.md` `## Handoff`, that becomes a `HARNESS-F-###` `Required` finding and a `NEEDS_ACTION` status. The main agent is then pointed at `/resolve-findings` for what is really an environment block.

Observed in this session's `/next-plan` harness run (session-local evidence, restated here): `Invoke-HarnessClaim.ps1` passed, and the `--agent-port` server (PID 3744, registered at 2026-10-08T21:42:43Z) exited at once with the single-instance log line above. The immediate process check reported `check.unexpected-exit` for role `server` with no crash report. A game server started outside the harness (PID 15536, no `--agent-port`) was running at the time.

`Documents/Plans/ChangeWorkflow/HoldHarnessLockForNormalLaunch.md` makes an agent-started normal launch hold the harness lock, so a harness run waits for it instead. Its `## Out of scope` excludes this preflight as a separate root cause. Even after that fix, a server started by a human outside any agent, such as from Visual Studio or Explorer, holds no lock and still triggers this.

Session provenance (machine-local; not reproducible after cleanup). The Client through Worktree fields name the session that observed the friction — the session `/next-plan-review` must reach — while the `Landing ref` line names a ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 7e1a7312-98f6-437d-ad5c-61651f58a4f3
- Worktree/branch UUID: e61b4640-26bd-4b7a-a2c0-5690beb0de97
- Session branch: claude/e61b4640-26bd-4b7a-a2c0-5690beb0de97
- Worktree: .claude\worktrees\BrokenEngine\e61b4640-26bd-4b7a-a2c0-5690beb0de97
- Landing ref: claude/e61b4640-26bd-4b7a-a2c0-5690beb0de97 (the observing session records and lands this Plan itself).
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/BlockHarnessLaunchOnRunningServer.md`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to one session alone
  (its diff limited to that session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design

The root cause above is already proven from the tree, so no transcript review is needed. Make the smallest fix inside `## In scope`. If the fix turns out to need anything outside that boundary, surface it for re-planning instead of expanding scope.

Recommended shape:

1. **Preflight.** Add one fence at the start of `launch.md` `## Launch`, before the directory setup and the server `Baseline`. It runs the same check `## Normal launch` uses: `Get-Process -Name 'BrokenEngineSandboxServer*' -ErrorAction SilentlyContinue | Select-Object Id, ProcessName`. Reusing that exact check keeps the two recipes parallel. The name pattern also covers the Debug, Profile, and Release executables from any worktree, all of which share the one mutex. The check belongs in the project recipe rather than in the generic worker steps because the server process name is project-specific. It runs after the claim, which worker step 5 already orders before launch. Running it before the claim would wrongly count another session's harness server, which the claim exists to wait out.
2. **Block.** When the fence lists a process, the recipe says to launch nothing and release the just-taken lock through worker step 44 with no PID arguments. The run then returns `BLOCKED` with one `Residuals` row naming each listed PID and process name as blocked by a running server, and no `HARNESS-F` finding. That matches `SKILL.md` `## Handoff`, which already treats a setup limitation as a blocked check and gives environment blocks a `Residuals` row. The recipe never stops, quits, or signals the listed process; that process belongs to the user.
3. **No match.** When the fence lists nothing, the launch continues exactly as today.

The author recommends the recipe fence over a new `Invoke-HarnessClaim.ps1` block code. The script would have to release a lock it had just acquired and add a new result code, while the fence reuses the existing check and the existing release path unchanged.

Live-verification exposure: no C++, determinism/CRC, wire, serialization, `.pack`, or replay surface changes. The check only reads the process list.

## Critical files

- `Projects/BrokenEngineSandbox/Documents/AgentHarness/launch.md` — `## Launch`

## In scope

- `launch.md` `## Launch`: the new preflight fence before the directory-setup fence, and the prose for the block outcome (launch nothing, release with no PID arguments, `BLOCKED` with the running-server `Residuals` row)

## Out of scope

- `launch.md` `## Normal launch`, which `Documents/Plans/ChangeWorkflow/HoldHarnessLockForNormalLaunch.md` owns
- `Invoke-HarnessClaim.ps1`, `Invoke-HarnessRelease.ps1`, `Invoke-AgentHarnessProcessCheck.ps1`, `.agents/skills/agent-harness/SKILL.md`, and `references/worker.md`: all reused unchanged
- The engine single-instance guard in `Engine/Source/Main.cpp`
- A non-harness client already running (the client is not single-instance)
- Any transcript path or transcript text in the repository

## Risk tier and invariants

Tier 2. Trigger: scoped tool behavior, confined to one harness launch recipe, with no C++, script, build, or bootstrap change.

Invariants:

- The preflight only reads the process list. It never stops, quits, or signals a listed process.
- A blocked run releases its own lock through the existing release script and never launches a harness process.
- With no server running, the launch sequence and its process-check results are unchanged.

## Acceptance criteria

- With a server running that was started without `--agent-port` (for example through the `## Normal launch` recipe), a harness run's claim passes and then no `--agent-port` server is started and no process-check `Baseline` or `Register` runs. The release returns exit `0` with the lock released. The handoff is `BLOCKED` with a `Residuals` row naming the running server's PID, and has no `HARNESS-F` finding and no `check.unexpected-exit`. The running server is still alive afterward.
- With no server running, the harness launch proceeds and the server's immediate `Check` exits `0`.
- The static-checks runner, invoked as `.agents/references/change-workflow.md` `#### Step 5 — Run targeted pre-review checks` documents it, reports every row the change triggers passing.

## Notes

- Area: `ChangeWorkflow/`, because the only file changed is an agent-run harness launch recipe. That is how agents work rather than what the product does, and it matches the related Plan above.
- No `dependsOn` and no Coordination: the related Plan changes `## Normal launch` only, so the two land independently in either order.
