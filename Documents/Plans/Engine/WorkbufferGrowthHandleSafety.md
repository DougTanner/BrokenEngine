<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-19T20:04:27.471Z","dependsOn":[]} -->
# Grow Workbuffer and thread-local scratch in place through a reserve-then-commit StableVector

## Context

`Workbuffer` hands out raw pointers into a `std::vector<std::byte>` it does not
own: `Data<T>()`, `Span<T>()`, and `View()` compute `mBuffer.data() + miBase` at
the moment of the call (`Common/Workbuffer.h:48,54,93,100`;
`Common/Workbuffer.cpp:75`), and `RawPushBuffer<T>` returns that address as the
pointer `ScopedWorkbufferAllocation<T>` caches in `mpData` for its lifetime
(`Common/Workbuffer.h:153-154,266-267,279`). `Grow` resizes the backing vector
(`Common/Workbuffer.cpp:78-95`), the `ThreadLocal`-owned `mWorkbufferMemory`
(`Common/Threading/ThreadLocal.h:59`; `Common/Threading/ThreadLocal.cpp:9,11`),
so the resize relocates the bytes and every handle, pointer, span, and view taken
before it points into freed storage. The code knows this: `Grow` calls
`DEBUG_BREAK()`, logs `kError` "live pointers invalidated" when a frame is open,
and resizes anyway so gameplay never fails; in Release that is a silent
use-after-free for every outer handle.

The same doubling-scratch pattern — a thread-local `std::vector` sized on first
use, a `ScopedSuppressAllocationTracking` guard, an overflow log, and a `resize`
that relocates — repeats at the collision, area-damage, and debug-render sites
named in `## In scope`. The collision and area-damage sites break on overflow
with `DEBUG_BREAK()` and a `kWarning` log; the debug-render site grows silently,
logging `kVerbose` with no break
(`Engine/Source/Graphics/Debug/DebugRender.cpp:47-49`). Each relocation is
harmless today only because nothing at those sites holds a pointer across it.

This design replaced an earlier offset-based-handle design, in which the handle
stored the frame's byte offset and recomputed its pointer per access and every
consumer that cached a raw pointer or span across a nested push was swept and
edited. Three independent alternative investigations converged on
non-relocating storage instead, and the reviews found the consumer sweep both
over-built and, at the `Islands::UpdateActiveIslands` site, incorrect.

`Engine/Source/Graphics/AnimationData.cpp:216-217,338` is the nested instance the
dependent Plan relies on: `EvaluateAnimation` holds a `PushBuffer<XMMATRIX*>`
reservation live across `EvaluateWorldMatrices`, which pushes a second one.
`Documents/Plans/Engine/OversizedSkinJointIndexMismatch.md:128,242` keeps both
reservations and the `EvaluateWorldMatrices` signature unchanged and depends on
this Plan for growth safety. That dependency stays correct: the storage change
below keeps the outer `XMMATRIX*` valid across the nested push with no edit to
`AnimationData.cpp`, so that Plan's growth-is-the-signal behavior holds as
written.

## Design

A new header-only `common::StableVector<T>` in a new `StableVector.h` under
`Common/` owns one `VirtualAlloc` address-space reservation and grows by
committing pages inside it, so its base address never changes and every
pointer, span, view, or handle into it stays valid across growth. Committing
pages with `VirtualAlloc(MEM_COMMIT)` inside address space that is already
reserved is the mechanism the lazy chunk pool's recommit path already uses
(`Engine/Source/File/PackChunks.cpp:1202`), made typed and growable.

Shape, limited to what the converted sites use:

- The constructor takes the instance's reserved byte count, rounds it up to the
  64 KiB allocation granularity, and stores it: it reserves nothing, because
  thread-local instances are constructed during `mi_process_init` and at every
  thread's start, so construction makes no OS call. Non-copyable and
  non-movable — its contract is that its bytes never move, and no converted site
  copies or moves one; a site that cannot pass the count as a constructor
  argument passes it through a default member initializer instead.
- `Resize(int64_t iCount)` grows only (`ASSERT(iCount >= Size())`) and returns
  at once when `iCount == Size()`. The first growing call reserves that byte
  count with `MEM_RESERVE`; every growing call then `ASSERT`s that
  `iCount * sizeof(T)` fits the reservation, commits `[0, iCount * sizeof(T))`
  with `MEM_COMMIT | PAGE_READWRITE` — re-committing an already committed page
  is a documented no-op, so no page bookkeeping is kept — and value-constructs
  the new elements in place. A null return from either `VirtualAlloc` call is
  an OS trust-boundary failure and `ASSERT`s.
