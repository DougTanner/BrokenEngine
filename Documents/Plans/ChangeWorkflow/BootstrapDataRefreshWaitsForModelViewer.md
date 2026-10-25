<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-24T22:35:20.513Z","dependsOn":[]} -->
# Fix: wrapper bootstrap — the primary data refresh waits for a running game but not for a running ModelViewer

## Context
The wrapper bootstrap refreshes the primary checkout's exported game data by
running the prebuilt DataPacker with `BT_DATAPACKER_NONINTERACTIVE=1`
(`.agents/scripts/Bootstrap-AgentTools.ps1:259-283`). Because a locked pack
makes a noninteractive DataPacker force Cancel on its OK/Cancel lock prompt
(`DataPacker/Source/AGENTS.md` `## Architecture`; the environment read is in
`DataPacker/Source/DiagnosticReporter.cpp`), the bootstrap first waits up to
`$WaitSeconds` for running processes that hold packs, and skips the run with a
warning only when one outlives that wait
(`.agents/scripts/Bootstrap-AgentTools.ps1:237-257`).

Verified root cause: that wait matches only game project process names —
`$gameProcessNames` is built from `$gameProjects` as `"<Project>*"`
(`.agents/scripts/Bootstrap-AgentTools.ps1:240`) and both `Get-Process` calls
(`:241`, `:251`) use only that list — so a running ModelViewer is never waited
for. ModelViewer holds the same packs a game client does:
- It is a client build (`BT_CLIENT` in
  `Tools/ModelViewer/Platforms/VisualStudio2026/ModelViewer.vcxproj:66`), and
  `PackChunks` opens every lazy pack (Audio, Islands, Texture) with a
  share-read-only lock for the whole process lifetime
  (`Engine/Source/File/PackChunks.cpp:317-323`).
- Its data directory is the build's `GameDataDirectory`, embedded as
  `kpcDefaultDataDirectory` (`ModelViewer.vcxproj:407`); the bootstrap's own
  ModelViewer prebuild passes the primary
  `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/Output/Data`
  (`.agents/scripts/Bootstrap-AgentTools.ps1:309-317`), and worktree
  ModelViewer binaries are seeded from that prebuild. Its executable name is
  `ModelViewer` (Release, Profile) or `ModelViewer.Debug` (Debug)
  (`ModelViewer.vcxproj:38-39`).

Observed consequence (from code, not a live run): with such a ModelViewer
open, a session-start refresh whose dirty types include Audio, Islands, or
Texture reaches the locked-pack prompt, which forces Cancel; DataPacker exits
nonzero and the bootstrap throws at `:283`, aborting session start, where a
running game would instead have been waited out or skipped with a warning.

Originating gap: recorded as a residual while preparing
the DataPacker fix that checks every pack a run replaces for a holder before its first rename, whose
scope excludes the wrapper bootstrap's wait, so the gap is outside
that change.

## Design
Author's recommendation: add the ModelViewer process name pattern
`ModelViewer*` to the process list the existing wait polls, so the same
bounded wait, re-snapshot, proceed, and skip-with-warning paths cover it, and
widen the two user-facing messages (`:243`, `:256`) and the comment at
`:238-239` from "the game" to the game or ModelViewer. Rationale: it reuses the
existing wait unchanged, and ModelViewer always reads BrokenEngineSandbox data,
the only game project under `Projects/`. Update the two documents that
describe the wait so they name ModelViewer too.

## Critical files
- `.agents/scripts/Bootstrap-AgentTools.ps1`
- `Documents/AGENTS.md`
- `Documents/FreshMachineSetup.md`

## In scope
- `.agents/scripts/Bootstrap-AgentTools.ps1` lines 237-257 only: the
  `$gameProcessNames` list, the comment above it, and the pause and
  wait-exhausted messages
- `Documents/AGENTS.md` `## Reference Material`, the `FreshMachineSetup.md`
  bullet's "running client or server" wording
- `Documents/FreshMachineSetup.md` step 5's sentence on what session start
  waits for (lines 64-68)

## Out of scope
- DataPacker source, including the lock probe, prompt, and the noninteractive
  rule
- ModelViewer, client, and server pack-opening code
- The wait budget value, the bootstrap mutex, the DataPacker mutex, the
  prebuild and stamp logic, and the ModelViewer prebuild block (`:305-318`)
- Any other wrapper or compile script

## Risk tier and invariants
Tier 3: build/bootstrap coordination that can block other sessions
(`.agents/references/risk-tiers.md`) — the wait runs while the bootstrap mutex
is held, so widening what it waits for lengthens peer session-start blocking.
No determinism, `.pack` format, `kiVersion`, wire, or replay exposure; the
exported data bytes are unchanged.

## Acceptance criteria
- With a ModelViewer open on the primary data and no game running, a wrapper
  session start whose refresh would replace a lazy pack prints the pause
  message naming the ModelViewer process, then proceeds and exports once
  ModelViewer closes within the budget
- If that ModelViewer stays open past the budget, session start skips the
  refresh with the wait-exhausted warning instead of failing
- With neither a game nor ModelViewer running, the refresh runs with no pause

## Notes
- The supplied evidence records no live setup that reproduced the failure; it
  was found by reading the bootstrap block.
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing.
