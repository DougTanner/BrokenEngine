<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-19T20:04:27.471Z","dependsOn":[]} -->
# Make Workbuffer handles survive a growth reallocation

## Context

`Workbuffer` hands out raw pointers into a `std::vector<std::byte>` it does not
own: `Data<T>()`, `Span<T>()`, and `View()` all compute `mBuffer.data() + miBase`
at the moment of the call (`Common/Workbuffer.h:48,54,93,100`;
`Common/Workbuffer.cpp:75`), and `RawPushBuffer<T>` returns
`mBuffer.data() + miBase` as the value `ScopedWorkbufferAllocation<T>` stores in
`mpData` at construction and returns from `operator T()` and `operator->()`
forever after (`Common/Workbuffer.h:127-155,266-267,271-280,291-295`).

`Grow` resizes that backing vector (`Common/Workbuffer.cpp:78-95`). The vector is
the `ThreadLocal`-owned `mWorkbufferMemory` (`Common/Threading/ThreadLocal.h:57,64`;
`Common/Threading/ThreadLocal.cpp:11`), so the resize reallocates and every live
handle, span, and view taken before it points into freed storage. The code
already knows this: `Grow` calls `DEBUG_BREAK()` and, when a frame is open, logs
at `kError` that "live pointers invalidated" — and then resizes anyway so
gameplay never fails. In Release, where `DEBUG_BREAK` does not stop anything, a
grow under an open frame is a silent use-after-free for every outer handle.

Safety today therefore rests entirely on caller discipline: `Common/AGENTS.md`
`## Allocation-Free Scratch` states only "Growth calls `DEBUG_BREAK()`: size the
buffer before the hot path instead of relying on growth". Nothing in the class
prevents a nested `Push`/`PushBuffer`/`Append` from growing the buffer while an
outer handle is live, and the repository has roughly 99 handle sites across
`Common/`, `Engine/`, and `Projects/` (grep `PushBuffer<`,
`ScopedWorkbufferArena`, `ScopedWorkbufferAllocation`) plus ten `Data<`/`Span<`
call sites, several of them nested.

`Engine/Source/Graphics/AnimationData.cpp:216-217,338` is a concrete nested
instance: `EvaluateAnimation` holds a `PushBuffer<XMMATRIX*>` reservation live
across `EvaluateWorldMatrices`, which pushes a second reservation. The skeleton
Plan sizes both reservations in place, keeps the nested pair, and depends on this
Plan for growth safety, so this Plan owns that site.

Pre-existing at session baseline `45d74910b4c9c2f0b80f06dc3c44c8c746f2ea36`, and
outside the implementation boundary of the Plan that session claimed
(`Documents/Plans/Engine/OversizedSkinJointIndexMismatch.md`, whose `## In scope`
removes the three artificial skeleton limits across `Common/DataFile.h`,
`Engine/Source/Graphics/AnimationData.cpp`, and DataPacker, widens
`MaterialInfo::uiJointCount`, bumps three format versions, and re-exports the
four tracked `.MODEL` intermediates).

## Design

The user directed the outcome: "fix the Workbuffer class so that it is
safe-by-default with growth". Growth must stay — it is the recovery path that
keeps gameplay running after an under-size — so the fix is to stop handles from
caching an address that a growth can invalidate.

Decided, at the user's direction: the durable handle becomes offset-based.
`ScopedWorkbufferAllocation<T>` stores the frame's base byte offset instead of
`mpData` — replacing the raw pointer, not added beside it — and
`operator T()`/`operator->()` recompute
`reinterpret_cast<T>(mBuffer.data() + offset)` on each access, so a grow between
construction and use resolves through the current backing storage.
`ScopedWorkbufferAllocation::Adopt<U>` keeps the same offset and only changes the
pointer type. `Data<T>()`, `Span<T>()`, and `View()` already recompute from
`mBuffer.data()` per call and need no change; they stay point-in-time results
whose returned pointer, span, or string_view must not be held across anything
that can push or append.

