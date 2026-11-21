<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-21T22:11:22.265Z","dependsOn":[]} -->
# Align model-only and Codex-disable skill controls in agy and Codex

## Context

`.agents/skills/codex-bridge/SKILL.md:5-8,30` declares that `/codex-bridge` is
dispatched automatically for routed review roles on non-Codex hosts and is
"never user-triggered or invoked by Codex callers". Currently it relies on
Claude Code's `user-invocable: false` frontmatter key (`SKILL.md:8`) and
Codex's `policy.allow_implicit_invocation: false` companion file
(`.agents/skills/codex-bridge/agents/openai.yaml:1-2`).

During `/external-skill-creator` validate mode on
`.agents/skills/codex-bridge`, two cross-client invocation-control gaps were
flagged outside the scope of activating `/codex-bridge` (which did not modify
`Validate-Skill.ps1`, `frontmatter-schema.md`, `client-compatibility.md`, or
`.codex/config.toml` and kept `.codex/agents/*` untouched):

1. **Antigravity CLI (`agy`) slash-command suppression**:
   Antigravity CLI reads `.agents/skills` directly and exposes each skill as
   `/skill-name`
   (`.agents/skills/external-skill-creator/references/client-compatibility.md:101-103`).
   Its changelog documents `disable-slash-command` as the frontmatter control
   that suppresses user `/skill-name` invocation while preserving model
   invocation, whereas `user-invocable: false` is Claude Code's control
   (`client-compatibility.md:37-38`). However,
   `.agents/skills/external-skill-creator/scripts/Validate-Skill.ps1:876,879,907-911`
   (`$allowedKeys`, `$booleanKeys`) rejects any frontmatter key outside
   `$allowedKeys` with `SCHEMA001`, and `disable-slash-command` is absent from
   `$allowedKeys`, `$booleanKeys`,
   `.agents/skills/external-skill-creator/references/frontmatter-schema.md:27-41`,
   and `client-compatibility.md:99-116`.
2. **Codex explicit `$codex-bridge` disablement**:
   `.agents/skills/codex-bridge/agents/openai.yaml:1-2` sets
   `policy.allow_implicit_invocation: false`, which disables Codex implicit
   invocation while still permitting explicit `$codex-bridge` invocation
   (`.agents/skills/external-skill-creator/references/frontmatter-schema.md:136-137`).
   Codex documents `[[skills.config]]` with `enabled = false` in `config.toml`
   to disable a skill entirely; `.codex/config.toml:1-29` currently configures
   only `features.multi_agent_v2` and `hooks.SessionStart` with no
   `[[skills.config]]` entry.

Whether the installed Antigravity CLI honors `disable-slash-command: true`
(and/or `user-invocable: false`) at runtime, and whether project-level
`.codex/config.toml` accepts a portable repo-relative path in
`[[skills.config]]` to disable `codex-bridge` across worktrees without machine-
specific absolute paths or edits to `.codex/agents/*`, is not yet confirmed by
direct probe. This Plan carries an investigation portion for both probes.

## Design

1. **Direct client probes before editing**:
   - **Antigravity CLI**: in a disposable skill or live `agy` session at the
     worktree root, test whether `agy` hides a skill with `user-invocable:
     false` alone from user `/skill-name` commands, whether
     `disable-slash-command: true` suppresses user `/skill-name` invocation
     while keeping model invocation available, and whether Claude Code ignores
     `disable-slash-command: true` without error. Record the installed `agy`
     version and observation date per
     `.agents/skills/external-skill-creator/references/client-compatibility.md:123-124`.
   - **Codex CLI**: in a Codex session at the worktree root, test whether
     project-level `.codex/config.toml` supports `[[skills.config]]` with
     `enabled = false` and a portable repo-relative path (such as
     `.agents/skills/codex-bridge/SKILL.md` or `.agents/skills/codex-bridge`)
     so `codex-bridge` is disabled for both implicit and explicit
     `$codex-bridge` invocation in any session worktree. Record the installed
     Codex version and observation date.
2. **Smallest fix inside `## In scope` based on probe results**:
   - Recommended Antigravity CLI shape (when the probe confirms
     `disable-slash-command: true` is needed and effective): add
     `disable-slash-command` to `$allowedKeys` and `$booleanKeys` in
     `.agents/skills/external-skill-creator/scripts/Validate-Skill.ps1:876,879`,
     document it in
     `.agents/skills/external-skill-creator/references/frontmatter-schema.md:27-41`
     and `client-compatibility.md:99-116`, and set `disable-slash-command: true`
     alongside `user-invocable: false` in
     `.agents/skills/codex-bridge/SKILL.md`. Rationale: keeping both keys
     preserves Claude Code's documented `user-invocable: false` control while
     adding Antigravity CLI's control through the single repository schema and
     validator. If the probe shows `agy` already honors `user-invocable: false`
     (or ignores `disable-slash-command`), document the verified behavior in
     `client-compatibility.md` instead of adding a no-op schema key.
   - Recommended Codex shape (when the probe confirms project-level
     `.codex/config.toml` accepts a portable repo-relative path in
     `[[skills.config]]`): add the `[[skills.config]]` disable entry for
     `codex-bridge` to `.codex/config.toml` while retaining
     `allow_implicit_invocation: false` in
     `.agents/skills/codex-bridge/agents/openai.yaml`, and document the control
     in `client-compatibility.md` `## Codex`. Rationale: `.codex/config.toml`
     is already tracked for project-wide Codex settings and keeps
     `.codex/agents/*` untouched.
   - If either client offers no portable repository-tracked control inside
     `## In scope` (for example if Codex `[[skills.config]]` requires a
     machine-specific absolute path), document the verified client limitation
     in `client-compatibility.md` and surface any fix requiring files outside
     `## In scope` (such as session wrappers) for re-planning rather than
     widening scope.

