<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-27T20:32:17.178Z","dependsOn":[]} -->
# Fix: Invoke-CompileBuild.ps1 — Shared data older than the build's shader commits is used silently and the client crashes at startup

## Context
Observed symptom: after a recovery rebase of the session branch onto primary
main `86302ad5`, the session's Shared-mode game builds (compile envelope
`Temp/AgentBuildEnvelopes/compile-20260927T202027587Z-builder.md`,
machine-local) resolved `DataBuildMode=Shared` with no Local trigger and
consumed the primary checkout's
`Projects\BrokenEngineSandbox\Platforms\VisualStudio2026\Output\Data`. Its
`Shader.pack` (mtime 15:34:09) predated two shader-layout commits the binary
now contained: `622ffa47` removed `GlobalLayout.fWindTexelSize`
(`Engine/Data/Shaders/ShaderGlobalLayout.h`), and `29ced7a1` moved the water
band counts from `ShaderGlobalLayout.h` to `ShaderMainLayout.h`. The CPU side
compiles those layout headers from the worktree source, while the SPIR-V came
from the stale pack, so the uniform layouts disagreed. The Debug client
crashed at startup with `VK_ERROR_DEVICE_LOST` at `ImGuiManager.cpp:664`
(`Temp/client-agent.log:93-95`, machine-local).

Nothing detected the mismatch. `.agents/skills/compile/scripts/Invoke-CompileBuild.ps1`
checks only the `-RunDataPacker` and Shared-over-Local cases in its game-target
data-mode guard (the `if ($isGameTarget)` block resolving `$selectedMode`);
`.agents/skills/compile/references/runtime-data-mode.md`
`## Generated Data directory inventory` states that a stale directory is not
detected; and `## Wrapper bootstrap Shared-data refresh` states that Shared
data is refreshed only at session start from the primary's current asset
inputs, "not checked against HEAD". Any worktree whose HEAD contains a
shader-source commit that landed after the last primary data refresh hits the
same crash.

Workaround forced: a failed harness run, a `/external-diagnose-bug` dispatch
(differential: the same sweep passed with the same `Shader.pack` before the
rebase, and only `622ffa47`/`29ced7a1` differ; primary reflog times against the
`Shader.pack` mtime), and the post-rebase replay determinism check could not be
run (the user accepted code inspection instead).

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: fbec346a-f24d-469f-a997-e16c51a3d95a
- Worktree/branch UUID: 72d1b862-47bb-49ec-8ba2-4d27d0d1c138
- Session branch: claude/72d1b862-47bb-49ec-8ba2-4d27d0d1c138
- Worktree: .claude\worktrees\BrokenEngine\72d1b862-47bb-49ec-8ba2-4d27d0d1c138
- Landing ref: claude/72d1b862-47bb-49ec-8ba2-4d27d0d1c138 (the observing
  session records and lands this Plan itself; the branch survives exactly as
  long as the worktree recorded above).
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/CompileSharedDataShaderStaleness.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <review ref>` in bounded friction mode with the landing ref
above, supplying client `claude` and the recorded conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below. If root-causing
shows the fix lies outside that boundary, surface it for re-planning instead of
expanding scope.

The author's recommendation, for the fix session to confirm: record which
shader sources the Shared data was exported from, and check that record
against the build's HEAD before MSBuild launches.

- Producer: after a successful primary data refresh in
  `.agents/scripts/Bootstrap-AgentTools.ps1` (the Shared-data refresh block),
  write a stamp beside the Shared Data directory holding the primary's
  `HEAD:Engine/Data/Shaders` and `HEAD:Projects/BrokenEngineSandbox/Data/Shaders`
  tree hashes, written only when those paths were clean before and after the
  export — the same pre/post tree-hash-plus-clean-status pattern the
  DataPacker prebuild stamp in that script already uses. Rationale: reuses an
  existing mechanism, costs two `git rev-parse` calls, and names exactly the
  inputs whose drift crashed the client.
- Consumer: in `Invoke-CompileBuild.ps1`, when the effective mode is Shared,
  compare the same two tree hashes at the build's HEAD (plus a clean-status
  check of those paths in the worktree) with the stamp, and stop with one typed
  block when the stamp is absent or differs, naming the needed refresh (a new
  session start refreshes primary data; a worktree whose HEAD is ahead of
  primary main has no matching Shared data and needs Local generation
  authorization). Rationale: the binary is built at this point, so the check
  covers every consumer of that binary; a harness-launch check would miss
  manual launches and duplicate the rule.

Alternatives not recommended: comparing `Shader.pack` mtime with commit times
(a rebase or squash rewrites committer dates, so the comparison is unreliable);
comparing against DataPacker's per-shader `.deps.meta` input fingerprints
(accurate, but couples the script to a DataPacker cache format that carries its
own version); checking only at harness launch (misses non-harness launches).

## Critical files
- `.agents/skills/compile/scripts/Invoke-CompileBuild.ps1` (the game-target
  data-mode guard block)
- `.agents/scripts/Bootstrap-AgentTools.ps1` (the Shared-data refresh block,
  only if the chosen detection needs a stamp written at refresh)
- `.agents/skills/compile/references/runtime-data-mode.md`
  (`## Generated Data directory inventory` staleness sentence and
  `## Wrapper bootstrap Shared-data refresh`)
- `.agents/skills/compile/SKILL.md` (only if it lists the typed block codes)

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the files named above: the data-mode
  guard block in `Invoke-CompileBuild.ps1`, the Shared-data refresh block in
  `Bootstrap-AgentTools.ps1`, the two named `runtime-data-mode.md` sections,
  and any typed-code list in the /compile skill that must name the new code

## Out of scope
- The landed change the session produced
- DataPacker, its fingerprint and cache formats, and generation
  (`-RunDataPacker`) behavior
- Local-mode behavior, the Local path-trigger set, and the context script that
  derives `dataBuildMode`
- Automatically refreshing primary data from `/compile` or the harness
- The Local absent-output block owned by
  `Documents/Plans/ChangeWorkflow/CompileLocalDataModePreflight.md`
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Tier 2 (scoped tool behavior): trigger is build/bootstrap coordination confined
to one session's own compile invocation plus a best-effort stamp write in the
existing bootstrap refresh; the new block stops only this worktree's build
before any side effect and cannot block other sessions. Escalate to Tier 3 if
the fix changes when or whether the bootstrap refresh runs, takes the bootstrap
mutex or build lock differently, or writes primary Data from `/compile`.
Shared never runs DataPacker; Local never falls back to Shared; an unchanged
Shared build whose shader sources match the stamp still proceeds. Never embed
transcript paths or home paths.

## Acceptance criteria
- The recorded symptom no longer reproduces under the documented invocation: a
  Shared-mode game build whose HEAD's shader-source trees differ from the ones
  the Shared data was exported from stops with the new typed code before
  MSBuild launches, and the message names the needed refresh
- A Shared-mode game build whose HEAD matches the exported shader sources still
  builds
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing

## Notes
Evidence is machine-local: the compile envelope and client log named in
`## Context`, and the `/external-diagnose-bug` differential. Shader commits
`622ffa47` and `29ced7a1` are in primary history.