That alone is not sufficient, because a caller that converts a handle to a raw
`T*` (or a span) once and then keeps using it across a nested push still holds a
stale address — `AnimationData::EvaluateAnimation` passing `pWorldMatrices` into
`EvaluateWorldMatrices` is exactly that shape. So the change also audits every
site that caches a raw pointer or span obtained from a handle across a possible
push or append, and either re-fetches through the handle at each use or records
in a comment why no push can occur in that region.

Keep the existing `DEBUG_BREAK()` and the `kError` log in `Grow`: an under-sized
buffer is still a sizing bug worth flagging. Once handles are offset-based, the
log line's "live pointers invalidated" claim is no longer accurate for handles
and must be reworded to describe what does remain invalidated (previously
obtained raw pointers, spans, and views), and the `Common/AGENTS.md`
`## Allocation-Free Scratch` wording updated to match.

An alternative considered and rejected: forbidding growth outright (throw or
fail-fast in `Grow`). It is plainly simpler, but it contradicts both the user's
"safe-by-default with growth" wording and the existing "growth still proceeds so
gameplay never fails" policy the class states in three places.

`Span<T>()` and `View()` keep their point-in-time shape. No consumer gets a
durable span: each named site below either re-derives the span or pointer after
the nested reservation, passes the owning handle or arena down so the callee
re-derives it, or copies the bytes out.

## Critical files

- `Common/Workbuffer.h:44-101,126-155,230-295` — `Data`/`Span` accessors,
  `RawPushBuffer`, and the `ScopedWorkbufferAllocation<T>` handle that caches
  `mpData`.
- `Common/Workbuffer.cpp:78-95` — `Grow`: the reallocation, the `DEBUG_BREAK`,
  and the `kError` message whose wording depends on the fix.
- `Common/Threading/ThreadLocal.h:57,64`, `Common/Threading/ThreadLocal.cpp:11` —
  the owning backing vector that `Grow` reallocates.
- `Engine/Source/Graphics/AnimationData.cpp:216-217,338` — the nested
  reservation that demonstrates the raw-pointer-across-push shape.
- `Common/Log/LogFormatters.h:77,97,287,307,344,385` — the six formatters
  (`std::wstring`, `std::filesystem::path`, `Wb`, `WbV2`, `WbV3`, `WbV4`) that
  make an ordinary `LOG` a nested workbuffer push.
- `Common/AGENTS.md` `## Allocation-Free Scratch` — the documented growth rule.

## In scope

- `ScopedWorkbufferAllocation<T>`: replace the cached `mpData` member with the
  frame base offset, recompute the pointer in `operator T()` and
  `operator->()`, and carry the offset through the move constructor and
  `Adopt<U>`.
- `Workbuffer::RawPushBuffer<T>` and `Workbuffer::PushBuffer<T>`: supply that
  offset to the handle in place of the raw pointer; the handle stores no
  pointer.
- `Workbuffer::Grow`: reword the `kError` message to state accurately what a
  grow invalidates after the handle change; keep the `DEBUG_BREAK` and the
  resize behavior.
- `Common/AGENTS.md` `## Allocation-Free Scratch`: rewrite the growth sentence —
  growth no longer invalidates a live `ScopedWorkbufferAllocation` handle,
  `DEBUG_BREAK()` remains the under-sizing signal, and a raw pointer, span, or
  view taken out of the workbuffer stays point-in-time.

The grep over `PushBuffer<`, `ScopedWorkbufferAllocation`, `.Data<`, `.Span<`,
and `.View()` across `Common/`, `Engine/`, `Projects/`, and `Tools/` returns 89
lines, 24 of them inside `Common/Workbuffer.h` itself. Of the 65 consumer lines,
the twelve below cache a raw pointer, span, or view across a nested
`Push`/`PushBuffer` or a workbuffer-formatting `LOG`, so an offset-based handle
alone does not make them safe and each must change:

