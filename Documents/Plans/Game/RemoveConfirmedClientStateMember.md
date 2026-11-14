<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:42:58.401Z","dependsOn":[]} -->
# Remove the ClientReconciler confirmed-client-state member

## Context

Found by a deep analysis of `Projects/BrokenEngineSandbox/Source/Network`
(architecture phase, plus one in-function finding), verified against commit
`4cf9d607`. Paths are under
`Projects/BrokenEngineSandbox/Source/Network/Client/`.

`ClientReconciler::mConfirmedClientState` (`ClientReconciler.h:35`) looks like
state kept between calls but keeps nothing:

- `ClientReconciler::Run` overwrites all three fields at its top
  (`ClientReconciler.cpp:73-75`) before the only reads (`:102`, and through it
  `ReconcileUpdateClientState`). The early returns at `:93` and `:98` leave
  values nothing reads, and `Reset` (`:145`) clears a value the next `Run`
  overwrites.
- `ConfirmedClientState::clientGridCoordinate` (`ClientReconciler.h:16`) has
  two writes (`ClientReconciler.cpp:73`, `:145`) and no read anywhere in the
  repository; the transfer scan uses `rWork.coord`.
- The struct is copied four times for one armor value: `Run` copies the member
  into `newConfirmedClientState` (`:102`) and back (`:137`), and
  `ReconcileUpdateClientState` copies its in-out parameter into a local
  (`ReconcileReplayClientState.cpp:16`) and back (`:79`).
- `AGENTS.md` `## Ownership` (third bullet) says the reconciler owns
  "confirmed client-state tracking"; no state survives a call. The code is
  trusted and the document is the side to correct.

In the same function, `ReconcileUpdateClientState` tests
`clientState.clientGlobalIdentifier.iValue != 0` inside the innermost of three
loops (`ReconcileReplayClientState.cpp:38-41`), under an
`if (bAnyFullReplay)` wrapper (`:18`). The function never writes the
identifier (it writes only `fPreviousClientArmor`), so the test is
loop-invariant, and `IsOwnedEntityTransfer`
(`Projects/BrokenEngineSandbox/Source/Frame/StatusChange.cpp`) is a pure read,
so skipping the loops when the identifier is 0 changes nothing.
`Documents/C++StyleGuide.txt` rule 62 asks for early returns in guard
sequences.

`mfPreviousClientArmor` is client-only visual state read by
`Game::UpdateCameraShake`; `Run` and its callee run on the main thread.

## Design

Recommended shape (the author's choice among equivalent ones): keep
`ConfirmedClientState` as the callee's in-out parameter with its two read
fields, and make it a local of `Run`. Replacing the struct with two parameters
is the other shape; it changes the callee's signature for no gain.

1. `ClientReconciler.h`: delete `ConfirmedClientState::clientGridCoordinate`
   and the member `mConfirmedClientState`.
2. `ClientReconciler::Run`: where the three member writes are today (before
   the dispatch, so both values are read at the same point as now), declare
   one local `ConfirmedClientState` initialized from
   `gpGame->ClientOwnedEntityIdentifier()` and `gpGame->mfPreviousClientArmor`.
   Pass that local to `ReconcileUpdateClientState`, delete
   `newConfirmedClientState` and the copy back into the member, and assign
   `gpGame->mfPreviousClientArmor` from the local where it is assigned today.
3. `ClientReconciler::Reset`: delete the `mConfirmedClientState = {};` line.
4. `ReconcileUpdateClientState`: delete the local copy and the write-back and
   use `rInOutState` directly; replace the `if (bAnyFullReplay)` wrapper with
   an early return, add an early return when the identifier is 0 before the
   loops, and delete the per-request identifier test.
5. `AGENTS.md` `## Ownership`, third bullet: remove the "confirmed
   client-state tracking" claim so the bullet names only the visual-error
   aggregation and the owned-entity transfer migration, through
   `/update-claude-docs`.

Behavior is unchanged: the same two values are read at the same point, the
same armor value is written back on the same paths, and the verbose log line
in the transfer scan is untouched.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Network/Client/ClientReconciler.h`
- `Projects/BrokenEngineSandbox/Source/Network/Client/ClientReconciler.cpp`
- `Projects/BrokenEngineSandbox/Source/Network/Client/ReconcileReplayClientState.cpp`
- `Projects/BrokenEngineSandbox/Source/Network/Client/AGENTS.md`

## In scope

- `ConfirmedClientState::clientGridCoordinate` and
  `ClientReconciler::mConfirmedClientState`: deleted.
- `ClientReconciler::Run`: the three member writes and their comment, the
  `newConfirmedClientState` copy, the call argument, and the two closing
  assignments.
- `ClientReconciler::Reset`: the member reset line.
- `ReconcileUpdateClientState`: the entry copy, the `bAnyFullReplay` wrapper,
  the identifier test, and the exit write-back.
- `AGENTS.md` `## Ownership`, third bullet.

## Out of scope

- The visual-error block in `Run` (its duplicated length expression was
  judged cosmetic) and the three position helpers.
- The order in which `ReconcileUpdateClientState` scans works when two
  transfers match (judged below the benefit bar).
- `Game::mfPreviousClientArmor`, `Game::UpdateCameraShake`, and replay
  metadata; `Documents/Plans/Game/RemoveClientStateFromReplayMeta.md` owns the
  replay-metadata side and excludes the reconciler.
- The dead declarations in `ClientReconciler.h`
  (`RemoveUnusedClientNetworkDeclarations.md` in this folder).

## Risk tier

Tier 2 — scoped behavior. Trigger: a field is removed from a struct declared
in a header and a class member is removed, which Tier 1's "no public signature
exposure" excludes. No determinism/CRC, wire, serialization, replay,
threading, or trust exposure: the state is client-only and main-thread.

## Acceptance criteria

- Code read: `ClientReconciler` has no `mConfirmedClientState`,
  `ConfirmedClientState` has no `clientGridCoordinate`, and
  `ReconcileUpdateClientState` holds no copy of its parameter and no
  identifier test inside a loop.
- Code read: `Client/AGENTS.md` `## Ownership` no longer says the reconciler
  tracks confirmed client state.
- `/compile` builds the BrokenEngineSandbox client.
- The repository static checks pass.

## Notes

No harness run backs this Plan; its source was a code read. The originating
in-function report suggested a harness run in which a focused owned entity
crosses a cell during a full replay; the supplied evidence records no setup
for one, and the code read above is decisive for this change.
