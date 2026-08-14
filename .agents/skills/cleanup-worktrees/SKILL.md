---
name: cleanup-worktrees
description: Remove Broken Engine session worktrees created by the Claude Code and Codex CLI wrapper scripts once they are 48+ hours old by folder creation time, regardless of dirty or unlanded state, and delete repository-root `Temp/` files 48+ hours old by last-write time. Use this skill manually each morning before creating new sessions; it leaves younger worktrees untouched and reports saved Codex refs without deleting them.
argument-hint: [preview]
allowed-tools: [Read, Bash, PowerShell]
disable-model-invocation: true
---

# Cleanup Worktrees

Clean retained Claude Code and Codex CLI worktrees whose folders are 48+ hours
old. Manual invocation authorizes removal of every wrapper-root worktree past
that age — including dirty or unlanded ones — with no second
confirmation.

The same run deletes files under the repository-root `Temp/` directory whose
last-write time is 48+ hours old, then removes emptied subfolders created 48+
hours ago; `Temp/` itself and younger files stay. `-Preview` lists eligible
Temp files instead of deleting them.

## Run

1. Accept either no argument or `preview`. Stop on any other argument.
2. Start in the primary checkout, never a session worktree. Resolve that root
   with `git rev-parse --show-toplevel`. The script blocks if the checkout's Git
   directory differs from its common Git directory; do not work around it.
3. Run the repository-owned script from that primary checkout root with no switch
   for cleanup, or `-Preview` for preview:

   ```powershell
   pwsh -NoProfile -File .agents/skills/cleanup-worktrees/scripts/cleanup-worktrees.ps1
   ```

   ```powershell
   pwsh -NoProfile -File .agents/skills/cleanup-worktrees/scripts/cleanup-worktrees.ps1 -Preview
   ```

Do not recreate failed commands with broader Git or filesystem operations. The
script deliberately refuses cleanup outside the two wrapper roots, saved-ref
deletion, and global pruning. Within those roots it deliberately force-removes
worktrees and force-deletes their `claude/`/`codex/` branches with
`git branch -D`.

A wrapper session still running after 48 hours will have its worktree removed —
close or land long-lived sessions before running cleanup.

## Report

Return the script's complete report, including:

- cleanup status and primary checkout identity
- removed worktrees and branches
- retained worktrees with exact reasons
- removed `Temp/` files and emptied subfolders (eligible-only under preview)
- reported saved `refs/codex/snapshots/*` refs
- residuals as the final line, with distinct errors and one retained-worktree
  count instead of duplicated retained entries
