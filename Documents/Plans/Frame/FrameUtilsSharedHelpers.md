<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-11T00:00:02.000Z","dependsOn":[]} -->
# Move the game-agnostic frame helper groups into engine FrameUtils

## Context

Three groups of helpers in the game frame layer contain no game concept at all — they operate on `XMVECTOR` positions, `engine::GridCoord`-derived areas, and engine collection tuples. A second game would have to copy all three verbatim. `Engine/Source/Frame/FrameUtils.h` already hosts exactly this kind of helper (`AllocateAndCopyCollections` at `:213-218`, `LogDifferencesCollections` at `:221-227`).

The three groups, verified in the tree:

- **Frame bounds and transfer geometry** — `FrameBounds`, `ComputeFrameBounds`, `ComputeFrameArea`, `kuiInitialTransferCapacity`, `IsOutOfBounds`, `ComputeTransferDelta` (`Projects/BrokenEngineSandbox/Source/Frame/Frame.h:94-139`). Consumed by `Frame/Collections/Blasters/Blasters.cpp:195`, `:219`, `Missiles/Missiles.cpp:337`, `:370`, `Spaceships/Spaceships.cpp:382`, `:408`, `Spaceships/SpaceshipsCombat.cpp:156`, `:196`, `Players/Players.cpp:291`, `Players/PlayersCombat.cpp:251`, `:290`, `Players/PlayersNavigation.cpp:71`, `:111`, `Frame/Frame.cpp:60` (`kuiInitialTransferCapacity`), `Frame/TerrainUtils.cpp:279`, and `Game.cpp:379` (`ComputeFrameArea` only — `Game.cpp` never uses `FrameBounds`).
- **The destroy sweep** — the same reverse-walk / predicate / release-owned-objects / remove loop appears four times: `Blasters.cpp:245-256`, `Missiles.cpp:396-408`, `Spaceships.cpp:446-455` (all three call `engine::DestroyElement`), and `Players.cpp:227-236` (same loop shape, but calls `engine::PushersPostRender::Remove` plus `engine::RemoveIndexableElement`).
- **Collection tuple folds** — `ValidateCollectionPair`/`ValidateCollectionPairs` (`Frame/Frame.cpp:11-31`) are already generic, and eight hand-written `std::apply` folds repeat four shapes at `Frame.cpp:574`, `604`, `620`, `636`, `653`, `683`, `698`, `713` (CRC, Write, Read, ServerRead — once for Interpolate, once for PostRender).

## Design

Move all three groups to `Engine/Source/Frame/FrameUtils.h` in namespace `engine`, beside the existing tuple helpers. No behavior change anywhere.

- Group (a) moves as-is: the inline functions, the `FrameBounds` struct, and `kuiInitialTransferCapacity` land beside the existing movement helpers, byte-identical bodies and comments included. `ComputeFrameArea` already takes `engine::GridCoord` (`FrameUtils.h` already includes `Frame/GridCoord.h` at `:3`); the `vecArea` lane convention (x=minX, y=maxY, z=maxX, w=minY) documented in the existing comments moves with the code. Game consumers already see `FrameUtils.h` through `Frame.h:3` → `FrameBase.h:21`, so no consumer include changes; they requalify to `engine::`.
- Group (b) becomes one function template beside the tuple helpers, taking the interpolate and post-render collections, a predicate callable returning whether index `i` must go, and a release callable performing the per-element removal (the collection types are template parameters, so the helper names no game type). The reverse `for (int64_t i = iCount - 1; i >= 0; --i)` walk and the `if (!predicate(i)) [[likely]] { continue; }` skip shape stay exactly as in the three `DestroyElement` sites; all four call sites pass their existing body as the two callables, so Players' different remover (`engine::PushersPostRender::Remove` + `engine::RemoveIndexableElement`, keyed by `puiIds[i]`) is supplied rather than special-cased. One deliberate delta: Players' loop today tests its condition positively with no `[[likely]]` annotation (`Players.cpp:228-235`); adopting the helper negates that condition into the shared skip shape and gains the annotation — an optimizer hint only, with identical results.
- Group (c): the three `ValidateCollectionPair`/`ValidateCollectionPairs` templates (`Frame.cpp:11-31`) move verbatim into `FrameUtils.h`, dropping only the `static` that a header template cannot keep; their four call sites (`Frame.cpp:746`, `:748`, `:766`, `:768`) requalify to `engine::`. Four new siblings wrap the four repeated `std::apply` shapes, each keeping the existing lambda fold expression and left-to-right order verbatim inside, taking the tuple by forwarding reference like the existing `AllocateAndCopyCollections`: `CollectionsCrc(common::crc_t sharedCrc, tuple)` returning the folded CRC, `CollectionsWrite(std::ostream&, tuple)`, `CollectionsRead(std::istream&, tuple)`, and `SharedCollectionsRead(std::istream&, tuple)`. The `SharedCollectionCrc`/`CollectionWrite`/`CollectionRead`/`SharedCollectionRead` calls inside resolve by argument-dependent lookup on the engine collection types, so no new include is needed. The eight game sites become one call each; the explicit `pPlayers` and scalar-member lines around them are untouched.

