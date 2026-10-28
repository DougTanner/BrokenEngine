<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-27T23:01:09.977Z","dependsOn":[]} -->
# Move ModelViewer into Projects

## Context and recommended design

ModelViewer is a standalone application built on the engine. It owns its entry
point, camera and viewer controls, and consumes BrokenEngineSandbox's packed
data. Its application role fits `Projects/` even though its engine membership
and executable structure differ from a client/server game.

The author recommends moving `Tools/ModelViewer` to `Projects/ModelViewer` and
keeping it in normal project discovery. Both existing discovery implementations
already select matching project files under `Projects/`; their selection and
ordering need no change.

Use the presence of a project's `Data/` directory to identify whether that
project owns data to pack. A project without that directory consumes data
identified by its build configuration. ModelViewer continues using its existing
explicit BrokenEngineSandbox data binding. This keeps discovery inclusive
without inventing ownership configuration or guessing which project supplies
the data.

## Execution order

1. **First, rebase the implementing session branch onto the latest `main` after
   the user's in-progress changes have landed.** The user requested this order
   before work begins.
2. Refresh the session baseline, repository instructions and affected-path
   inventory. Reconcile the plan with the newly landed changes before editing.
3. Record the execution card and complete the required plan reviews before
   implementation.
4. On the rebased, unedited tree, record project discovery and a stable hidden
   ModelViewer capture using the existing capture recipe and default scene.
5. Implement the relocation and the bounded changes below, then verify them.

## In scope

The following regions are the target and ceiling. Make the smallest complete
change within them, plus required mechanical includes or declarations. Naming a
file does not authorize unrelated changes in it.

- `Tools/ModelViewer/**`: relocate the complete tracked tree to
  `Projects/ModelViewer/**`. Preserve project identity, source contents,
  application architecture and relative paths that remain valid at the new
  location. Rebuild generated outputs at the destination.
- `DataPacker/Source/Main.cpp`, `RunCommand`'s zero-argument project loop: skip a
  discovered project when its project-local `Data/` directory is absent. Keep
  `DiscoverGameProjects` inclusive and retain explicit-invocation missing-input
  failures.
- `.agents/scripts/Bootstrap-AgentTools.ps1`, ModelViewer solution/output/source
  snapshot paths and automatic committed-project data export loop: update the
  relocated paths and skip projects without `Data/` before changing export
  stamps or attempting packing. Preserve the existing ModelViewer prebuild's
  Sandbox data binding.
- `.agents/scripts/AgentScriptCommon.psm1`,
  `Write-AgentModelViewerResolvedData`: update the destination of the viewer's
  generated `Build/ResolvedData.props`. Keep `Get-AgentGameProjects` inclusive
  and preserve its existing ordering.
- `.agents/scripts/Build-WorktreeTarget.ps1`, ModelViewer source snapshot and
  target-directory selections: update the relocated paths.
- `.agents/skills/compile/scripts/Resolve-CompileContext.ps1`,
  `Get-ContextRecommendedTargets`' viewer-path test and the project selection
  block: recognize the new viewer path; keep ModelViewer outside ordinary
  client/server project selection so the existing implicit Sandbox selection
  remains available. Preserve the dedicated `-Target ModelViewer` route and
  its explicit Sandbox data-owner validation.
- `.agents/skills/compile/scripts/Invoke-CompileBuild.ps1`, ModelViewer solution
  and selective-target project mappings: update the relocated paths. Preserve
  existing target names, configurations and Shared/Local data resolution.
- `.agents/skills/update-vcxproj/scripts/Resolve-VcxprojMembership.ps1`,
  `Resolve-PathMapping`'s viewer source rule: update the source prefix and retain
  the dedicated ModelViewer mapping ahead of generic project handling.
- `.agents/skills/update-vcxproj/scripts/Update-EngineVcxprojItem.ps1`,
  `Get-Games` and its automatic engine membership enumeration: keep ModelViewer
  outside the ordinary client/server pair requirement and bulk engine-file
  enrollment. Preserve the viewer's selective membership and ordinary games'
  existing pair validation.
