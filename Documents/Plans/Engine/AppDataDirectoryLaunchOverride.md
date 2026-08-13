<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-12T18:50:25.106Z","dependsOn":[]} -->
# Give harness runs a per-worktree AppData root so concurrent sessions stop overwriting each other's replays

## Context

Every client and server process on the machine writes its saves, settings, caches, and replay artifacts into
one fixed per-user directory. `FileManager::FileManager()`
(`Engine/Source/File/FileManager.cpp:111-126`) resolves `FOLDERID_RoamingAppData` and appends
`game::kGameName`, with no override of any kind — for example
`C:\Users\<user>\AppData\Roaming\Broken Engine Sandbox Server\`. Every worktree on the machine therefore
shares one directory, because the path derives from the user profile and the game name, never from the
checkout.

Replay artifacts inside it use fixed names. `Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.cpp:110-115`
builds every replay component from the literal prefix `F7.replay` (`F7.replay.grid`, `F7.replay.meta`,
`F7.replay.<coordKey>`, `.frames`, `.checksums`, `.fullframes`), and the manifest is written to the literal
`F7.replay.manifest` (`:244`, `:257`). Nothing in the name encodes the worktree, session, or run.

The consequence is silent cross-session destruction that the existing coordination cannot prevent. The default
harness lock correctly serializes runs in time, so two sessions never run simultaneously — but the second
session's recording still overwrites the first session's files, which the first session is still relying on as
its pre-change baseline. Observed directly: at 2026-08-12T18:09Z a foreign session running
`Documents/Plans/Frame/ReplayDelayedCoordActivation.md` work (worktree `6104a774-...`) overwrote five of eight
pre-change baseline replay files belonging to the session that produced
`Documents/Plans/Frame/FrameUtilsSharedHelpers.md`, even though both sessions held the harness lock properly
at different times. Recovery was only possible because that session had copied the baseline aside first, and
it cost a manual restore plus a digest verification pass (`Temp/FrameUtilsAcceptance/restore-report.json`,
`Temp/FrameUtilsAcceptance/appdata-sha256-current.txt`). Without those copies the baseline was simply gone,
and the loss is silent — no error, no warning, and the corrupted comparison would have been believed.

Pre-existing and outside the boundary where it was found: the claimed Plan's `## In scope` covered only frame
helper moves, and neither the AppData root nor the replay naming is a file it authorizes changing.

## Design

Add an explicit AppData root override and have harness launches supply a worktree-scoped one, reusing the
`--data-directory` mechanism that already exists for exactly this shape of problem. Chosen over per-run replay
filenames (which changes persistence naming, the F7 recording flow, and every reader) and over a documented
"copy your baseline aside first" contract (which does not prevent the overwrite, only helps recovery from it).

- `Engine/Source/LaunchOptions.h`: add `std::filesystem::path appDataDirectory;` to `LaunchOptions` beside the
  existing `dataDirectory` member, with the same style of trailing comment — the struct comment at `:12-13`
  already states that harness plans append fields here as new launch args are added.
- `Engine/Source/LaunchOptions.cpp`: add an `--app-data-directory` branch modelled byte-for-byte on the
  existing `--data-directory` branch at `:67-105` — clear the field, require a value, require an absolute
  path, canonicalize, require an existing directory, and set `bOk = false` with the matching `kError` log on
  each failure. Do not invent different validation: the two options are the same trust-boundary shape.
- `Engine/Source/File/FileManager.cpp:111-126`: when `gLaunchOptions.appDataDirectory` is non-empty, use it in
  place of the `SHGetKnownFolderPath(FOLDERID_RoamingAppData)` result; otherwise keep the existing call and
  its existing failure fallback verbatim. The `mAppDataDirectory.append(game::kGameName)`,
  `create_directory`, and the `kDebug` log at `:124-126` stay unchanged and run in both cases, so client and
  server keep their distinct per-game subdirectory names underneath the supplied root and one root can be
  passed to both processes.
- `Projects/BrokenEngineSandbox/Documents/AgentHarness.md`: the project harness doc owns the concrete launch
  block, so add `--app-data-directory <absolute worktree-scoped path>` to both the server and the client
  launch lines, using a path beneath the worktree's own `Temp` directory. Since the option requires the
  directory to already exist, state creating it as a documented pre-launch step, alongside the existing
  instruction to create log parents under `$ROOT\Temp`.
- `.agents/skills/agent-harness/SKILL.md`: the Launch section currently states "`--data-directory` is the only
  data-root override". Correct that sentence so it names both overrides and their distinct meanings — the data
  root selects packed assets, the AppData root selects where saves, settings, caches, and replays are written.

No file format, serialized layout, replay component name, or CRC changes; only the directory the same files
are written into. Crash reporting keeps its own independent `FOLDERID_RoamingAppData` resolution
(`Engine/Source/CrashReport.cpp:28`) because it must work from fixed buffers during heap corruption.

## Critical files

- `Engine/Source/LaunchOptions.h` — the new `appDataDirectory` member
- `Engine/Source/LaunchOptions.cpp` — the new `--app-data-directory` parse branch
- `Engine/Source/File/FileManager.cpp` — `FileManager::FileManager()` AppData root resolution
- `Engine/Source/File/AGENTS.md` — the File Contracts bullet naming the data-root override
- `.agents/skills/agent-harness/SKILL.md` — Launch section override sentence
- `Projects/BrokenEngineSandbox/Documents/AgentHarness.md` — the concrete client/server launch block

## In scope

- The `appDataDirectory` member, its `--app-data-directory` parse branch, and the FileManager constructor's
  root selection
- The harness launch block gaining the argument, with its pre-launch directory-creation step
- The `agent-harness` SKILL.md sentence and the `Engine/Source/File/AGENTS.md` bullet that state which
  overrides exist

## Out of scope

- Replay component naming (`GameSaveLoad.cpp:110-115`, `:244`, `:257`) and any per-session or per-run filename
- `CrashReport.cpp`'s independent AppData resolution
- The harness lock, its ownership or staleness rules, and anything about run serialization
- The temp-directory resolution below the AppData block in the same constructor
- Any save, replay, or settings file format, version, or compatibility path
- Automatic cleanup or pruning of the new per-worktree directories

## Risk tier and invariants

Tier 3 — the change spans independently owned subsystems (engine startup and File, plus the agent-harness
launch contract), and it relocates the save and replay artifact root that verification evidence depends on.
Invariants: client and server must resolve the same root for a given launch, or a harness scenario silently
splits its state across two directories; an absent option must produce byte-identical behavior to today,
including the `SHGetKnownFolderPath` failure fallback; the option is a trust boundary and rejects a relative
or missing directory the way `--data-directory` does; `game::kGameName` still separates client from server
beneath the root; no file format or CRC input changes.

## Acceptance criteria

- Launched without the option, client and server write to the same `%APPDATA%\<game name>` directory as before,
  including the existing `kDebug` "AppData directory" log line.
- Launched with `--app-data-directory <existing absolute path>`, both processes write saves, settings, and
  replay artifacts beneath that path, and the shared per-user directory is untouched for the whole run.
- A relative path, a missing value, and a non-existent directory each fail startup with the option's `kError`
  log, matching `--data-directory`'s behavior.
- A harness replay record-and-replay run driven through the documented launch block produces its complete
  `F7.replay.*` set under the worktree-scoped root with matching per-tick CRCs.
