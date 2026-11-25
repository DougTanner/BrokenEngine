# Codex Bridge

## Purpose

Runs one routed review brief on Codex headless in a worktree from a non-Codex
host and returns its result and out-file path without its work entering this
session. A bridge run is the assigned role's delegated execution context
([`subagent-reporting.md`](subagent-reporting.md)
`## Delegated execution context`), not an inline run; since `codex exec` loads
no role definition, the brief's `Role` line carries it.

## When to use

Dispatched per the Codex-bridge rule in
[`change-workflow.md`](change-workflow.md) `### Delegation roles`.

## Inputs

- Prompt — fresh repo-relative `Temp/` file holding one task brief
  (`subagent-reporting.md` `## Task brief`), filled as a native dispatch would
  be.
- Worktree — this session's worktree.
- `-Skill` — bare skill name from the `-ListRoutes` key (`$routes` and
  `$presets` own preset/sandbox selection and validation).
- `-Mode` — pass the `<mode>` suffix whenever the `-ListRoutes` key is
  `<skill>:<mode>`; omit otherwise.

## Steps

1. Select the launch environment before checking models. Use the direct shell
   on hosts such as Claude Code. On Antigravity CLI, use the official
   per-command host request when available and granted, with the configured
   Codex PATH (confirm worker survival across calls only for workflows that
   resume after the launch shell exits; direct launches follow step 3). Use
   desktop PowerShell when that environment is unavailable or unproven. Honor a
   declined action.

   Once per non-Codex session before the first launch, run the check from the
   adopted worktree root in the selected environment with the same PATH and
   temporary-root overrides as the launch.

   ```powershell
   pwsh -NoProfile -File .agents/scripts/Invoke-Codex.ps1 -CheckModels
   ```

   For desktop commands, give the user the exact command with actual values.
   Quote PowerShell values as literals and double embedded apostrophes. Set
   only the PATH and temporary-root overrides deliberately used for the run.
   Append `> '<fresh Temp/receipt>'` to capture stdout and have the user return
   the receipt path and `$LASTEXITCODE`.

   Proceed after a passing receipt. On a non-zero exit, halt and report the
   receipt's diagnostics (see `Invoke-Codex.ps1` `.NOTES` for exit codes and
   receipt fields). Done when passed or reported.
2. End the prompt with the verdict-line contract: the worker's last non-empty
   response line must be `PASS`, `CHANGES-REQUIRED: <n>`, or
   `BLOCKED: <reason>`, outside the skill's or lens's complete output — after
   `Residuals` for the shared handoff
   ([`subagent-handoff.md`](subagent-handoff.md) `## Handoffs`), or after a
   lens's own output shape (e.g. `/external-architecture-review` Step 9) —
   supplementing `Status` (without replacing it) where that shape declares it.
   Done when the prompt states it.
3. Run the public launch once from the same worktree root and environment as
   step 1, with a fresh repo-relative `Temp/` `<out>`. For direct launch and
   wait, use a timeout `>= 600000` ms on blocking-shell hosts such as Claude
   Code. For a direct Antigravity CLI launch, pass `-WaitBudgetSeconds 0` to
   `Invoke-Codex.ps1` via `run_command` with `WaitMsBeforeAsync: 10000` and
   await its asynchronous wakeup, because `run_command` has no 600s cap and its
   Windows Job Object terminates detached children if the parent exits early
   with `running`. Resolve an ambiguous return using the receipt and run
   identity before retrying.

   ```powershell
   pwsh -NoProfile -File .agents/scripts/Invoke-Codex.ps1 -Skill <skill> [-Mode <mode>] -Worktree '<worktree>' -PromptFile '<prompt>' -OutFile '<out>' [-WaitBudgetSeconds 0] [-NoRetry]
   ```

   A `completed` receipt includes the findings on stdout. `Invoke-Codex.ps1`
   `.NOTES` owns receipt, wait, and separator. Done when the receipt returns.
4. After the original shell exits with `running`, use the actual run ID and
   resume with one call per receipt under step 3's wait rule. If the caller
   cannot see desktop worker records, run the wait from the same desktop
   worktree root with the same deliberate environment overrides. Capture its
   stdout and exit code separately from the launch.

   ```powershell
   pwsh -NoProfile -File .agents/scripts/Invoke-Codex.ps1 -Wait '<runId>'
   ```

   Duration alone is not a failure. Done on `completed`, `malformed`, or
   `failed`.
5. Proceed on `completed` (`retried: true` needs no action); route `malformed`
   or `failed` to `## Rules`. Done when handled.

## Rules

- On a `failed` `read-only` receipt (see `Invoke-Codex.ps1` `.NOTES` for
  exit-code semantics), diagnose the cause first by inspecting the run and
  stderr records from the original launch environment (`$recordDirectory`,
  `Get-RunRecordPath`, and `Get-StderrRecordPath` in `Invoke-Codex.ps1`) when
  available, or using the receipt's `reason` when those records are missing. Fix
  or report non-transient script, auth, or config errors immediately; for
  transient failures, retry up to 2 more times with a fresh `<out>`, waiting 10
  seconds before the first retry and 60 seconds before the second using a timer,
  and report to the user with `reason` if retries still fail. Report
  `malformed`, a non-zero `Invoke-Codex.ps1` process exit, or step 1 block/error
  to the user with `reason` (or `checks`/`message`) without another diagnostic
  call.
- If a `completed` `read-only` result passes the verdict-line check but is
  judged malformed (leaked notes or wrong task), re-dispatch the identical
  prompt once with a fresh `<out>` and `-NoRetry`; a second malformed result is
  a genuine failure.
- `read-only` runs cannot write `<worktree>/Temp/` or `$env:TEMP`; keep handoffs
  within the `subagent-handoff.md` `## Handoffs` shared character cap without
  `Temp/` overflow writes (`Invoke-Codex.ps1` captures full output to `<out>`).
- For `workspace-write` routes, attempt each run once (never re-dispatch; report
  failed or judged-malformed outcomes with `git status`). Findings-only
  `workspace-write` runs write only their skill's gitignored `Temp/` files,
  never edit tracked files, and may run alongside `read-only` Step 6 peers.
  While an editing `workspace-write` run is live, hold
  `Invoke-StaticChecks.ps1`, concurrent writers, and any review reading C++ or
  dual-language headers — such as an overlapping `/glsl-review` during Step 6
  post-validator or post-round re-runs — until it terminates.
- Verify launcher changes without Codex CLI from Bash with a stub `codex` first
  on `PATH` (`PATH=<stub dir>:$PATH pwsh -NoProfile -File
  .agents/scripts/Invoke-Codex.ps1 -Skill ...`; PowerShell has no inline prefix
  form).

## References

- [../scripts/Invoke-Codex.ps1](../scripts/Invoke-Codex.ps1) — launcher
  (`.NOTES` for diagnosing receipts, exit codes, verdict check, retry, and
  `-CheckModels`).
