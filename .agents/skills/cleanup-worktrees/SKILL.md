---
name: cleanup-worktrees
description: >-
  Remove every worktree registered to the current Broken Engine repository,
  wherever it lives, once it is 48+ hours old by folder creation time,
  regardless of dirty or unlanded state, delete repository-root `Temp/` files
  48+ hours old by last-write time, and delete Codex CLI session logs under
  `~/.codex/sessions` 30+ days old by last-write time. Use this skill manually
  each morning before creating new sessions, once per primary checkout; it
  leaves younger worktrees untouched and reports saved Codex refs without
  deleting them.
argument-hint: [preview]
allowed-tools: [Read, Bash, PowerShell]
disable-model-invocation: true
---

# Cleanup Worktrees

## Purpose

Clean every worktree registered to the repository, except the primary checkout,
whose folder is 48+ hours old, repository-root `Temp/` files 48+ hours old, and
Codex session logs 30+ days old.

## When to use

Run this skill manually each morning before creating new sessions. Any
registered worktree older than 48 hours is removed with its uncommitted work,
whether a wrapper session is still running in it or you created it by hand —
close or land long-lived sessions before running cleanup. Each primary checkout
(for example `BrokenEngine` and `BrokenEnginePublic`) only sees its own
worktrees, so run it once from each.

## Steps

1. Accept either no argument or `preview`, and stop on any other argument. Done
   when the argument is one of those two.
2. Start in the primary checkout, never a session worktree, resolving that root
   with `git rev-parse --show-toplevel`. Done when the run directory is that
   resolved root.
3. Run the repository-owned script from that primary checkout root with no switch
   for cleanup, or `-Preview` for preview. Done when that invocation has returned
   its report.

   ```powershell
   pwsh -NoProfile -File .agents/skills/cleanup-worktrees/scripts/cleanup-worktrees.ps1
   ```

   ```powershell
   pwsh -NoProfile -File .agents/skills/cleanup-worktrees/scripts/cleanup-worktrees.ps1 -Preview
   ```

## Handoff

Return the script's complete report, including:

- cleanup status and primary checkout identity
- removed worktrees and branches
- retained worktrees with exact reasons
- removed `Temp/` files and emptied subfolders (eligible-only under preview)
- removed Codex session logs summarized per day folder with count and bytes,
  and emptied folders (eligible-only under preview)
- reported saved `refs/codex/snapshots/*` refs
- residuals as the final line, with distinct errors and one retained-worktree
  count instead of duplicated retained entries

## Rules

- Manual invocation authorizes removal of every registered worktree of the
  repository except the primary checkout, wherever its folder lives, past 48
  hours old by folder creation time — including dirty or unlanded ones — with no
  second confirmation.
- The same run deletes files under the repository-root `Temp/` directory whose
  last-write time is 48+ hours old, then removes emptied subfolders created 48+
  hours ago; `Temp/` itself and younger files stay, and reparse-point files and
  folders are skipped. `-Preview` lists eligible Temp files instead of deleting
  them.
- The same run deletes Codex CLI session log files (`rollout-*.jsonl` and
  `rollout-*.jsonl.zst` in `YYYY\MM\DD` folders) under the per-user
  `~/.codex/sessions` whose last-write time is 30+ days old, matching Claude
  Code's 30-day transcript retention. Each is deleted by its exact literal path;
  reparse points and any other name are skipped, and the pass is skipped with a
  residual when `.codex` or `sessions` is a reparse point. Emptied folders
  created 30+ days ago are removed one at a time; `~/.codex/archived_sessions`
  is untouched. `-Preview` lists the eligible logs per day folder instead of
  deleting them.
- The script blocks if the checkout's Git directory differs from its common Git
  directory; do not work around it.
- Do not recreate failed commands with broader Git or filesystem operations.
- The script deliberately refuses saved-ref deletion and global pruning, and
  never removes a worktree folder Git has not registered. It deliberately
  force-removes eligible worktrees and force-deletes their `claude/`/`codex/`/`opencode/`
  branches with `git branch -D`; a worktree on any other branch is removed but
  its branch is left.
