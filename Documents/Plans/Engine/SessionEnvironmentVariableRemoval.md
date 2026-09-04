<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-03T22:57:44.162Z","dependsOn":[]} -->
# Retire the remaining session-identity environment variables

## Context

The session wrapper `.agents/scripts/Start-AgentWorktreeSession.ps1` publishes session identity to the
agent process through `BROKEN_ENGINE_*` process environment variables. The user directed that session
state live in worktree-local files instead of environment variables ("We shouldn't be using env vars,
but temp files saved in the worktree"). The `FinalizeLandingAssertions` change carried out the first
half of that direction for one variable only: it moved the baseline hint into the gitignored
worktree-local file `Temp/session-baseline` and removed `BROKEN_ENGINE_BASELINE` entirely. The other
session-identity variables were proven out of that change's scope and are recorded here.

All line citations below are taken from commit `b047dc9a805dae35ef09bd70bedfbe2c8306c4aa`, read with
`git show b047dc9a:<path>`. `Start-AgentWorktreeSession.ps1` and
`.agents/skills/compile/scripts/Resolve-CompileContext.ps1` were being edited concurrently by the
`FinalizeLandingAssertions` change when this Plan was authored, so the implementer re-locates each
site by symbol and text rather than trusting the line numbers, and expects the
`BROKEN_ENGINE_BASELINE` sites named in those citations to be gone already.

Verified current state at `b047dc9a`, six session-identity variables besides the baseline:

- `BROKEN_ENGINE_SESSION_OWNER` — write `Start-AgentWorktreeSession.ps1:48`; no reader anywhere in the
  repository outside the save/restore array entry at `:32`.
- `BROKEN_ENGINE_WORKTREE_PATH` — write `Start-AgentWorktreeSession.ps1:49`; one reader,
  `Resolve-CompileContext.ps1:169` (`$rootInput = Select-ContextInput $RepositoryRoot
  'BROKEN_ENGINE_WORKTREE_PATH'`).
- `BROKEN_ENGINE_SESSION_BRANCH` — write `Start-AgentWorktreeSession.ps1:50`; no reader.
- `BROKEN_ENGINE_PRIMARY_CHECKOUT` — write `Start-AgentWorktreeSession.ps1:51`; one reader,
  `Resolve-CompileContext.ps1:180` (`$primaryInput = Select-ContextInput $PrimaryCheckout
  'BROKEN_ENGINE_PRIMARY_CHECKOUT'`).
- `BROKEN_ENGINE_TARGET_BRANCH` — write `Start-AgentWorktreeSession.ps1:52`; no reader. The same value
  is already persisted to the worktree as `targetBranch` in `Temp/session-sidecar.json`
  (`broken-engine-session-sidecar/v1`), written at `Start-AgentWorktreeSession.ps1:154-155` and read by
  `Get-AgentWorktreeSessionContext` in `AgentWorktreeSession.psm1`.
- `BROKEN_ENGINE_AGENT_CLIENT` — write `Start-AgentWorktreeSession.ps1:54`; no reader.

All six are written by `function Set-AgentWorktreeEnvironment` (`:47-55`) and round-tripped by the
save/restore machinery at `:30-44` (`$environmentNames` array, `$previousEnvironment` hashtable, and
`Restore-AgentWorktreeEnvironment`, called from the `finally` block at `:205`).

Two further variables are deliberately not part of this work. `BROKEN_ENGINE_MSBUILD_PATH` and
`BROKEN_ENGINE_BUILD_LOCK_WAIT_SECONDS` are externally set build pins read by
`Tools/WorktreeCli/BuildCommand.cpp`, not session identity.

## Design

The author's recommendation is to delete all six variables rather than convert any of them to a file,
because a value nobody reads needs no storage at all and the two that are read have an existing,
equally authoritative source.

- Four variables have no reader in the repository: `BROKEN_ENGINE_SESSION_OWNER`,
  `BROKEN_ENGINE_SESSION_BRANCH`, `BROKEN_ENGINE_TARGET_BRANCH`, and `BROKEN_ENGINE_AGENT_CLIENT`.
  The recommendation is to delete their assignments and their `$environmentNames` entries and write no
  replacement file. Writing a worktree file for a value with no consumer would satisfy the letter of
  the user's direction while adding an unused second source of truth. The target branch specifically
  already has its worktree-local home in `Temp/session-sidecar.json`, so a consumer that needs it later
  reads the sidecar through `Get-AgentWorktreeSessionContext`.
- Two variables are read, and only as fallback hints inside `Resolve-CompileContext.ps1`, which already
  derives the same two values from Git when the hint is absent: the repository root from
  `git -C $PSScriptRoot rev-parse --show-toplevel` (`Resolve-CompileContext.ps1:170-172`) and the
  primary checkout from the parent of `git rev-parse --path-format=absolute --git-common-dir`
  (`:181-184`). The recommendation is to delete both `Select-ContextInput` environment-variable
  arguments so the explicit parameter is followed directly by that derivation, and to delete the
  wrapper's two assignments. Rationale: the script derivation runs from the script's own location, and
  the canonical invocation form in the root `AGENTS.md` requires running the repo-relative script path
  from the session worktree root, so the derived root is that same session worktree and the derived
  primary is that worktree's Git common directory — the identical values the wrapper was exporting. A
  worktree-local file naming the worktree it sits in would be a second source of truth for something
  Git answers directly.
- With the six removed, `$environmentNames` holds only `BROKEN_ENGINE_CLIENT_ARGUMENTS`. The
  recommendation is to collapse the array plus hashtable plus `Set-AgentWorktreeEnvironment` into a
  single saved string for that one variable, restored in the existing `finally` block, and to delete
  `Set-AgentWorktreeEnvironment` and its call at `:148` because its body becomes empty. The explicit
  clear at `:181` stays: it must run before the client launches so the child process does not inherit
  the transport value.
- `BROKEN_ENGINE_CLIENT_ARGUMENTS` stays an environment variable. `.claude/claude-worktree.sh:36`
  exports it and `Start-AgentWorktreeSession.ps1:18` consumes it before any session worktree has been
  created or reattached, so there is no worktree to hold a file at that moment. It is a one-hop
  Bash-to-PowerShell argument transport, not session state, and the comment at `:14-16` records why the
  arguments cannot travel as `pwsh -File` parameters.
- `Select-ContextInput` keeps its current signature and behavior if any other call site still passes an
  environment-variable name after the `FinalizeLandingAssertions` change; if no call site does, the
  author recommends simplifying it to the explicit-input check rather than leaving an unused parameter.

## Critical files

- `.agents/scripts/Start-AgentWorktreeSession.ps1` — `$environmentNames`/`$previousEnvironment`/
  `Restore-AgentWorktreeEnvironment` (`:30-45`), `Set-AgentWorktreeEnvironment` (`:47-55`) and its call
  (`:148`), and the `finally` block (`:204-206`).
- `.agents/skills/compile/scripts/Resolve-CompileContext.ps1` — the two `Select-ContextInput` calls at
  `:169` and `:180`, and `Select-ContextInput` itself.
- `.agents/skills/compile/references/worker.md` — the identity-precedence prose at `:230-231` naming
  `BROKEN_ENGINE_WORKTREE_PATH` and `BROKEN_ENGINE_PRIMARY_CHECKOUT`.
- `.agents/scripts/AgentWorktreeSession.psm1` — read-only reference for the sidecar contract; this Plan
  recommends no change to it.

## In scope

- Deleting the six assignments in `Set-AgentWorktreeEnvironment` and the six matching
  `$environmentNames` entries in `.agents/scripts/Start-AgentWorktreeSession.ps1`, and collapsing the
  now single-variable save/restore machinery for `BROKEN_ENGINE_CLIENT_ARGUMENTS`, including deleting
  `Set-AgentWorktreeEnvironment` and its call site when its body becomes empty.
- Removing the `'BROKEN_ENGINE_WORKTREE_PATH'` and `'BROKEN_ENGINE_PRIMARY_CHECKOUT'` arguments from the
  two `Select-ContextInput` calls in `.agents/skills/compile/scripts/Resolve-CompileContext.ps1`, and
  simplifying `Select-ContextInput` only if it retains no environment-variable call site.
- Updating the identity-precedence prose in `.agents/skills/compile/references/worker.md` that names the
  two removed variables.
- A repository-wide search for any remaining reference to the six names, in scripts, skills, references,
  and AGENTS.md files, and removing each stale mention found.

## Out of scope

- `BROKEN_ENGINE_CLIENT_ARGUMENTS`: its export, decode, clear, and comments stay exactly as they are.
- `BROKEN_ENGINE_MSBUILD_PATH` and `BROKEN_ENGINE_BUILD_LOCK_WAIT_SECONDS`, their readers in
  `Tools/WorktreeCli/BuildCommand.cpp`, and their documentation in `Tools/WorktreeCli/AGENTS.md`.
- `Temp/session-sidecar.json`, its `broken-engine-session-sidecar/v1` schema, its strict two-property
  validation in `AgentWorktreeSession.psm1`, and `Temp/session-baseline` and its reader — no new field,
  no schema version bump, no new session file.
- Worktree creation, reattach validation, bootstrap, provisioning, cache seeding, the DataPacker build,
  the rebase-in-progress banner, and the landing or plan-claim flows.
- Any C++ change, and any change to how the baseline itself is resolved.

## Risk tier and invariants

Tier 2, scoped behavior of the session-wrapper and compile-context tooling: it changes how one unit
obtains values it already obtains, with no format, trust boundary, determinism, wire, or serialization
exposure. Escalate to Tier 3 if implementation finds it must change the session sidecar schema, the
bootstrap or build-lock coordination, or anything that could block other sessions from starting.

Invariant to preserve: a wrapper-launched session and a directly invoked
`pwsh -NoProfile -File .agents/skills/compile/scripts/Resolve-CompileContext.ps1` from the session
worktree root must resolve the same repository root and primary checkout after the change as before it.

## Acceptance criteria

- `git grep -n BROKEN_ENGINE_ -- .agents .claude .codex Tools ':(glob)**/AGENTS.md'` reports only
  `BROKEN_ENGINE_CLIENT_ARGUMENTS`, `BROKEN_ENGINE_MSBUILD_PATH`, and
  `BROKEN_ENGINE_BUILD_LOCK_WAIT_SECONDS`. `Documents/Plans/**` is exempt: Plan prose, including this
  file, names the removed variables.
- `Resolve-CompileContext.ps1` run from a session worktree root with no `-RepositoryRoot` and no
  `-PrimaryCheckout` argument reports the same `repositoryRoot` and `primaryCheckout` values it reports
  before the change, in a shell where the two variables are unset.
- A wrapper reattach through `.claude/claude-worktree.sh --reattach-worktree <path>` still launches the
  client with its arguments intact, proving the retained transport variable is unaffected.

## Notes

- Originating gap: the out-of-scope residual of `Documents/Plans/Engine/FinalizeLandingAssertions.md`,
  whose `## Design` bounded that change to `BROKEN_ENGINE_BASELINE` and recorded the remaining six as a
  follow-up. That Plan file is deleted at its completion, so this Plan carries no `dependsOn` edge to
  it; if the baseline work has not landed when this Plan is claimed, expect the
  `BROKEN_ENGINE_BASELINE` sites to still be present and leave them to that change.
- The four reader-less variables may still be read by something outside this repository — a developer's
  shell profile or an ad-hoc script. No such consumer is known, and the repository cannot prove a
  negative about untracked files; the implementer should mention the removal in the landing summary.
