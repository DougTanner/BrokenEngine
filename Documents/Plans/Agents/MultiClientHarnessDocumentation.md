<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-20T19:36:27.499Z","dependsOn":[]} -->
# Fix: /agent-harness — no documented way to run two simultaneous clients

## Context

Observed during this session's `/agent-harness` acceptance run for
`Documents/Plans/Engine/ServerTimescalePacketToEngine.md`. The acceptance
criterion required a late-joining second client while a first client stayed
connected. Disconnect/reconnect cannot substitute, because
`ServerSessionRuntime::ResetOnLastClientLeave` resets the server timescale to
`1/1` when the only client leaves, which destroys the state the criterion has
to observe.

Both harness surfaces document exactly one client: the skill fixes the client
agent port at `27101` (`.agents/skills/agent-harness/SKILL.md:14`, and the
`Wait-IslandSceneReady.ps1` invocation at `SKILL.md:99`), and the launch
recipe in `Projects/BrokenEngineSandbox/Documents/AgentHarness.md:36` passes
`--agent-port 27101` with a single shared `--app-data-directory` root
(`AgentHarness.md:17-37`), with the readiness sequence at `AgentHarness.md:47`
also fixed to `27100` then `27101`. No documented invocation exists for a
second concurrent client, so the run had to deviate from documented usage.

The deviation succeeded and is what this Plan proposes to document: the second
client was launched on agent port `27102` with its own app-data root
(`Temp/AppData-TimescaleB`) and its own log file, waited on with
`pwsh -NoProfile -File .agents/skills/agent-harness/scripts/Wait-HarnessPing.ps1 ... -Port 27102`,
and quit through its own port before the standard
`Invoke-HarnessRelease.ps1` call, which then returned `status:pass`.
`Invoke-HarnessRelease.ps1:11` accepts only one `-ClientPort`, so the extra
client must be quit before release or it is left running.

Secondary observation from the same run: a client's `Network` log level can
only be raised after that client connects, so handshake-time `kDebug` lines are
unloggable for a late-joining client. Evidence for the adopted timescale ratio
had to come from state queries instead.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 357139aa-01d5-4f78-ba58-11bb765439db
- Worktree/branch UUID: 65bfe19b-6fc8-4440-9423-553f7c9d3ac0
- Session branch: claude/65bfe19b-6fc8-4440-9423-553f7c9d3ac0
- Worktree: .claude\worktrees\BrokenEngine\65bfe19b-6fc8-4440-9423-553f7c9d3ac0
- Landing ref: claude/65bfe19b-6fc8-4440-9423-553f7c9d3ac0
  Fallback once the recorded ref is gone:
  `git log --diff-filter=A --format=%H -- Documents/Plans/Agents/MultiClientHarnessDocumentation.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design

In a new session, run `/next-plan-review claude/65bfe19b-6fc8-4440-9423-553f7c9d3ac0`,
supplying client `claude` and conversation session ID
`357139aa-01d5-4f78-ba58-11bb765439db`. Root-cause the friction from the proven
transcript — confirming the exact second-client launch, readiness, and shutdown
commands that worked and whether the app-data-root separation was actually
required — then make the smallest resulting documentation fix inside the
`## In scope` boundary below. If root-causing shows the fix lies outside that
boundary (for example, that `Invoke-HarnessRelease.ps1` must learn about extra
client ports rather than the documentation instructing an explicit quit first),
surface it for re-planning instead of expanding scope.

## Critical files

- `Projects/BrokenEngineSandbox/Documents/AgentHarness.md` — launch recipe
  (lines ~17-37), readiness sequence (line ~47)
- `.agents/skills/agent-harness/SKILL.md` — port statement (line ~14),
  readiness/scene-ready invocations (lines ~91-99), release step
- `.agents/skills/agent-harness/references/command-reference.md` — only if the
  documented multi-client convention belongs in the command reference

## In scope

- Root-cause investigation via `/next-plan-review`, run with the recorded
  client and the landing ref named in `## Design`, plus the recorded
  conversation session ID
- The smallest resulting documentation change, confined to the files named
  above: a documented convention for running a second simultaneous client
  (its own agent port `27102`, its own `--app-data-directory` root, its own log
  file, its own `Wait-HarnessPing.ps1 -Port` readiness wait, and an explicit
  quit through its own port before the standard `Invoke-HarnessRelease.ps1`
  call, which covers only `27100` and `27101`)
- Optionally, a short note recording that a client's `Network` log level can be
  raised only after that client connects, so handshake-time `kDebug` lines are
  unavailable for a late-joining client

## Out of scope

- The landed change the observing session produced
  (`Documents/Plans/Engine/ServerTimescalePacketToEngine.md`)
- Any C++ or engine change, including the connect-time log-level ordering and
  `ServerSessionRuntime::ResetOnLastClientLeave`
- Changing `Invoke-HarnessRelease.ps1`, `Wait-HarnessPing.ps1`, or any other
  bundled harness script's parameters or behavior
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants

Expected Tier 2: it adds a documented invocation to the `/agent-harness` skill
surface, which the root `AGENTS.md` Step 2 trigger classifies as skill behavior
rather than documentation. It is Tier 1 only if root-causing shows the fix is
purely clarifying prose that changes no documented invocation. Escalate if the
fix reaches build/bootstrap coordination or a bundled script. Never embed
transcript paths or home paths.

## Acceptance criteria

- `Projects/BrokenEngineSandbox/Documents/AgentHarness.md` and
  `.agents/skills/agent-harness/SKILL.md` agree on one convention for a second
  simultaneous client, so the recorded symptom — a two-client scenario with no
  documented invocation — no longer reproduces
- The documented convention names the second client's agent port, app-data
  root, log file, readiness wait, and the explicit quit that must precede
  `Invoke-HarnessRelease.ps1`
- `/validate-skill` passes for the changed `.agents/skills/agent-harness/SKILL.md`;
  `WorktreeCli plan validate` exits `0`
