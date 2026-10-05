---
name: claude-to-codex
description: >-
  Run one task on Codex headless from Claude Code — a review, research, or file
  edits in a worktree — with a selectable preset, model, reasoning effort, and
  sandbox, and return its result. Use only when the user explicitly asks to run
  a task on Codex from Claude Code, or when another skill routes its Codex
  launch here. Codex callers never invoke it.
allowed-tools: [Read, Write, Bash]
---

# Claude to Codex

## Purpose

Runs one caller-written prompt on Codex headless in a worktree, read-only or
with edits, and returns its result and out-file path without the run's work
entering this session.

## When to use

- The user explicitly asks, in the current session, to run a task on Codex from
  Claude Code.
- Another skill routes its Codex launch here, as `/codex-review` does.

Codex callers never invoke it.

## Inputs

- Prompt — the caller-written task in a fresh repo-relative file under `Temp/`.
- Worktree — this session's worktree, and no other: the model check reads this
  checkout's `.codex/agents` pins, the launcher the worktree's.
- `-Agent` — `fable`, `opus`, `sol`, or `sonnet`, default `sol`; selects the
  model and reasoning effort pinned in `.codex/agents/<agent>.toml`.
- `-Model` and `-Effort` — only when the user names a model or an effort; each
  overrides the preset's value.
- `-Sandbox` — `read-only` by default; `workspace-write` only when the task must
  edit files in the worktree. A `workspace-write` run writes the worktree's
  files and permitted temporary storage only: it cannot write the worktree's
  `.agents/` directory, the linked worktree's Git index or refs, which live in
  the main repository's `.git`, or the machine-local lock storage under
  `%LOCALAPPDATA%`, and this launch restricts its network access. No sandbox
  mode reaches those resources.

## Steps

1. Before the first launch in this Claude Code session, run the model check from
   the worktree root, and run it again with `-Model <slug>` for an override not
   yet checked this session:

   ```powershell
   pwsh -NoProfile -File .agents/skills/claude-to-codex/scripts/Test-CodexModels.ps1 [-Model <slug>]
   ```

   It prints one JSON line. Exit `0` passes. On exit `2`, a block, halt and show
   the user each `checks` row whose `reason` is set: the pinned `slug` and the
   `newest` listed name. On exit `1`, an error, halt and report its `message`.

   For a launch another skill routed here, a block or error also ends that
   launch as a genuine failure under `## Rules`.

   Done when the check has passed this session for every model this launch
   uses, or the user has been shown the block or error.
2. End the prompt with the final-line contract: its last non-empty line must be
   `PASS`, `CHANGES-REQUIRED: <n>`, or `BLOCKED: <reason>`, where `PASS` on a
   task that is not a review means the task finished.

   Done when the prompt file states that contract.
3. Launch with one bare blocking call, issued with a call timeout of at least
   `600000` ms and never wrapped in a loop or chained with another command:

   ```powershell
   pwsh -NoProfile -File .agents/skills/claude-to-codex/scripts/Invoke-Codex.ps1 -Worktree '<worktree>' -PromptFile '<prompt>' -OutFile <out> [-Agent <agent>] [-Model <slug>] [-Effort <level>] [-Sandbox <read-only|workspace-write>]
   ```

   Pass a repo-relative `<out>` such as `Temp/<name>-out.md`; the launch
   tolerates an existing out-file, so `<out>` must be a fresh path that does not
   yet exist.

   A `completed` receipt carries the result on the same stdout, so the success
   path never reads `<out>`; the launcher's `.NOTES` One-call contract owns the
   receipt, wait, and separator.

   Done when that single call has returned its receipt.
4. A run that outruns the budget answers `running` with its `runId`. Resume it
   with exactly one further bare call per wake, also issued with a call timeout
   of at least `600000` ms and never in a loop:

   ```powershell
   pwsh -NoProfile -File .agents/skills/claude-to-codex/scripts/Invoke-Codex.ps1 -Wait <runId>
   ```

   The launcher's `.NOTES` One-call contract owns how that wait resumes the run.
   Duration alone is never a failure: a run that stays `running` across many
   waits is progressing normally.

   Done when the wait ends in `completed`, `malformed`, or `failed`.
5. Act on the terminal status: `completed` proceeds, and `retried: true` needs
   no action; `malformed` or `failed` goes to `## Rules`.

   Done when a `completed` result is in hand or `## Rules` is applied to a
   `malformed` or `failed` one.

## Rules

- A genuine failure is a `failed` status, a `malformed` status, a non-zero
  launcher exit, or a step 1 block or error on a routed launch. Report it to the
  user, or to the skill that routed the launch, with the receipt's `reason` — for
  the model check, its `checks` rows with `reason` set or its `message` — and
  without a further diagnostic call.
- One bounded exception covers what the verdict-line check cannot see: when a
  `completed` read-only result passed it but this session judges it malformed
  anyway — leaked drafting notes, or an answer to a different task — re-dispatch
  the identical prompt exactly once, with a fresh `<out>` and `-NoRetry` added
  to the same launch arguments. That result is final; a second malformed result
  is a genuine failure.
- Never launch a task that needs a resource the `-Sandbox` input says no mode
  reaches — `.agents/` writes, Git index or refs writes, lock storage, or the
  network; run it in a Claude role instead. Such tasks include skill-file edits
  (any change under `.agents/`), `/compile`, `/finalize-changes`, and Plan
  creation through `New-PlanFile.ps1`.
- Attempt a `workspace-write` run exactly once, never re-dispatching it: report
  any failed or judged-malformed outcome to the user together with the
  worktree's `git status`.
- Leave the worktree to Codex while a `workspace-write` run is live; edit it
  only after the run reaches a terminal status.
- To verify a change to either script on a machine without the Codex CLI, issue
  the same call from the Bash tool with a stand-in `codex` first on `PATH` —
  `PATH=<stub dir>:$PATH pwsh -NoProfile -File
  .agents/skills/claude-to-codex/scripts/<script>.ps1 ...` — which stays one
  invocation; PowerShell has no inline prefix form, so this verification form is
  Bash-tool only.

## References

- [scripts/Invoke-Codex.ps1](scripts/Invoke-Codex.ps1) — the launcher; its
  `.NOTES` owns the receipt fields, exit codes, verdict-line check, and
  automatic read-only retry.
- [scripts/Test-CodexModels.ps1](scripts/Test-CodexModels.ps1) — the model
  check; its header owns the block conditions and exit codes.