- `.agents/scripts/Invoke-IncludeOrderCheck.ps1`, obsolete ModelViewer-specific
  excluded-file and project-prefix entries: remove the old tool-path entries
  so the existing discovered-project entries cover the relocated viewer.
- `.agents/skills/agent-harness/scripts/Invoke-ModelViewerCapture.ps1`, executable
  path resolution: use the relocated Debug executable.
- Root `AGENTS.md`, `Directory Structure`: describe the application role of
  `Projects` and ModelViewer's location.
- `Projects/AGENTS.md`, `Overview`, `Standard Project Layout` and their tooling
  contract: distinguish general project discovery, data ownership and the
  additional requirements of client/server games. Document that absent `Data/`
  means automatic packing skips the project and its build supplies data routing;
  it does not imply server or engine-membership capabilities.
- `.agents/skills/compile/references/runtime-data-mode.md`, viewer generated
  properties path and wrapper automatic-data-refresh description: synchronize
  them with relocation and the data-owner selection rule.
- `.agents/skills/update-vcxproj/references/worker.md`, viewer source membership
  table row: update the source path while retaining its dedicated mapping.
- `.agents/skills/agent-harness/references/model-viewer.md`, application-contract
  link: point to the relocated owning document.

## Out of scope

- No application behavior changes, `GameBase` conversion, network session,
  server executable, viewer-owned `Data/` directory or independent asset pipeline.
- No new generic client/server target support for ModelViewer, ownership
  configuration, default data-owner inference, compatibility paths or migration
  of generated outputs.
- No renaming of discovery APIs, metric-history redesign, unrelated refactoring,
  abstractions, adjacent fixes or unit tests.
- Absence of `Data/` does not relax ordinary games' server-pair validation or
  authorize automatic enrollment of engine files in ModelViewer.

## Risk and invariants

Treat implementation as Tier 3: it changes the shared bootstrap's automatic
export selection while that path coordinates session startup, so a failure can
block other sessions. It also integrates project discovery, data packing,
compilation and project maintenance. The current save-only change is Tier 1.

Preserve inclusive, identically ordered PowerShell/C++ project discovery;
ModelViewer's explicit Sandbox data ownership in Shared and Local modes; its
selective engine-file membership; and current ordinary client/server build
selection and validation. The relocation keeps the same directory depth, so do
not rewrite valid relative solution, project or documentation paths.

Absence of `Data/` intentionally stops being an automatic-packing error. Explicit
packing requests still report missing input. There is no new data format,
simulation, wire, serialization, threading or trust-boundary behavior.

## Verification and acceptance

- Record the unedited-tree project list after the initial rebase. After the
  move, discovery includes ModelViewer and automatic packing/bootstrap export
  selection includes only projects owning `Data/`. Confirm no export stamp is
  touched for a skipped project.
- Verify explicit packing still rejects missing input by reading the existing
  rejection path; do not create a hand-edited input solely to exercise it.
- Verify ModelViewer resolves Sandbox data in both Shared and Local modes, with
  viewer properties and generated headers written at the new location. Confirm
  the explicit Sandbox packing inputs remain intact.
- Through `/compile`, build DataPacker `Release|x64`, ModelViewer `Debug|x64` and
  `Release|x64`, and BrokenEngineSandbox client/server `Debug|x64`. Confirm the
  existing ModelViewer target and implicit ordinary Sandbox selection work.
- Run static checks, project/filter validation and every required C++, script,
  documentation and skill review. Confirm the viewer source mapping resolves
  before generic project handling, viewer membership stays selective, and
  ordinary game pair validation remains strict.
- Through `/agent-harness`, run the existing hidden ModelViewer capture recipe
  before and after the relocation with fresh app-data directories, the default
  scene and `-RequireStable`. Require stable captures, code `ok`, a clean exit,
  `windowVisible: false`, no crash reports or `DEBUG_BREAK at` entries, and
  unchanged scene appearance against the baseline capture.
- Search active build, bootstrap, maintenance, capture and documentation regions
  for obsolete `Tools/ModelViewer` references; none remain outside historical
  plan records.

Landing is a separate step after implementation verification.
