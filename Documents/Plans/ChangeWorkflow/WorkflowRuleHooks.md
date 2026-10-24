<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-24T12:28:32.205Z","dependsOn":[]} -->
# Enforce the mechanically checkable shell rules with Claude Code hooks

## Context
Several shell rules live only as prose. The Bundled scripts bullet of root
`AGENTS.md` `### Directives` says: never change the working directory (no
`cd`, no `Set-Location`), never use `-ExecutionPolicy Bypass`, never run a
repository script by absolute path, and run one script per shell call with
nothing chained before or after it. The one exception is that, from the
PowerShell tool only, a call may use its own output: assign it, pipe it to
`ConvertFrom-Json`, or read `$LASTEXITCODE` after it. Root `AGENTS.md`
`## Environment` says to change tracked files with the host `Edit` tool, not
with shell edits. The Claude Code host's environment note says the Git stash
stack is shared by every worktree, so a session never uses bare `git stash` or
`git stash pop`; it uses `git stash push -u -m <tag>`, `git stash list`,
`git stash apply <sha>`, and `git stash drop`.

A prompt rule can be skipped. A hook cannot. The user read Anthropic articles on
multi-agent design and context engineering ("Enforcement lives in the harness")
and asked for this Plan. The user also set a constraint for this work: tokens
are limited, so there are no replay evals and no throwaway worktrees that
re-run old Plans.

`.claude/settings.json` already has a `hooks` object with three `SessionStart`
entries. It has no `PreToolUse` or `SubagentStop` entry.

## Design
This is the author's recommended design.

1. Verify the hook contract first. A worker does not delegate, so the executing
   main session runs `/verify-external-claims` (which dispatches one `locator`
   as its evidence worker) on the Claude Code hooks documentation, with one
   claim for each of these input and output propositions:
   - the `PreToolUse` stdin fields (`tool_name`, `tool_input.command`, `cwd`),
     that a matcher `Bash|PowerShell` matches both shell tools, and that exit
     code 2 blocks the call and shows stderr to the model;
   - whether the `SubagentStop` input carries the subagent's final message and
     its agent type, how exit code 2 or a block decision makes the subagent
     continue, and whether a `stop_hook_active` field marks a repeat stop.
   If a `PreToolUse` verdict refutes its claim, the script follows the
   documented contract. If a `PreToolUse` verdict is `UNRESOLVED`, the executing
   main session stops and reports it to the user before implementation. The
   `SubagentStop` check in step 4 is kept only if every `SubagentStop` verdict
   (final message, agent type, the block mechanism, and `stop_hook_active`) is
   `VERIFIED`; a `REFUTED` or `UNRESOLVED` verdict on any of them drops it, and
   the drop is recorded in the handoff.

