<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T18:23:04.770Z","dependsOn":[]} -->
# Simplify BufferFrame pair aliases with a structured binding

## Context

`Engine/Source/Network/Server/Server.cpp`, `Server::BufferFrame`, iterates the `gridUpdates` span as `const std::pair<GridCoord, GridUpdateData>& rGridUpdate`, then names its two components `rCoordinate` and `rUpdateData` with separate declarations. C++17 structured binding expresses both meaningful component names directly and removes redundant type and component plumbing.

## Design

Replace these two consecutive declarations in `Server::BufferFrame`:

```cpp
const GridCoord& rCoordinate = rGridUpdate.first;
const GridUpdateData& rUpdateData = rGridUpdate.second;
```

with:

```cpp
const auto& [rCoordinate, rUpdateData] = rGridUpdate;
```

Keep the existing indentation, range-based loop, and every subsequent expression unchanged. The binding refers to the existing const pair and provides const access to its existing components for the same iteration lifetime.

## Critical files

- `Engine/Source/Network/Server/Server.cpp`: implementation, exclusively the initial two component alias declarations in `Server::BufferFrame`.
- `Engine/Source/Network/Server/AGENTS.md`, `## Buffered State`: existing contract to preserve; no edit.
- `Documents/C++StyleGuide.txt`, rule 15(d): existing structured-binding permission; no edit.

## In scope

Only the two-to-one declaration replacement inside the `Server::BufferFrame` range-based `gridUpdates` loop.

## Out of scope

Other functions in `Server.cpp`, including `BufferFullFrame` and `Receive`; other pair aliases; range-for conversions; `.first`/`.second` cleanup elsewhere; public signatures; container choices; transport, compression, ring, pruning, logging, or allocation changes; new helpers, tests, or instrumentation.

## Risk and invariants

Tier 1: local behavior-preserving declaration simplification with no public signature or invariant exposure. Preserve both component names and const access without copying the pair or `GridUpdateData`. Input ownership and lifetime remain unchanged. Preserve one delta per supplied coordinate/tick, ring contiguity and ordering, and pruning from supplied coordinates. Preserve deterministic state, CRC values, layout, serialization, protocol, and all error handling.

## Performance

The const-reference binding aliases the same existing pair components. It adds no copies, allocations, branches, traversal, or work and makes no runtime speedup claim. Diff inspection establishes this directly; benchmarking is unnecessary.

## Documentation and style

No documentation or style-policy edits are needed. Rule 15(d) already permits `auto` for structured bindings, and this declaration is inside the range-based loop body rather than its range-for declaration. Preserve the established `rCoordinate` and `rUpdateData` names and source formatting. The server buffered-state documentation remains accurate because no behavior or ownership changes. Run the normal C++ style and documentation review routes at implementation time without expanding the edit scope.

## Verification

Review the diff to confirm the two declarations became exactly the one const-reference binding and every consumer, loop bound, and remaining statement is unchanged. Confirm the range-for declaration still supplies `rGridUpdate` and the binding is its only use in the loop body. Build the server target through `/compile` under its required configurations; do not invent a separate build command. No runtime harness run or unit tests are needed for this declaration-only change.

## Acceptance criteria

1. `Server::BufferFrame` directly binds `rCoordinate` and `rUpdateData` with `const auto& [rCoordinate, rUpdateData] = rGridUpdate;` with the existing range-for pair reference retained.
2. The source diff contains only this declaration replacement; all consumers and control flow remain identical.
3. Both names access the original const pair components without a value copy, preserving the lifetime, buffering, CRC, and serialization invariants above.
4. The server target compiles successfully through `/compile` in its required configurations; relevant style and documentation review finds no required policy change.
