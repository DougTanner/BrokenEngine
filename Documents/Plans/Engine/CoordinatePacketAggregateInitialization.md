<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T18:24:19.372Z","dependsOn":[]} -->
# Initialize coordinate packet aggregates from their base fields

## Context

`Server::WriteBufferedFramePacket` in `Engine/Source/Network/Server/ServerSend.cpp` prepares a designated `NetworkMessages::CoordUpdateFields fields` value. Each of its update and resend branches then constructs an empty derived message, casts that message to `CoordUpdateFields&`, and assigns `fields` before serialization. Initializing the base directly removes two casts and two separate assignments while keeping the prepared field names visible.

`ServerCoordUpdateMessage` and `ServerCoordResendMessage` in `Engine/Source/Network/NetworkMessages.h` each have one public nonvirtual base, `CoordUpdateFields`, no direct instance members, and no declared constructors. Their static constants and visitor functions do not add aggregate elements. The base contains scalar fields and `PacketPayload`, which contains only a pointer and size; all copies and assignments here are implicit memberwise operations. C++17 permits direct bases as aggregate elements and initializes them from the corresponding initializer clause ([WG21 P0017R1, technical specification](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/p0017r1.html)).

## Design

In `Server::WriteBufferedFramePacket`, replace the update branch's empty declaration and subsequent base assignment with:

```cpp
NetworkMessages::ServerCoordUpdateMessage message {fields};
```

Replace the equivalent resend pair with:

```cpp
NetworkMessages::ServerCoordResendMessage message {fields};
```

Keep the existing designated initializer of `fields`, both branch conditions, and both `NetworkMessages::Write(rWorkbuffer, message)` calls exactly as they are. Keep the locals mutable because `Write` takes a mutable message reference. Add no constructors, helper functions, casts, temporary base values, or message-definition changes.

Append this narrow exception to `Documents/C++StyleGuide.txt` rule 42, preserving its existing designated-initializer rule and example: "Exception: A derived aggregate with a single public nonvirtual base and no direct instance members may initialize that base from an existing value of the base type: Derived message {fields};. Keep designated initialization for the prepared base value."

## Critical files

- `Engine/Source/Network/Server/ServerSend.cpp`: `Server::WriteBufferedFramePacket`, implementation edits only.
- `Documents/C++StyleGuide.txt`: rule 42, documentation edit only.
- `Engine/Source/Network/NetworkMessages.h`: read-only proof of `PacketPayload`, `CoordUpdateFields`, both derived message types, `Write`, and `MessageWriter` behavior.

## In scope

- Exactly the two message declaration/base-assignment pairs within `Server::WriteBufferedFramePacket`.
- The single rule 42 exception described above.

## Out of scope

- `Server::BufferFrame`, `Server.cpp`, any other packet construction sites, and broad aggregate modernization.
- Changes to message declarations, member defaults, visitors, packet identifiers, field ordering, protocol versions, Frame versions, payload ownership, ring buffering, or client code.
- New abstractions, compatibility handling, tests, runtime instrumentation, benchmarks, and unrelated style cleanup.

## Risk and invariants

Tier 1: a local behavior-preserving initialization substitution plus its narrow style policy exception. This does not change a wire, serialization, layout, trust, or deterministic-state surface.

Preserve every `CoordUpdateFields` value: load generation, slot index, epoch, tick, echoed timestamp, shared CRC, and the compressed payload pointer and size. The original empty initialization is immediately overwritten in full; direct base copy initialization produces the same values without invoking custom copy behavior. Neither branch adds derived instance state.

Preserve packet type selection and `Visit` order: type byte, load generation, slot index, epoch, tick, echoed timestamp, shared CRC, payload size, then the same payload bytes. `Write` calls the derived type's `Visit`; `MessageWriter` emits individual fields and synchronously appends the payload. Object padding is never serialized, so this plan promises identical wire bytes, not identical unspecified padding. Payload storage and lifetime remain owned by the existing buffered frame. No protocol or Frame version bump is needed.

## Performance

Both forms perform a shallow memberwise copy of the same scalar fields and payload view. Direct initialization eliminates the source-level empty initialization and later assignment, introduces no allocations or indirection, and leaves serialization calls unchanged. Optimized performance is expected to remain neutral; no timing claim or benchmark gate is necessary for this mechanical substitution.

## Documentation and style

Rule 42 currently requires designated struct initializers without covering base-subobject initialization. Its exception is necessary because the derived aggregate cannot designate its inherited fields as direct members. Retain designated initialization of `fields`; do not generalize the exception to positional member initialization or multiple bases. No AGENTS or network architecture change is needed because all documented transport and lifetime contracts remain unchanged. Run the workflow's applicable C++ style and documentation reviews against this scope; do not duplicate rule 42 in skill prose.

## Verification

During implementation, build the server through `/compile` using its documented serialized build workflow; both branches must compile in the server target. Do not run a build during plan authoring.

Review the final diff and unchanged message definitions to establish that each aggregate still has exactly the base described above, that the base and payload have no custom copy behavior, and that the designated source value, branches, and `Write` calls are unchanged. Trace both derived `Visit` functions through `CoordUpdateFields::VisitFields` and `MessageWriter` to confirm the wire field sequence and payload bytes remain identical. This source proof and server build are sufficient; no harness run or new unit tests are required.

## Acceptance criteria

| Criterion | Evidence |
|---|---|
| Both branches initialize their derived message directly from `fields`, with no base casts or separate assignments. | Scoped diff of `Server::WriteBufferedFramePacket`. |
| Every transmitted field, type, field order, payload pointer/size, and write call remains unchanged. | Scoped diff plus unchanged message/visitor and memberwise-copy inspection. |
| Message layouts, identifiers, versions, buffered-frame ownership, and deterministic state remain unchanged. | No edits to their definitions or producers; only the two specified statement pairs change in C++. |
| Rule 42 allows precisely the single-base, no-direct-instance-member case while retaining designated base initialization. | Rule 42 diff. |
| Both update and resend initializations are accepted by the repository toolchain. | Successful server build via `/compile`. |
| No expansion into `BufferFrame` or other construction sites occurs. | Final diff contains only the two named edit regions and rule 42. |
