<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-20T23:41:58.645Z","dependsOn":[]} -->
# Remove OpenCode support

## Context

The user directed that OpenCode support be removed from the repository
entirely. They also chose to leave `.opencode/` out of the repository-wide
instruction-authoring policy and its static checks, so after that policy lands
the `.opencode/` role definitions are the only agent role definitions it does
not govern. Removing the client keeps one supported set of clients: Claude Code
and Codex CLI.

OpenCode support at baseline `88a5c0740d49b7a0f1c8c87aeaf96d7ecb1cb75e`
consists of:

- the tracked `.opencode/` tree: `.opencode/opencode.json` (client config,
  `manager` primary agent, task permissions, user-only skill gates),
  `.opencode/opencode-worktree.ps1` (session wrapper that calls
  `.agents/scripts/Start-AgentWorktreeSession.ps1 -Client opencode`),
  `.opencode/command/next-plan.md`, and eight role definitions under
  `.opencode/agent/` (`builder`, `implementer`, `locator`, `mechanic`,
  `planner`, `researcher`, `reviewer`, `runner`) mirroring `.claude/agents/`;
- the `opencode` client value in the shared session scripts:
  `.agents/scripts/Start-AgentWorktreeSession.ps1:3` (`ValidateSet`) and `:13`
  (lowercase client guard and its message);
  `.agents/scripts/Initialize-AgentWorktreeSession.ps1:3` (`ValidateSet`), the
  function `Assert-AgentWorktreeOpenCodeSkills` (`:54`-`:65`), and its call
  branch at `:126`-`:127`;
- the `opencode/` session-branch prefix:
  `.agents/scripts/AgentWorktreeSession.psm1:68` (comment) and `:78`
  (`Get-AgentWorktreeSessionContext` branch regex);
  `.agents/skills/cleanup-worktrees/scripts/cleanup-worktrees.ps1:243`
  (disposable session-branch test);
  `.agents/skills/next-plan/scripts/NextPlanWorkflowCommon.psm1:21`
  (`Get-NextPlanContext` blocker message);
- the `.opencode/` sweep root in
  `.agents/skills/external-skill-creator/scripts/Find-SkillInboundReferences.ps1:27`
  (`$script:SweepRoots`);
