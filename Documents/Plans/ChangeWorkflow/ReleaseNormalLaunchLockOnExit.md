<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-30T02:01:40.878Z","dependsOn":[]} -->
# Fix: agent-harness Normal launch — harness lock stays held after the user exits the game

## Context
Observed symptom (2026-10-30 UTC): after the user closed the client and server
of an `/agent-harness` Normal launch, each of three consecutive Normal relaunches
from the same worktree got `claim.foreign-owner` from
`pwsh -NoProfile -File .agents/skills/agent-harness/scripts/Invoke-HarnessClaim.ps1 -RepositoryRoot '<worktree>' -Session 'normal-launch' -Project 'BrokenEngineSandbox'`
against the session's own previous `normal-launch` owner. Each relaunch then
needed the manual worker steps 12-15 (listener check, `quit` to the validated
listeners, `lock token`, `lock steal --expect`) before launching. The third claim, made about 2 minutes
after the previous launch, blocked about 180 s (37 attempts) waiting for
staleness, although both game processes had already exited and nothing was
listening on 27100 or 27101. The user asked that exiting the client/server
release the lock automatically.

Root cause, confirmed from the current tree:
- `.agents/skills/agent-harness/references/launch.md` `## Normal launch` claims
  the lock under session `normal-launch` "that is never released" (line 170),
  drops the owner token ("Do not keep the claim's owner token", line 194), and
  says to "never heartbeat or release the lock" (line 223). The lock is only
  freed by going stale five minutes after the claim and being taken over by
  worker steps 11-15 (lines 221-229).
- `Invoke-HarnessClaim.ps1` waits until the holder's `heartbeatAt` is
  `$StaleHeartbeatSeconds` (300, line 33) old before it checks for a project
  server started without `--agent-port` (lines 384-390). Nothing tells it the
  Normal-launch processes are gone, so a relaunch inside the five minutes waits
  out the rest of them, and every relaunch ends in `claim.foreign-owner`.
- `Tools/AgentHarness/HarnessLockCommands.cpp` `RunHarnessLockCommand`: `lock
  release` deletes the record only when `--owner` matches (lines 219-236) and
  otherwise exits 2 with the record unchanged, so a release by a stale token
  cannot free another holder's lock.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: ed3190fd-0fb3-4d5f-8bd0-3ec7456205b7
- Worktree/branch UUID: 8db4c3ff-e4f0-4f7c-b043-b5e0946c54be
- Session branch: claude/8db4c3ff-e4f0-4f7c-b043-b5e0946c54be
- Worktree: .claude\worktrees\BrokenEngine\8db4c3ff-e4f0-4f7c-b043-b5e0946c54be
- Landing ref: claude/8db4c3ff-e4f0-4f7c-b043-b5e0946c54be
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/ReleaseNormalLaunchLockOnExit.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
The root cause above is already confirmed from the tree, so the transcript is
not needed. Only if the current tree no longer matches `## Context`, in a new
session run `/next-plan-review claude/8db4c3ff-e4f0-4f7c-b043-b5e0946c54be` in
bounded friction mode, supplying client `claude` and the conversation session
ID above. Then make the smallest fix inside the `## In scope` boundary below.
If the fix turns out to lie outside that boundary, surface it for re-planning
instead of expanding scope.

The author recommends, for these reasons, tying the release to the exit of the
two launched processes:
- A new script under `.agents/skills/agent-harness/scripts/` takes the
  AgentHarness path, the owner token, and the exact server and client PIDs. It
  waits until every supplied PID is gone, then runs
  `lock release --key default --owner '<owner token>'` once and exits. While it
  waits it stamps `lock heartbeat --key default --owner '<owner token>'` at an
  interval well under the 300 s staleness, and stops stamping once a heartbeat
  is refused with exit `2` (another owner took over). It never stops a process
  and searches no process by name. With no PID supplied (the server launch
  failed, so the client was never started) it releases at once.
- `## Normal launch` keeps the claim result's `owner` and `agentHarness` (or the
  steal's new `owner`) only long enough to start that script detached and
  hidden (`Start-Process` with a hidden window) right after the launch block,
  in a separate shell call so it also starts when that block threw, passing
  only the PIDs that started. The session itself still keeps no owner token
  after the launch, so `/compile` still never sends `quit` to the user's
  processes and the processes are still left running for the user.
