<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-21T15:53:35.323Z","dependsOn":[]} -->
# Load subdirectory AGENTS.md files in Codex and agy sessions

## Context

The repository's instruction layout assumes every ancestor `AGENTS.md` of a
file an agent reads enters that agent's context:
`.agents/skills/update-claude-docs/references/content-rules.md:39-41` states
"Every ancestor AGENTS.md loads automatically alongside a child document" and
therefore forbids a child `AGENTS.md` from referencing its ancestors. At
baseline `a575b465`, 86 tracked `AGENTS.md` files sit below the root
(`git ls-files '*/AGENTS.md'`), so the subsystem invariants they carry (for
example `Engine/Source/Frame/AGENTS.md` `## Simulation and Visual State`) reach
an agent only through that loading.

Observed symptom, probed on 2026-11-21 with Antigravity CLI (`agy`) 1.5.14 and
the installed Codex CLI. Setup: a subdirectory under the gitignored `Temp/`
tree of a session worktree held an `AGENTS.md` carrying a secret word beside a
data file; each client was started at the worktree root and asked to read the
data file, then to state the secret word.

- `agy` (`agy -p`): read the data file with its file-read tool, then answered
  that it knew no secret word and that only the root `AGENTS.md` was in its
  context. `agy` does not load a subdirectory `AGENTS.md` when it reads a file
  there.
- Codex (`codex exec -s read-only`): read the data file through the shell
  (`Get-Content`), not a file-read tool, then answered the same. Codex does
  not load it either.
- Claude Code: a Read of a Plan under `Documents/Plans/ChangeWorkflow/`
  returned `Documents/AGENTS.md` and `Documents/Plans/AGENTS.md` as context.
  Claude Code brings in every ancestor `AGENTS.md` of a file it reads, which
  is the behavior the layout relies on.

Hook facts from the same `agy` probe, relevant to any hook-based fix:

- `agy` runs hook commands with the working directory `<workspace>\.agents`
  (the folder holding `hooks.json`), so a command path must be relative to
  `.agents/` (`pwsh -NoProfile -File ../<path>`); a path relative to the
  workspace root fails with pwsh exit 64. A failing `PreToolUse` hook blocks
  the tool call (fail-closed); one that exits 0 with empty stdout allows it.
- Every `agy` hook stdin carries a top-level `cwd` (the absolute workspace
  root), `hookEventName`, `conversationId`, `transcriptPath`, and
  `workspacePaths`; `PreToolUse` adds
  `toolCall` (`name`, `id`, `args`) and `stepIdx`. No `agy`-specific
  project-directory environment variable exists;
  `ANTIGRAVITY_CONVERSATION_ID` is set.
- `agy` `SessionStart` stdout `{"injectSteps":[{"userMessage":"..."}]}`
  reached the model. The `agy` `hooks` documentation page lists a
  `PostToolUse` event with the `toolCall` name and arguments on stdin and an
  empty `{}` as its output; whether `PostToolUse`, or any other event, can
  inject context was not probed.
- Codex hooks are configured in `.codex/config.toml`; at baseline only
  `SessionStart` hooks running `.agents/scripts/Write-SessionWorkflowContext.ps1`
  are configured there. Whether any Codex hook event fires on a file read, and
  can add context there, was not probed.

The root cause on each client — which hook event, if any, can carry the
ancestor `AGENTS.md` text into context at the point of a file read — is not
confirmed; this Plan carries an investigation portion.

## Design

1. Confirm the mechanism before editing, per client, by direct probe in a
   session worktree (a subdirectory `AGENTS.md` with a secret word under
   `Temp/`, as in `## Context`):
   - `agy`: whether a `PostToolUse` hook on the file-read tools can inject
     context despite the documented `{}` output (for example through an
     `injectSteps` form like `SessionStart`'s), and if not, whether
     `PreToolUse` or another event can. Record the tool names `agy` uses for
     file reads and the `toolCall.args` key holding the path.
   - Codex: whether any hook event fires on a file read — including the shell
     command form the probe observed (`Get-Content`) — and whether that event
     can add model-visible context.
2. Recommended shape, should both clients expose such an event: one PowerShell
   script under `.agents/scripts/` that takes the hook payload, resolves the
   read path against the payload's `cwd`, collects every ancestor `AGENTS.md`
   from the file's directory up to (excluding) the root `AGENTS.md`, drops
   ones already injected this session, and emits them in each client's
   context-injection form; registered in `.agents/hooks.json` for `agy` and
   `.codex/config.toml` for Codex. It fails open (exit 0, empty output) on any
   unexpected payload, because an `agy` hook failure blocks the tool call.
   Rationale: one script mirrors the existing shared `SessionStart` script
   both clients already call.
3. Make the smallest fix inside the `## In scope` region. If a client offers
   no event that can inject context on a file read, or the fix needs a file
   outside that region (for example a wrapper script, a role file, or a
   change to the instruction layout itself), stop and surface it for
   re-planning instead of widening scope.

Change Workflow tier: Tier 2 — trigger: scoped tool behavior of one subsystem
(agent hook configuration and its script), with no determinism, wire,
serialization, threading, or trust-boundary exposure. Exposure: an `agy` hook
that fails blocks the tool call it hooks, so the script's fail-open path is an
invariant.

## Critical files

- `.agents/hooks.json` — `agy` hook configuration.
- `.codex/config.toml` — Codex hook configuration.
- `.agents/scripts/` — the new hook script.
- `.agents/skills/update-claude-docs/references/content-rules.md:39-41` — the
  assumption this Plan makes true on Codex and `agy` (read-only reference).

## In scope

- The root-cause investigation in `## Design` step 1 for `agy` and Codex.
- The fix region: hook entries in `.agents/hooks.json` and `.codex/config.toml`,
  and one new hook script under `.agents/scripts/`.

## Out of scope

- Claude Code configuration (`.claude/settings.json`): Claude Code already
  loads ancestor `AGENTS.md` files.
- Restructuring or moving any `AGENTS.md` file, or changing
  `content-rules.md`'s ancestor rule.
- The session wrapper scripts, role definitions, and `SessionStart` context
  script (`.agents/scripts/Write-SessionWorkflowContext.ps1`).
- Any C++ or engine change.

## Acceptance criteria

- In a Codex session and in an `agy` session, each started at a session
  worktree root, reading a file under `Engine/Source/Frame/` brings
  `Engine/Source/Frame/AGENTS.md` and its ancestor `Engine/Source/AGENTS.md`
  into context: asked afterwards, the agent states a fact that only those files
  carry.
- The recorded symptom no longer reproduces: the `Temp/` secret-word probe in
  `## Context` answers the secret word on both clients.
- A malformed or unexpected hook payload leaves the hooked tool call allowed.

## Notes

- Live verification is a manual client session per `## Acceptance criteria`;
  no `/agent-harness` run applies.
- The `agy` hook working directory is `.agents/`, unlike Codex and Claude
  Code; resolve paths from the payload's `cwd`, not the process directory.
