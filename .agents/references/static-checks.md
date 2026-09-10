# Applicable Static Checks

Implementation slices run the focused reads, searches, traces, and self-checks
needed for their own internal coherence. After propagation, an `implementer` or
main runs the full applicable static pass at the Run targeted pre-review checks
step; that step's row in the Change Workflow says which.
Run every row the combined change triggers, and no row it does not.
Compilation, PREfast, and Clang-Tidy are not part of this pass: that
step's `builder` bullet and
`Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md:23-26` own
them.

| Changed artifact | Check |
|---|---|
| C++ | focused reads, searches, and traces inside each implementation slice; no full runner row |
| a changed file under `.agents/skills/`, `Documents/Plans/**` file, or markdown file | after propagation, `pwsh -NoProfile -File .agents/scripts/Invoke-StaticChecks.ps1 -RepositoryRoot <worktree root> -Baseline <baseline SHA>` |

The runner selects and runs those three checks itself and emits one
`broken-engine-static-checks/v1` envelope holding a `checks` row per check:
one `validate-skill` row carrying a `results` entry per skill package that holds
a changed file and a head-side `SKILL.md` — and per such package when
the `/external-skill-creator` validate mode's own validator script changed — `plan-scheduler` for a
changed Plan, and `markdown-links` for the relative link targets and heading
anchors in every changed markdown file. Add optional `-Head <commit>` to select
the changed files from a committed head instead of from the working tree, and
the `-IncludeUntracked` switch to include untracked files when checking the
working tree. Only `markdown-links` reads content from that commit; the `validate-skill`
row runs the validator over the working tree's copy of every selected package, and
head mode reaches `plan-scheduler` only through the inventory's Plan-touched
trigger, whose run reports the working tree's scheduler state.

Focused evidence from an implementation slice may be reused when its inputs
are unchanged. An edit during `/update-affected-code`, finding resolution, or
another later stage invalidates only evidence whose inputs it changes; rerun
those affected checks before relying on them.
