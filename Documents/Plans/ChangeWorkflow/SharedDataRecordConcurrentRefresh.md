<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-15T22:49:21.559Z","dependsOn":[]} -->
# Fix: /compile Shared data record — listing captures a concurrent primary Shared data refresh

## Context
Observed symptom. A `/compile` of `BrokenEngineSandboxServer` Debug in Shared
data mode (`DataBuildMode=Shared`, `RunDataPacker=false`, `GameDataDirectory`
the primary checkout's
`Projects\BrokenEngineSandbox\Platforms\VisualStudio2026\Output\Data`) succeeded
(MSBuild exit 0, started 22:10:57 UTC) while another session's primary Shared
data refresh was running. The record it wrote,
`Output\SharedDataRecord.BrokenEngineSandboxServer.Debug.txt`, listed the
in-flight DataPacker staging files `Shader.h.tmp`, `Shader.manifest.tmp`, and
`Shader.pack.tmp`. The next
`.agents/skills/agent-harness/scripts/Invoke-HarnessClaim.ps1` run refused with
`shared-data.changed`: "The Shared data differs from this worktree's Server
Debug record: 7 line(s) ... first 'Shader.h.tmp'". Timeline (local time): the
primary `SharedDataStamp.txt` was rewritten at 17:11:02, the record was written
at 17:11:35, and at 17:13 the primary data directory held no `.tmp` files.
Workaround it forced: a blocked baseline harness run, a second ~5-minute
`/compile` to rewrite the record, and a re-dispatched harness runner.

Durable code facts the investigation starts from:
- `.agents/skills/compile/scripts/Invoke-CompileBuild.ps1:349-361` takes the
  listing with `Get-AgentDataListing` before `Invoke-WorktreeCliBuild` and
  writes it with `Write-AgentSharedDataRecord` after a successful build. The
  listing step takes no lock shared with the refresh.
- `Get-AgentDataListing` (`.agents/scripts/AgentScriptCommon.psm1`) lists every
  file at any depth, so DataPacker's same-directory staging files
  (`<Name>.manifest.tmp`, `<Name>.pack.tmp`, `<Name>.h.tmp`, created in
  `DataPacker/Source/Main.cpp` beside the finals and renamed over them at
  publish) enter the listing whenever a refresh is mid-export.
- `Compare-AgentSharedDataRecord` (same module) compares record and current
  listing line by line, so any line in only one of them refuses.
- The refresh in `.agents/scripts/Bootstrap-AgentTools.ps1` (the
  `foreach ($gameProject in $gameProjects)` export loop) deletes
  `SharedDataStamp.txt`, runs DataPacker into the primary data directory, then
  rewrites the stamp, all under the bootstrap mutex, which `/compile`'s Shared
  path never takes. `Resolve-CompileContext.ps1` chooses Shared from the stamp
  before the build, so a refresh can start between that choice and the listing.
- Excluding `.tmp` lines alone may not suffice: the refresh's publish renames
  change the finals' sizes and write times too, so a listing taken mid-refresh
  can still differ from the post-refresh directory.

Root cause is not confirmed: whether the listing should exclude staging files,
be retaken or rejected when the stamp changed during the build, or be
coordinated with the refresh is open.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 55233b90-5aca-42bc-a86a-268e4e3d8263
- Worktree/branch UUID: 82e6d120-7288-496e-8cea-1e57980cade4
- Session branch: claude/82e6d120-7288-496e-8cea-1e57980cade4
- Worktree: .claude\worktrees\BrokenEngine\82e6d120-7288-496e-8cea-1e57980cade4
- Landing ref: claude/82e6d120-7288-496e-8cea-1e57980cade4, which survives
  exactly as long as the worktree recorded above.
  The review ref the invoker passes to /next-plan-review: the recording session
  is the observing session, and worktree reuse moves its branch past its
  landing, so the invoker never reviews the ref itself: it resolves the ref to
  the commit that added this Plan with
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
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
First root-cause the friction from the current tree and this Plan's
`## Context` with `/external-diagnose-bug`. Only when the transcript is
genuinely needed, in a new session run `/next-plan-review <review ref>` in
bounded friction mode, with the review ref the provenance block above defines,
supplying the recorded client and the recorded conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below. If root-causing
shows the fix lies outside that boundary, surface it for re-planning instead of
expanding scope.

The author recommends preferring a fix that keeps `/compile` from writing a
record whose listing a concurrent refresh produced (for example, comparing the
primary stamp before and after the build) over one that makes `/compile` wait on
the bootstrap mutex, because a wait would couple every Shared build to a
refresh that can run for a long time; the investigation decides.

## Critical files
- `.agents/skills/compile/scripts/Invoke-CompileBuild.ps1`
- `.agents/scripts/AgentScriptCommon.psm1`
- `.agents/skills/compile/references/runtime-data-mode.md`

## In scope
- Root-cause investigation as `## Design` states, reading
  `.agents/scripts/Bootstrap-AgentTools.ps1` and `DataPacker/Source/Main.cpp`
  for the refresh's ordering without changing them
- The smallest resulting fix, confined to the Shared-record branch of
  `Invoke-CompileBuild.ps1` (the `if (-not $runGeneration)` block),
  `Get-AgentDataListing`, `Write-AgentSharedDataRecord`, and
  `Compare-AgentSharedDataRecord` in `AgentScriptCommon.psm1`, and the record
  description in `runtime-data-mode.md` `## Generated Data directory inventory`
  when the fix changes what it states

## Out of scope
- The landed change the session produced
- `Bootstrap-AgentTools.ps1`, DataPacker, and the harness claim and launch
  scripts' refusal codes
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior: the `/compile` Shared record writer and
its listing helper). Escalate to Tier 3 if the fix adds coordination with the
bootstrap refresh that can block other sessions, such as a wait on the
bootstrap or DataPacker mutex. The harness must still refuse a record taken
before a refresh completed; the fix must not make a stale record compare equal.
Never embed transcript paths or home paths.

## Acceptance criteria
- The recorded symptom no longer reproduces under the documented invocation: a
  Shared `/compile` overlapping a primary Shared data refresh never writes a
  record listing `.tmp` staging files that a later harness claim then refuses
  on
- A Shared `/compile` with no concurrent refresh still writes a record the next
  harness claim accepts
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
