# Select runtime data mode

## Mode selection

Every worktree game build uses one data mode and one authoritative directory for both client and server of its game project, `Projects\<Name>` below:

- Shared: its Data directory is the primary checkout's
  `Projects\<Name>\Platforms\VisualStudio2026\Output\Data`; this mode consumes
  primary generated headers/packs and disables every DataPacker build/export
  step.
- Local: its Data directory is the same path under the worktree's own
  repository root, and every Local game build is a generation build
  (`## Local generation`).
- In the primary checkout both modes resolve to the primary `Output\Data`. A
  Local game build there re-exports into that directory and first deletes the
  primary data stamp, so worktrees resolve Local until the next clean refresh
  (`## Wrapper bootstrap Shared-data refresh`).

The build resolves the mode by one rule, with no override and no generation
authorization:

- The data inputs are `DataPacker/**`, `Engine/Data/**`,
  `Projects/<Name>/Data/**`, and `Common/DataFile.h`. A path whose filename is
  `AGENTS.md` (ordinal case-insensitive) is not a data input unless one of its
  directory segments is `Raw` (ordinal case-insensitive), because DataPacker
  packs any file under a `Raw` directory whatever its extension.
- The data stamp, `SharedDataStamp.txt` beside the game's primary
  `Output\Data`, holds the primary commit the last clean Shared-data refresh
  exported from (`## Wrapper bootstrap Shared-data refresh`).
- The build compares the worktree — committed, staged, unstaged, and untracked
  — with that commit over the data inputs and reports the outcome as
  `dataBuildModeReason`:
  - `stamp-match` — no data input differs: Shared.
  - `inputs-differ` — a data input differs: Local, with the differing paths
    reported.
  - `stamp-missing` — the stamp does not exist: Local. A refresh deletes the
    stamp for as long as its export runs, so a game build during a running
    refresh resolves Local; when no data input of the worktree changed,
    waiting for the refresh to finish and rebuilding resolves Shared again.
  - `stamp-unresolvable` — the stamp's text does not resolve to a commit:
    Local.

Local never falls back to Shared data.

- A session worktree is created without the `Engine/Data/Islands` and
  `Engine/Data/Textures` source trees, which Git sparse checkout skips: run
  `git sparse-checkout disable` in the worktree (about 1.25 s, safe with a
  dirty worktree, keeps uncommitted edits) before editing or adding an asset
  under either tree: an edit target that is not on disk cannot be changed, and
  an addition made while the tree is skipped leaves the tracked bytes beside it
  still absent.
- An absent worktree `Data` output is not missing setup. A linked worktree
  that has never generated has no `Output\Data` directory at all; that is the
  legitimate expected starting state. A Local generation build has DataPacker
  seed it from the primary checkout itself — `## Local generation` states the
  exact mechanism and its conditions. Never ask the user to pre-stage output
  or hand-run an export because the directory is absent; the genuine blockers
  are validation and environment failures DataPacker reports itself — an
  unrecognized reparse point or insufficient disk — never a user-prepared
  directory.

## Repository-root detection and mode derivation

The game projects detect the authoritative repository-root Git marker. A linked worktree has a `.git` file and defaults `RunDataPacker=false`; the primary checkout has a `.git` directory and preserves ordinary Local Visual Studio behavior by defaulting true. An explicit Local `RunDataPacker=true` permits deliberate worktree generation; Shared rejects true.

The resolved context's `changedPaths` is the changed-path set measured from the session baseline. Its `dataBuildMode` and `dataBuildModeReason` are the mode decision `## Mode selection` states, measured from the data stamp, and the build uses that mode.

## Build invocation and derived properties

The build invocation derives the mode, both directories, and the four `DataBuildMode`/`RunDataPacker`/`GameDataDirectory`/`GeneratedDataIncludeRoot` properties itself, for every client/server solution, project, and selective compile alike; no agent command composes them, and no invocation form overrides the mode.

