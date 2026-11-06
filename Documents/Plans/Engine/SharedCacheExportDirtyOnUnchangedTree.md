<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-05T14:08:26.861Z","dependsOn":[]} -->
# Fix: DataPacker shared cache — repeat export of an unchanged tree reports Scene and Shader dirty

## Context

Observed symptom: the `/compile` second export of BrokenEngineSandbox client
Debug, on a tree unchanged since the first export, did not come back clean. It
logged `"Scene" is dirty, running export`, reran Blender
(`Exporting Blender scene` for
`Projects/BrokenEngineSandbox/Data/Models/MolniaFbx/source/MolniaAnimated.fbx`),
then logged `"Shader" is dirty, running export`, 55 `Shader dependency changed`
lines for `Engine/Data/Shaders/ShaderLayoutsBase.h`, and `Input fingerprint
changed` for `Engine/Data/Shaders/Model/Model.frag` and
`Engine/Data/Shaders/Water/Water.frag`, ending at `Exported: 58`. `Water.frag`
carried no change in the observing session. The run's no-dirty-job check was
not met on that pass. The workaround was one extra builder pass, 296 s, plus a
later double export of the final tree, which was clean with no marker change
between passes, so a lone tree exported twice does not by itself reproduce this.

Separately, the shared marker
`%LOCALAPPDATA%\BrokenEngine\DataPackerCache\BrokenEngineSandbox\Models\MolniaFbx\source\MolniaAnimated.fbx.glb.b1.blender`
was found rewritten at 08:45 local with baseline hashes, seven minutes before
that 08:52 run, likely by a sibling worktree; no evidence names the writer. The
run then rewrote it at 08:53:07 with the session's `scriptHash`.

Code facts at the time of recording:

- `ExportScene::GetBlenderExportMarkerPath`
  (`DataPacker/Source/ExportJobs/ExportScene.cpp:347`) names the marker with
  `kiBlenderExportVersion` (`DataPacker/Source/BlenderExport.h:3`) alone, while
  `BlenderRecipe` (`DataPacker/Source/BlenderExport.cpp:110`) holds `scriptHash`
  and `BlenderExportDirty` (`DataPacker/Source/BlenderExport.cpp:138`) reports
  dirty on any recipe mismatch. Trees with different
  `DataPacker/Scripts/ExportBlenderScene.py` content therefore share one marker
  slot.
- `DataPacker/Source/FileManager.cpp:375` places every entry in one per-project
  cache directory that all worktrees share. The comment at
  `DataPacker/Source/ExportJobs/ExportJob.cpp:143` states the intent that
  exporters built from different sources keep separate entries, and each entry is
  named by exporter version only, so trees with the same version and different
  inputs also share `.v<N>.chunk` and `.v<N>.meta` slots.
- `BlenderExportDirty` returns `true` from every branch without a log line, so
  the log does not say which comparison failed.

Why the Shader job was dirty is undiagnosed: a sibling's shader export rewriting
the shared `.meta` and dependency sidecars fits the log, and nothing proves it.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 8efba1af-3a94-4fc9-834c-ff190a2db7f1
- Worktree/branch UUID: 059d7be2-5d08-4321-9713-a6986b2e0d61
- Session branch: claude/059d7be2-5d08-4321-9713-a6986b2e0d61
- Worktree: .claude\worktrees\BrokenEngine\059d7be2-5d08-4321-9713-a6986b2e0d61
- Landing ref: claude/059d7be2-5d08-4321-9713-a6986b2e0d61
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/Engine/SharedCacheExportDirtyOnUnchangedTree.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design

First root-cause the friction from the current tree and this Plan's `## Context`
through `/external-diagnose-bug`. A candidate reproduction, not yet run: export
one worktree, export a second worktree whose `ExportBlenderScene.py` or shader
inputs differ, then repeat the first worktree's export. Temporary `kTemp` logs in
`BlenderExportDirty` can name the failing comparison.

Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <landing ref>` in bounded friction mode, supplying the
recorded client and the recorded conversation session ID. Then make the smallest
fix inside the `## In scope` boundary below. If root-causing shows the fix lies
outside that boundary, surface it for re-planning instead of expanding scope.

If the diagnosis confirms that sibling trees overwrite one Blender marker slot,
the author recommends separating the slot by recipe content rather than by any
worktree path, because a path would break the no-absolute-worktree-path rule in
`DataPacker/Source/ExportJobs/AGENTS.md` `## Shader Dependencies` and a shared
slot is what lets each tree invalidate the other. A fix that changes only cache
metadata follows the no-full-re-export verification rule in
`DataPacker/Source/AGENTS.md` `## Architecture`.

## Critical files

- `DataPacker/Source/BlenderExport.cpp` — `BlenderRecipe`, `BlenderExportDirty`
- `DataPacker/Source/BlenderExport.h` — `kiBlenderExportVersion`
- `DataPacker/Source/ExportJobs/ExportScene.cpp` —
  `ExportScene::GetBlenderExportMarkerPath`
- `DataPacker/Source/ExportJobs/ExportJob.cpp` — cache entry naming,
  `ExportJob::CheckDirty`
- `DataPacker/Source/ExportJobs/ExportShader.cpp` — dependency sidecar naming,
  `ExportShader::CheckDirty`
- `DataPacker/Source/ExportJobs/AGENTS.md` — its shared cache marker text, only
  when the fix changes what that text states

## In scope

- Root-cause investigation of both the Scene and the Shader dirty report, as
  `## Design` states
- The smallest resulting fix, confined to the functions and regions named under
  `## Critical files`

## Out of scope

- The landed change the session produced
- Exported chunk, `.pack`, and `.manifest` bytes; export `Version(...)` values
- `InputFingerprints.cache`, Gaea caches, texture encoding, and the tracked
  `.PreExport` marker
- `ExportBlenderScene.py` conversion behavior
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants

Expected Tier 2: the highest trigger is one tool's runtime behavior, DataPacker
cache entry naming and dirty checks, with no exported bytes changed. Escalate to
Tier 3 if the fix changes a serialized chunk or `.pack` format or reaches
build/bootstrap coordination that can block other sessions. Determinism/CRC,
wire, replay, client/server affinity, threading, and GPU layouts are not exposed;
scene jobs keep their existing asynchronous launch, and worktrees keep sharing
clean chunks. Verification is a `/compile` export, not a live `/agent-harness`
run. Never embed transcript paths or home paths.

## Acceptance criteria

- The recorded symptom no longer reproduces: under the reproduction the diagnosis
  confirms, a repeat export of an unchanged tree reports no dirty Scene or Shader
  job and launches no Blender
- A changed `ExportBlenderScene.py`, Blender source, or shader input in that same
  tree still reports its job dirty
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
