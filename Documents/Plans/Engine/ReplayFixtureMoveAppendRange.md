<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T18:28:30.851Z","dependsOn":[]} -->
# Express replay fixture move appending with C++23 as_rvalue

## Context

Inventory item F104 (`std::views::as_rvalue`) passed review for one local clarity improvement. `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationFixtures.cpp:961`, `DrainReplayTransferFixtures`, appends each queued fixture vector into the transfer manager with `insert(end(), make_move_iterator(begin()), make_move_iterator(end()))` at line 980. This is correct but repeats endpoints to express a whole-vector move append. The proposed replacement names that operation and its consumed source once. No runtime defect or measured performance improvement is claimed.

The source is `sFixture.replayTransferFixtures`, declared in the module's static fixture state at line 28; the destination is `rTransferManager.mTransfers.try_emplace(rCoord).first->second` at line 979. These are distinct owners. `game::StatusChange` in `Projects/BrokenEngineSandbox/Source/Frame/StatusChange.h:175` remains the element type. The source map is cleared once after the loop at line 982.

The installed MSVC 14.51.36231 headers in `C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.51.36231/include/` provide `as_rvalue_view` in `ranges:1458`, size forwarding at `ranges:1518`, and `vector::append_range` at `vector:1017`. The latter sends sized ranges through `_Append_counted_range`; the view does not own an element buffer. The server project selects `stdcpp23` at lines 123, 177, and 246. The standard proposal [P2446R2, wording](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2022/p2446r2.html#wording) specifies move-iterator endpoints and size forwarding; [P1206R7](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2022/p1206r7.pdf) defines the container range methods.

## Design

The author's recommendation is to replace only the append statement with:

```cpp
rDestinationTransfers.append_range(std::views::as_rvalue(rTransfers));
```

The temporary view borrows the live lvalue vector and exposes the same element move references as the existing move iterators. `append_range` consumes it synchronously, appending in source order after any existing destination elements. The source is neither aliased by the destination nor cleared until the loop completes. An empty source appends no elements. Destination allocation remains ordinary vector capacity growth; no intermediate element storage, explicit reserve, or allocation policy change is introduced. This is a clarity change, without a promise of an exact capacity or speed improvement.

Add `<ranges>` once between `<random>` and `<ratio>` in the C++ header group of `Common/ExternalHeaders.h` if it is absent when implementing. The existing game PCH exposes that owner to the translation unit. Retain an existing include if another independent adoption has already added it.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationFixtures.cpp` — the single move-append statement in `DrainReplayTransferFixtures`.
- `Common/ExternalHeaders.h` — conditional addition of `<ranges>` in the existing standard-header group.

Read-only contract owners are `Projects/BrokenEngineSandbox/Source/Agent/AGENTS.md`, `Common/AGENTS.md`, and `Documents/C++StyleGuide.txt` rule 47. Rule 47 and the Common header-ownership contract already suffice; no new style rule or semantic documentation edit is recommended. Perform the required `/update-claude-docs` synchronization review during implementation, recording no update when those contracts remain accurate.

## In scope

- The one move-iterator insertion expression in `DrainReplayTransferFixtures`.
- The single missing `<ranges>` include, if needed.

## Out of scope

Other insertions or range migrations, `RunGit` (separate F078 work), fixture generation, replay orchestration, transfer payloads, public signatures, serialization or version bumps, helpers, stored views, reserve calls, compatibility fallbacks, build configuration, project membership, general ranges policy, unrelated include sorting, unit tests, and benchmark infrastructure.

## Risk and invariants

Recommended future Change Workflow classification: Tier 1, local behavior-preserving work with no public signature or invariant exposure. The common-header addition enables library syntax and changes no subsystem contract. The function's existing session ownership check, coordinate traversal, frame creation and swapping, destination map insertion, prior destination contents, per-vector element order, and final source-map clear remain unchanged. Draining remains independent of replay state. Preserve deterministic injected state, CRC inputs, wire/save/replay formats, thread ownership, and server-only affinity. There is no change to what any deterministic or serialized surface computes or carries.

## Acceptance criteria

| Criterion | Decisive evidence |
|---|---|
| Move append has direct range syntax | Diff shows the exact replacement in the named function and only the optional header addition. |
| Lifetime and order are preserved | Source inspection confirms distinct owners, an immediate temporary view, unchanged destination-prefix retention and source-order traversal, and the existing clear after the loop. |
| No new buffering or allocation policy | Sized vector view reaches the supported counted append path; no temporary vector, reserve, allocator change, or stored view appears. |
| Contracts remain unchanged | Review confirms all session/frame/map logic and payload types are untouched; documentation synchronization reports existing owners remain accurate. |
| Supported configurations compile | Successful sandbox server and client builds through `/compile`. |

## Verification

Review the bounded diff against the acceptance table, inspect the current supported library's size-forwarding/count-aware append path, and run `/compile` for `BrokenEngineSandboxServer` and `BrokenEngineSandbox`, Debug x64. The server instantiates the new expression; the client checks the shared-header effect. Apply the compile skill's affected-target policy if it requires additional consumers. Complete the normal C++ style, comment, affected-code, correctness, and documentation synchronization reviews. No live run is needed for this local equivalence proof; if a concrete runtime concern arises, use `/agent-harness` for any fixture execution. Add no unit tests.

## Notes

This plan is self-contained and relies on neither the feature inventory nor temporary review receipts. Searches of live Plans for `DrainReplayTransferFixtures`, `replayTransferFixtures`, `as_rvalue`, and `ServerSimulationFixtures` found no plan owning this implementation boundary. `RunGitAppendRange` owns a different function and file. The independent JPEG stride-view adoption may add the same header: its idempotent include requirement imposes no directional prerequisite or mandatory coordination. Dependencies are empty.
