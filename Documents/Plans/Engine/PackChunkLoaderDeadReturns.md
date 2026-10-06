<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-06T21:35:15.915Z","dependsOn":[]} -->
# Replace the dead returns after ASSERT(false) in PackChunkLoader::ResetChunkRangeReloadState

## Context

Rule 70 of `Documents/C++StyleGuide.txt` (`:455`): use `std::unreachable()` instead of a dead fallback return after `ASSERT(false)`. `ASSERT` always throws on a false condition: the macro (`Common/ErrorUtils.h:20`) calls `common::Assert`, which logs and throws `std::runtime_error` (`Common/ErrorUtils.cpp:14-21`) in every configuration.

Verified gap (tree at `099e3933`): `PackChunkLoader::ResetChunkRangeReloadState` in `Engine/Source/File/PackChunkLoader.cpp` has two such dead returns — `:163-164` (the `kPending` branch) and `:172-173` (the offset/length mismatch branch). A repository-wide search for `ASSERT(false)` followed by `return`, `break`, or `continue` finds no other site. Neither `/code-style-review` mechanism reaches them, since both review only session-changed ranges; they surfaced during the `/next-plan` run of `Documents/Plans/ChangeWorkflow/StyleGuideReviewCoverage.md`.

## Design

Replace each of the two `return;` statements directly after `ASSERT(false);` with `std::unreachable();`, keeping the `ASSERT(false)` lines and their comments. `<utility>` is already included through `Common/ExternalHeaders.h:116`.

Change Workflow tier: Tier 1. Highest trigger: local behavior-preserving style work — the replaced statements are unreachable because `ASSERT(false)` always throws; no signature, invariant, determinism/CRC, threading, or data surface changes. The file compiles into both the client and server projects.

## Critical files

- `Engine/Source/File/PackChunkLoader.cpp`

## In scope

- `PackChunkLoader::ResetChunkRangeReloadState`: the two statements at `:164` and `:173`.

## Out of scope

- Restructuring the branches (for example folding them into conditional `ASSERT`s), and every other function in the file.
- The rule 13 `PackChunkLoader.h:66` item recorded in `Documents/Investigations/ChangeWorkflow/OwnIntegerTypeSweepDeferredFixes.md`.

## Notes

- Build the client and the server through `/compile` to confirm the change compiles.
