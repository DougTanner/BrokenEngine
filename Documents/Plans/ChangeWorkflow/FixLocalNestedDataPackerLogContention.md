<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-24T01:21:35.487Z","dependsOn":[]} -->
# Fix: .agents/skills/compile/scripts/Invoke-CompileBuild.ps1 — nested DataPacker build hits DataPacker.log sharing violation on Local builds

## Context

- 2026-11-24 (`00:28:47Z` and `00:33:26Z`), `agy/59a7e75f-7f37-4419-8429-d804bc090af1`,
  selector `.agents/skills/compile/scripts/Invoke-CompileBuild.ps1` (Local
  `BrokenEngineSandboxServer` Debug and `ModelViewer` Debug builds): both builds
  failed with MSBuild `error MSB3073` in the `PreBuildEvent` from
  `Engine/Platforms/VisualStudio2026/Engine.props:80-93` after the nested
  `devenv` build of `DataPacker/Platforms/VisualStudio2026/DataPacker.sln`
  (`Release|x64`, `DataPacker.vcxproj`) exited in ~0.25 s reporting
  `Failed to write to log file` on
  `DataPacker\Platforms\VisualStudio2026\Build\Release\DataPacker.log` because
  another process was using it. Main had to redispatch both targets, and
  `ModelViewer` required an additional retry before succeeding.
- Observed current-tree interaction: in a linked worktree Local build,
  `.agents/skills/compile/scripts/Invoke-CompileBuild.ps1:427-437` already runs
  `Invoke-WorktreeCliBuild -LocalDataPacker` (serialized on `datapacker.lock` by
  `Tools/WorktreeCli/BuildCommand.cpp:371-403`), then passes
  `/p:DataBuildMode=Local`, `/p:RunDataPacker=true`, and `/p:DevEnvDir=...` to
  each target build (`:362-367`, `:465`, `:488`), which holds only that target's
  own lock (`brokenenginesandboxserver.lock`, `modelviewer.lock`, etc.).
  `Engine/Platforms/VisualStudio2026/Engine.props:84-87` still unconditionally
  runs `devenv` on `DataPacker.sln` (`Release|x64`) inside `PreBuildEvent`,
  outside `datapacker.lock`, allowing concurrent Local builds in the same
  worktree to collide on `DataPacker.log`.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: agy
- Conversation session ID: 6953f189-bd31-4061-bdf2-0b285e6a400d
- Worktree/branch UUID: 59a7e75f-7f37-4419-8429-d804bc090af1
- Session branch: agy/59a7e75f-7f37-4419-8429-d804bc090af1
- Worktree: .agy\worktrees\BrokenEngine\59a7e75f-7f37-4419-8429-d804bc090af1
- Landing ref: agy/59a7e75f-7f37-4419-8429-d804bc090af1
  The review ref the invoker passes to /next-plan-review has two cases. When
  the `Observed in an earlier session` line is present, it is that line's
  landed commit hash, with no lookup. When that line is absent, the recording
  session is the observing session, and worktree reuse moves its branch past
  its landing, so the invoker never reviews the ref itself: it resolves the ref
  to the commit that added this Plan with
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
  Codex discovery requires a registered worktree, and Claude or agy review
  requires the conversation session ID above.

## Design

First confirm the build defect's root cause through `/external-diagnose-bug`
from the current tree and this Plan's `## Context` (confirming which concurrent
process held `DataPacker.log` — another target's nested `PreBuildEvent`
`devenv`, a concurrent `-LocalDataPacker` build, or an MSBuild node lingering
after `-LocalDataPacker` — and checking whether direct IDE Local
linked-worktree builds still rely on `Engine.props:84-87`). Only when
transcript evidence is genuinely needed, in a new session run
`/next-plan-review <review ref>` in bounded friction mode, with the review ref
the provenance block above defines, supplying client `agy` and the recorded
conversation session ID.

Recommended direction (subject to root-cause confirmation): when
`Invoke-CompileBuild.ps1` has already built `DataPacker` Release through
`WorktreeCli` (`-LocalDataPacker`), suppress the redundant nested `devenv`
`DataPacker` invocation in `Engine/Platforms/VisualStudio2026/Engine.props:84-87`
(for example via an MSBuild property passed by `Invoke-CompileBuild.ps1`),
preserving the existing `Engine.props` behavior for direct Visual Studio builds
where that property is unset. Rationale: eliminating the redundant unserialized
nested `devenv` build removes the shared `DataPacker.log` contention without
changing `WorktreeCli` lock granularity or IDE behavior.

Then make the smallest fix inside the `## In scope` boundary below. If
root-causing shows the fix lies outside that boundary (for example in
`Tools/WorktreeCli/BuildCommand.cpp`), surface it for re-planning instead of
expanding scope.

## Critical files

- `.agents/skills/compile/scripts/Invoke-CompileBuild.ps1` — `Invoke-WorktreeCliBuild`
  and the Local generation section (`:427-494`).
- `Engine/Platforms/VisualStudio2026/Engine.props` — `PreBuildEvent` linked-worktree
  nested `DataPacker` build block (`:80-93`).
- `.agents/skills/compile/references/runtime-data-mode.md` — `## Build invocation
  and derived properties` and `## Local generation` (update if the build
  properties or `DevEnvDir` contract change).
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md` —
  `PreBuildEvent` bullet (`:61-66`, update if `Engine.props` behavior changes).

## In scope

- Root-cause investigation as `## Design` states.
- The smallest resulting fix, confined to
  `.agents/skills/compile/scripts/Invoke-CompileBuild.ps1` (Local generation
  path), `Engine/Platforms/VisualStudio2026/Engine.props` (`PreBuildEvent`), and
  synchronizing `.agents/skills/compile/references/runtime-data-mode.md` and
  `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md` if build
  properties or `PreBuildEvent` behavior change.

## Out of scope

- The landed `MatchLightingAmbientToDirections` change.
- `Tools/WorktreeCli/` C++ build-lock implementation, primary checkout
  `DataPacker` bootstrap (`Engine.props:88-91`), Shared-mode builds, and
  unrelated skills or scripts.
- Any transcript path or transcript text in the repository.

## Risk tier and invariants

Expected Tier 2 (scoped build-script behavior); escalate to Tier 3 if the fix
modifies `Engine/Platforms/VisualStudio2026/Engine.props` build/bootstrap
coordination in a way that can affect other sessions or primary/IDE builds.
Preserve `ThirdPartyLibraryDirectory` candidate relinking
(`Invoke-CompileBuild.ps1:428-431`), primary Visual Studio `DataPacker`
bootstrap (`Engine.props:88-91`), and Windows PowerShell 5.1 compatibility in
`PreBuildEvent`. Never embed transcript paths or home paths.

## Acceptance criteria

- The recorded symptom (`DataPacker.log` sharing violation and `error MSB3073`
  from the nested `DataPacker` build) no longer reproduces when concurrent Local
  `/compile` builds (such as `Server` Debug and `ModelViewer` Debug) run in the
  same linked worktree.
- A Local `/compile` build in a linked worktree still builds `DataPacker`
  Release cleanly and runs `DataPacker.exe` export without regressing direct
  Visual Studio or primary builds.
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every row
  the change triggers passing.

## Notes

Proven pre-existing and out of scope of `MatchLightingAmbientToDirections`:
neither `Invoke-CompileBuild.ps1` nor `Engine.props` was part of that change.
Not a duplicate of any existing Plan.
