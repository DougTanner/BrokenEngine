<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-19T22:17:18.021Z","dependsOn":[]} -->
# Make ThirdParty changes resolve a Local data build

## Context
From a linked worktree, `/compile` links every consumer — DataPacker included,
through the `ThirdPartyLibraryDirectory` environment variable that nested
`devenv` DataPacker builds inherit — against the session's ThirdParty
candidate when the session's changed-path set holds a non-Markdown
`ThirdParty/` path (`.agents/skills/compile/scripts/Invoke-CompileBuild.ps1:272-279`,
for the Client, Server, ClientServer, ModelViewer, and DataPacker targets;
trigger emitted by `.agents/skills/compile/scripts/Resolve-CompileContext.ps1:120-135`:
a path starting `ThirdParty/` and not ending `.md`, both ordinal
case-insensitive).

The data mode, however, is resolved only from the data inputs
`Test-AgentDataInputPath` accepts (`.agents/scripts/AgentScriptCommon.psm1:202-211`:
`DataPacker/**`, `Engine/Data/**`, `Projects/<Name>/Data/**`,
`Common/DataFile.h`; `.agents/skills/compile/references/runtime-data-mode.md`
`## Mode selection`). A session that changes only ThirdParty — a submodule pin,
a `ThirdParty/Prebuilts` wrapper, or `ThirdParty.vcxproj` — resolves Shared
(`stamp-match`), which disables every DataPacker build and export, so no export
runs with the candidate-linked DataPacker. The session verifies against the
primary's data, exported with the primary's libraries, and a ThirdParty change
that breaks or alters export (assimp, tinygltf, stb, meshoptimizer, openexr,
bc7enc, and others compile into DataPacker) first surfaces in the primary
wrapper bootstrap's Shared-data refresh after landing.

## Design
Add the candidate trigger's paths to `Test-AgentDataInputPath` with the
trigger's exact rule, so the two never disagree. A gitlink change reports as
the bare `ThirdParty/<lib>` path, which that rule already matches.

`Test-AgentDataInputPath` has other consumers, which the widened predicate
also changes; the author recommends accepting each, since each asks the same
question (can this path change exported data):
- `Get-AgentDataInputChanges` (mode selection, `dataInputChanges`).
- The primary bootstrap's data stamp: a refresh writes the stamp only when no
  data input differs from the recorded commit.
- `.agents/skills/compile/scripts/Get-CompileRewrittenPaths.ps1:66`.
- `.agents/skills/finalize-changes/scripts/Invoke-FinalizePrimaryMovementCheck.ps1:283-284`:
  a landing whose session and the moved primary both hold ThirdParty paths now
  rebuilds the game projects.

Update `runtime-data-mode.md` `## Mode selection`'s data-input list and every
other document that enumerates the data inputs (the executing session greps
`.agents` and `AGENTS.md` files for `Common/DataFile.h`).

## Critical files
- `.agents/scripts/AgentScriptCommon.psm1`
- `.agents/skills/compile/references/runtime-data-mode.md`

## In scope
- `Test-AgentDataInputPath` in `AgentScriptCommon.psm1`.
- Every document listing the data inputs, starting with `runtime-data-mode.md`
  `## Mode selection`.

## Out of scope
- The candidate trigger, `ThirdPartyLibraryDirectory`, and the candidate build.
- `Bootstrap-AgentTools.ps1`, `Provision-WorktreeThirdParty.ps1`, and WorktreeCli
  sources.
- DataPacker's own dirty rules.

## Risk tier and invariants
Tier 3 — build/bootstrap coordination that can block other sessions: the
predicate decides every session's data mode, the primary data stamp, and the
landing rebuild set. Invariant: the predicate's ThirdParty rule matches the
candidate trigger exactly. No C++, wire, save, or CRC change.

## Acceptance criteria
- In a worktree whose only change is an untracked non-Markdown file under
  `ThirdParty/Prebuilts/` (removed afterwards), the `/compile` context reports
  `dataBuildMode` `Local`, `inputs-differ`, with that path in
  `dataInputChanges`; with only an untracked `.md` file there instead, it
  reports `Shared`.
- The static-checks runner reports every triggered row passing.
