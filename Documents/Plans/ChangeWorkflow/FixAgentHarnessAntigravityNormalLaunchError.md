<!-- broken-engine-plan/v2 {"createdUtc":"2026-11-26T20:10:00.563Z","dependsOn":[]} -->
# Fix: .agents/skills/agent-harness — 0x800700e8 launch errors and helper exits

## Context
During interactive testing on Antigravity CLI (`agy`) on 2026-11-26, three
distinct launch failures were observed across two launch paths in
`.agents/skills/agent-harness/`:

- Normal-launch server start (`references/launch.md`, `## Normal launch`, line
  337): `Start-Process` launching the Debug server executable
  (`BrokenEngineSandboxServer.Debug.exe`) with `--loopback-only` and
  `--data-directory <gameDataDirectory>` failed with Windows error `2147942632`
  (`0x800700e8` / `ERROR_NO_DATA`, pipe being closed).
- PowerShell probe during the same normal-launch sequence: launching PowerShell
  7 (`pwsh.exe`) with `-NoProfile -Command Start-Sleep 15` also failed with
  Windows error `2147942632` (`0x800700e8` / `ERROR_NO_DATA`).
- Earlier separate-shell harness launches: server and client launches via
  `scripts/Invoke-HarnessLaunch.ps1` (line 156, `Start-Process`) exited when
  Antigravity CLI's `run_command` shell returned.

Hypotheses to test during root-cause investigation include whether Antigravity
CLI's `run_command` enforces a kill-on-close Windows Job Object, closes
inherited ConPTY or pipe handles on shell exit, or interacts with PowerShell
`Start-Process` handle inheritance.

In the current tree, `launch.md` (`## Launch`, lines 14–21; `## Normal launch`,
lines 262–266 and 337–343) already conditions execution on host process/window
lifetime and provides a user desktop-launch fallback when the host cannot keep
processes alive, but does not yet account for the `0x800700e8` launch error or
define a verified Antigravity CLI launch or fallback rule for these failures.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: agy
- Conversation session ID: 2cff7567-eff1-45a8-a2e1-30a4d0840f36
- Worktree/branch UUID: 4b97191b-8d41-4c51-85ec-f11a415112ae
- Session branch: agy/4b97191b-8d41-4c51-85ec-f11a415112ae
- Worktree: .agy/worktrees/BrokenEngine/4b97191b-8d41-4c51-85ec-f11a415112ae
- Landing ref: agy/4b97191b-8d41-4c51-85ec-f11a415112ae.
  The review ref the invoker passes to `/next-plan-review` has two cases. When
  the `Observed in an earlier session` line is present, it is that line's
  landed commit hash, with no lookup. When that line is absent, the recording
  session is the observing session, and worktree reuse moves its branch past
  its landing, so the invoker never reviews the ref itself: it resolves the ref
  to the commit that added this Plan with
  `git log --follow --diff-filter=A -n 1 --format=%H <ref> -- <this plan path>`
  and passes that commit. When the recorded ref is gone or that command prints
  nothing (a later session moved this Plan), it runs the same command with
  `<ref>` omitted. A periodic Plan-history squash can make either command
  return an unrelated aggregate commit, and a landing that also deletes a
  similar file can make it return another session's commit, so review the
  result only when the commit is attributable to the observing session recorded
  above alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before `/cleanup-worktrees` removes the worktree recorded
  above: Codex discovery requires a registered worktree, and Claude or agy
  review requires the conversation session ID above.

## Design
First confirm the root cause of the `0x800700e8` (`ERROR_NO_DATA`) launch
errors and the separate-shell helper exits on Antigravity CLI through
`/external-diagnose-bug` from the current tree and this Plan's `## Context`,
testing the Job Object, ConPTY/pipe inheritance, and PowerShell `Start-Process`
hypotheses separately for each observation. When diagnosis genuinely needs
transcript evidence from the observing session for an observation, in a new
session run `/next-plan-review <review ref>` in bounded friction mode for that
single observation, passing the review ref the provenance block above defines,
the recorded client (`agy`), the recorded conversation session ID, and the one
recorded friction under investigation (invoking once per observation that needs
transcript evidence so bounded friction mode applies rather than a full audit).

Then make the smallest fix inside the `## In scope` boundary below. The author
recommends evaluating whether a detached or job-breakaway launch mechanism (or
an explicit Antigravity-compatible launch or fallback rule in `launch.md` and
`Invoke-HarnessLaunch.ps1` if the host enforces kill-on-close Job Objects)
cleanly prevents both `0x800700e8` and unexpected child exit, without weakening
lock ownership, single-instance checks, or process-check tracking. If
root-causing shows the fix requires changes outside the `## In scope` boundary
below — including other files under `.agents/skills/agent-harness/`,
`Tools/AgentHarness/`, or host configuration — surface it for re-planning
instead of expanding scope.

