---
name: codex-bridge
description: >-
  Run one routed review skill on Codex headless from a non-Codex host (Claude
  Code, Antigravity CLI, OpenCode) under the launcher's route table. Dispatched
  automatically for routed review roles; never user-triggered or invoked by
  Codex callers.
user-invocable: false
allowed-tools: [Read, Write, Bash, PowerShell]
---

# Codex Bridge

## Purpose

Runs one routed review brief on Codex headless in a worktree from a non-Codex
host and returns its result and out-file path without its work entering this
session. A bridge run is the assigned role's delegated execution context
([`../../references/subagent-reporting.md`](../../references/subagent-reporting.md)
`## Delegated execution context`), not an inline run; since `codex exec` loads
no role definition, the brief's `Role` line carries it.

## When to use

- A non-Codex host (Claude Code, Antigravity CLI, OpenCode) dispatches a
  [`scripts/Invoke-Codex.ps1`](scripts/Invoke-Codex.ps1) `$routes` review per
  [`../../references/change-workflow.md`](../../references/change-workflow.md)
  `### Delegation roles`.

Never user-triggered; Codex callers never invoke it.

## Inputs

- Prompt — fresh repo-relative `Temp/` file holding one task brief
  (`subagent-reporting.md` `## Task brief`), filled as a native dispatch would
  be.
- Worktree — this session's worktree.
- `-Skill` — bare skill name from `Invoke-Codex.ps1` `$routes` (whose `$routes`
  and `$presets` solely own preset/sandbox mappings, replacing role model and
  effort, and reject unrouted entries before spawning Codex); see
  [`change-workflow.md`](../../references/change-workflow.md)
  `### Delegation roles` for partial-skill routing limits.
- `-Mode` — required whenever the skill's `Invoke-Codex.ps1` `$routes` key has a
  `<skill>:<mode>` suffix; omit otherwise.

## Steps

1. Once per non-Codex session before the first launch, run from the worktree
   root:
   `pwsh -NoProfile -File .agents/skills/codex-bridge/scripts/Invoke-Codex.ps1 -CheckModels`.
   Exit `0` passes; on exit `2` halt and show each `checks` row with `reason`
   (`slug`, `newest`); on exit `1` halt and report `message`. Done when passed
   or reported.
2. End the prompt with the verdict-line contract: the worker's last non-empty
   response line must be `PASS`, `CHANGES-REQUIRED: <n>`, or
   `BLOCKED: <reason>`, outside the skill's or lens's complete output — after
   `Residuals` for the shared handoff
   ([`../../references/subagent-handoff.md`](../../references/subagent-handoff.md)
   `## Handoffs`), or after a lens's own output shape (e.g.
   `/external-architecture-review` Step 9) — supplementing `Status` (without
   replacing it) where that shape declares it. Done when the prompt states it.
3. Launch with one bare call placing `-Skill` (and `-Mode`) before `-Worktree`,
   never looped or chained, with a fresh repo-relative `Temp/` `<out>`. Wait
   rule: on blocking-shell hosts (e.g. Claude Code) pass a timeout `>= 600000`
   ms; on Antigravity CLI (`run_command`, cap `10000` ms) pass
   `WaitMsBeforeAsync: 10000` and await async reactive wakeup without polling,
   then act on stdout:

   ```powershell
   pwsh -NoProfile -File .agents/skills/codex-bridge/scripts/Invoke-Codex.ps1 -Skill <skill> [-Mode <mode>] -Worktree '<worktree>' -PromptFile '<prompt>' -OutFile <out>
   ```

   A `completed` receipt prints the result on stdout (do not read `<out>`);
   `Invoke-Codex.ps1` `.NOTES` owns receipt, wait, and separator. Done when the
   receipt returns.
4. If `running`, resume per wake with one bare call under step 3's per-host wait
   rule, never looped or chained:

   ```powershell
   pwsh -NoProfile -File .agents/skills/codex-bridge/scripts/Invoke-Codex.ps1 -Wait <runId>
   ```

   Duration alone is not a failure. Done on `completed`, `malformed`, or
   `failed`.
5. Proceed on `completed` (`retried: true` needs no action); route `malformed`
   or `failed` to `## Rules`. Done when handled.

## Rules

- Report genuine failures (`failed`, `malformed`, non-zero exit, or step 1
  block/error) to the user with `reason` (or `checks`/`message`) without another
  diagnostic call.
- If a `completed` `read-only` result passes the verdict-line check but is
  judged malformed (leaked notes or wrong task), re-dispatch the identical
  prompt once with a fresh `<out>` and `-NoRetry`; a second malformed result is
  a genuine failure.
- `read-only` runs cannot write `<worktree>/Temp/` or `$env:TEMP`; keep handoffs
  within the `subagent-handoff.md` `## Handoffs` shared character cap without
  `Temp/` overflow writes (`Invoke-Codex.ps1` captures full output to `<out>`).
- For `workspace-write` routes in `Invoke-Codex.ps1` `$routes`, attempt each run
  once (never re-dispatch; report failed or judged-malformed outcomes with
  `git status`). Findings-only `workspace-write` runs write only their skill's
  gitignored `Temp/` files, never edit tracked files, and may run alongside
  `read-only` Step 6 peers. While an editing `workspace-write` run is live, hold
  `Invoke-StaticChecks.ps1`, concurrent writers, and any review reading C++ or
  dual-language headers — such as an overlapping `/glsl-review` during Step 6
  post-validator or post-round re-runs — until it terminates.
- Verify launcher changes without Codex CLI from Bash with a stub `codex` first
  on `PATH` (`PATH=<stub dir>:$PATH pwsh -NoProfile -File
  .agents/skills/codex-bridge/scripts/Invoke-Codex.ps1 -Skill ...`; PowerShell
  has no inline prefix form).

## References

- [scripts/Invoke-Codex.ps1](scripts/Invoke-Codex.ps1) — launcher (`$routes`,
  `$presets`, and `.NOTES` for receipts, exit codes, verdict check, retry, and
  `-CheckModels`).
