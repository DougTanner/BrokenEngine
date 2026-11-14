---
name: reset-worktree
description: >-
  Ready a landed wrapper session worktree for another /next-plan run: refuse
  unless it is clean and holds no commits the primary branch lacks, then
  fast-forward the session branch to the primary tip, delete the worktree's
  Temp directory, and rerun the setup a newly created session worktree gets.
  Use only when the user explicitly invokes this skill.
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
   pwsh -NoProfile -File .agents/skills/reset-worktree/scripts/Reset-AgentWorktree.ps1
   ```

2. Report the script's final `Reset worktree` line on exit 0, or its refusal
   or error message on exit 1. Done when the user has the outcome.

## Rules

- Leave the cause of a refusal or error for the user to resolve. A refusal
  changes nothing; after an error the branch may already be fast-forwarded and
  `Temp` deleted, and a rerun is safe once the cause is resolved.
- The reset deletes everything in the worktree's `Temp` directory, including
  the previous run's logs and handoff files. Any error during or after that
  delete and before setup rewrites the session sidecar, such as a process still
  holding a file in `Temp`, can leave the sidecar missing; the script refuses
  every rerun while that file is missing, as its message says.
- The reset does not change the worktree folder's creation time, which
  `/cleanup-worktrees` checks for removal.