No file is created or deleted, no moved code is client- or server-only, and no aggregation-header edit is needed, so there are no `BT_CLIENT`/`BT_SERVER` guard changes and `/update-vcxproj` is not triggered.

## Critical files

- `Engine/Source/Frame/FrameUtils.h` — new home for all three groups
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.h`, `Frame/Frame.cpp` — removals and call-site rewrites
- `Frame/Collections/Blasters/Blasters.cpp`, `Missiles/Missiles.cpp`, `Spaceships/Spaceships.cpp`, `Players/Players.cpp` — destroy-sweep call sites
- Remaining group (a) consumers listed in Context (`SpaceshipsCombat.cpp`, `PlayersCombat.cpp`, `PlayersNavigation.cpp`, `TerrainUtils.cpp`, `Game.cpp`) — requalification only

## In scope

- Moving group (a) verbatim into `engine` and requalifying every consumer
- Adding the group (b) destroy-sweep template and rewriting the four loops listed in Context to call it
- Moving `ValidateCollectionPair`/`ValidateCollectionPairs` and adding the four fold siblings, rewriting the eight `std::apply` sites in `Frame.cpp`
- Any `Engine/Source/Frame/AGENTS.md` or game `Frame/AGENTS.md` sentence that names the old owner of a moved helper

## Out of scope

- The cross-cell transfer harvest loop and `TransferRequest` (`Frame.h:83-92`) — the payload-type ownership decision is still open, so it is not a Plan yet
- Any change to fold order, walk direction, byte stream, or the `vecArea` lane convention
- Moving `kfCellWidth`/`kfCellHeight`/`kTickNs`/`SeedFromGridCoord` — owned by `Documents/Plans/Frame/EngineOwnedFrameConstants.md`
- Moving `TracePointToFrameExit` — owned by `Documents/Plans/Frame/TerrainTraceToEngine.md`
- Extending the destroy-sweep helper to any collection that does not already contain this loop
- Rewriting the parallel `std::apply` folds inside engine `FrameBase.cpp` onto the new siblings — those already live in the engine and are not duplication a second game would copy

## Risk tier and invariants

Tier 3 — the fold helpers sit directly on the CRC and the save/replay and cross-build wire streams, so the Change Workflow serialization trigger applies even though nothing about the format changes. (The Risk score below is the separate historical anchor: mechanical, compile-checked, easily reverted.) Invariants: the four fold shapes keep left-to-right evaluation over the same tuple in the same order, so the emitted byte stream and CRC mixing sequence are unchanged; the destroy sweep keeps reverse iteration (swap-and-pop safety) and the same predicate-to-release ordering per element; `ValidateCollectionPairs`' `static_assert` on equal tuple sizes survives the move.

## Acceptance criteria

- Client and server compile; a harness replay determinism run shows matching per-tick CRCs.
- A save written before the change loads after it, and a server full state is consumed by a client built from the new code (`ServerRead` path exercised).
- Diff review confirms no fold body or walk direction changed while moving, that each rewritten fold site still passes the same tuple builder (`GameInterpolateCollections`/`GamePostRenderCollections`) on the same frame object, and that the only annotation delta is the documented one on the Players sweep.

## Scores

Effort 2 / Impact 3 / Risk 1
