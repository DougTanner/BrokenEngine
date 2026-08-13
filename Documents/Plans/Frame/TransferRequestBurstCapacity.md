<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-13T12:21:03.142Z","dependsOn":[]} -->
# Size the per-tick transfer-request budget for single-tick bursts

## Context

`FramePostRender::transferRequests` is the transient per-tick buffer that carries cross-cell transfers out of
one simulated cell. It is reserved exactly once, at construction, and cleared exactly once per tick, so its
reserved capacity is a **per-tick budget shared by all four producers**:

- `Engine/Source/Frame/FrameUtils.h:69` — `inline constexpr size_t kuiInitialTransferCapacity = 32;`
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.h:117-118` — `std::vector<TransferRequest> transferRequests;`,
  commented "Transient transfer output buffer (not serialized, not in CRC/equality)".
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:38` — the only
  `transferRequests.reserve(engine::kuiInitialTransferCapacity)`, in the `FramePostRender` constructor.
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:142` — the only `transferRequests.clear()`, at the top of
  `FramePostRender::Update`.

Four producers push into that one budget, each preceded by an identical guard that logs `kError` and calls
`DEBUG_BREAK()` when `size() == capacity()` — i.e. immediately before the `push_back` that would reallocate:
`Blasters.cpp:202-210`, `Missiles.cpp:375-383`, `Spaceships.cpp:409-433`,
`Players/PlayersNavigation.cpp:113-136` (all under
`Projects/BrokenEngineSandbox/Source/Frame/Collections/`). All four carry the same verbatim comment:

```
// Heap realloc warning: capacity exceeded during burst transfers. Expected max ~1-2/tick
// per source frame — anything higher suggests entities are re-flagging kTransfer across
// iterations, DestroyElement isn't removing them, or there's an unexpected push path.
```

Why this is being recorded now. `Documents/Plans/Frame/DestroyElementReverseWalkSkip.md` removed the
`DestroyElement` loop-index adjustment that made six reverse walks skip an index. Before that fix, a batch of
flagged elements dribbled out across several ticks; after it, every flagged element in a pass is transferred in
the **same** tick. That roughly doubles the peak per-tick burst against an unchanged budget of 32. The guard
comment above already names "DestroyElement isn't removing them" as a suspected cause of anomalous counts —
that was exactly the defect that Plan fixed.

Proven pre-existing and out of scope there. The overflow was always reachable on the old code; the fix only
lowered the number of simultaneously-flagged elements needed to reach it. Nothing caps blasters, missiles, or
spaceships per cell — `engine::GrowPairedCollections`
(`Engine/Source/Frame/Collections/CollectionLifecycle.h:28-45`) grows collections unbounded at
`2 * capacity + 1` — so a large enough engagement straddling a cell boundary can exceed any fixed reserve.
`DestroyElementReverseWalkSkip.md`'s `## Out of scope` explicitly excludes "the transfer-request payload types
and the harvest loop shape themselves", and its `## In scope` names no capacity constant, guard, or push site,
so changing them there would have been unauthorized scope expansion past both that Plan and the
minimum-sufficient-change directive.

Severity is low and self-limiting, which is why it is debt rather than a blocker: `clear()` does not shrink
capacity, so a single overflow reallocates the vector once and the guard never trips again for that
`FramePostRender`'s lifetime. `DEBUG_BREAK()` (`Common/ErrorUtils.h:11`) is inert unless `kbDebugBreak` is set
**and** a debugger is attached, and `kbDebugBreak` is true only in `BT_DEBUG`
(`Projects/BrokenEngineSandbox/Source/Pch.h`), so in a shipping run the symptom is one `kError` line plus one
untracked heap allocation inside the simulation loop. None of the four push sites is wrapped in
`ScopedSuppressAllocationTracking`.

## Design

This Plan deliberately does **not** presuppose that 32 is too small or that suppression is needed. It fixes the
*rules* by which those two questions are answered, so the implementation is decision-complete while the numbers
come from measurement on the post-fix code.

**Step 1 — measure.** On code that already contains the `DestroyElementReverseWalkSkip` fix, drive a harness
scenario that pushes a large group of entities across one cell boundary in a single tick (see
`## Acceptance criteria`) and record the peak per-tick `transferRequests.size()` observed for one
`FramePostRender`, summed across all four producers. Use the existing guard's `Pushed:`/`Capacity:` log fields
as the observation channel — no new instrumentation is added, and any temporary instrumentation used to obtain
the number is removed before the change lands.

**Step 2 — apply decision rule A (capacity).** If the measured peak reaches or exceeds
`kuiInitialTransferCapacity`, raise the constant at `FrameUtils.h:69` to the smallest power of two that is at
least twice the measured peak, and update the four guard comments' "Expected max ~1-2/tick per source frame"
sentence so it states the measured expectation instead of the stale one. If the measured peak stays below the
constant, change no value and record the measured peak in the completion evidence. In both branches the four
guard comments' claim about `DestroyElement` is now historically resolved and must be corrected, because that
suspected cause has been fixed and the sentence would otherwise send a future reader after a defect that no
longer exists.

