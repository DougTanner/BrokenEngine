<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T17:46:38.612Z","dependsOn":[]} -->
# Express replay publication traversal with const iterators

## Context

`ServerBroadcaster::BuildTickPublication` in `Engine/Source/Network/Server/ServerBroadcaster.cpp:87` defines `ForEachPublicationCoord`. Its replay branch visits `game::gpGame->mActiveCoordinates` with mutable vector iterators, although it only reads coordinates. A prefix `std::find` emits each coordinate at its first occurrence. The loop's current iterator is needed as the prefix endpoint, so retaining the iterator loop expresses the algorithm directly.

`Engine/Source/GameBase.h:276` declares `mActiveCoordinates` as `std::vector<GridCoord>`. `Engine/Source/Frame/GridCoord.h:11` provides const defaulted equality. Both callback instantiations accept `const engine::GridCoord&`: the first counts coordinates/status changes, and the second copies coordinates into publication storage. Neither mutates the source vector. Const iterators therefore express the existing read-only contract with no new machinery or runtime work.

## Design

In the replay-only iterator loop inside `ForEachPublicationCoord`, make exactly these three accessor substitutions together:

1. Initialize `it` from `game::gpGame->mActiveCoordinates.cbegin()` instead of `begin()`.
2. Compare `it` against `game::gpGame->mActiveCoordinates.cend()` instead of `end()`.
3. Pass `game::gpGame->mActiveCoordinates.cbegin()` as the first argument to `std::find` instead of `begin()`.

Keep `auto`, `++it`, the prefix endpoint `it`, the searched value `*it`, the equality against `it`, and `rCallback(*it)` unchanged. Updating both `begin()` calls keeps the two `std::find` iterator arguments the same type.

Const vector iterators preserve traversal and equality work, allocation behavior, and the existing quadratic replay-only deduplication complexity. No performance penalty is expected and no speedup is claimed.

## Critical files

- `Engine/Source/Network/Server/ServerBroadcaster.cpp` — the only implementation file to change; `ServerBroadcaster::BuildTickPublication`, local `ForEachPublicationCoord`, replay active-coordinate loop.
- `Engine/Source/GameBase.h` and `Engine/Source/Frame/GridCoord.h` — read-only evidence for the vector and const equality contract.
- `Engine/Source/Network/Server/AGENTS.md` — read-only publication ordering, server affinity, and view-lifetime invariants.

## In scope

The three accessor substitutions above, confined to the replay active-coordinate iterator loop and its prefix search in `ServerBroadcaster::BuildTickPublication`.

## Out of scope

Changes to the non-replay range loop, callbacks, broadcast-status/transfer traversal, deduplication algorithm, coordinate storage, allocation, publication lifetimes, serialization, CRC, replay formats, protocol versions, reverse iterators, or unrelated iterator sites. No new helpers, dependencies, tests, or repository-wide adoption policy.

No style-guide or AGENTS.md amendment is warranted: existing iterator/`auto`/const-read conventions already support this spelling, and no architectural contract changes. Perform the normal documentation review during implementation without manufacturing a documentation edit.

## Risk tier and invariants

Future implementation is Tier 1: a local, behavior-preserving expression change with no public signature or invariant exposure. Although the loop participates in replay publication, the change alters neither computed coordinates nor their order, format, or lifetime.

- Search exactly the preceding prefix `[cbegin(), it)` and emit each active coordinate once at its first occurrence in replay mode.
- Preserve empty-vector behavior, publication coordinate order, both callback invocations, and all downstream values and counts.
- Preserve source-vector contents, all allocations, iterator validity, and publication-view lifetime through synchronous consumption.
- Preserve non-replay behavior and the existing server-only affinity.

## Acceptance criteria

1. The source diff consists only of the three specified substitutions within the named loop.
2. Loop and prefix-search iterators are consistently const iterators; const equality and both const-reference callbacks remain applicable.
3. Inspection establishes identical loop bounds, prefix membership, first-occurrence ordering, callback work, and allocation/complexity behavior.
4. The BrokenEngineSandbox server Release target compiles and links through `/compile`.

## Verification

During future implementation, inspect the source diff and both callback instantiations against the criteria above, then use `/compile` for BrokenEngineSandbox server Release. The server owns this translation unit; a client build is unnecessary for these local substitutions. Follow the normal C++ correctness, style, comment, affected-code, and documentation checks at their workflow stages.

No `/agent-harness` run or benchmark is required: the exact diff and successful compilation decisively establish this mechanical change, with no altered runtime algorithm. Do not add unit tests. These future checks have not been run as part of writing this Plan.
