<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T18:43:03.195Z","dependsOn":[]} -->
# Move the completed scene model path into cleanup ownership

## Context

`ExportScene::WriteModelFile` in `DataPacker/Source/ExportJobs/ExportScene.cpp` constructs a mutable local `std::filesystem::path path` from `mInputPath`, appends `.MODEL`, writes that output, flushes and closes the stream, and checks `fileStreamOut.good()`. Its final statement currently copies `path` into `mIntermediateFiles` (baseline line 757). The function then returns without another use of the local.

`ExportScene.h` declares `mIntermediateFiles` as `std::vector<std::filesystem::path>`. `ExportScene::CleanupOnFailure` removes each path held in that vector and clears it. The completed local can therefore transfer its value directly to cleanup ownership, avoiding the copy of a potentially heap-backed path. This is a concrete local use of move semantics with no added work or public interface change.

Evidence was inspected against baseline `d29fed456d3ede935c5e672f95f13d6733f0660c`. This plan contains the rationale needed for implementation and does not depend on an investigation document. Existing Plans searched for `ExportScene`, `WriteModelFile`, `mIntermediateFiles`, and `B21` do not own this statement; the export-job destructor plan touches a separate declaration.

## Design

Replace only the final insertion in `ExportScene::WriteModelFile`:

```cpp
mIntermediateFiles.push_back(std::move(path));
```

Keep the local mutable and retain the existing `push_back` operation. `std::filesystem::path` has a non-throwing move constructor; the inserted element retains the original path value, and the moved-from local is only destroyed. A vector allocation failure does not require preserving a local that immediately leaves scope. Keep the insertion after the existing stream close and success check, preserving when cleanup assumes ownership.

`Common/ExternalHeaders.h` already includes `<utility>` (baseline line 113), and this function already uses `std::move` for its optimized vertices. No include, helper, comment, style-guide amendment, or AGENTS.md amendment is warranted. Existing style rules already cover qualified standard-library calls and mutable locals used for ownership transfer. No serialized payload or export-version change is warranted.

## Critical files

- `DataPacker/Source/ExportJobs/ExportScene.cpp`: edit the final insertion in `ExportScene::WriteModelFile`; inspect `ExportScene::CleanupOnFailure` as its consumer.
- `DataPacker/Source/ExportJobs/ExportScene.h`: read-only evidence for `mIntermediateFiles`' element type.
- `Common/ExternalHeaders.h`: read-only evidence for the existing `<utility>` include.

## In scope

Only replace `mIntermediateFiles.push_back(path)` with `mIntermediateFiles.push_back(std::move(path))` at the end of `ExportScene::WriteModelFile`.

## Out of scope

All other move candidates, including `AllRegisteredWorktreesClear` worktree paths; collection reservation or reallocation policy; `emplace_back` conversions; cleanup refactoring; output generation, serialization, cache metadata, and export-version changes; new headers, helpers, comments, documentation policy, tests, or benchmark infrastructure.

## Risk and invariants

Future implementation risk is Tier 1: one local behavior-preserving ownership transfer with no public signature or invariant exposure. Escalate and reassess if implementation requires any change beyond that final argument expression.

- The stored path remains exactly the `.MODEL` path used to create the completed output.
- Cleanup registration remains after flush, close, and stream-success verification.
- No read or reference to the local `path` survives the move.
- Failure cleanup removes the same outputs and clears the same ownership list.
- Emitted bytes, metadata, export versions, and client/server simulation are unchanged.
- The change adds no allocation or extra path construction; the inserted path uses move construction instead of copy construction.

## Acceptance criteria

1. The implementation diff contains only the specified insertion-argument change.
2. Source inspection confirms the local is non-const, the stream check still precedes insertion, and insertion is the last statement; `CleanupOnFailure` still consumes the same vector element value.
3. Existing headers provide `std::move` without an include change, and DataPacker Release compiles successfully.
4. The scoped review finds no additional ownership, output-format, documentation, or performance work necessary.

## Verification

At implementation time, inspect the final diff and the producer/cleanup consumer to settle criteria 1, 2, and 4 and the invariants. Run the repository-required C++ review and cleanup workflow, keeping any resulting changes within the approved scope. Use `/compile` for `DataPacker` in `Release|x64` to settle criterion 3. Do not build client/server targets or run `/agent-harness`: there is no changed game behavior or runtime-observable acceptance criterion. No asset re-export, unit tests, or performance benchmark is required for this local transfer. These are future checks; no implementation or build has been performed while writing this plan.
