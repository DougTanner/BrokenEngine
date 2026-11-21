<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-20T23:51:27.942Z","dependsOn":[]} -->
# ReconcileDispatchResult Carries the Active-Work Span

## Context

`ClientReconciler::Run`
(`Projects/BrokenEngineSandbox/Source/Network/Client/ClientReconciler.cpp:98`)
builds `std::span<const engine::CoordWork>(mDispatcher.mWorks.data(),
dispatch.iActiveCount)` from the public `ReconcileDispatcher::mWorks`
(`Engine/Source/Network/Client/ReconcileReplay.h:184`) and
`ReconcileDispatchResult::iActiveCount` (`ReconcileReplay.h:169`). The game
therefore depends on the dispatcher's internal rule that the active works are
the first `iActiveCount` slots of a reused vector
(`Engine/Source/Network/Client/ReconcileReplay.cpp:352-377`). `mWorks` has no
other reader outside `ReconcileReplay.cpp` (`:361-372`, `:407`, `:526`).

Inside `ReconcileDispatcher::Run`, `result.iActiveCount` is set at `:380`, read
by the diagnostics lambda (`.iActiveCoords`, `:387`) and the burst log
(`:512`). `ClientReconciler::Run` tests `dispatch.iActiveCount == 0` at `:87`
and passes the span it built to `ReconcileUpdateClientState` and
`GetClientReplayedPosition`.

## Design

Replace `ReconcileDispatchResult::iActiveCount` with
`std::span<const CoordWork> activeWorks`, set in `ReconcileDispatcher::Run`
where `iActiveCount` is set today, to the first `iActiveCount` slots of
`mWorks`. Derive the diagnostics `.iActiveCoords` and the burst log count from
its size. Move `mWorks` under `private:`. In `ClientReconciler::Run`, test
`dispatch.activeWorks.empty()`, delete the local span, and pass
`dispatch.activeWorks` to both consumers.

The span borrows `mWorks`; the result is a local consumed before the next
`Run` or `Reset`, so no lifetime changes.

## Critical files

- `Engine/Source/Network/Client/ReconcileReplay.h`
- `Engine/Source/Network/Client/ReconcileReplay.cpp`
- `Projects/BrokenEngineSandbox/Source/Network/Client/ClientReconciler.cpp`

## In scope

- `ReconcileDispatchResult`: replace `iActiveCount` with `activeWorks`.
- `ReconcileDispatcher`: move `mWorks` to `private:`.
- `ReconcileDispatcher::Run`: set `result.activeWorks`; derive the
  `PublishDiagnostics` `.iActiveCoords` value and the "Replay burst" log
  `Coords` value from its size.
- `ClientReconciler::Run`: the empty test, removal of the local span, and the
  two consumer arguments.

## Out of scope

- The dispatcher's internal mutable `activeWorks` span and the local
  `iActiveCount` in `ReconcileDispatcher::Run`, and the worker code.
- `ReconcileProfiling`, desync selection, and `ReconcileDispatcher::Reset`.
- Any documentation: no `.md` names the removed member.

## Acceptance criteria

- Clean `/compile` build of the BrokenEngineSandbox client (the code is
  `BT_CLIENT`-only).

## Notes

- Change Workflow tier: Tier 2. Trigger: an engine public struct member and a
  class access change in `ReconcileReplay.h` consumed by the game reconciler.
  Behavior-preserving; no CRC, wire, save, or threading surface changes.
