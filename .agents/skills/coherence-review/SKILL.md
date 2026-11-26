---
name: coherence-review
description: >-
  Review session-changed non-C++, non-GLSL artifacts for semantic coherence as
  the fresh `reviewer` role of the Change Workflow Review and resolve
  correctness step, in one of two modes: the combined pass that also carries the
  Apply the triggered cleanup and Verify the acceptance table steps, and the
  coherence pass every other change uses. Use for every Review and resolve
  correctness review of a changed non-C++, non-GLSL artifact type (GLSL routes
  to `/glsl-review`). Findings only; never edits tracked files.
allowed-tools: [Read, Grep, Glob, Write, PowerShell]
---

# Coherence Review

## Purpose

Review the changed bytes of the non-C++, non-GLSL artifact types a Review and
resolve correctness dispatch owns for semantic coherence, and in combined mode
carry the Change Workflow Review and resolve correctness, Apply the triggered
cleanup, and Verify the acceptance table steps in that one dispatch.

## When to use

- Combined mode: the Review and resolve correctness combined pass, whenever
  `.agents/scripts/Get-ChangeWorkflowSteps.ps1` lists `/coherence-review` in
  combined mode — except when a Tier-1 change already passed combined mode and
  later adds or edits a follow-up Plan (even if the script still prints combined
  mode): run coherence mode with `PathPrefix` on the added or edited Plan `.md`
  and changed sibling support-folder paths (or prefixes), recheck invalidated
  carried items (non-landing `/verify-acceptance`,
  `/progressive-disclosure-review`, `/external-skill-creator` validate mode),
  and run separately triggered checks (final `Invoke-StaticChecks.ps1` and, at
  landing, `Executable Plan check`).
- Coherence mode: every other change with a changed non-C++, non-GLSL artifact
  (Tier 2+, Tier 1 outside combined mode, or the Tier-1 late-Plan exception
  above). These keep their separate Apply the triggered cleanup and Verify the
  acceptance table dispatches unchanged, and the scope, minimality, and
  simplicity checks run only at Tier 2+.
- GLSL shaders and dual-language headers route to `/glsl-review`, not
  `/coherence-review`.
- The Verify and land step's landing acceptance table is never folded into
  either mode; see
  [`/verify-acceptance`](../verify-acceptance/SKILL.md) `## When to use`.

## Inputs

- Tier 2+ only: the authorization source for the scope checks, as
  [`scope-authorization.md`](../../references/scope-authorization.md)
  "Authorization source:" defines it.
- `Baseline` — full baseline Git SHA, repo toplevel, and committed head if any.
- `Scope` — first-round: untracked paths and, optionally in coherence mode after
  an earlier pass on prior non-C++ files, `PathPrefix` (comma-separated paths or
  directory prefixes for newly added or edited non-C++ files; see `## When to
  use` for late-Plan sibling scope). Re-review: affected files, regions,
  `First-round findings`
  (`../../references/subagent-reporting.md` `## Task brief`), and for Coretide
  paths `coretide` revisions from the inventory supplying those regions.
- The tier and the mode; in combined mode, which Apply the triggered cleanup
  checks the change triggers and whether the Verify the acceptance table step
  applies.
- In combined mode, every input each triggered cleanup skill's `## Inputs`
  requires, such as a focused re-review's `First-round findings`, and, when the
  Verify the acceptance table step applies, every input `/verify-acceptance`
  `## Inputs` requires.

First-round region inventory (`-Head` for a committed head;
`-UntrackedPath`/`-IncludeUntracked` and `-PathPrefix` from `Scope`):

```powershell
pwsh -NoProfile -NonInteractive -Command 'Import-Module ./.agents/scripts/SessionChangeInventory.psm1; Get-SessionChangeInventory -RepositoryRoot (Get-Location).Path -Baseline <baseline SHA> -Regions -CoherenceOnly|Select-Object status,coretide,counts,entries,regions|ConvertTo-Json -Depth 5'
```

## Handoff

Return the shared handoff from
[`subagent-handoff.md`](../../references/subagent-handoff.md) `## Handoffs`,
with these declared extension fields:

- `Scope: PASS | NEEDS_ACTION | not applicable (Tier 1) | not supplied`
- `Coherence` — one line per semantic finding, naming its shared `Findings` row
  by ID and adding only what that one-line row cannot carry, or an explicit
  no-finding statement.
- `Apply the triggered cleanup` — combined mode only; one row per triggered
  cleanup skill, naming the skill, its status, and the `Findings` row IDs it
  reports.
- `Criteria` — as [`/verify-acceptance`](../verify-acceptance/SKILL.md)
  `## Handoff` defines it; combined mode only, omitted when the Verify the
  acceptance table step does not apply.

In combined mode no check is dropped: each component keeps its owning skill, its
own result block, and its own pass condition; only the dispatch is shared. Each
triggered cleanup skill's own complete handoff goes verbatim into one `Temp/`
file, cited under `Evidence` as that path plus one selector per skill, with that
skill's `Apply the triggered cleanup` row inline. Never summarize, reformat, or
merge them into the fields above. A finding more than one triggered skill
reports appears once as a shared `Findings` row, and each skill's verbatim
handoff and `Apply the triggered cleanup` row names it by that ID instead of
restating it.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. The review steps and rules the
  dispatched reviewer runs.
