---
name: reset-worktree
description: >-
  Ready a landed wrapper session worktree for another /next-plan run: refuse
  unless it and its Coretide clone are clean and hold no commits the primary
  branch or the clone's upstream lacks, then fast-forward the session branch to
  the primary tip, delete the worktree's Temp directory, and rerun the setup a
  newly created session worktree gets. Use only when the user explicitly
  invokes this skill.
disable-model-invocation: true
allowed-tools: [Bash, PowerShell]
---

# Reset Worktree

## Purpose

Reuse one wrapper session worktree across `/next-plan` runs by bringing it to
the primary tip and rerunning the setup session creation runs.

## When to use

- The user explicitly invokes this skill in a session worktree whose last run
  has landed. The order after a landing is: `/reset-worktree` in the same
  conversation, right after its `SESSION COMPLETE` line, so the reset stays out
  of the next run's context; then a fresh conversation (`/clear` in Claude
  Code); then `/next-plan`.

## Steps

1. From the session worktree root, run the reset script in the background and
   await its completion. Done when the script has exited.

   ```powershell
   pwsh -NoProfile -NonInteractive -File .agents/skills/reset-worktree/scripts/Reset-AgentWorktree.ps1
   ```

2. On exit 0, report the final `Reset worktree` line, then end the turn's final
   message, after a newline, with the bare line `WORKTREE RESET DONE` followed
   by the bare last line `/clear?`, asking nothing else. On any nonzero exit,
   report stderr refusal/error output and print neither line.

## Rules

- Leave the cause of a refusal or error for the user to resolve, using the
  script's diagnostic; a rerun is safe once the cause is resolved.
- The reset deletes everything in the worktree's `Temp` directory, including
  the previous run's logs and handoff files; session metadata lives outside
  `Temp` and is preserved (`Initialize-AgentWorktreeSession.ps1` and
  `Get-AgentWorktreeSessionContext` own its storage and derivation).
- The reset does not change the worktree folder's creation time, which
  `/cleanup-worktrees` checks for removal.