- `Size()`, `Data()` (const and non-const), and `operator[]` (const and
  non-const, index `ASSERT`ed in range). A site that calls `std::vector::at`
  uses `operator[]`; the one `std::sort` call iterates from `Data()`.
- The destructor destroys the constructed elements and releases the reservation
  with `MEM_RELEASE`.

Reservation policy: each site passes its own reserved byte count, 64 times the
bytes its initial or documented pre-allocate size occupies, so growth past it is
a runaway rather than an under-size; a reservation charges no commit and no
memory until a page is touched. Per site:

- `ThreadLocal`'s workbuffer storage: the reserved byte count is a constructor
  parameter of its own, separate from the initial size, so reducing the initial
  size does not shrink the reservation. It defaults to `64 * iWorkbufferSize` —
  640 MiB for the 10 MiB main thread (`Engine/Source/Main.cpp:100`) and 4 MiB
  for a 64 KiB worker (`Common/Threading/Multithreading.cpp:15`).
- `Workbuffer::mSavedBase` and `mSavedSize`: 64 times 64 `int64_t` entries is
  32 KiB, so 64 KiB each after the granularity round-up.
- `ZonePair::indicesA` and `indicesB`: 64 times
  `kiCollisionZonePreallocate` `int64_t` entries, 1 MiB each.
- `sLayerPairZones`: 64 times `kiCollisionLayerPairPreallocate`
  `LayerPairZones`, about 4 MiB.
- `CollisionEventScratch::candidates`: 64 times
  `kiCollisionCandidatePreallocate` `CollisionCandidate`, about 3 MiB;
  `pendingResults`: 64 times `kiCollisionResultPreallocate`
  `PendingCollisionResult`, about 6 MiB.
- `sResultEntries` and `sResultSpans`: a fixed 64 MiB each rather than 64 times
  their pre-allocate constants, because each is first sized to
  `std::max(constant, actual)` from live data
  (`Engine/Source/Frame/Collision.cpp:437,474`), so a 64x reserve would be a
  hard per-thread ceiling of 65,536 entries and spans. At 64 MiB the ceilings
  are 838,860 entries (80-byte `CollisionResult`) and 4,194,304 spans (16-byte
  `CollisionResultSpan`), reachable only by pathological input.
- `sTestedBGeneration` has no pre-allocate constant of its own: it is sized
  lazily from the layer-B object count
  (`Engine/Source/Frame/Collision.cpp:668-673`), the same live-data shape as
  the two result arrays, so it takes the same fixed 64 MiB reserve — a ceiling
  of 16,777,216 objects (4-byte `uint32_t`), reachable only by pathological
  input.
- `sAreaDamageSources`: 64 times `kiAreaDamageSourcePreallocate`
  `AreaDamageSource` is 32 KiB, so 64 KiB after the round-up.
- `DebugRenderType::layouts`: 64 times `kiInitialDebugRender`
  `shaders::DebugRenderLayout`, about 512 MiB per type and about 2 GiB across
  the four types, on the client only.

Address-space total: the 2,048 zone index lists — the 8x8 zones times two index
lists times 16 pre-allocated layer pairs at
`Engine/Source/Frame/Collision.h:9-13` — dominate a simulation thread at about
2 GiB, its remaining scratch adding about 211 MiB — 192 MiB of that the three
fixed 64 MiB live-data reserves — and each other engine thread that owns a
`ThreadLocal` adds three instances. The main thread plus
`HardwareCoreCount() - 1` workers on the server, one fewer on the client, which
also reserves a core for the render thread (`Engine/Source/Main.cpp:139-144`),
reach about 71 GiB on a 32-thread machine, well under the 128 TiB x64 user
address space, with the client's debug-render reservations adding about 2 GiB
once. Growth past a reservation is impossible by construction and `ASSERT`s: no
fallback, no relocation. The one-time first-use cost is two `VirtualAlloc` calls
per instance, about 4,100 on a thread's first `Collision::SetupZones`.

A `VirtualAlloc` page never passes through the tracked `operator new`
(`Engine/Source/Memory/GlobalAllocator.cpp:37-70` hooks only the C++ allocation
operators), so every `ScopedSuppressAllocationTracking` guard and `// Heap:`
rationale at a converted site goes away, and the converted sites add no tracked
heap allocation.

`Workbuffer` switches `mBuffer` to a `StableVector<std::byte>&` and its two
frame-bookkeeping vectors `mSavedBase`/`mSavedSize` to `StableVector<int64_t>`.
`Grow` keeps its `DEBUG_BREAK()` and its `kError` log as the under-sizing signal
and keeps growing to twice the need; only the claim that live pointers are
invalidated goes, because none are. `ScopedWorkbufferAllocation<T>` keeps its
cached pointer, which now stays valid for the handle's lifetime; `Data<T>()`,
`Span<T>()`, `View()`, and every consumer are unchanged.

