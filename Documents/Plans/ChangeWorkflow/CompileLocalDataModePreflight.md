<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-27T20:11:10.702Z","dependsOn":[]} -->
# Fix: Invoke-CompileBuild.ps1 — Local data mode with no Local output and no generation authorization fails as a bare C1083

## Context
Observed symptom: the session's only `Engine/Data/**` change was one C++-only
line inside the `#if defined(BT_ENGINE)` branch of the dual-language header
`Engine/Data/Shaders/ShaderLayoutsBase.h`. That path matched the
`Engine/Data/**` Local trigger (`.agents/skills/compile/references/runtime-data-mode.md`
`## Mode selection`), so `.agents/skills/compile/scripts/Invoke-CompileBuild.ps1`
resolved `DataBuildMode=Local` with `RunDataPacker=false` and pointed
`GameDataDirectory` at the worktree's own
`Projects\BrokenEngineSandbox\Platforms\VisualStudio2026\Output\Data`, which did
not exist. All four Client/Server Debug/Release builds then failed in 6-7 s each
with `error C1083: Cannot open include file: 'Data/DataTypes.h'` from
`Engine/Source/File/FileManager.h(3,1)`; no typed pre-flight code explained
that the cause was Local mode without generation authorization. The data-mode
guard block in `Invoke-CompileBuild.ps1` (the `if ($isGameTarget)` block that
resolves `$selectedMode`) checks only the `-RunDataPacker` and Shared-over-Local
cases.

Workaround forced: four doomed builds (about 91k tokens), a user question, a
temporary revert of the header line, a Shared rebuild, and a restore; the
landed header line was never compiled.

This conflicts with a documented rule: `runtime-data-mode.md` `## Mode
selection` states "An absent worktree `Data` output never produces a typed
block" and that without authorization the build fails as an ordinary
missing-header compile error. The user accepted this finding with a typed
pre-flight block as one allowed direction, so a fix choosing it updates that
sentence in the same change.

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
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/CompileLocalDataModePreflight.md`,
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

The author's recommendation, for the fix session to confirm: add one typed
pre-flight block in the game-target data-mode guard of `Invoke-CompileBuild.ps1`
that fires when the effective mode is Local, `-RunDataPacker` is absent, and the
Local Data directory has no `DataTypes.h`, with a message naming the trigger
paths from `triggerMatches` and stating that a Local build here needs generation
authorization (`-RunDataPacker`) or a Local-trigger-free change. Rationale: one
existence check before MSBuild launches, general to every Local trigger, and it
turns four doomed builds into one typed stop. It keeps the documented rule that
an agent never pre-stages or hand-exports data; update the contradicting
`## Mode selection` sentence to say the build blocks with that code instead of
failing as a compile error.

The narrower-trigger alternative (exempting dual-language headers under
`Engine/Data/Shaders` when the change is confined to C++-only preprocessor
regions) is not recommended: proving a diff is confined to a `BT_ENGINE`
branch needs preprocessor-aware parsing in the context script, and a wrong
exemption would silently build against stale shader data.

## Critical files
- `.agents/skills/compile/scripts/Invoke-CompileBuild.ps1` (the game-target
  data-mode guard block)
- `.agents/skills/compile/references/runtime-data-mode.md` (`## Mode
  selection`, the absent-output paragraph)
- `.agents/skills/compile/SKILL.md` (only if it lists the typed block codes)

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the files named above: the data-mode
  guard block in `Invoke-CompileBuild.ps1`, the absent-output paragraph of
  `runtime-data-mode.md` `## Mode selection`, and any typed-code list in the
  /compile skill that must name the new code

## Out of scope
- The landed change the session produced
- The Local path-trigger set and the context script that derives
  `dataBuildMode`, unless root-causing proves the recommended block
  insufficient (then surface for re-planning)
- DataPacker, generation (`-RunDataPacker`) behavior, and Shared-mode behavior
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Tier 2 (scoped tool behavior): trigger is build/bootstrap coordination confined
to one session's own compile invocation; the new block stops only this
worktree's build before any side effect and cannot block other sessions.
Escalate to Tier 3 if the fix reaches shared primary data, wrapper bootstrap, or
the build lock. Local never falls back to Shared, and an authorized generation
build must still seed an absent Local Data directory. Never embed transcript
paths or home paths.

## Acceptance criteria
- The recorded symptom no longer reproduces under the documented invocation: a
  game build whose effective mode is Local, without `-RunDataPacker`, in a
  worktree with no Local Data output stops with the new typed code before
  MSBuild launches
- An authorized `-RunDataPacker` Local build in the same state still proceeds
  to generation
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing

## Notes
Evidence: the observing session's compile envelope
`Temp/AgentBuildEnvelopes/compile-20260927T193557806Z-24048.md`
(machine-local; four `failureKind: msbuild` results with `DataBuildMode=Local`,
`RunDataPacker=false`).