- The heartbeat keeps a waiting `Invoke-HarnessClaim.ps1` claim from breaking to
  `claim.foreign-owner` while the release is pending, so the third acceptance
  criterion holds whether the game ran under or over five minutes after the
  claim. Without it the claim breaks once the holder's `heartbeatAt` is 300 s
  old and no project server started without `--agent-port` runs
  (`Invoke-HarnessClaim.ps1` lines 384-390), which a game run past five minutes reaches as soon as the server
  exits, while the client may still be open. With it the holder never reads
  stale while the script runs, so the claim waits until the release frees the
  lock and then acquires it, or ends a longer wait with `claim.wait-timeout`,
  which the caller answers by re-invoking; the claim script needs no change.
- The owner match in `lock release` and `lock heartbeat` means a harness run
  that took the lock over in the meantime keeps it. A release-on-exit script
  that dies (for example when a reboot kills it) stops stamping, so its holder
  goes stale and the claim's existing stale break and the takeover through
  worker steps 11-15 stay the fallback for a lock the script never released.

Alternatives the author considered and does not recommend:
- `Invoke-HarnessClaim.ps1` treating a `normal-launch` holder as stale once no
  project server started without `--agent-port` runs: it removes the wait but
  still needs worker steps 12-15 on every relaunch, which is not an automatic
  release.
- AgentHarness evaluating a recorded process's liveness: a C++ change against
  `Tools/AgentHarness/AGENTS.md`, whose lock rule says the executable records
  `heartbeatAt`/`heartbeatPid` but never evaluates them.

## Critical files
- `.agents/skills/agent-harness/references/launch.md`
- `.agents/skills/agent-harness/references/worker.md`
- A new release-on-exit script under `.agents/skills/agent-harness/scripts/`

## In scope
- Confirm the root cause as `## Design` states.
- The new release-on-exit script.
- `launch.md` `## Normal launch`: the opening paragraph (the claim "that is
  never released" and the "no ... heartbeat, or release" list, lines 168-171),
  the claim bullets for exit `0` and the completed steal (what is kept and for
  how long), the launch block (starting the script), and the closing paragraph
  (launch failure, release on exit, what a harness run started meanwhile sees).
- `worker.md` `#### Release-only recovery` intro: only the wording that calls
  the Normal-launch lock "never-released"; a `normal-launch` holder stays an
  early exit in step 44.

## Out of scope
- The landed change the session produced
- `Invoke-HarnessClaim.ps1`, whose existing stale-holder break (lines 384-390)
  stays the fallback for a release-on-exit script that died;
  `Invoke-HarnessRelease.ps1`, `Invoke-HarnessLaunch.ps1`, and the AgentHarness
  `lock` C++
- Worker steps 5-15 and 39-42, and the Normal launch semantics other than the
  lock's release and heartbeat: normal windows, per-user AppData, no agent-only
  arguments, and processes the session never quits or stops
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Tier 2, trigger: scoped tool behavior of one subsystem (the agent-harness skill
and its scripts); escalate if the fix reaches build/bootstrap coordination.
Invariants: the script's heartbeats and its release run only with the Normal
launch's own owner token, so they can never free or refresh another owner's
lock; a script that is no longer running stamps nothing, so its holder goes
stale under the existing 300 s rule; no game process is quit or stopped by the
session or the script; no determinism, CRC, wire, serialization, or C++
exposure. Never embed transcript paths or home paths.

## Acceptance criteria
- Live `/agent-harness` check. Setup, as the observing session recorded it:
  run the `## Normal launch` recipe for BrokenEngineSandbox, let the claiming
  agent turn end, then the user closes the client and the server windows. Within
  a few seconds of the last window closing,
  `& '<AgentHarness path>' lock status --key default` prints `{"held":false}`.
- A second Normal launch started within five minutes of the first one's claim,
  after the user closed both windows, gets exit `0` from `Invoke-HarnessClaim.ps1`
  on its first attempt, with no `claim.foreign-owner` and no takeover steps.
- A harness run's claim made while the user's game is still running waits, and
  once the user closes the game it gets exit `0` from `Invoke-HarnessClaim.ps1`,
  with no `claim.foreign-owner` and no takeover steps. When the game stays open
  past one invocation's 480 s wait budget, an invocation ends with
  `claim.wait-timeout` and is re-invoked as worker step 7 documents (as
  `launch.md` line 203 does for a Normal launch); exit `0` arrives after those
  re-invocations.
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing.
