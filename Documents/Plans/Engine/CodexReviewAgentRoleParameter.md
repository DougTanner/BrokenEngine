<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-04T21:15:53.690Z","dependsOn":[]} -->
# Add a role parameter to the Codex dispatch script so Claude Code can run researcher workers on Codex

## Context

`/plan-alternatives` (`.agents/skills/plan-alternatives/`, added in the session that
recorded this residual) dispatches one to three blind `researcher` workers per run. In Claude
Code those researchers can only run as Claude subagents today, so every alternatives run
spends Claude tokens on work the Codex headless route could carry, exactly as reviews
already do.

The single repository dispatch path to Codex, `.codex/codex-review.ps1`, is hard-wired to
the reviewer mapping:

- `Invoke-CodexAttempt` (`.codex/codex-review.ps1:145-159`) passes the literals
  `-m gpt-5.6-sol` and `-c 'model_reasoning_effort="medium"'`.
- Its `param` block (`:43-65`) exposes only `-Worktree`, `-PromptFile`, `-OutFile`,
  `-NoRetry`, `-Wait`, and `-InternalRunId`; there is no model, effort, or role parameter.
- `.agents/skills/codex-review/SKILL.md:15-25` scopes the whole route to delegated
  reviewer and auditor roles.

The per-role model pins already live in `.codex/agents/`: `fable.toml`, `opus.toml`,
`sol.toml`, and `sonnet.toml`, each with `model` and `model_reasoning_effort` keys.
`sol.toml` carries exactly `gpt-5.6-sol` and `medium`, which is what the script hard-codes,
so the script duplicates one role's pin instead of reading it.

The root `AGENTS.md` role table maps `researcher` to the Opus column, and its Codex line maps
Opus to `gpt-5.6-luna` at `max` effort, so a Codex researcher resolves through
`.codex/agents/opus.toml`.

The user deferred this plumbing to a follow-up when `/plan-alternatives` was created; the
alternatives run that produced the recommended design below is recorded in `## Notes`.

## Design

Recommended approach, with the reasoning that selected it:

1. Add one parameter, `-Agent <name>`, to `.codex/codex-review.ps1`, defaulting to `sol`.
   The name is a `.codex/agents/<name>.toml` basename, which is already how the root
   `AGENTS.md` role table's Model column resolves a role on Codex.
2. Read `model` and `model_reasoning_effort` from that TOML and use the two read values in
   `Invoke-CodexAttempt` in place of the two hard-coded literals. `-Agent sol` then produces
   a byte-identical command line to today's, so the reviewer route is unchanged.
3. Exit with the script's existing blocked receipt shape when the named role file is
   missing or unreadable, or when either key is absent, and document that blocked code in
   `.agents/skills/codex-review/references/receipts.md` next to the existing blocked codes.
4. Add the Codex dispatch as the Claude Code route to the `/plan-alternatives` bullet of
   the root `AGENTS.md` Prepare and explore alternatives step, which is the dispatch recipe
   main follows: one clause stating that in Claude Code each researcher is dispatched
   through the Codex script with `-Agent opus` when Codex is available, and as the Claude
   `researcher` subagent otherwise. If a per-worker instruction is also needed, it goes in
   `.agents/skills/plan-alternatives/SKILL.md` `## Inputs`, which points at that bullet —
   never in `.agents/skills/plan-alternatives/references/worker.md`, which holds only the
   researcher's own steps. Codex callers need no route change: they already are the role
   mapping.

Settle one question before implementing rather than treating it as a design option: run
`codex exec --help` against the installed Codex CLI and check whether it accepts an
`--agent <name>` flag that resolves `.codex/agents/*.toml` natively. If it does, step 2
collapses to passing that flag instead of `-m`/`-c`, and the TOML read is not written at
all. The TOML read described above is the fallback implementation for a CLI without that
flag. Either way the parameter surface, the default, and the blocked exit stay as written.

No change to `.codex/New-CodexReviewPrompt.ps1` or its prompt template is expected: a
researcher reviews no diff, and `/plan-alternatives` main already assembles a per-axis brief.
The recommendation is that main writes that brief to a file under `Temp/` and passes it as
`-PromptFile`, with the brief instructing the researcher to end with the mandated terminal
`PASS` line. That keeps `Test-CodexVerdictLine`, the automatic retry, the detached wait, and
the inline `--- findings ---` return working unchanged, because all four key on the
verdict line and the out-file, not on prompt provenance.