**Step 3 — apply decision rule B (allocation tracking).** Determine from
`Engine/Source/Memory/AGENTS.md` and the tracker's own activation conditions whether the four push sites
actually execute under main-loop allocation tracking. If they do, wrap exactly the `push_back` call at each of
the four sites in `ScopedSuppressAllocationTracking` with a `// Heap:` comment naming the unbounded-collection
reason, because no finite reserve can be proven sufficient while `GrowPairedCollections` is unbounded. If they
do not, add nothing.

Nothing else changes. The buffer stays a `std::vector`, the guard keeps its `kError` + `DEBUG_BREAK()` shape,
and the reserve stays a single construction-time call. Capacity is not simulation state: `transferRequests` is
excluded from serialization, CRC, and equality (`Frame.h:117-118`), so no value in the deterministic stream
moves and `game::Frame::kiVersion` is untouched.

## Critical files

- `Engine/Source/Frame/FrameUtils.h` — `kuiInitialTransferCapacity` at `:69`, the sole capacity constant.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Blasters/Blasters.cpp` — capacity guard and
  `transferRequests.push_back(request)` at `:202-211`.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/Missiles.cpp` — same guard and push at
  `:375-384`.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/Spaceships.cpp` — same guard and push at
  `:409-434`.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersNavigation.cpp` — same guard and push at
  `:113-137`.
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp` — the sole `reserve` at `:38` and the sole `clear()` at
  `:142`, read as evidence of the shared per-tick budget; neither is expected to change.
- `Engine/Source/Memory/AGENTS.md` — the allocation-tracking and `ScopedSuppressAllocationTracking` contract
  that decision rule B is evaluated against.

## In scope

- The value of `engine::kuiInitialTransferCapacity` at `Engine/Source/Frame/FrameUtils.h:69`, changed only per
  decision rule A.
- The four duplicated "Heap realloc warning" guard comments named in `## Critical files`, corrected to state the
  measured per-tick expectation and to drop the now-fixed `DestroyElement` suspicion.
- `ScopedSuppressAllocationTracking` wrapping of exactly the four `transferRequests.push_back(request)` calls
  named in `## Critical files`, added only per decision rule B, each with a `// Heap:` comment.
- Any sentence in `Engine/Source/Frame/AGENTS.md` or
  `Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md` that states the per-tick transfer budget or the
  expected burst size, if such a sentence exists.

## Out of scope

- Bounding, capping, or rate-limiting entity counts per cell, and any change to
  `engine::GrowPairedCollections` or collection growth policy.
- Replacing `std::vector<TransferRequest>` with another container, moving it to `gpThreadLocal->mWorkbuffer`,
  hoisting it to `FramePostRenderBase`, or any restructuring of the transfer buffer's ownership.
- The `TransferRequest` payload layout, the harvest loop shape, `ComputeTransferDelta`, transfer validation,
  adjacency checks, transfer application, and the deferred injection queue.
- The guard's severity, its `kError` level, its log field list, and the `DEBUG_BREAK()` call itself.
- `DestroyElement`, `DestroySweep`, walk direction, and everything owned by
  `Documents/Plans/Frame/DestroyElementReverseWalkSkip.md`.
- Serialization, CRC membership, `game::Frame::kiVersion`, wire format, and save/replay compatibility.
- Consolidating the four duplicated guards into one shared helper.

## Risk tier and invariants

Change Workflow **Tier 2** (scoped behavior in one subsystem: the per-tick transfer buffer's capacity and
allocation-tracking treatment). The Tier-3 triggers are deliberately not reached, and this is the boundary
condition to re-check during implementation: `transferRequests` is documented at `Frame.h:117-118` as not
serialized and not in CRC or equality, so a reserve size cannot move the deterministic stream, and no
serialized layout, wire format, or replay compatibility surface is touched.

Escalate to Tier 3 and re-plan rather than widening the change if implementation finds that it must touch
serialized state, CRC membership, `kiVersion`, the per-cell parallel dispatch, or `GrowPairedCollections`.

Invariants to preserve:

- Client and server stay bit-identical: any capacity or suppression change applies to both builds, never one.
- The per-tick CRC sequence is unchanged by this Plan — a capacity change must be observably CRC-neutral.
- All four producers keep sharing one budget cleared once per tick; no producer gains its own buffer.
- No new heap allocation is introduced into the simulation loop; suppression only silences an allocation that
  the code cannot avoid, and only where the tracker actually observes it.

## Acceptance criteria

- The measured peak per-tick `transferRequests.size()` from step 1 is reported as a literal number, taken from
  a harness scenario in which more than ten entities of at least two different collection types cross the same
  cell boundary on the same tick, with the source cell's count for at least one of those collections reaching
  zero in that single pass.
- Under that same scenario, no "Transfer capacity hit" `kError` line appears from any of the four producers.
- A record-then-play replay determinism run on the changed build reports no checksum mismatch,
  `LogDifferences CRC Client`, or `CONFIRMED DESYNC` line, proving the change is CRC-neutral.
- If decision rule A changed the constant, the four guard comments state the measured expectation and no longer
  name `DestroyElement` as a suspected cause; if it did not, the comments are still corrected on the
  `DestroyElement` sentence and the measured peak is recorded in the completion evidence.
- If decision rule B added suppression, the scenario runs to completion in a `BT_DEBUG` build with a debugger
  attached without an allocation-tracking `DEBUG_BREAK()`; if it did not, the written justification cites the
  tracker contract that shows these sites are untracked.
- Client and server compile.
