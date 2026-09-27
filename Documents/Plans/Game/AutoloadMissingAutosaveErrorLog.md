<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-27T19:22:02.097Z","dependsOn":[]} -->
# Fix: server startup autoload logs a kError when no autosave exists in a fresh app-data root

## Context
Observed symptom: during a `/next-plan` run's `/agent-harness` scenario, the
server was launched per
`Projects/BrokenEngineSandbox/Documents/AgentHarness/launch.md` `## Launch`
with `--app-data-directory <worktree>\Temp\AppData` (the shared harness
app-data root), which was fresh at that launch. At startup, before
`Enter main loop` and before any client connected, the server logged a `kError`
line:

`ReadGrid ServerAutosave.save failed: version 0 != 209`

Root cause: a missing autosave, not a corrupt one. The server's startup load
`game::GameSaveLoad::Autoload`
(`Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.cpp:132-144`, called
from `Projects/BrokenEngineSandbox/Source/Game.cpp:50`, which falls back to `CreateNewFrame` on `false`) calls
`engine::ReadGridSave` unconditionally. `FileManager::OpenFile`
(`Engine/Source/File/FileManager.cpp:244-258`) returns an unopened stream for a
missing file and logs only at `kDebug`; `common::Read`
(`Common/Serialization.h:62-68`) is a bare `stream.read`, so the header read
fails and leaves `iVersion` at its initial 0 (`Engine/Source/File/GridSave.cpp:74-78`),
and `ReadGridSave` logs the same `version 0 != 209` `kError` it logs for a
corrupt file. Local evidence: the app-data root's directory time was just after
the run stamp, the server log shows the error before `Enter main loop`, and the
only `ServerAutosave.save` present afterward has header 209, written at exit.
`launch.md` `## Launch` already states "A fresh root has no autosave", so every
server launch into a fresh or empty app-data root logs this `kError`; the
change under test touched no save or server code.

Rework it forced: the harness criterion "no new error-level log lines" failed
(finding HARNESS-F-001) on a line unrelated to the change. The main session had
to judge the line unrelated by hand and then send that judgment to the
adversarial review for confirmation. The checkpoint reviewer named `launch.md`
as the emitter; the emitter is the server's startup autoload.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: cd82f5ff-dcd2-4860-8802-294e58cc038c
- Worktree/branch UUID: b4b9fd7e-d1db-4331-beed-bec1c459a34b
- Session branch: claude/b4b9fd7e-d1db-4331-beed-bec1c459a34b
- Worktree: .claude\worktrees\BrokenEngine\b4b9fd7e-d1db-4331-beed-bec1c459a34b
- Landing ref: claude/b4b9fd7e-d1db-4331-beed-bec1c459a34b — the observing
  session records and lands this Plan itself, so the session branch's tip is
  that session's final commit; the branch survives exactly as long as the
  worktree recorded above.
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/Game/AutoloadMissingAutosaveErrorLog.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
A missing autosave is the normal first-launch state, so the startup autoload
treats it as "nothing to load" rather than a failed read. In
`game::GameSaveLoad::Autoload`, before the `engine::ReadGridSave` call, return
`false` without logging when
`engine::gpFileManager->Exists({engine::FileFlags::kAppDataDirectory, engine::FileFlags::kRead}, std::filesystem::path("ServerAutosave.save"))`
is `false` (`FileManager::Exists`, `Engine/Source/File/FileManager.cpp:218-221`,
is public). `Game.cpp:50` then creates a new frame exactly as it does today. An
existing autosave still goes through `ReadGridSave` unchanged, so a corrupt or
old-version autosave keeps its `kError`. The transcript is not needed for this
fix; only if it becomes genuinely needed, in a new session run
`/next-plan-review claude/b4b9fd7e-d1db-4331-beed-bec1c459a34b` in bounded
friction mode, supplying client `claude` and conversation session ID
`cd82f5ff-dcd2-4860-8802-294e58cc038c`.

## Critical files
- `Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.cpp` — `GameSaveLoad::Autoload`,
  the server's startup autosave load; the authorized fix boundary

## In scope
- `game::GameSaveLoad::Autoload` in
  `Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.cpp`: the missing-file
  early return `## Design` states, and nothing else in that function

## Out of scope
- The landed change the session produced
- `engine::ReadGridSave` and every other part of `Engine/Source/File/GridSave.cpp`,
  `FileManager`, `Common/Serialization.h`, `GameSaveLoad::ServerLoad` (an explicit
  load of a missing file stays an error), the save format, and `kiVersion`
- `Projects/BrokenEngineSandbox/Documents/AgentHarness/launch.md`: its "A fresh
  root has no autosave" and "The server loads its exit autosave" sentences in
  `## Launch` stay correct after the fix
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Tier 2 (scoped game behavior: one server-only startup path's handling of a
missing file). It touches no save format, `kiVersion`, or trust boundary: the
existence check precedes the read, and an existing file still takes the full
validated `ReadGridSave` path. Invariants: a valid autosave still loads at
startup; a present but corrupt or old-version autosave still logs its `kError`
and falls back to a new frame. Never embed transcript paths or home paths.

## Acceptance criteria
- A server launched by the `launch.md` `## Launch` recipe into a fresh
  app-data root that holds no `ServerAutosave.save` logs no error-level line at
  startup and reaches `Enter main loop`
- A second server launch into the same root, after the first exited cleanly and
  wrote its exit autosave, loads that autosave at startup with no error-level
  line
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