The same swap at each other thread-local doubling-scratch site keeps that
site's existing overflow `DEBUG_BREAK()` and log; a site that grows silently
today keeps growing silently. `DebugRender` keeps its GPU descriptor rebind:
`ResizeDynamicBufferIfNeeded`
(`Engine/Source/Graphics/Managers/BufferManager.cpp:475`) recreates the GPU
storage buffer when the instance count outgrows it, and the rebind follows that
GPU recreation, which is independent of the CPU staging vector's address — so
the rebind is still needed and stays as it is.

Behavior is otherwise identical: no value written to simulation state changes,
frames stay LIFO, and the 64 KiB-aligned reservation base keeps every frame
start 16-byte aligned through the existing round-up.

## Critical files

- `StableVector.h`, new, under `Common/`.
- `Common/Common.h:20,29` — the aggregation header; `StableVector.h` must
  precede `Common/Threading/Multithreading.h`, which reaches `Workbuffer.h` through
  `PersistentWorker.h` and `ThreadLocal.h`.
- `Common/Workbuffer.h:14-19,69-72,105-116,126-145,153,170,176-177` and
  `Common/Workbuffer.cpp:6-70,78-95` — the backing-storage type, the growth
  sites, the guards, and the comments and log that claim invalidation.
- `Common/Threading/ThreadLocal.h:39,59`,
  `Common/Threading/ThreadLocal.cpp:6,9` — the constructor signature and the
  sole construction site of the workbuffer storage.
- `Engine/Source/Frame/Collision.h:9-16,110-121`,
  `Engine/Source/Frame/Collision.cpp:41-55,66-93,101-114` — the collision
  scratch declarations and definitions, and the pre-allocate constants the
  reserves are derived from.
- `Engine/Source/Frame/AreaDamage.h:6,29`,
  `Engine/Source/Frame/AreaDamage.cpp:6-27`.
- `Engine/Source/Graphics/Debug/DebugRender.cpp:8,15,18-23,39-52,140-146`.
- `Common/AGENTS.md` `## Allocation-Free Scratch` and
  `## Headers, Validation, and Platform`, `Engine/Source/Frame/AGENTS.md`
  `## Architecture`, `Engine/Source/Graphics/Debug/AGENTS.md` `## Lifecycle` —
  the documented growth rules.
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj`
  and `DataPacker/Platforms/VisualStudio2026/DataPacker.vcxproj` with their
  `.filters` — the new header's membership, alongside `Workbuffer.h`.

## In scope

- New `StableVector.h` under `Common/`: add `common::StableVector<T>` with the
  shape, constructor reserved-byte count, and asserts stated in `## Design`;
  include it from `Common/Common.h` before
  `Common/Threading/Multithreading.h`; add it to both project files and their
  filters next to `Workbuffer.h`.
- `Workbuffer` (`Common/Workbuffer.h`): the constructor parameter and the
  `mBuffer` member become `StableVector<std::byte>&`; `mSavedBase` and
  `mSavedSize` become `StableVector<int64_t>`, each constructed with its
  `## Design` reserve and sized by `Resize(64)` in the constructor; `PushBack`,
  `RawPush`, `RawPushBuffer`, `Data`, and `Span` in the header and `Append`,
  `AppendFloat`, and `View` in `Common/Workbuffer.cpp` read
  `Size()` and `Data()`; `RawPush` and `RawPushBuffer` grow the two bookkeeping
  vectors with `Resize` and drop their `ScopedSuppressAllocationTracking` guard
  and `// Heap:` line; the comments at `Common/Workbuffer.h:21,109-110,131`
  describe the new storage.
- `Workbuffer::Grow` (`Common/Workbuffer.cpp:78-95`):
  `mBuffer.Resize(iNeededCapacity * 2)`; keep `DEBUG_BREAK()` and the `kError`
  log; reword the comments and the log text to say the buffer was under-sized
  and that outstanding pointers stay valid; drop the guard and `// Heap:` line.
- `ThreadLocal` (`Common/Threading/ThreadLocal.h:39,59`,
  `Common/Threading/ThreadLocal.cpp:6,9`): the constructor gains a trailing
  `int64_t iWorkbufferReserveSize = 0` parameter, `0` meaning
  `64 * iWorkbufferSize`, so no existing construction site changes;
  `mWorkbufferMemory` becomes `StableVector<std::byte>`, constructed with that
  reserved byte count and sized by `Resize(iWorkbufferSize)` in the
  constructor body; the comments at `Common/Threading/ThreadLocal.h:42,57` keep
  describing the aliasing.
