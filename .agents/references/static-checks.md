# Applicable Static Checks

The Step-4 checks an `implementer` runs itself, by changed artifact type. Run
every row the change triggers, and no row it does not. Compilation, PREfast, and
Clang-Tidy are not implementer checks here: root
[AGENTS.md](../../AGENTS.md) Step 4's `builder` bullet and
`Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md:23-26` own
them.

| Changed artifact | Check |
|---|---|
| C++ | the focused reads, searches, and traces that prove internal coherence, which `/implement-plan` already requires |
| a changed `SKILL.md`, `Documents/Plans/**` file, or markdown file | `pwsh -NoProfile -File .agents/scripts/Invoke-StaticChecks.ps1 -RepositoryRoot <worktree root> -Baseline <baseline SHA>` |

The runner selects and runs those three checks itself and emits one
`broken-engine-static-checks/v1` envelope holding a `checks` row per check:
`validate-skill` for each changed `SKILL.md`, `plan-scheduler` for a changed
Plan, and `markdown-links` for the relative link targets and heading anchors in
every changed markdown file. Add optional `-Head <commit>` to check a
committed head instead of the working tree, and the `-IncludeUntracked` switch
to include untracked files when checking the working tree.
