<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-15T19:17:09.430Z","dependsOn":[]} -->
# Fix: agent-harness / AgentHarness.md — fixed AppData root contaminates fresh-state comparisons across runs

## Context

The runtime acceptance for
`Documents/Plans/Graphics/CameraUpdateDecomposition.md` required a pre/post
zoom-and-jump comparison. The documented launch block in
`Projects/BrokenEngineSandbox/Documents/AgentHarness.md:15-36` fixes
`$AppDataRoot` to `Temp\AppData`, and the generic launch guidance in
`.agents/skills/agent-harness/SKILL.md:69-73` leaves the project document to
define fresh-state notes. The run used the documented claim command:

`pwsh -NoProfile -File .agents/skills/agent-harness/scripts/Invoke-HarnessClaim.ps1 -RepositoryRoot $ROOT -Session 'postchange-camera-comparison'`

followed by the documented `Start-Process` launch with
`--app-data-directory $QuotedAppData`. The server `reset` command reset server
state but did not clear the client's persisted `ClientState.bin`. The runtime
load path is durable evidence of the behavior: `Engine/Source/Main.cpp:275-276`
calls `game::LoadClientState()`, and
`Projects/BrokenEngineSandbox/Source/ClientSettings.cpp:362-379` reads the
versioned client state and applies its saved camera height target.

The post run therefore loaded the previous run's camera zoom target: the
pre-run settled eye.z `0x43BDECA5` became the post-run pre-wheel eye.z, and the
same `mouse` background wheel command with `notches: -2` produced post eye.z
`0x43CB51F8`. `Temp/PostChangeCameraComparison/camera-zoom-jump-comparison.json`
records `status: comparison-failure` and the failed required-eye-z criterion
(SHA-256
`1c6ca36577d066755886e062e4b40de7657039f8efc4d021eff2106e18388a07`). This
forced diagnosis and a repeated post run with a distinct fresh root,
`Temp/PostChangeFreshAppData`, where the artifact records no `ClientState.bin`
before launch and the comparison passes with eye.z `0x43BDECA5` and derived
height `0x43BAECA5` (`Temp/PostChangeFreshCameraComparison/camera-zoom-jump-comparison.json`,
SHA-256
`c084ce46c573e3ad317c0553d0fe8cd3ff257cc870a342d5e172fcd25b967240`). The
pre-change baseline used for that comparison is
`Temp/PreChangeCameraBaseline/camera-zoom-jump-baseline.json` (SHA-256
`5243ca0e0a70bb35a77a1173da44e5b9d1d5b36c35259c2b0c88bdb6c3e330f2`).

The claimed Camera Plan changes only
`Projects/BrokenEngineSandbox/Source/Graphics/Camera.cpp` and `Camera.h`;
its `## In scope` contains no harness launch, AppData, or client-state work.
The false required condition was that a documented fresh-state comparison was
fresh after `reset` while the fixed AppData root silently retained client
settings. The fresh-root rerun passed, so this is `/next-plan` tooling
friction rather than an active acceptance failure.

Session provenance (machine-local; not reproducible after cleanup):
- Client: codex
- Conversation session ID: none
- Worktree/branch UUID: 5d4601dd-f02c-412e-89ef-c1ce07e5e022
- Session branch: codex/5d4601dd-f02c-412e-89ef-c1ce07e5e022
- Worktree: .codex\worktrees\BrokenEngine\5d4601dd-f02c-412e-89ef-c1ce07e5e022
- Landing ref: the session branch above, whose tip is the session's final
  commit and which survives exactly as long as the worktree recorded above.
  Fallback once that branch is gone:
  `git log --diff-filter=A --format=%H -- <this plan path>`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to this session alone
  (its diff limited to this session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes this worktree: Codex
  transcript discovery requires the producing worktree to remain registered,
  and Claude review requires the exact conversation session ID above.

## Design

In a new session, run `/next-plan-review <landing ref>` supplying the recorded
Codex client and landing ref. Root-cause the friction from the proven
transcript, then make the smallest fix inside the `## In scope` boundary below.
The fix must make the documented fresh-state comparison establish an isolated
client AppData state instead of silently inheriting a prior run's settings,
while preserving the ordinary shared-root launch behavior for scenarios that
deliberately need persistence. If root-causing shows the fix lies outside the
two launch-documentation sections below, surface it for re-planning instead of
expanding scope.

## Critical files

- `.agents/skills/agent-harness/SKILL.md` — `## Launch`, especially the
  AppData override and project fresh-state guidance (`:69-73`)
- `Projects/BrokenEngineSandbox/Documents/AgentHarness.md` — `## Launch`,
  especially the fixed `$AppDataRoot` setup and its persistence paragraph
  (`:15-36`)

## In scope

- Root-cause investigation via `/next-plan-review`, run with the recorded
  landing ref and Codex client; a Codex review supplies the client and landing
  ref only
- The smallest resulting fix, confined to the `## Launch` guidance in the two
  critical files above, so a documented fresh-state comparison cannot silently
  reuse persisted client settings

## Out of scope

- The landed Camera change and `Documents/Plans/Graphics/CameraUpdateDecomposition.md`
- `Engine/Source/Main.cpp`,
  `Projects/BrokenEngineSandbox/Source/ClientSettings.cpp`, server `reset`,
  client-state file format, and any runtime persistence implementation
- Claim, wait, release, or other bundled harness scripts; packed data,
  replay, wire/protocol, CRC, serialization, threading, allocation, shader,
  and build/bootstrap changes
- Unrelated skills or project documents; any transcript path or transcript
  text in the repository

## Risk tier and invariants

Expected Tier 2 (scoped harness tool/document behavior); Tier 1 if the accepted
fix is documentation only. Escalate if the fix reaches runtime persistence or
build/bootstrap coordination. The documented comparison must use an isolated
fresh client state, deliberate persistence scenarios must remain possible, and
no determinism/CRC, serialization/`.pack`, replay, wire, threading,
allocation, shader, or build contract may change. Never embed transcript paths
or home paths.

## Acceptance criteria

- The recorded symptom no longer reproduces under the documented invocation:
  a pre/post zoom-and-jump comparison does not inherit a prior run's
  `ClientState.bin` through the default fresh-state recipe
- The documented fresh-state setup records an absent or otherwise explicitly
  reset client state before launch, and the repeated comparison reaches the
  expected eye.z/height bits without the forced rerun
- Persistence-dependent scenarios retain an explicit documented path rather
  than being silently reset
- `/validate-skill` passes for any changed `SKILL.md`; WorktreeCli `plan
  validate` exits `0` with `status: valid` and `code: ok`

## Notes

This Plan is keyed to the pair (agent-harness / BrokenEngineSandbox
`AgentHarness.md` launch path, fixed `Temp\\AppData` cross-run client-state
contamination of a documented fresh-state comparison). A later observation of
the same pair is a duplicate, not a new residual.

`Documents/Plans/Agents/AgentHarnessOwnerTokenReclaimLoop.md` covers refreshed
owner-token bookkeeping across stop/stage/re-claim iterations; it shares the
project document but has a different symptom, implementation boundary, and
verification strategy, so it is not a duplicate.
