---
name: cleanup-worktrees
description: Remove every worktree registered to the current Broken Engine repository, wherever it lives, once it is 48+ hours old by folder creation time, regardless of dirty or unlanded state, and delete repository-root `Temp/` files 48+ hours old by last-write time. Use this skill manually each morning before creating new sessions, once per primary checkout; it leaves younger worktrees untouched and reports saved Codex refs without deleting them.
argument-hint: [preview]
allowed-tools: [Read, Bash, PowerShell]
disable-model-invocation: true
---

# Cleanup Worktrees

## Purpose

Clean every worktree registered to the repository, except the primary checkout,
whose folder is 48+ hours old.

## When to use

Run this skill manually each morning before creating new sessions. Any
registered worktree older than 48 hours is removed with its uncommitted work,
whether a wrapper session is still running in it or you created it by hand —
close or land long-lived sessions before running cleanup. Each primary checkout
(for example `BrokenEngine` and `BrokenEnginePublic`) only sees its own
worktrees, so run it once from each.

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

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. The run steps and the rules bounding
  them.