ModelViewer uses this same resolution and generation flow with BrokenEngineSandbox
as data owner, including selective builds. Its project generates an ignored
`Build/ModelViewerDataRoot.h` from the resolved directory. The invocation also
writes `ModelViewer/Platforms/VisualStudio2026/Build/ResolvedData.props`, which the
viewer project imports so configured Visual Studio builds retain the same data
mode, generated-header root and default launch root. The viewer's identity never
changes pack names or export inputs. Viewer builds do not write sandbox harness
Shared-data records.

Every Local game build sets `RunDataPacker=true`; every other build is `false`. A Local game build also needs `DevEnvDir` for the game project's nested DataPacker build. The invocation resolves `DevEnvDir` for every game build, so an unresolvable `DevEnvDir` blocks every game build, Shared included, under `context.unresolved` naming `dev-env-dir.unresolved`; only a Local game build passes it, so it cannot leak into a Shared one.

## Generated Data directory inventory

A correctly generated Data directory holds 23 files: `Data.h`, `DataTypes.h`,
and one nonempty header, manifest, and pack for each of `Audio`, `Scene`,
`Islands`, `Model`, `Shader`, `Texture`, and `Raw`. The build does not validate
the selected sandbox Data directory's inventory or contents, so a malformed, partial,
or stale directory is not detected as such — it surfaces only as an ordinary
compile or runtime failure. One exception covers Shared data provenance: the
`/agent-harness` claim and launch scripts compare a Shared directory with the
listing in its build's record. Every successful game build that is not a
selective `-Files` compile, in either data mode and including the generation
build, writes that
record as `SharedDataRecord.<Target>.<Configuration>.txt` beside its
executables: a format marker, the `GameDataDirectory` it passed to MSBuild,
and, for a Shared build only, the listing of that directory — each file's
relative path, size, and last-write time. The `/agent-harness` claim reads its
data directory from that record; `/agent-harness` worker steps 7 and 21 own the
refusal codes and their routing.

ModelViewer's precompile target requires this inventory to exist and reports the
exact missing path before compiling. Pack contents still pass through the engine's
ordinary runtime validation.

## Wrapper bootstrap Shared-data refresh

The wrapper bootstrap refreshes Shared data on a best-effort basis: at session
start it runs the primary's DataPacker over the primary checkout's current
asset inputs, which are not checked against HEAD, once per game project whose
client `.vcxproj` is committed at the primary `HEAD`, into that game's primary
`Output\Data`, and it skips the runs when the DataPacker prebuild was not
stamped. When a game's primary `Output\Data` is missing, DataPacker generates
it automatically. That first export is a full Engine/Data re-export into an
empty per-game DataPacker cache and can take hours while the bootstrap mutex
is held, so peer session starts can time out. It first waits up to its `-WaitSeconds`
budget for any process named after one of the games it refreshes (`<Name>*`)
to exit, and skips the runs with a warning only if one is still alive when that
budget expires. A peer session starting mid-session can therefore move primary
Data under this session, and a regenerated header can force a recompile here.

The same refresh also runs on demand, from the session worktree root, as
`pwsh -NoProfile -File .agents/scripts/Bootstrap-AgentTools.ps1 -RepositoryRoot '<primary checkout>' -SharedDataOnly`.
It skips the AgentTools and ThirdParty builds and the ModelViewer prebuild,
keeps the DataPacker prebuild, the bootstrap mutex, the game wait, and the
DataPacker prebuild stamp and data stamp rules, and skips the DataPacker prebuild
and the refresh, with a warning naming a wrapper session start, when primary
ThirdParty changed since the last prebuild or the DataPacker prebuild stamp is
missing or unreadable. Every skip exits 0, so its
`Refreshed primary data at` line is the only sign the refresh ran. It runs this
worktree's copies of the bootstrap scripts against the primary checkout, so a
session runs it without asking the user unless this worktree's own committed
or uncommitted changes, measured from its merge base with primary main, touch
`.agents/scripts/`; then it asks the user first. One run can outlast a tool-call timeout, up to
`-WaitSeconds` each for the bootstrap mutex and the game wait, then the
DataPacker prebuild and the export, so it runs where a tool-call timeout cannot
kill it, for example with the host's background-execution parameter such as
`run_in_background`; a killed shell
stops the export midway and leaves the primary data unstamped.

