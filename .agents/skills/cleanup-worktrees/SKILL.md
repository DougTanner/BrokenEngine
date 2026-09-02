---
name: cleanup-worktrees
description: Remove Broken Engine session worktrees created by the Claude Code and Codex CLI wrapper scripts once they are 48+ hours old by folder creation time, regardless of dirty or unlanded state, and delete repository-root `Temp/` files 48+ hours old by last-write time. Use this skill manually each morning before creating new sessions; it leaves younger worktrees untouched and reports saved Codex refs without deleting them.
argument-hint: [preview]
allowed-tools: [Read, Bash, PowerShell]
disable-model-invocation: true
---

# Cleanup Worktrees

## Purpose

Clean retained Claude Code and Codex CLI worktrees whose folders are 48+ hours
old.

## When to use

Run this skill manually each morning before creating new sessions. A wrapper
session still running after 48 hours will have its worktree removed —
close or land long-lived sessions before running cleanup.

## Handoff

Return the script's complete report, including:

- cleanup status and primary checkout identity
- removed worktrees and branches
- retained worktrees with exact reasons
- removed `Temp/` files and emptied subfolders (eligible-only under preview)
- reported saved `refs/codex/snapshots/*` refs
- residuals as the final line, with distinct errors and one retained-worktree
  count instead of duplicated retained entries

## References

- [`references/worker.md`](references/worker.md) — the run steps and the rules
  bounding them.
