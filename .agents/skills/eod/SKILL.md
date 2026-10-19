---
name: eod
description: End the day for the current wrapper session - stop new work, stop running subagents and record their continuation capsules, leave the worktree in a stable state without landing, and give the user the exact command that reattaches the worktree and resumes the session. Use only when the user explicitly invokes this skill.
disable-model-invocation: true
---

# End of Day

## Purpose

Stop the session at a stable point, save what the next session needs in the
worktree, and give the user the command that reattaches the worktree and
resumes the work.

## When to use

- The user explicitly invokes this skill to stop for the day and resume the
  session later.

## Steps

1. Limit the rest of the turn to the steps below, leaving every new dispatch,
   edit, build, harness run, and user question to the resumed session. Done
   when no new work has started.
2. Stop every running subagent and background command with the host's stop,
   except a `/finalize-changes` worker, which may be changing primary: let it
   finish and keep its landing outcome for steps 5 and 7. Done when the host
   reports nothing running.
3. Build each stopped worker's continuation capsule from its partial evidence,
   as [`subagent-reporting.md`](../../references/subagent-reporting.md)
   `## Whether a worker is still running, and interruption` describes. Done
   when every stopped worker has one.
4. From the worktree root, run `Get-AgentWorktreeSessionContext` as
   [`subagent-reporting.md`](../../references/subagent-reporting.md)
   `## Task brief` shows, and `git status --porcelain`. Done when `Worktree`,
   `Branch`, `PrimaryRoot`, and the uncommitted files are in context.
5. Write `Temp/eod-resume.md` in the worktree, replacing any earlier copy. Open
   it with: "Resume from this file: redispatch each stopped worker as a fresh
   worker of the same role carrying its capsule, then take the next action."
   Done when the file holds every item listed next, or `none` for it.
   - Objective and current Change Workflow step.
   - Each stopped worker's role and continuation capsule.
   - The finished `/finalize-changes` worker's landing outcome.
   - Uncommitted files, and builds or checks still open.
   - Each pending user question, verbatim.
   - The next action.
6. Build the resume command from the `Branch` prefix with the table below,
   copying `Worktree` from step 4; for `claude/`, convert it with `cygpath -u`.
   Done when the command is built, or `Branch` has none of these prefixes and
   the message will say no reattach command applies.

   | Prefix | Shell at `PrimaryRoot` | Command | Conversation |
   | --- | --- | --- | --- |
   | `claude/` | Git Bash | `.claude/claude-worktree.sh --reattach-worktree '<path>'` | The wrapper adds `--resume`; pick this conversation. |
   | `codex/` | PowerShell 7 | `.\.codex\codex-worktree.ps1 -ReattachWorktree '<path>'` | Starts a new conversation. |
   | `opencode/` | PowerShell 7 | `pwsh -NoProfile -File .opencode/opencode-worktree.ps1 -ReattachWorktree '<path>'` | Starts a new conversation. |

7. End the turn. Done when its final message names what stopped, step 5's
   landing outcome, uncommitted files, open builds or checks, pending questions,
   the resume file, the command, where to run it, its Conversation note, and,
   for a new conversation, the prompt
   `Read Temp/eod-resume.md and resume the session from it.`

## Rules

- Leave every change uncommitted in the retained worktree; landing,
  committing, and worktree removal wait for the resumed session, apart from
  what a `/finalize-changes` worker left running in step 2 completes.
- Keep the resume state in `Temp/eod-resume.md`: `Temp/` is gitignored and
  survives reattach, and the Codex and OpenCode wrappers start a new
  conversation, which the final message alone would not reach.
- Write the final message under
  [`change-workflow.md`](../../references/change-workflow.md)
  `### User Interaction`.