2. Add one PowerShell 7 script, `.agents/scripts/Test-ShellCommandRules.ps1`.
   It reads the hook JSON from stdin. It exits 0 (allow) for any tool other than
   `Bash` or `PowerShell`, and for input it cannot parse, so a hook bug never
   blocks all shell use; the prose rules still apply. It exits 2 with one
   stderr line that names the correct form when a check fails. Its header
   comment documents each check. Before matching, it removes the text inside
   single-quoted strings, double-quoted strings, PowerShell here-strings, and
   Bash heredoc bodies, so a commit message or script argument that mentions a
   bad form is not blocked. The one exception is the script-path checks below,
   which also read a quoted script path, such as the path inside a
   `-Command "& '<path>' ..."` string.
   Checks, each a deterministic pattern:
   - Directory change: `cd`, `Set-Location`, or `Push-Location` as a command
     word (at the start, or after `;`, `&&`, `||`, `|`, `(`, or a newline).
   - `-ExecutionPolicy Bypass` in any case, including the `-ep` and `-exec`
     abbreviations.
   - Absolute script path: a `.ps1` or `.psm1` path that is rooted (a drive
     letter, `/`, `\`, or `~`) and whose normalized form starts with the
     input's `cwd`.
   - Chained script call: a repository script call (`pwsh` with `-File` and a
     relative `.ps1` path, `pwsh -Command "& '<relative .ps1>' ..."`, or
     `Import-Module ./<path>`) with any other command before or after it, split
     on `;`, `&&`, `||`, `|`, a newline, or `$(...)`. Redirections (`>`, `2>`,
     `2>&1`) are not chaining. From the PowerShell tool only, these are
     allowed: `$<name> = ` before the call, `| ConvertFrom-Json` after it, a
     later statement that only reads `$LASTEXITCODE`, and calls to the imported
     module's functions after `Import-Module ./<path>`. From the Bash tool none
     of them are allowed.
   - Git stash: `git` (with optional `-C <path>` options) then `stash` is
     blocked when it is bare, `pop`, `save`, `clear`, `push` without both `-u`
     (or `--include-untracked`) and `-m`, or `apply` without a hexadecimal SHA
     argument. Every other `git stash` form is allowed.
   - In-place shell edit: `sed` with `-i` (alone, combined like `-Ei`, or with a
     suffix like `-i.bak`) or `--in-place`, and `perl` with `-i` (including
     `-pi`), unless every file argument is under `Temp/`.
   A check that cannot be made without blocking a documented invocation (see
   the corpus check in `## Acceptance criteria`) is dropped from the script and
   listed in the handoff. It is never weakened into an approximation.

3. Register the script in `.claude/settings.json` `hooks` as a `PreToolUse`
   entry beside the `SessionStart` entries: matcher `Bash|PowerShell`, type
   `command`, command
   `pwsh -NoProfile -File .agents/scripts/Test-ShellCommandRules.ps1`. This is
   the same relative form the `SessionStart` entries use.

4. Only if step 1 keeps it: add `.agents/scripts/Test-SubagentHandoffForm.ps1`
   and register it as a `SubagentStop` entry. It checks only the agent types
   `implementer` and `reviewer`. The author's rationale: those two
   `.claude/agents/*.md` files are the ones that tell the role to author the
   shared handoff; `builder` and `locator` return other forms. It allows every
   other agent type, including the host's built-in agents. It also allows a
   stop the input marks as a repeat (`stop_hook_active`), so a subagent is sent
   back at most once. It blocks when the final message lacks a line that is
   exactly `Status: PASS`, `Status: NEEDS_ACTION`, or `Status: BLOCKED`; lacks a
   line starting with any of `Findings:`, `Changed files:`, `Decisive checks:`,
   `Build required:`, `Evidence:`, `Residuals:`; or has one of those field
   lines after the `Residuals:` line. The block message names the missing or
   misplaced field and cites `.agents/references/subagent-handoff.md`
   `## Handoffs`. It does not check the character cap, because skills declare
   different caps.

The hooks are Claude Code only. Codex and OpenCode read the same prose, so this
Plan removes no prose rule.

Cost: the hook starts `pwsh` on every shell call. Acceptance records the added
time per call.

## Critical files
- `.agents/scripts/Test-ShellCommandRules.ps1` — new `PreToolUse` hook script
- `.agents/scripts/Test-SubagentHandoffForm.ps1` — new `SubagentStop` hook
  script, only if step 1 of `## Design` keeps it
- `.claude/settings.json` — the `hooks` object

## In scope
- The `/verify-external-claims` verification in step 1 of `## Design`
- The new `.agents/scripts/Test-ShellCommandRules.ps1` with the checks
  `## Design` lists
- The new `.agents/scripts/Test-SubagentHandoffForm.ps1`, only if step 1 keeps
  it
- New `PreToolUse` and (if kept) `SubagentStop` entries in the `hooks` object
  of `.claude/settings.json`
- Test inputs and timing output under `Temp/` (gitignored)

## Out of scope
- Codex and OpenCode hooks, including `.codex/config.toml`
- Removing, rewording, or adding any prose rule in `AGENTS.md`, `.agents/`, or
  agent definitions
- The `SessionStart` entries and `Write-SessionWorkflowContext.ps1`
- Any other hook event
- The handoff character cap
- Tracked test files or test scripts; testing by trying to break rules in a
  live session

## Risk tier and invariants
The executing session classifies the tier: this adds two scripts and a settings
change that runs on every Claude Code shell call. Invariants:
- Every documented invocation form stays allowed.
- Input the script cannot parse, and every non-shell tool, is allowed.
- The `SessionStart` entries are unchanged.
- No home path or transcript text in any tracked file.

## Acceptance criteria
- The `/verify-external-claims` verdicts are recorded in the handoff with their
  `Sources` rows, including whether the `SubagentStop` check is kept or dropped
  and why.
- For each kept `PreToolUse` check, JSON inputs under `Temp/` fed directly to
  the script on stdin show: at least one bad form exits 2 with one stderr line
  naming the correct form, and its documented form exits 0. The stash cases
  cover bare, `pop`, `push -u -m <tag>`, `list`, `apply <sha>`, and `drop`. The
  chained cases cover the three PowerShell-only own-output uses, from both the
  PowerShell tool (exit 0) and the Bash tool (exit 2).
- Corpus check: every backticked command starting with `pwsh -NoProfile` in
  root `AGENTS.md` and `.agents/**/*.md`, fed as a `PowerShell` tool input,
  exits 0. Any check that blocks one of them is dropped and listed in the
  handoff.
- A non-shell tool input and an unparsable input both exit 0.
- If kept, the `SubagentStop` script exits 0 for a valid `implementer` handoff,
  for a `stop_hook_active` repeat, and for an agent type outside `implementer`
  and `reviewer`; it exits 2 for a missing `Status` line, a missing field, and
  a field after `Residuals:`.
- `.claude/settings.json` parses as JSON and its `SessionStart` entries are
  byte-identical to the baseline.
- The added time per call is recorded in the handoff: the median of 20 timed
  runs of the hook script on one allowed input.
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing.