- `Collision` (`Engine/Source/Frame/Collision.h:110-121`,
  `Engine/Source/Frame/Collision.cpp`): `ZonePair::indicesA`/`indicesB`,
  `CollisionEventScratch::candidates`/`pendingResults`, `sLayerPairZones`,
  `sResultEntries`, `sResultSpans`, and `sTestedBGeneration` become
  `StableVector`s, each carrying its `## Design` reserve — the four members of
  `ZonePair` and `CollisionEventScratch` through a default member initializer,
  because their owners are default-constructed, and the four `Collision`
  thread-local statics at their definitions
  (`Engine/Source/Frame/Collision.cpp:107-114`); `LayerPairZones`'s constructor
  sizes each index list with `Resize`; the first-use and growth sites at
  `Engine/Source/Frame/Collision.cpp:189-202`, `:281-286`, `:335-342`,
  `:370-383`, `:433-444`, `:470-481`, `:494-500`, `:626-632`, and `:668-673`
  call `Resize` and drop their guards and `// Heap:` lines, each keeping exactly
  the overflow signal it has today and gaining none — the `DEBUG_BREAK()` and
  `kWarning` log stay at `:189-202`, `:335-342`, `:494-500`, and `:626-632`, and
  the other five sites keep growing with no such signal; every `.at(` on these
  becomes `operator[]`, the `std::sort` at `:394` iterates from `Data()`, and
  `:690` and
  `:738` read `Data()` and `Size()`; the comments at `:51-52` and `:101-103`
  describe the new storage.
- `AreaDamage` (`Engine/Source/Frame/AreaDamage.h:29`,
  `Engine/Source/Frame/AreaDamage.cpp:6-27,39`): `sAreaDamageSources` becomes a
  `StableVector` carrying its `## Design` reserve at its definition
  (`Engine/Source/Frame/AreaDamage.cpp:6`); `Add` sizes and grows it with
  `Resize`, drops both guards and `// Heap:` lines, and keeps its
  `DEBUG_BREAK()` and `kWarning` log; `.at(` becomes `operator[]`.
- `DebugRender` (`Engine/Source/Graphics/Debug/DebugRender.cpp:15,39-52,140-146`):
  `DebugRenderType::layouts` becomes a `StableVector` whose default member
  initializer carries its `## Design` reserve, with the trailing `{}` dropped
  from each `sTypes` entry (`Engine/Source/Graphics/Debug/DebugRender.cpp:18-23`)
  so that initializer applies to a type that cannot be copied or moved;
  `AddLayout` sizes and grows it with `Resize`, drops both guards and
  `// Heap:` lines, and keeps its `kVerbose` growth log; `BeginRender` copies
  from `Data()` and keeps the `ResizeDynamicBufferIfNeeded` rebind unchanged.
- `Common/AGENTS.md` `## Allocation-Free Scratch`: rewrite the growth sentence —
  growth commits more of a fixed reservation in place, so handles, pointers,
  spans, and views stay valid across it; `DEBUG_BREAK()` and the `kError` log
  remain the under-sizing signal; growth past the reservation asserts.
  `## Headers, Validation, and Platform`: add `StableVector.h` to the reuse list
  with its one-line definition.
- `Engine/Source/Frame/AGENTS.md` `## Architecture`: the thread-local scratch
  bullet — the containers are `StableVector`s that reserve nothing until first
  use, grow in place without tracking suppression, and keep the overflow
  `DEBUG_BREAK` naming the constant to raise.
- `Engine/Source/Graphics/Debug/AGENTS.md` `## Lifecycle`: staging grows in
  place; the descriptor rebind follows GPU buffer recreation.

Every consumer of `Workbuffer` handles, `Data`, `Span`, and `View` recompiles
unchanged, `AnimationData::EvaluateAnimation` included.

## Out of scope

- The `ScopedWorkbufferAllocation<T>` class, including `Adopt<U>`
  (`Common/Workbuffer.h:256-264`); `ScopedWorkbufferArena`; `Push`/`Pop`
  accounting; `ShrinkLastPushBuffer`; the `Wb` wrappers and formatters.
- Any consumer-site change: no sweep of `PushBuffer<`, `Data<`, `Span<`, or
  `View()` callers, and no edit to `Engine/Source/Graphics/AnimationData.cpp`.
- Removing growth, changing the `2x` growth factor, removing an existing
  `DEBUG_BREAK()` or overflow log, or adding an overflow signal where a site
  grows silently today.