- `Engine/Source/Graphics/AnimationData.cpp:216-217`
  `AnimationData::EvaluateAnimation` — the handle-derived `XMMATRIX*` is read
  inside `EvaluateWorldMatrices`, which pushes at `:338`. Take
  `const common::ScopedWorkbufferAllocation<XMMATRIX*>&` in
  `EvaluateWorldMatrices` — declared at `AnimationData.h:13`, defined at
  `AnimationData.cpp:329`, and called only by `AnimationData::EvaluateAnimation`
  at `AnimationData.cpp:217` — so the signature change edits all three sites and
  every world matrix resolves through the offset after the nested reservation.
- `Engine/Source/Audio/StaticVoices.cpp:305` `StaticVoices::PriorityPass` — the
  `PriorityEntry* pPriority` alias taken at `:306` is read at `:334-335`, across
  the `common::Wb` logs at `:381` and `:389`. Delete the alias, index through the
  handle, and convert it to a raw pointer only inside the `std::sort` call
  arguments, which is why the alias exists.
- `Engine/Source/Graphics/Islands.cpp:208-212` `Islands::UpdateActiveIslands` —
  the four `uint32_t*` aliases are read across `IslandTerrain::AcquireTextureSlot`
  (`:258`), whose first-mint path pushes through
  `Engine/Source/Frame/IslandTerrainResidency.cpp:157` ->
  `FirstMintTextureSlot` (`:94`) -> `CreateElevationTextureFromHeightmap`
  (`:115`, defined at `:66`) -> `Texture::Create`
  (`Engine/Source/Graphics/Objects/Texture.cpp:169`) -> its `VkName` at `:208`
  -> `VkNameImpl`'s `Push`/`Append`
  (`Engine/Source/Graphics/GraphicsUtils.cpp:77-80`), which is live wherever
  `kbVulkanDebugLayers` is true (Debug).
  Replace the four aliases with accessors that recompute
  `static_cast<uint32_t*>(puiPerTemplateScratch) + N * miTemplateCount` at each
  use, and restate the `:203-206` comment, which reasons only about taking the
  pointers after one reservation.
- `Engine/Source/Graphics/Islands.cpp:181` `Islands::UpdateActiveIslands` — take
  `const common::ScopedWorkbufferArena&` instead of
  `std::span<const GridCoord> rActiveCoords` and re-derive
  `Span<const GridCoord>()` after the `:208` reservation; the caller's span is
  otherwise taken before that push.
- `Engine/Source/Graphics/Objects/PipelineDescriptorWriter.cpp:479-487`
  `PipelineDescriptorWriter::Write` — `DescriptorWriteCursor::pImageInfos`
  caches the raw pointer across `WriteModelDescriptor`'s nested workbuffer use.
  Hold a reference to the handle in the cursor and resolve each image-info write
  through it; restate the `:472-478` comment, which documents the reliance on a
  stable pointer.
- `Engine/Source/Network/Client/ClientSessionRuntime.cpp:539`
  `ClientSessionRuntime::SynchronizeSubscriptions` — `pDesiredCoords` is read
  inside `UnsubscribeStaleCoords`' loop, which sends through
  `NetworkManager::SendSimplePacket` (`Engine/Source/Network/NetworkManager.h:49`)
  and pushes. Pass `const common::ScopedWorkbufferArena&` to
  `UnsubscribeStaleCoords` and `BuildSubscriptionQueue` and re-derive
  `Data<GridCoord>()` at each use inside their loops.
- `Engine/Source/Network/NetworkSerialization.cpp:255-259`
  `SerializeStatusChangeBatch` — `pCursor` is derived from `pDest` before the
  grouping reservation at `:259` and written through afterwards. Change the
  third parameter (`NetworkSerialization.h:36`, one caller) to
  `const common::ScopedWorkbufferAllocation<uint8_t*>&` and derive `pCursor`
  after that reservation.
- `Engine/Source/Network/NetworkSerialization.cpp:401-407`
  `CompressStatusChangeBatch` — `pSerialized` is taken before
  `SerializeStatusChangeBatch`, which pushes. Pass `serializedAllocation` down
  and derive `pSerialized` only afterwards, for the `LZ4_compress_default` read.
