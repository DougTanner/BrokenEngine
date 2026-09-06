<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-02T00:17:51.107Z","dependsOn":[]} -->
# Fix: DataPacker — retained build log carries no export job summary

## Context
After a Local generation build run through
`pwsh -NoProfile -File .agents/skills/compile/scripts/Invoke-CompileBuild.ps1 -Target Client -Configuration Debug -DataBuildMode Local -RunDataPacker -Baseline <sha>`,
both retained build logs (machine-local, cited by name only:
`brokenenginesandbox-20260902T000839168Z-15968.log` and
`brokenenginesandbox-20260902T000957134Z-29492.log` under `Temp/AgentBuildLogs`)
contained only the DataPacker banner — data directories, project, cache
directory, output directory, emitted at `kDebug` from
`DataPacker/Source/FileManager.cpp:397,421,427`. At the default log threshold
the logs held no per-job export line, no job count, no "N jobs clean /
M exported" summary, and no DataPacker elapsed time.

Proving that the warm run performed zero exports therefore could not be done
from the retained log. It instead required three full filesystem inventories
with hashes and timestamps across 3,081 cache files, repeated before and after
the run, purely to establish that nothing had been re-encoded.

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
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/Engine/DataPackerRunSummaryLog.md`,
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
a starting point rather than a decision, is one `kInfo` line at the end of a
DataPacker run reporting the counts of clean, re-exported, and failed jobs plus
the run's elapsed time; the rationale is that a single end-of-run line makes a
warm run provable from the retained log without adding per-job output at the
default threshold, which would reintroduce log volume the level policy in the
root `AGENTS.md` keeps at `kVerbose`. If root-causing shows the fix lies outside
that boundary, surface it for re-planning instead of expanding scope.

## Critical files
- `DataPacker/Source/Main.cpp` — export job discovery, dirty aggregation, and
  the run's top-level flow that would own the summary
- `DataPacker/Source/ExportJobs/ExportJob.cpp` — the per-job clean/dirty
  decision the counts would come from
- `DataPacker/Source/AGENTS.md` — the documented DataPacker diagnostic and
  reporting contract

## In scope
- Root-cause investigation via `/next-plan-review`, run with the recorded Claude
  client, the review ref named in `## Design`, and the recorded conversation
  session ID
- The smallest resulting fix, confined to end-of-run summary reporting in the
  DataPacker run flow and the per-job state it counts, plus the matching
  `DataPacker/Source/AGENTS.md` prose

## Out of scope
- Export algorithms, cache identity, fingerprints, chunk or pack contents, and
  any change to what is exported
- DataPacker cache identity, versioning, and reuse behavior
- Build log retention, capture, or the build result envelope owned by the
  compile skill
- Unrelated skills or scripts; any transcript path or transcript text in the
  repository

## Risk tier and invariants
Expected Change Workflow Tier 2 (scoped DataPacker tool behavior); escalate if
the fix reaches serialization, cache identity, or build coordination. Preserve
byte-identical exported output for a given exporter and input set, the existing
diagnostic reporter's ownership of failure presentation, the noninteractive
diagnostic contract, and the repository log-level policy. Never embed transcript
paths or home paths.

## Acceptance criteria
- A warm-cache DataPacker run is provable as zero-export from the retained build
  log alone, without filesystem inventories, at the default log threshold
- A run with dirty inputs reports the exported and failed counts consistent with
  the work it actually performed
- Exported output and DataPacker failure reporting are unchanged; DataPacker
  builds pass through `/compile`; plan validate exits 0