- Changing any initial size or pre-allocate constant, except the temporary,
  unlanded reduction of the `Engine/Source/Main.cpp:100` initial workbuffer size
  argument — with the reservation held at its production value — that
  `## Acceptance criteria` uses and restores before landing.
- `StableVector` features beyond `## Design`: no shrink or decommit, no copy or
  move, no iterators, no statistics, and no configuration surface other than the
  constructor's reserved byte count.
- `Engine/Source/Agent/Commands/ReplayFixtures.cpp:85,291-298`: its
  `TransferCaptureSnapshot` is copied by value
  (`Engine/Source/Agent/Commands/ReplayFixtures.cpp:119,131`,
  `Engine/Source/File/Replay.cpp:697`,
  `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationFixtures.cpp:123`)
  and nothing holds a pointer across its `push_back`, so the swap would add copy
  and move semantics to the type for no stability gain.
- `Collision::sLayers` (`Engine/Source/Frame/Collision.cpp:123-138`): fixed at
  compile time; overflow asserts rather than grows.
- Per-cell collection storage
  (`Engine/Source/Frame/Collections/CollectionMemory.h`): per-cell SOA columns
  whose layout is CRC-load-bearing, not thread-local scratch.
- Server and client steady-state singletons: sized once at startup and never
  grown in the main loop.
- ImGui vertex and index buffers: owned by Dear ImGui's own allocator, not by
  engine scratch.
- Every other `std::vector`, and the mimalloc arena.

## Risk tier and invariants

Tier 3. Trigger: a `Common/` shared utility consumed across Frame, Graphics,
Agent, and Network, with the conversion editing Frame simulation scratch and a
client debug renderer.

Preserve these invariants:

- Simulation values stay bit-identical: storage addressing only, no value
  written changes, so PostRender CRC and replay determinism are unaffected.
- Frames stay strictly LIFO and every frame starts 16-byte aligned: the
  reservation base is 64 KiB-aligned, so the existing round-up keeps the
  guarantee.
- No tracked heap allocation is added: `VirtualAlloc` bypasses `operator new`,
  and every converted site's suppression guard is removed rather than moved.
- An outstanding `ScopedWorkbufferAllocation` handle, pointer, span, or view
  stays valid across a grow.
- Growth past the reservation asserts; it never falls back or relocates.

Roles: `/compile` (client and server), `/agent-harness` for the runtime checks,
`/repo-code-review`, `/comment-review`, `/adversarial-review`,
`/update-claude-docs` followed by `/progressive-disclosure-review` for the
`AGENTS.md` edits, `/update-vcxproj` for the new header.

## Acceptance criteria

- With the `Engine/Source/Main.cpp:100` construction temporarily reduced to a
  small initial size while its reservation stays at the production value — for
  example an initial `4 * 1024` with `64 * 10 * 1024 * 1024` passed as the
  reserved byte count, so growth commits inside the shipped reservation instead
  of exceeding it — a harness client run logs the `Workbuffer::Grow` `kError`
  line at least once with a frame open and continues to gameplay with no crash,
  no assert, and no CRC mismatch — an outer handle's bytes read back intact
  across the grow; the construction is restored to `10 * 1024 * 1024` with the
  default reserve before landing.
- With the shipped sizes, a harness run reaches gameplay with no `DEBUG_BREAK`,
  assert, or `kError` from `Workbuffer`, `Collision`, `AreaDamage`, or
  `DebugRender`.
- A replay determinism check over a recorded session still matches.
- Client and server build clean through `/compile`; the new `StableVector.h` is
  a member of both project files and their filters.
- No `ScopedSuppressAllocationTracking` or `// Heap:` remains in
  `Common/Workbuffer.h`, `Common/Workbuffer.cpp`,
  `Engine/Source/Frame/AreaDamage.cpp`, or
  `Engine/Source/Graphics/Debug/DebugRender.cpp`, and the only pair left in
  `Engine/Source/Frame/Collision.cpp` is the out-of-scope `sLayers`
  pre-allocation guard at `Engine/Source/Frame/Collision.cpp:125-126`.

## Notes

Origin: user direction during the `/next-plan` run on
`Documents/Plans/Engine/OversizedSkinJointIndexMismatch.md` ("safe-by-default
with growth"), pre-existing at that session's baseline
`45d74910b4c9c2f0b80f06dc3c44c8c746f2ea36`. The non-relocating
reserve-then-commit mechanism, the set of converted sites, the assert on
exceeding the reservation, the retained under-sizing signals, and the tier are
user decisions; the type's name, its shape, and the per-site reservation
sizing are this Plan's choices.