- `Engine/Source/Network/Server/ServerBroadcaster.cpp:154-157`
  `ServerBroadcaster::BuildTickPublication` — the three publication pointers are
  read across the nested status-change compression at
  `NetworkSerialization.cpp:401`. Keep the pointers and the high-water pre-grow
  at `:148-151`, which is what makes them safe, and restate the `:149` and `:153`
  comments against the new handle contract: the pre-grow is now the site's own
  reason, not the class's.
- `Engine/Source/Agent/Commands/ClientNetworkFixtures.cpp:51`
  `ReceiveSubscribeAccept` — the `View()` of the built packet is read by
  `Client::Receive`, whose handlers push at
  `Engine/Source/Network/Client/ClientReceive.cpp:224,248,551,568`. Copy the
  built bytes into a local `std::vector<uint8_t>` and pass that span.
- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ClientSubscriptionFixtures.cpp:283`
  `CommandClientCancelledSubscriptionFixture` — same `Span<uint8_t>`-into-`Receive`
  shape; same copy-out fix.
- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerFaultFixtures.cpp:259`
  `CommandServerPreHandshakeAckFixture` — same `View()`-into-`Receive` shape;
  same copy-out fix.
- `Projects/BrokenEngineSandbox/Source/Game.cpp:296` `Game::UpdateActiveIslands`
  — pass `subscribedArena` itself to the new `Islands::UpdateActiveIslands`
  signature above instead of `Span<const engine::GridCoord>()`, and restate the
  `:284-286` comment, which reasons only about LIFO pop order.

Every other handle user recompiles unchanged: the remaining 53 consumer lines
either access the handle itself at each use, or take a pointer, span, or view
and consume it with no push or append in between.

## Out of scope

- Removing growth, changing the resize policy or the `2x` growth factor, or
  changing the initial workbuffer size.
- Removing the `DEBUG_BREAK()` in `Grow`, `RawPush`, or `RawPushBuffer`.
- The `ScopedWorkbufferArena` implementation, `Push`/`Pop` frame accounting,
  `mSavedBase`/`mSavedSize` nesting depth, and `ShrinkLastPushBuffer` semantics.
  Passing an existing arena by reference to a callee named in `## In scope`
  changes none of those.
- Changing the `Workbuffer` public API shape beyond the handle internals named
  above, converting callers to a different scratch mechanism, or reducing the
  number of nested reservations in any caller for its own sake.
- The `std::formatter` specializations and the `Wb`/`WbV2`/`WbV3`/`WbV4`
  wrappers.

## Risk tier and invariants

Expected Change Workflow Tier 3. Trigger: the change spans independently owned
subsystems — a `Common/` shared utility whose handle contract is consumed from
Graphics, Network, Profile, File, Audio, Frame, Ui, and game code — and the
caller audit edits several of those subsystems.

Preserve these invariants:

- Frames stay strictly LIFO and every frame still starts 16-byte aligned, so
  typed SIMD scratch remains safe.
- Simulation results stay bit-identical: this is a storage-addressing change
  only, with no change to any value written, so PostRender CRC and replay
  determinism are unaffected.
- No heap allocation is added to tracked paths; the only allocation remains the
  existing suppressed under-size recovery resize.
- A grow that happens while a frame is open no longer invalidates a live
  `ScopedWorkbufferAllocation` handle.

## Acceptance criteria

- With a deliberately under-sized workbuffer, a nested push that grows the
  buffer leaves an outer `ScopedWorkbufferAllocation` handle resolving to the
  current backing storage and the data it wrote intact.
- No site in `Common/`, `Engine/`, or `Projects/` holds a raw pointer or span
  taken from a handle across a push or append without a comment stating why no
  push can occur there.
- Client and server build clean through `/compile`, and a harness run reaches
  gameplay with no new `DEBUG_BREAK`, assert, or `kError` from `Workbuffer`.
- A replay determinism check over a recorded session still matches.

## Notes

Origin: user direction during the `/next-plan` run on
`Documents/Plans/Engine/OversizedSkinJointIndexMismatch.md`. Both the outcome
("safe-by-default with growth") and the offset-based handle that delivers it are
user decisions.