Change Workflow tier: Tier 2 — trigger: scoped behavior of one tooling
subsystem (skill frontmatter validation in `Validate-Skill.ps1`, skill metadata
in `codex-bridge/SKILL.md`, and Codex skill configuration in
`.codex/config.toml`). No determinism/CRC, wire, serialization,
`.pack`/`kiVersion`, replay, affinity, threading, allocation, shader, build,
trust-boundary, or live `/agent-harness` exposure.

## Critical files

- `.agents/skills/external-skill-creator/scripts/Validate-Skill.ps1` —
  `$allowedKeys` and `$booleanKeys` (`:876-879`).
- `.agents/skills/external-skill-creator/references/frontmatter-schema.md` —
  `## Fields` table (`:25-48`) and `## Codex companion file` (`:126-145`).
- `.agents/skills/external-skill-creator/references/client-compatibility.md` —
  `## Codex` (`:79-97`), `## Antigravity CLI` (`:99-104`), and
  `## Cross-Client Check` (`:105-124`).
- `.agents/skills/codex-bridge/SKILL.md` — frontmatter (`:1-10`).
- `.codex/config.toml` — project-level Codex configuration (`:1-29`).

## In scope

- The direct Antigravity CLI and Codex CLI probes described in `## Design`
  step 1.
- The fix region:
  - `.agents/skills/external-skill-creator/scripts/Validate-Skill.ps1`:
    `$allowedKeys` and `$booleanKeys` (if the `agy` probe confirms
    `disable-slash-command`).
  - `.agents/skills/external-skill-creator/references/frontmatter-schema.md`
    and `client-compatibility.md`: document the verified `agy` and Codex
    controls, installed client versions, and observation dates.
  - `.agents/skills/codex-bridge/SKILL.md`: frontmatter invocation key(s),
    retaining `user-invocable: false`.
  - `.codex/config.toml`: `[[skills.config]]` entry disabling `codex-bridge`
    (if the Codex probe confirms portable relative-path support).

## Out of scope

- `.codex/agents/*` (must remain untouched) and
  `.agents/scripts/Get-ChangeWorkflowSteps.ps1` (must remain host-agnostic).
- OpenCode skill/command registration: OpenCode support is scheduled for
  removal under `Documents/Plans/ChangeWorkflow/RemoveOpenCodeSupport.md`.
- `.agents/skills/codex-bridge/scripts/Invoke-Codex.ps1` routing, presets,
  sandboxes, or receipt schemas.
- Session wrapper scripts (`.agents/scripts/Start-AgentWorktreeSession.ps1`,
  `Initialize-AgentWorktreeSession.ps1`), `.agents/hooks.json`, or Codex
  `SessionStart` hooks in `.codex/config.toml`.
- Any C++, shader, or Visual Studio project file.

## Acceptance criteria

- Direct client probe results (with installed `agy` and Codex versions and
  observation dates) are recorded and reflected accurately in
  `.agents/skills/external-skill-creator/references/client-compatibility.md`.
- Recorded symptoms no longer reproduce where supported by portable repo-level
  controls:
  - In Antigravity CLI, `/codex-bridge` is not exposed as a user slash command
    while remaining available for model invocation by non-Codex review
    dispatch, and Claude Code still treats `codex-bridge` as model-only
    (`user-invocable: false`).
  - In Codex CLI, `codex-bridge` is disabled (neither implicitly matched nor
    explicitly invocable as `$codex-bridge`) via `.codex/config.toml` without
    absolute machine paths and without modifying `.codex/agents/*` (or, if
    Codex rejects relative paths in `.codex/config.toml`, the limitation is
    documented and surfaced for re-planning).
- When `Validate-Skill.ps1` changes, all validator-change checks in
  `.agents/skills/external-skill-creator/references/frontmatter-schema.md:164-174`
  pass using disposable `-AllowOutsideSkills` packages (including valid boolean
  `disable-slash-command: true`/`false` and invalid non-boolean rejection),
  plus `Validate-Skill.ps1` returns `VALID` (exit `0`) on
  `.agents/skills/external-skill-creator` and `.agents/skills/codex-bridge`.
- `Invoke-StaticChecks.ps1` passes with zero failures across all changed files.

## Notes

- `.codex/config.toml` is also named by
  `Documents/Plans/ChangeWorkflow/LoadSubdirectoryAgentsMdInCodexAndAgy.md` for
  file-read hooks; `[[skills.config]]` entries are independent of hook tables,
  so neither Plan depends on the other.
- Live `/agent-harness` verification does not apply; verification uses live
  `agy` and Codex CLI probes plus the skill validator and static checks.
