---
name: codex-bridge
description: >-
  Run one task — an existing subagent role and skill such as a reviewer,
  researcher, planner, or implementer — on Codex headless from Claude Code
  under one of two presets and a selectable sandbox, and return its result. Use
  only when the user explicitly asks to run a task on Codex. Codex callers never
  invoke it.
allowed-tools: [Read, Write, Bash]
---

# Codex Bridge

## Purpose

Runs one task brief on Codex headless in a worktree, read-only or with edits,
and returns its result and out-file path without the run's work entering this
session. A bridge run of an assigned role and skill is that role's delegated
execution context
([`../../references/subagent-reporting.md`](../../references/subagent-reporting.md)
`## Delegated execution context`), not an inline run; since `codex exec` loads
no role definition, the task brief's `Role` line carries it.

## When to use

- The user explicitly asks, in the current session, to run an existing subagent
  role and skill on Codex from Claude Code.

Codex callers never invoke it.

## Inputs

- Prompt — a fresh repo-relative file under `Temp/` holding one task brief on
  the form in
  [`../../references/subagent-reporting.md`](../../references/subagent-reporting.md)
  `## Task brief`, filled exactly as a Claude subagent dispatch of that role and
  skill would be.
- Worktree — this session's worktree.
- `-Agent` — omit it unless the user asks for `astra`. The launcher's `$Agent`
  default and preset table own the preset and each preset's model and effort,
  which replace the role definition's.
- `-Sandbox` — `read-only` by default and for findings-only or research work;
  `workspace-write` only when the task must edit files in the worktree. A
  `workspace-write` run writes the worktree's files and permitted temporary
  storage only: it cannot write the worktree's `.agents/` directory, the linked
  worktree's Git index or refs, which live in the main repository's `.git`, or
  the machine-local lock storage under `%LOCALAPPDATA%`, and this launch
  restricts its network access. No sandbox mode reaches those resources.

## Steps

1. Before the first launch in this Claude Code session, run the model check from
   the worktree root:

   ```powershell
   pwsh -NoProfile -File .agents/skills/codex-bridge/scripts/Invoke-Codex.ps1 -CheckModels
   ```

   It prints one JSON line. Exit `0` passes. On exit `2`, a block, halt and show
   the user each `checks` row whose `reason` is set: the pinned `slug` and the
   `newest` listed name. On exit `1`, an error, halt and report its `message`.

   Done when the check has passed this session, or the user has been shown the
   block or error.
2. End the prompt, after the role's handoff, with the final-line contract: its
   last non-empty line must be `PASS`, `CHANGES-REQUIRED: <n>`, or
   `BLOCKED: <reason>`, where `PASS` on a task that is not a review means the
   task finished. The verdict line follows the skill's complete handoff, outside
   it, so the handoff still ends with `Residuals` as
   [`../../references/subagent-handoff.md`](../../references/subagent-handoff.md)
   `## Handoffs` requires, and its token supplements the handoff's `Status`,
   never replacing it.

   Done when the prompt file states that contract.
3. Launch with one bare blocking call, issued with a call timeout of at least
   `600000` ms and never wrapped in a loop or chained with another command:

   ```powershell
   pwsh -NoProfile -File .agents/skills/codex-bridge/scripts/Invoke-Codex.ps1 -Worktree '<worktree>' -PromptFile '<prompt>' -OutFile <out> [-Agent <agent>] [-Sandbox <read-only|workspace-write>]
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
   pwsh -NoProfile -File .agents/skills/codex-bridge/scripts/Invoke-Codex.ps1 -Wait <runId>
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
  launcher exit, or a step 1 block or error. Report it to the user with the
  receipt's `reason` — for the model check, its `checks` rows with `reason` set
  or its `message` — and without a further diagnostic call.
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
- To verify a change to the launcher on a machine without the Codex CLI, issue
  the same call from the Bash tool with a stand-in `codex` first on `PATH` —
  `PATH=<stub dir>:$PATH pwsh -NoProfile -File
  .agents/skills/codex-bridge/scripts/Invoke-Codex.ps1 ...` — which stays one
  invocation; PowerShell has no inline prefix form, so this verification form is
  Bash-tool only.

## References

- [scripts/Invoke-Codex.ps1](scripts/Invoke-Codex.ps1) — the launcher; its
  `.NOTES` owns the receipt fields, exit codes, verdict-line check, and
  automatic read-only retry, and the `-CheckModels` receipt, block conditions,
  and exit codes.
