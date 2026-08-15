<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-15T12:28:41.995Z","dependsOn":[]} -->
# Fix: agent-harness / AgentHarness.md — a repeated stop, stage, re-claim loop leaves the refreshed owner token to manual bookkeeping

## Context

The stopped-server AppData scenarios require staging files under the harness
AppData root while the server is not running. The
`#### Replay manifest v3 integrity matrix` section of
`Projects/BrokenEngineSandbox/Documents/AgentHarness.md` states that discipline
directly: stop and release the server, back up the artifacts under
`$AppDataRoot\Broken Engine Sandbox Server`, restore the backup before each
change, and relaunch. Each scenario in that matrix is therefore one iteration of
release, stage on disk, re-claim, relaunch, observe.

The harness identity does not survive that loop by itself.
`.agents/skills/agent-harness/SKILL.md:38` holds one owner token across
relaunches and directs a caller to "quit and release before the phase, then
reclaim afterward" for a long non-harness phase, and `:30-36` mints a fresh
`owner` token on every `Invoke-HarnessClaim.ps1` call. Every socket command,
`Wait-HarnessPing.ps1`, `Wait-IslandSceneReady.ps1`, and
`Invoke-HarnessRelease.ps1` requires that current token. The documented recipes
cover a single claim/release run end to end; neither the skill nor the project
harness doc states how the refreshed token is carried into the next iteration's
invocations.

The forced rework: for each scenario iteration the worker released the lock,
staged the modified files, re-claimed, and then hand-edited the newly minted
owner GUID into its local helper invocations before the next launch. That is
manual identity bookkeeping repeated once per scenario, and a stale GUID left in
one invocation produces an ownership failure — SKILL.md treats an owner mismatch
as a hard stop — rather than the scenario result under test, so a bookkeeping
slip is easy to misread as a scenario failure.

The claimed Plan for this run was
`Documents/Plans/Agents/AgentHarnessModifiedReplayStaging.md`, whose `## In scope`
covers only the `#### Replay manifest v3 integrity matrix` section's staging
recipe plus, conditionally, one new agent-harness staging script — a condition
that was not taken. The claim/release identity path named below is outside that
boundary, so this is `/next-plan` tooling friction rather than an in-scope
acceptance failure.

Session provenance (machine-local; not reproducible after cleanup):
- Client: claude
- Session: 7b66a9dc-883e-4dd2-a8a1-10814ed90ee4
- Subagent task: a1f49c4126461f1a0
- Session branch: claude/68da05af-c6e0-4e32-979d-87ba545ba868
- Worktree: .claude\worktrees\BrokenEngine\68da05af-c6e0-4e32-979d-87ba545ba868
- Landing commit: `git log --diff-filter=A --format=%H -- <this plan path>`
- Run the review before /cleanup-worktrees removes this worktree: Codex
  transcript discovery requires the producing worktree to remain registered,
  and Claude review requires the exact session id above.

## Design

In a new session, run `/next-plan-review <landing commit>` supplying the
recorded client and session id, root-cause the friction from the proven
transcript, then make the smallest fix inside the `## In scope` boundary below.
If root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

The outcome to deliver: an agent running repeated stop/stage/re-claim iterations
follows one documented loop in which the refreshed owner token reaches every
subsequent invocation without hand-editing a GUID. Two candidate shapes are
visible from the symptom and root-causing decides between them — they are
alternatives, not a set to implement together: a short stated loop beside the
existing stopped-server staging discipline that fixes where the refreshed token
is captured and how each per-iteration invocation reads it, or a bundled driver
under `.agents/skills/agent-harness/scripts/` that performs one release, stage,
re-claim, relaunch iteration and returns the current owner token in its typed
result. Either shape keeps one claim per iteration and the existing
release-before-staging requirement.

## Critical files

- `Projects/BrokenEngineSandbox/Documents/AgentHarness.md` — the
  `#### Replay manifest v3 integrity matrix` section, which owns the
  stopped-server backup, staging, and relaunch discipline the loop wraps
- `.agents/skills/agent-harness/SKILL.md` — the `## Provision and claim` section
  (`:30-38`) that mints the owner token and states the quit/release/reclaim rule,
  and the `## Lifecycle and release` section (`:119-127`) whose release call
  consumes that token; read-only unless the chosen shape adds a bundled script
  that must be named there
- `.agents/skills/agent-harness/scripts/` — the bundled-script location, relevant
  only if root-causing selects the bundled-driver shape

## In scope

- Root-cause investigation via `/next-plan-review` with the recorded provenance
- The smallest resulting fix, confined to the
  `#### Replay manifest v3 integrity matrix` section of
  `Projects/BrokenEngineSandbox/Documents/AgentHarness.md` plus, only if
  root-causing selects the bundled-driver shape, one new script under
  `.agents/skills/agent-harness/scripts/` and the `agent-harness/SKILL.md`
  sentence that names it

## Out of scope

- The landed change this session produced, and its claimed Plan
- Any change to owner-token minting, lock storage, lease duration, heartbeat
  semantics, the no-steal rule, or the five-minute staleness definition
- The existing H1-H6 rejection cases, the manifest format, and the artifact
  staging repair itself, which stay exactly as they are and are owned by
  `Documents/Plans/Agents/AgentHarnessModifiedReplayStaging.md`
- Any harness command, response field, or schema change
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants

Expected Tier 2 (scoped harness tool behavior and its verification
documentation); Tier 1 if the accepted fix is documentation only. Escalate if the
fix reaches the lock implementation or shared build/bootstrap coordination — that
is outside the boundary above and is surfaced for re-planning instead. The loop
must keep the server stopped and the lock released while files are staged, must
never hold or reuse a released token, must never steal from or disturb a foreign
owner, and must preserve the backup-and-restore discipline so a staged
modification never becomes permanent. Never embed transcript paths or home paths.

## Acceptance criteria

- The recorded symptom no longer reproduces: consecutive stop, stage, re-claim,
  relaunch iterations run from the documented procedure with no hand-edited owner
  GUID in any invocation
- Each iteration's socket commands, readiness wait, and release use that
  iteration's current owner token, and no invocation carries a released token
- Files are staged only while the server is stopped and the lock is released, and
  the existing backup-and-restore discipline is preserved
- `/validate-skill` passes for any changed `SKILL.md`; WorktreeCli
  `plan validate` exits `0` with `status: valid` and `code: ok`

## Notes

This Plan is keyed to the pair (`agent-harness` stop/stage/re-claim loop, manual
owner-token bookkeeping per iteration). A later observation of the same pair is a
duplicate, not a new residual.
`Documents/Plans/Agents/AgentHarnessModifiedReplayStaging.md` covers what to do
to the artifact and manifest bytes once the server is stopped, and
`Documents/Plans/Agents/AgentHarnessClaimWaitPath.md` covers waiting for a
foreign owner to release; neither is a duplicate of this one.

`Documents/Plans/Agents/AgentHarnessModifiedReplayStaging.md` edits the same
`#### Replay manifest v3 integrity matrix` section of
`Projects/BrokenEngineSandbox/Documents/AgentHarness.md`. The two are not
directional — either may land first — but whichever lands second re-reads that
section as it then stands and keeps the other's text intact: that Plan owns the
artifact and manifest repair steps, this Plan owns the surrounding
release/stage/re-claim loop and its owner-token handling.
