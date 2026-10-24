<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-24T21:19:24.844Z","dependsOn":[]} -->
# Fix: DataPacker — a locked pack is detected only at its own type's publish, after earlier types already published

## Context
Observed symptom: during a `/next-plan` run, a `builder` ran `/compile` Local
generation builds of the BrokenEngineSandbox client and server (Debug) after
new model folders were added under `Projects/BrokenEngineSandbox/Data/Models/`.
DataPacker reported "Data Packer - Cannot Publish Output" for the sandbox
`Texture.pack`, with the message that it "is locked by a running game client or
server". The process holding it was this worktree's `ModelViewer.exe`, which
always reads BrokenEngineSandbox data and was not started by the session. The
lock was found only when the Texture type reached its publish, after the Scene
and Model types had already published new packs, so the output `Data` directory
was left mixed: new Scene and Model packs beside the old Texture pack. Both
builds failed. Recovery took a user round trip to close ModelViewer and a full
rerun of the build dispatch.

Code facts from the current tree:
- `PublishManifestAndPack` (`DataPacker/Source/Main.cpp:123-202`) probes only
  the one type's destination pack for DELETE access, just before that type's
  manifest rename; a sharing or lock violation raises the OK/Cancel prompt built
  at `:156-163`, whose text comes from `lockedFailure` (`:125`) and names only a
  game client or server.
- `MainThread` (`DataPacker/Source/Main.cpp:697-767`) exports and publishes the
  types one after another (Scene, Island, then Audio, Model, Shader, Texture,
  Raw through `RunAllMainExports`), and `RunDirtyExport` (`:492-575`) publishes
  each dirty type as soon as its own export finishes, so a lock on a later
  type's pack surfaces only after earlier types have replaced their output.
- In a linked worktree DataPacker runs noninteractive and an OK/Cancel warning
  forces Cancel (`DataPacker/Source/AGENTS.md` `## Architecture`), so the
  locked type fails while the earlier types stay published.
  `DataPacker/Source/AGENTS.md` `## Design Patterns` (Atomic output bullet)
  documents the per-type probe and prompt.

The supplied evidence records no `/agent-harness` setup: ModelViewer was
already running from the worktree's own ModelViewer Output when the builds
started.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 5dcbdc70-8ebe-4339-a698-b4d8b614d50e
- Worktree/branch UUID: f524889d-98ac-40da-8484-00ea2c8f9455
- Session branch: claude/f524889d-98ac-40da-8484-00ea2c8f9455
- Worktree: .claude\worktrees\BrokenEngine\f524889d-98ac-40da-8484-00ea2c8f9455
- Landing ref: claude/f524889d-98ac-40da-8484-00ea2c8f9455
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/Engine/DataPackerLockedPackBeforePublish.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above. OpenCode transcript review remains unsupported regardless of worktree
  retention.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <review ref>` in bounded friction mode — the landing ref —
supplying the recorded client and the recorded conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below. If root-causing
shows the fix lies outside that boundary, surface it for re-planning instead of
expanding scope.

Author's recommendation: before the first manifest or pack of a run is
published, probe every existing `.pack` in the output directory with the same
DELETE-access probe `PublishManifestAndPack` already uses, and when any is
locked raise the existing OK/Cancel prompt once, listing the locked packs,
before anything is replaced; a Cancel there fails the run with no output
changed. A warm run that publishes nothing never reaches the probe, so a running
client, server, or ModelViewer does not disturb a clean run. Keep the per-type
probe and prompt as the fallback for a process that opens a pack after the
up-front check. Extend `lockedFailure`'s wording to name ModelViewer alongside
the game client and server. Rationale: the up-front check reuses the existing
probe and prompt and removes the mixed-output outcome for the common case of a
viewer or game already running when the export starts.

## Critical files
- `DataPacker/Source/Main.cpp`
- `DataPacker/Source/AGENTS.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to `DataPacker/Source/Main.cpp`
  (`PublishManifestAndPack`, `lockedFailure`, and the publish ordering in
  `RunDirtyExport`/`MainThread`) and the Atomic output bullet of
  `DataPacker/Source/AGENTS.md` `## Design Patterns`

## Out of scope
- The landed change the session produced
- Export formats, `.pack`/`.manifest` layout, `kiVersion`, chunk contents, and
  dirty-check rules
- The single-instance mutex, `FileManager` materialization, the diagnostic
  reporter's interactive/noninteractive rules, and the wrapper bootstrap's wait
  for a running client or server
- ModelViewer, client, and server pack-opening code
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior: DataPacker's publish step only, with
formats and exported bytes unchanged); escalate if the fix reaches
build/bootstrap coordination, such as the wrapper bootstrap's primary
DataPacker run or the `"BrokenEngineDataPacker"` mutex. Publishing stays
same-directory renames, manifest before pack, header before pack. Never embed
transcript paths or home paths.

## Acceptance criteria
- The recorded symptom no longer reproduces under the documented invocation:
  with ModelViewer running from the worktree and a dirty sandbox data input, a
  `/compile` Local generation client build reports the locked pack before any
  `.manifest` or `.pack` in the sandbox output `Data` directory is replaced
  (their last-write times are unchanged), and the message names ModelViewer as a
  possible holder
- With nothing holding a pack, the same build publishes every dirty type as
  before
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
