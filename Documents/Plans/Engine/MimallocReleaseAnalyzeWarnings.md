<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-16T01:21:18.616Z","dependsOn":[]} -->
# Fix: Mimalloc.cpp — Release builds report /analyze warnings from mimalloc

## Context
Observed symptom: in a `/next-plan` run, a `/compile` `builder` built
BrokenEngineSandbox Client and Server and ModelViewer, each in Release x64,
through `.agents/skills/compile/scripts/Invoke-CompileBuild.ps1` with no
`-Prefast` (the envelopes carry `/p:RunCodeAnalysis=false`). Every Release
build succeeded (`status: success`, exit 0) but its envelope `diagnostics`
held the same 14 `severity: warning` entries, all in ThirdParty mimalloc
sources and none in session-changed code:

- C6326 — `src/prim/windows/prim.c:781`, `include/mimalloc/alloc/page.h:76`,
  `src/util/options.c:294`, `:297`, `:305`, `src/arena/arena-page.c:497`,
  `src/alloc/page.c:868`, `src/profile/pprof.c:660`
- C6239 — `include/mimalloc/alloc/page.h:76`
- C6011 — `src/alloc/free.c:265`
- C6236 — `src/alloc/page.c:868`
- C6246 — `src/heap/init.c:410`, `src/heap/theap.c:488`, `:518`

The Debug builds of the same targets reported none. The execution card's
acceptance criterion "all build without warnings or errors" therefore could not
pass for any Release build: the `builder` returned `NEEDS_ACTION`, and main
waived an approved criterion on its own judgment. The friction surfaced through
`.agents/skills/compile/SKILL.md`, whose handoff has no warning row, but the
warnings themselves contradict a documented build policy.

What the current tree shows:

- `Engine/Source/Memory/Mimalloc.cpp` is the one translation unit that compiles
  mimalloc (`#include "mimalloc/src/static.c"`). It wraps the include in
  `#pragma warning(push, 0)` and
  `#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)`. No file in the
  repository, mimalloc included, includes `<CodeAnalysis/Warnings.h>`, the MSVC
  header that defines `ALL_CODE_ANALYSIS_WARNINGS`, so that pragma most likely
  names an undefined macro and disables nothing (unconfirmed).
- Its per-file `ClCompile` metadata in
  `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj`,
  `BrokenEngineSandboxServer.vcxproj`, and
  `Projects/ModelViewer/Platforms/VisualStudio2026/ModelViewer.vcxproj` sets
  `WarningLevel` `TurnOffAllWarnings` and `TreatWarningAsError` false, but not
  `EnablePREfast`, so it inherits the Release `ClCompile` block's
  `<EnablePREfast>true</EnablePREfast>`.
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md` states
  that this vendor implementation "retains its ThirdParty warning policy:
  compiler warnings and warning promotion are disabled", and, in its "Microsoft
  code analysis" bullet, that `EnablePREfast` keeps cl running `/analyze` on
  agent Release builds and must never be dropped to quiet a warning. The
  observed output contradicts the first statement for analyzer warnings.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 683f420b-b2ee-4d0d-acde-5cf06ff34e64
- Worktree/branch UUID: 42225028-bbfa-4078-98f5-84becc296ea6
- Session branch: claude/42225028-bbfa-4078-98f5-84becc296ea6
- Worktree: .claude\worktrees\BrokenEngine\42225028-bbfa-4078-98f5-84becc296ea6
- Landing ref: claude/42225028-bbfa-4078-98f5-84becc296ea6, the session branch
  above, which survives exactly as long as the worktree recorded above.
  The review ref the invoker passes to /next-plan-review: worktree reuse moves
  this branch past its landing, so the invoker never reviews the ref itself: it
  resolves the ref to the commit that added this Plan with
  `git log --follow --diff-filter=A -n 1 --format=%H <ref> -- <this plan path>`
  and passes that commit. When the recorded ref is gone or that command prints
  nothing (a later session moved this Plan), it runs the same command with
  `<ref>` omitted. A periodic Plan-history squash can make either command
  return an unrelated aggregate commit, and a landing that also deletes a
  similar file can make it return another session's commit, so review the
  result only when the commit is attributable to the observing session recorded
  above alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's
`## Context`: confirm why the Release `/analyze` pass still reports mimalloc
warnings through `Mimalloc.cpp`'s suppression, for example by checking whether
`ALL_CODE_ANALYSIS_WARNINGS` is defined at that pragma. Only when the
transcript is genuinely needed, in a new session run
`/next-plan-review <review ref>` in bounded friction mode, with the review ref
the provenance block above defines, supplying the recorded client and the
recorded conversation session ID. Then make the smallest fix inside the
`## In scope` boundary below. If root-causing shows the fix lies outside that
boundary, surface it for re-planning instead of expanding scope.

The author recommends fixing the vendor translation unit rather than the
`/compile` handoff or execution-card wording: the documented ThirdParty warning
policy already says this unit reports no warnings, so making the build honor it
lets a "builds without warnings" criterion pass as written, with no new rule
separating ThirdParty warnings from changed-code warnings. Preferred order:

1. If the pragma names an undefined macro, make the existing suppression work
   in `Mimalloc.cpp` (include the header that defines it, or disable analysis
   for the included vendor code by another source-level means).
2. Only if no source-level suppression silences `/analyze` there, turn
   `EnablePREfast` off in `Mimalloc.cpp`'s per-file `ClCompile` metadata in all
   three projects. This is per-file only; the Release `ClCompile` block's
   `EnablePREfast` gate stays on for every other source.

Update the Mimalloc bullet in
`Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md` only if the
chosen fix changes what that bullet states.

## Critical files
- `Engine/Source/Memory/Mimalloc.cpp`
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj`
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandboxServer.vcxproj`
- `Projects/ModelViewer/Platforms/VisualStudio2026/ModelViewer.vcxproj`
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the warning suppression in
  `Mimalloc.cpp`, the `Mimalloc.cpp` per-file `ClCompile` item in each of the
  three project files, and the Mimalloc bullet of the BrokenEngineSandbox
  VisualStudio2026 `AGENTS.md`

## Out of scope
- Any edit under `ThirdParty/`, including mimalloc sources
- The project-level Release `EnablePREfast` gate, `RunCodeAnalysis`,
  `BrokenEngineAnalysis.ruleset`, and `CodeAnalysisTreatWarningsAsErrors`
- `Mimalloc.cpp`'s arena and reserve definitions
  (`MI_DEFAULT_ARENA_EAGER_COMMIT`, `MI_DEFAULT_RESERVE_OS_MEMORY`) and every
  other compiler setting of that unit
- `.agents/skills/compile/`, the `/compile` handoff shape, and execution-card
  wording
- The landed change the session produced
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior: one vendor translation unit's build
configuration; trigger: build configuration of one unit, no runtime,
determinism, wire, or data-layout exposure, since `/analyze` and warning
pragmas do not change generated code). Escalate if the fix reaches
build/bootstrap coordination. Invariants: every non-vendor source keeps
`/analyze` in Release; mimalloc's compiled code and reserve sizes are
unchanged. Never embed transcript paths or home paths.

## Acceptance criteria
- `/compile` Release x64 builds of BrokenEngineSandbox Client, Server, and
  ModelViewer report no warning diagnostics in their envelopes; the recorded
  symptom no longer reproduces under the documented invocation
- The Debug x64 builds of the same targets still succeed with no warnings
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