Each game's refresh also writes the data stamp (`## Mode selection`). Before
its export it deletes the stamp, aborting the
bootstrap run if the delete fails, and records `HEAD`. After a successful
export it writes that commit only when the data inputs (`## Mode selection`)
show no committed or uncommitted difference from it, so a commit landed during
the export also leaves it unwritten; when they differ, its warning lists the
differing paths and worktree game builds resolve Local until a clean refresh.
A failure to record `HEAD` or to compare likewise warns and leaves the stamp
unwritten. An export can itself rewrite tracked data inputs, such as island BC
textures and scene `.PreExport` markers; the stamp then stays unwritten until
those files are committed in the primary, which restores Shared at the next
refresh. A skipped refresh leaves the stamp alone. A primary DataPacker run
outside the bootstrap, such as a Visual Studio build in the primary checkout,
rewrites Shared data without updating the stamp; the next refresh stamps it
again.

Ordinary Shared work otherwise remains unchanged: primary Data, no `RunDataPacker`, and no materialization or export.

## Local generation

Local mode may build the worktree Release DataPacker as a standalone compile
check (`-Target DataPacker`). Every Local game build is a generation build: the
invocation internally invokes the worktree DataPacker exactly once with
`--materialize-data` over `Engine\Data` and `Projects\<Name>\Data` into the
Local Data directory, requires exit `0`, then runs the one
`RunDataPacker=true` game build. Because that
invocation reads the whole of `Engine\Data`, it first restores any
sparse-skipped tree itself — when the worktree's `core.sparseCheckout` is
still `true` it runs `git sparse-checkout disable` in the worktree and blocks
under `data.sparse-restore-failed` if that exits non-zero — so a generation
build needs no manual restore. Materialization performs no exports and never
creates, inspects, or materializes Attribution. In a validated linked
worktree, dirty output uses copy-on-write materialization, staging primary
files and atomically replacing only the affected recognized link.

The data mode and the Local generation build are per game: a request that
builds several games resolves each game's mode separately and runs a
generation build for each Local game, each materializing that game's `Data`
into its own Local Data directory.

Local generation forbids Gaea unless `-AllowGaeaExport` is passed: the
invocation scopes `BT_DATAPACKER_FORBID_GAEA_EXPORT=1` around the whole
generation build and restores the caller's prior environment exactly.
`-AllowGaeaExport` sets `BT_DATAPACKER_ALLOW_GAEA_EXPORT=1` for that build and restores its prior value in `finally`; it omits the default negative guard while preserving any caller-provided forbid guard. Only an explicit user-approved plan
or acceptance criterion requiring an island re-bake authorizes it; the
automatic rule never omits the guard, and changed island inputs, DataPacker
code, fingerprints, or cache state alone never authorize a Gaea run. A
generation build that fails reporting
`Gaea.Swarm export blocked by BT_DATAPACKER_FORBID_GAEA_EXPORT=1` means this
worktree's island inputs differ from the shared Gaea bake. When the worktree
is behind primary main, rebase onto it; a change to this worktree's own island
inputs exports only through `-AllowGaeaExport` under that authorization.
Otherwise report the failure, and never pass `-AllowGaeaExport` or retry
without that authorization. `-ForbidExpensiveExport` additionally scopes
`BT_DATAPACKER_FORBID_EXPENSIVE_EXPORT=1` around the same generation build and
restores the caller's prior environment exactly; `DataPacker/Source/AGENTS.md`
owns when that guard is wanted, in its `BT_DATAPACKER_FORBID_EXPENSIVE_EXPORT`
paragraph. `BT_DATAPACKER_FORBID_EXPENSIVE_EXPORT` retains its broader
independent meaning; never clear either caller-provided guard.
`-AllowGaeaExport` and `-ForbidExpensiveExport` take effect on a Local game
build and have no effect on a Shared one, where no export runs.

Never copy worktree DataPacker/data source changes into the primary checkout to test them.