- user documentation: `README.md:44` (Node.js prerequisite "required only to
  install OpenCode"), `README.md:162`-`:163` ("three clients", "Codex CLI and
  OpenCode from PowerShell 7"), `README.md:314`-`:333` (`### OpenCode`
  section); `Documents/FreshMachineSetup.md:25`-`:26` (agent CLI list),
  `:110`-`:111` (OpenCode wrapper run), `:122`-`:123` (skills check "and
  `.agents/skills` for OpenCode").

No C++ source, project file, `.gitignore`, or `.gitattributes` names OpenCode
(`git grep -i opencode` at baseline).

## Design

Delete every tracked `.opencode/` file with `git rm`, and remove the `opencode`
client from each site listed in `## Context`, leaving the Claude Code and Codex
behavior byte-for-byte equivalent:

- Session scripts: the `ValidateSet` becomes `'claude', 'codex'`; the client
  guard tests only those two and its message names only them;
  `Initialize-AgentWorktreeSession.ps1` drops
  `Assert-AgentWorktreeOpenCodeSkills` and calls `Assert-AgentWorktreeSkillsLink`
  unconditionally.
- Branch prefix: the regex alternation becomes `(?:claude|codex)`, the
  cleanup test drops its `opencode/` clause, and both comments and messages
  name only `claude/` and `codex/`.
- Sweep roots: drop the `.opencode/` entry; the per-root counts follow from
  `$script:SweepRoots`, and `semantic-review.md` names no root list, so no
  documentation changes with it. `schemaVersion` stays unchanged, because the
  result shape is unchanged and only the root set the script already reports in
  `roots` shrinks.
- Documentation: remove the Node.js prerequisite bullet, the `### OpenCode`
  README section, and the FreshMachineSetup OpenCode wrapper bullet; reword the
  client-count and shell sentences and the skills-check parenthetical to name
  Claude Code and Codex only.

Per the root `AGENTS.md` rule against backward compatibility, no alias,
deprecation message, or `opencode/` branch handling is kept, so an existing
`opencode/<uuid>` worktree on a user machine is no longer a session worktree to
`/next-plan`, and `/cleanup-worktrees` leaves its branch as a non-session
branch.

Change Workflow tier: Tier 2. Trigger: scoped tool behavior — the session
wrapper scripts' accepted client set and session-branch recognition shrink,
across the independently owned session, cleanup-worktrees, next-plan, and
external-skill-creator scripts (Tier 2 "independently owned cross-subsystem
integration"). The Claude Code and Codex paths are unchanged, so no
build/bootstrap coordination behavior changes; no determinism/CRC, wire,
serialization, `.pack`/`kiVersion`, replay, threading, allocation, shader,
build, or affinity exposure.

## Critical files

- `.opencode/` (whole tracked tree, deleted)
- `.agents/scripts/Start-AgentWorktreeSession.ps1`
- `.agents/scripts/Initialize-AgentWorktreeSession.ps1`
- `.agents/scripts/AgentWorktreeSession.psm1`
- `.agents/skills/cleanup-worktrees/scripts/cleanup-worktrees.ps1`
- `.agents/skills/next-plan/scripts/NextPlanWorkflowCommon.psm1`
- `.agents/skills/external-skill-creator/scripts/Find-SkillInboundReferences.ps1`
- `README.md`
- `Documents/FreshMachineSetup.md`

## In scope

- Delete `.opencode/opencode.json`, `.opencode/opencode-worktree.ps1`,
  `.opencode/command/next-plan.md`, and `.opencode/agent/builder.md`,
  `implementer.md`, `locator.md`, `mechanic.md`, `planner.md`,
  `researcher.md`, `reviewer.md`, `runner.md`.
- `Start-AgentWorktreeSession.ps1`: the `$Client` `ValidateSet` and the
  lowercase client guard with its message.
- `Initialize-AgentWorktreeSession.ps1`: the `$Client` `ValidateSet`, the
  function `Assert-AgentWorktreeOpenCodeSkills`, and the `if ($Client -ceq
  'opencode')` / `else` call pair (becomes one unconditional
  `Assert-AgentWorktreeSkillsLink $Worktree`).
- `AgentWorktreeSession.psm1`: the comment above and the branch regex inside
  `Get-AgentWorktreeSessionContext`.
- `cleanup-worktrees.ps1`: the session-branch `StartsWith` test.
- `NextPlanWorkflowCommon.psm1`: the `Get-NextPlanContext` blocker message.
- `Find-SkillInboundReferences.ps1`: the `.opencode/` entry of
  `$script:SweepRoots`.
- `README.md`: the Node.js prerequisite bullet, the `## AI Coding CLIs`
  client-count and shell sentence, and the `### OpenCode` section.
- `Documents/FreshMachineSetup.md`: the `## 2. Install prerequisites` agent CLI
  list, the `## 6. First wrapper run` OpenCode bullet, and the skills-check
  parenthetical in that section.

## Out of scope

- `Documents/Investigations/ChangeWorkflow/MergingImplementerSteps.md`: a
  findings record answered at a fixed commit; its OpenCode figures stay as
  recorded history.
- Any `.claude/`, `.codex/`, or `.agents/skills/**/*.md` content beyond the
  listed sites; any C++ or Visual Studio project file.
- User-machine state: `~/.opencode/`, installed `opencode-ai` packages, and
  existing `opencode/<uuid>` worktrees or branches.
- Any new client, alias, or migration handling.

## Acceptance criteria

- `git ls-files .opencode` prints nothing.
- `git grep -n -i opencode -- ':!Documents/Plans/ChangeWorkflow/RemoveOpenCodeSupport.md'`
  matches only
  `Documents/Investigations/ChangeWorkflow/MergingImplementerSteps.md`.
- The static-checks runner reports no failure in the changed files.
- `pwsh -NoProfile -File .agents/skills/external-skill-creator/scripts/Find-SkillInboundReferences.ps1 -SkillName next-plan`
  exits `0` and its `roots` no longer lists `.opencode/`.

## Notes

- `.claude/claude-worktree.sh` and `.codex/codex-worktree.ps1` call the same
  session scripts with `claude` and `codex`; their behavior must not change.
- Live verification: none required; the next wrapper-started session exercises
  the edited session scripts.
