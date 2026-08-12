<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-11T00:00:10.000Z","dependsOn":[]} -->
# Close the float and repeated-group gaps in the engine message visitors

## Context

`engine::NetworkMessages::MessageWriter` (`Engine/Source/Network/NetworkMessages.h:18-87`) and `MessageReader` (`:89-209`) are the shared visitor pair every engine packet is written and read through. They cover `uint8_t`, `uint16_t`, `int32_t`, `uint32_t`, `int64_t`, `uint64_t`, `GridCoord`, payload blobs, null-terminated strings, and optional GUIDs. Two things are missing: there is no `float` field overload anywhere in the file, and there is no support for a bounded repeated group.

Because of those two gaps the game hand-rolled a parallel visitor pair for one message: `game::GameMessages::FleetSyncMessage::FleetWriter` (`Projects/BrokenEngineSandbox/Source/Network/GameMessages.h:85-127`) and `FleetReader` (`:129-171`) re-implement `Field(uint64_t)`, `Field(int64_t)`, `Field(uint8_t)`, and add `Float`, `Alive`, and `MemberCount`. `ReadPayload` (`:213-260`) then re-implements the bounded-count pattern by hand — read the wire count, reject a negative count or one exceeding `cursor.Remaining() / kiFleetHeaderSize`, resize, and recheck `cursor.Has(...)` before each element. `FleetReader`'s raw `Read*` calls carry no bounds checks of their own; every bound lives in `ReadPayload`.

That duplicated bounded-count logic is the security-relevant part: it is the defence against a hostile count, and having a second copy means a fix to one can miss the other. The only external callers are `FleetSyncMessage::WritePayload` (`ServerFleetSerialization.cpp:26`) and `ReadPayload` (`PlayerEvents.cpp:58`); both keep their signatures.

## Design

Three additions to `MessageWriter`/`MessageReader`, matching the existing style:

- `void Field(const float& rValue) const { mrWorkbuffer.PushBack<float>(rValue); }` on the writer, and `void Field(float& rValue) { Read(rValue, sizeof(rValue), ReadFloat); }` on the reader (`engine::ReadFloat`, `NetworkCursor.h:55`, is already visible via the existing include), so a malformed short packet invalidates the reader the same way every other field does.
- A bounded-count pair: writer `void BoundedCount(const int64_t& riCount, int64_t) const` (writes the count via `Field`; the item-size argument exists only for visitor symmetry), reader `void BoundedCount(int64_t& riCount, int64_t iItemMinSize)` — reads the count via `Field`, then invalidates when `riCount < 0` or `riCount > mCursor.Remaining() / iItemMinSize`, the divide chosen over a multiply to avoid hostile-count overflow. This is the shape `ReadPayload` implements today.
- Reader `bool AtEnd() const { return mCursor.Remaining() == 0; }`, needed so `ReadPayload`'s existing trailing-bytes rejection survives the rewiring.

Then delete `FleetWriter` and `FleetReader` and rewire `FleetSyncMessage` onto the engine visitors:

- `WritePayload` constructs `MessageWriter` over the workbuffer; the leading fleet count is emitted through `BoundedCount`. Byte output is identical; the `ASSERT` size accounting stays.
- `ReadPayload` constructs `MessageReader` over the payload span (no `Type()` call — the fleet payload carries no type byte; the type byte was consumed upstream). The outer loop becomes: `BoundedCount(iFleetCount, kiFleetHeaderSize)`, reject on `!IsValid()`, resize, then per fleet visit the header and members, checking `IsValid()` each iteration; the final line becomes `return reader.IsValid() && reader.AtEnd();`. The explicit `cursor.Has(kiFleetHeaderSize)` pre-checks are subsumed by the reader's per-field bounds checks.
- `VisitFleetHeader` replaces `rVisitor.MemberCount(riMemberCount)` with `rVisitor.BoundedCount(riMemberCount, kiFleetMemberSize)` and `rVisitor.Float(...)` with `rVisitor.Field(...)`. Note one internal difference: the member-count bound is now checked mid-header, before `iFlagshipIndex` and `fNavigationDelay` are consumed, so the ceiling is up to one member looser than today's post-header check; an over-by-one hostile count still fails when the member reads exhaust the cursor and invalidate the reader, so every malformed payload is rejected whole exactly as before, and the `resize` stays bounded by `Remaining() / kiFleetMemberSize`.
- `Alive` becomes a `uint8_t` conversion at the visit site: `VisitFleetMember` keeps its field order (global id, then alive byte) but takes the alive value as a deduced `uint8_t` reference parameter; `WritePayload` passes a converted local (`bAlive ? 1 : 0`), `ReadPayload` converts back to `bAlive` after the visit.

`VisitFleetHeader` and `VisitFleetMember` stay in the game — they describe this game's fleet layout — and only the visitor types they are instantiated with change. No file is added or deleted, so `/update-vcxproj` is not triggered.

## Critical files

- `Engine/Source/Network/NetworkMessages.h` — the `float` overloads, `BoundedCount` pair, and `AtEnd`
- `Projects/BrokenEngineSandbox/Source/Network/GameMessages.h` — deletions and rewiring

## In scope

- Adding the `float` field overload to both engine visitors
- Adding the `BoundedCount` pair and the reader's `AtEnd` accessor
- Deleting `FleetWriter` (`GameMessages.h:85-127`), `FleetReader` (`:129-171`), and the hand-rolled bounded checks inside `ReadPayload` (`:213-260`), rewiring `FleetSyncMessage`, `VisitFleetHeader`, and `VisitFleetMember` onto the engine visitors as specified in Design
- Any `Engine/Source/Network/AGENTS.md` or game `Network/AGENTS.md` sentence that names the old owner

## Out of scope

- The fleet wire layout: `kiFleetCountSize`, `kiFleetHeaderSize`, `kiFleetMemberSize`, their `static_assert`s, and the field order in `VisitFleetHeader`/`VisitFleetMember` all stay exactly as they are
- The fleet-specific validation that is not a count bound — the `iFlagshipIndex` range checks stay in `ReadPayload`
- The signatures of `WritePayload` and `ReadPayload` and their two callers
- Any other engine message, and any new field type beyond `float`
- `Documents/Architecture/Network.md`'s protocol description, which does not change because no byte moves

## Risk tier and invariants

Tier 3 — wire format code and a trust boundary against hostile input. Invariants: the emitted byte stream is identical before and after, so an old client and a new server interoperate; the bounded-count check keeps using division rather than multiplication; earlier elements consume payload, so overrun past the cursor is impossible — enforced now by the reader's per-field `Has` checks instead of the per-header pre-check; a reader that runs out of bytes sets its invalid flag rather than reading past the cursor, and a payload invalid at any point is rejected whole (`ReadPayload` returns `false`, the caller discards); `ReadPayload` still rejects any trailing bytes; no `resize` can exceed `Remaining()` divided by the element's minimum wire size.

## Acceptance criteria

- Client and server compile; `FleetWriter` and `FleetReader` no longer exist.
- A byte-level comparison of a `FleetSyncMessage` payload produced before and after the change is identical for the same fleet state.
- Fault injection — negative count, count larger than the remaining payload, truncated final member, trailing garbage — is rejected exactly as before, with no read past the cursor.
- A harness run with fleets created, joined, and deleted shows client fleet state matching the server.

## Scores

Effort 1 / Impact 2 / Risk 1
