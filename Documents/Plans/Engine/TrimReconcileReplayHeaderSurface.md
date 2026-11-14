<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:44:42.470Z","dependsOn":[]} -->
# Remove three redundant declarations from ReconcileReplay.h

## Context

Found by the architecture review phase of a deep analysis of
`Engine/Source/Network/Client` at commit `d87ad840`. Line numbers are from
that commit; locate by symbol if they moved. Paths without a directory are
under `Engine/Source/Network/Client/`. Each item is a declaration in
`ReconcileReplay.h` that repeats something or reaches further than its users.

### ReconcileCoord is declared for one file

`ReconcileCoord` is declared at `ReconcileReplay.h:141`, defined at
`ReconcileReplay.cpp:332`, and called once, at `ReconcileReplay.cpp:472`,
after its definition. The other seven free functions declared beside it are
each used from a second translation unit. `Documents/C++StyleGuide.txt` rule
66 makes a file-local function `static`. The name also appears as a diagram
label in `Documents/Architecture/GameReconciliation.md` (lines 25 and 89),
which stays true.

### iReplayStackCount mirrors replayStack.size()

`CoordScratch::iReplayStackCount` (`ReconcileReplay.h:72`) always equals
`replayStack.size()`. Every writer changes both together: `Reset`
(`ReconcileReplay.h:94-95`), the four replay-base resets
(`ReconcileReplay.cpp:29-33`, `71-75`; `ReconcileReplayTick.cpp:24-28`,
`335-339`), and `ReconcileRunTickCoord` (`ReconcileReplayTick.cpp:170-171`).
No game code writes either. Readers:

- `ReconcileReplayTick.cpp:106`, `182`, `207`, `296`;
- `ReconcileReplay.cpp:393`, `518`, `522`, `534`;
- in the game directory `Projects/BrokenEngineSandbox/Source/Network/Client/`,
  `ClientReconciler.cpp:54` and `ReconcileReplayClientState.cpp:29`.

### miActiveCount duplicates the dispatch result

`ReconcileDispatcher::miActiveCount` (`ReconcileReplay.h:177`) is written at
`ReconcileReplay.cpp:434` and `571`, copied into `result.iActiveCount` at
line 437, and read outside the class only at
`Projects/BrokenEngineSandbox/Source/Network/Client/ClientReconciler.cpp:103`,
where the dispatch result with the same value is in scope (`dispatch`, from
the `Run` call at lines 89-91).

Unmet condition: `ReconcileReplay.h` declares no file-local function and no
member that repeats another value.

Originating gap: that review's module-cohesion lens. It is not an unmet
acceptance criterion of any change. It is pre-existing: the analysis session
changed no source.

## Design

Recommended (the author's recommendation):

- Delete the `ReconcileCoord` declaration and add `static` to its definition.
- Remove `iReplayStackCount` and its `Reset` line. Replace each read with
  `std::ssize(rScratch.replayStack)`, and each
  `replayStack.at(iReplayStackCount - 1)` with `replayStack.back()`. Remove
  the writes at the reset sites and in `ReconcileRunTickCoord`.
- Remove `miActiveCount` and its two writes; set
  `result.iActiveCount = iActiveCount` in `ReconcileDispatcher::Run`; make
  `ClientReconciler.cpp:103` read the dispatch result's `iActiveCount`.

The LOG calls at `ReconcileReplay.cpp:518` and `522` keep their text
byte-for-byte: the harness resimulation check matches that line.

`Projects/Coretide` is a private game project that is absent from a session
worktree, and it may read all three names. The executing session searches
`Projects/Coretide` in the primary checkout for `iReplayStackCount`,
`miActiveCount`, and `ReconcileCoord`, and reports what it finds before it
removes them. Every game project's readers change in the same landing.

Risk tier: Tier 2 (the author's classification). Trigger: public members of
an engine header that game projects read are removed, on the client reconcile
chain whose committed ring is CRC-compared. Nothing new is computed: each
removed value is replaced by the value it copied. Threading is unchanged. No
packet, save, `.pack`, or `kiVersion` change.

## Critical files

- `Engine/Source/Network/Client/ReconcileReplay.h`
- `Engine/Source/Network/Client/ReconcileReplay.cpp`
- `Engine/Source/Network/Client/ReconcileReplayTick.cpp`
- `Projects/BrokenEngineSandbox/Source/Network/Client/ClientReconciler.cpp`
- `ReconcileReplayClientState.cpp` in that same game directory

## In scope

- The `ReconcileCoord` declaration and the linkage of its definition.
- `CoordScratch::iReplayStackCount`: the member, its `Reset` line, and every
  write and read listed in `## Context`.
- `ReconcileDispatcher::miActiveCount`: the member, its two writes, the
  `result.iActiveCount` assignment, and the one game read.

## Out of scope

- The visibility of `ClientDesyncCore::RecoverFromDesync` and of
  `ClientSessionRuntime::PollDiscovery`, `SetDesiredCoordinates`, and
  `ResetClock`: no rule asks for a change, and both classes expose public
  data members.
- `ReconcileInputs` and the signature of `ReconcileDispatcher::Run`.
- `CoordScratch::Reset` beyond the one removed line.
- Merging the two LOG calls at `ReconcileReplay.cpp:516-523`.
- `Documents/Architecture/GameReconciliation.md`.

## Invariants

- The ring layout every path commits is byte-identical to today's.
- The "Reconcile post-replay" log lines keep their text and argument values.

## Acceptance criteria

1. A search of `Engine` and `Projects/BrokenEngineSandbox` finds no
   `iReplayStackCount` and no `miActiveCount`, and `ReconcileReplay.h` does
   not declare `ReconcileCoord`.
2. `/compile` builds the BrokenEngineSandbox client and server.
3. The replay determinism recipe in
   `Projects/BrokenEngineSandbox/Documents/AgentHarness/replay.md` shows no
   CRC mismatch.
4. The resimulation check in
   `Projects/BrokenEngineSandbox/Documents/AgentHarness/verification.md`
   passes as that document states it.

The supplied evidence records no harness setup for criteria 3 and 4: nothing
was built or run. The executing session sets them up from the harness
documentation.

## Coordination

No Plan below depends on this one, and it depends on none. All edit the same
reconcile files; whichever lands second re-derives its regions by symbol from
the landed files.

- `Documents/Plans/Engine/ShareReconcileSnapshotSearchAndBaseReset.md` moves
  the replay-base reset, which writes `iReplayStackCount`, into one helper.
  If that Plan lands first, this Plan removes the statement from the helper;
  if this Plan lands first, that helper holds four statements.
- `Documents/Plans/Engine/RemoveUnreachableReconcileBranches.md` and
  `Documents/Plans/Engine/RemoveRedundantReconcileReplayState.md` edit
  `ReconcileCoord`, `RunReplay`, and the tick functions that read
  `iReplayStackCount`.