Alternative considered and not recommended: `-Model` and `-Effort` parameters supplied by
each call site. It is a smaller script edit, but it copies the role-to-model mapping into
every caller, where it can drift from the root `AGENTS.md` table, and it leaves the two
duplicated literals in place instead of removing them.

## Critical files

- `.codex/codex-review.ps1` — the `param` block (`:43-65`) and `Invoke-CodexAttempt`
  (`:145-159`)
- `.codex/agents/opus.toml`, `.codex/agents/sol.toml` — the read pins (researcher and the
  default); `fable.toml` and `sonnet.toml` are the sibling formats, read-only here
- `.agents/skills/codex-review/SKILL.md` — the documented parameter and role scope
- `.agents/skills/codex-review/references/receipts.md` — `## Blocked exit codes`
- Root `AGENTS.md` — the `/plan-alternatives` bullet of the Prepare and explore
  alternatives step, which carries main's dispatch recipe

## In scope

- `.codex/codex-review.ps1`: the `param` block gains `-Agent`, defaulting to `sol`;
  `Invoke-CodexAttempt` takes its model and effort from the resolved role instead of the
  two literals; one blocked exit for an unknown or unreadable role name
- `.agents/skills/codex-review/SKILL.md`: the inputs and role scope wording that names what
  the route may run
- `.agents/skills/codex-review/references/receipts.md`: one added blocked code entry
- Root `AGENTS.md`: the `/plan-alternatives` bullet's Claude Code route clause, and
  `.agents/skills/plan-alternatives/SKILL.md` `## Inputs` only if a per-worker instruction
  is needed

## Out of scope

- `.codex/New-CodexReviewPrompt.ps1`, its prompt template, and the review evidence assembly
- Any change to how reviews route: every review dispatch keeps going through
  `/codex-review` with the reviewer mapping
- The retry, detached-run, receipt, wait, and verdict-line mechanics beyond the added
  blocked code
- The contents of `.codex/agents/*.toml`, the root `AGENTS.md` role table, and role names
- Routing any other role (`implementer`, `planner`, `builder`, `mechanic`) to Codex
- Claude-side `subagent_type` dispatch and `.claude/agents/*.md`

## Risk tier and invariants

Tier 2. Trigger: scoped behavior of one tool — the agent dispatch script and the skill
prose that documents it — with no determinism, wire, serialization, threading, or trust
surface touched, and no cross-subsystem ownership. Invariants preserved:

- `-Agent sol` reproduces today's command line exactly, so no existing review dispatch
  changes behavior.
- The role-to-model mapping keeps its single owning location: the root `AGENTS.md` table
  plus `.codex/agents/*.toml`. No model or effort literal is added to a call site or to a
  skill file.
- A researcher still runs read-only and dispatches no worker, matching
  `.agents/skills/plan-alternatives/references/worker.md` `## Rules`.

## Acceptance criteria

- `.codex/codex-review.ps1` contains no `gpt-` model literal and no
  `model_reasoning_effort` literal.
- A review dispatched with the default `-Agent` returns the same receipt shape and verdict
  handling as before the change.
- A `/plan-alternatives` researcher dispatched through the script on the researcher role returns a
  candidate handoff with the terminal verdict line, and the script reports it as a
  well-formed result.
- An unknown role name returns the blocked exit documented in
  `.agents/skills/codex-review/references/receipts.md`, and spawns no Codex run.
- `/validate-skill` passes for every changed `SKILL.md`.

## Notes

- The recommended design is the axis-1 "Reuse" candidate from the `/plan-alternatives` run
  held in the session that recorded this residual; it was preferred over the `-Model`
  /`-Effort` variant on reuse of `.codex/agents/*.toml` and on removing the duplicated
  literals.
- `Documents/Plans/Engine/CodexImplementerRoleModelRouting.md` investigates whether Codex
  in-session `spawn_agent` children resolve their mapped model. It shares the subject of
  role-to-model resolution but changes different bytes — it explicitly excludes the
  `/codex-review` route — so neither Plan blocks the other.
