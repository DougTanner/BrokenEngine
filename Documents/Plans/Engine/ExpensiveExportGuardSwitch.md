<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-02T00:17:45.033Z","dependsOn":[]} -->
# Fix: /compile — no documented way to set the expensive-export guard for a DataPacker run

## Context
Acceptance verification of a warm-cache DataPacker run needed
`BT_DATAPACKER_FORBID_EXPENSIVE_EXPORT=1` in the environment of
`pwsh -NoProfile -File .agents/skills/compile/scripts/Invoke-CompileBuild.ps1 -Target Client -Configuration Debug -DataBuildMode Local -RunDataPacker -Baseline <sha>`.
That script sets only the Gaea guard: it defines
`$script:GaeaGuardVariable = 'BT_DATAPACKER_FORBID_GAEA_EXPORT'` at line 49 and
scopes it around the generation build at lines 342-358, restoring the caller's
prior value afterwards. It exposes no parameter for the separate
expensive-export guard, which `DataPacker/Source/AGENTS.md:21` documents as the
guard for "verification runs that must stop before Gaea or texture
export/encoding", and which
`.agents/skills/compile/references/runtime-data-mode.md:31` mentions only as a
caller-provided variable the script must never clear.

The only route that honored the root `AGENTS.md` one-script-per-shell-call rule
was a Bash-tool environment prefix,
`BT_DATAPACKER_FORBID_EXPENSIVE_EXPORT=1 pwsh -NoProfile -File ...`, which is
not a documented invocation form for a bundled script. The session had to use
an undocumented invocation shape to obtain a guarded verification run.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 910b0da8-2f86-421a-b3c1-49d44781cd6a
- Worktree/branch UUID: 9e8570fa-0b70-4aaf-95e6-5e3699d37cfb
- Session branch: claude/9e8570fa-0b70-4aaf-95e6-5e3699d37cfb
- Worktree: .claude\worktrees\BrokenEngine\9e8570fa-0b70-4aaf-95e6-5e3699d37cfb
- Landing ref: claude/9e8570fa-0b70-4aaf-95e6-5e3699d37cfb
  Fallback once the recorded ref is gone:
  `git log --diff-filter=A --format=%H -- Documents/Plans/Engine/ExpensiveExportGuardSwitch.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
In a new session, run `/next-plan-review claude/9e8570fa-0b70-4aaf-95e6-5e3699d37cfb`,
supplying the recorded Claude client and the recorded conversation session ID.
Root-cause the friction from the proven transcript, then make the smallest fix
inside the `## In scope` boundary below. The author's recommendation, offered as
a starting point rather than a decision, is a `-ForbidExpensiveExport` switch on
`Invoke-CompileBuild.ps1` that scopes
`BT_DATAPACKER_FORBID_EXPENSIVE_EXPORT=1` around the generation build exactly
the way the Gaea guard is scoped and restored today, documented in
`.agents/skills/compile/references/runtime-data-mode.md`; the rationale is that
it reuses the mechanism already present in the script instead of adding a second
one, and keeps the existing rule that a caller-provided guard is never cleared.
If root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

## Critical files
- `.agents/skills/compile/scripts/Invoke-CompileBuild.ps1` — lines 49 and
  342-358, the guard variable definition and its scoped application
- `.agents/skills/compile/references/runtime-data-mode.md` — line 31, the
  documented guard policy
- `.agents/skills/compile/SKILL.md` — the documented invocation surface

## In scope
- Root-cause investigation via `/next-plan-review`, run with the recorded Claude
  client, the review ref named in `## Design`, and the recorded conversation
  session ID
- The smallest resulting fix, confined to the guard handling and parameter
  surface of `Invoke-CompileBuild.ps1` and the compile skill documentation of
  that surface in the files named above

## Out of scope
- The landed change the session produced
- DataPacker's own guard semantics and the code paths that honor
  `BT_DATAPACKER_FORBID_EXPENSIVE_EXPORT`, including the IBL coverage gap owned
  by `Documents/Plans/Engine/IblExpensiveExportGuard.md`
- Gaea guard policy and its authorization rules
- Unrelated skills or scripts; any transcript path or transcript text in the
  repository

## Risk tier and invariants
Expected Change Workflow Tier 2 (scoped tool behavior of one script and its
documentation); escalate if the fix reaches build/bootstrap coordination.
Preserve the existing Gaea guard behavior, the exact restoration of the caller's
prior environment, and the rule that a caller-provided guard is never cleared.
Never embed transcript paths or home paths.

## Acceptance criteria
- A guarded warm-cache DataPacker verification run is reachable through the
  documented `pwsh -NoProfile -File .agents/skills/compile/scripts/Invoke-CompileBuild.ps1 ...`
  invocation alone, with no environment prefix and no second command in the
  shell call
- An unguarded run and a run with the caller's own guard already set both behave
  as they do today, with the prior environment restored afterwards
- `/validate-skill` passes for any changed `SKILL.md`; plan validate exits 0