## Critical files
- `.agents/skills/agent-harness/references/launch.md`
- `.agents/skills/agent-harness/scripts/Invoke-HarnessLaunch.ps1`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to:
  - `.agents/skills/agent-harness/references/launch.md` (`## Launch` and
    `## Normal launch`)
  - `.agents/skills/agent-harness/scripts/Invoke-HarnessLaunch.ps1` (the
    `Start-Process` launch and post-launch registration/readiness block around
    lines 144–175)

## Out of scope
- The landed change the observing session produced
- Engine, game, or `Tools/AgentHarness/` C++ source changes
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior under `.agents/references/risk-tiers.md`,
as it modifies harness launch script/recipe behavior without altering
build/bootstrap coordination or engine invariants); escalate to Tier 3 if
re-planning reaches build/bootstrap coordination or cross-subsystem integration.
Preserve single-instance server checks, lock ownership/heartbeat/release
semantics, and process-check registration invariants. Never embed transcript
paths or home paths.

## Acceptance criteria
Ordered reproduction setups from the observing session:
1. Normal-launch server error: on Antigravity CLI (`agy`), run the
   `## Normal launch` claim block in `launch.md`
   (`Invoke-HarnessClaim.ps1 -Session 'normal-launch' -Project 'BrokenEngineSandbox'`),
   then run the `## Normal launch` pair-start block (`launch.md:318–355`,
   `Start-Process` launching `BrokenEngineSandboxServer.Debug.exe` with
   `--loopback-only` and `--data-directory <gameDataDirectory>`).
2. PowerShell probe error: in the same Antigravity CLI `run_command`
   environment during that sequence, launch `pwsh.exe` with
   `-NoProfile -Command Start-Sleep 15` (the supplied evidence records no
   further harness command or UI setup for this probe).
3. Separate-shell helper exits (independent harness run): after any server,
   client, and harness claim from setup 1 are stopped and released so no
   `BrokenEngineSandboxServer*` or client process is running, on Antigravity
   CLI (`agy`) acquire a fresh harness claim and run paths per `launch.md`
   `## Launch`, then invoke `Invoke-HarnessLaunch.ps1` with its full required
   claim and path parameters (`-RepositoryRoot`, `-Role server`,
   `-GameDataDirectory`, `-AgentHarness`, `-Owner`, `-ProcessCheckStatePath`,
   `-AppDataDirectory`, `-LogFile`, `-GameName`; `Start-Process` at line 156)
   in one `run_command` shell, verify server survival after shell exit with a
   later owned ping and identity check, then invoke `-Role client` with the
   same claim and run paths in a separate `run_command` shell, verify client
   survival after shell exit with a later owned ping and identity check, and
   clean up via the release block (the supplied evidence records no additional
   UI actions).

Acceptance conditions:
- Executing setup 1 per the documented normal-launch path on Antigravity CLI no
  longer produces `error 2147942632 (0x800700e8)` and either leaves the server
  and client running after the launch command completes or deterministically
  routes to the documented user desktop-launch path without partial orphan
  launches or leaked harness locks
- Setup 2 (`pwsh.exe -NoProfile -Command Start-Sleep 15`) is exercised during
  diagnosis and verification: either it no longer produces `0x800700e8` under
  the fixed invocation, or, if root-causing proves the probe error is an
  Antigravity CLI host defect outside `## In scope`, that out-of-scope
  host-defect disposition is recorded explicitly without claiming the fix
  resolved it
- Executing setup 3 as an independent claimed run succeeds without
  `launch.server-running` refusal or unexpected child exit on shell close, with
  positive post-exit survival evidence (owned ping and identity check for both
  server and client after their launch shells exit), or — if diagnosis proves
  the host cannot keep processes alive across `run_command` exit —
  deterministically routes to the documented user desktop-launch fallback in
  `launch.md` without partial orphan launches or leaked harness locks
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing

## Notes
- Filed at the user's explicit request ("File a bug report") after both
  `0x800700e8` errors occurred during a normal-launch attempt on Antigravity
  CLI.
- Related pre-existing `Documents/Investigations/ChangeWorkflow/WorkflowIncidents.md`
  rows (two rows as of 2026-11-26 for `.agents/skills/agent-harness` —
  `server exits when the launch shell ends`) record the same shell-close
  termination symptom for `Invoke-HarnessLaunch.ps1 -Role server` on `agy`;
  those rows were left untouched because this Plan was filed directly from the
  user's bug report rather than a three-row incident conversion.
