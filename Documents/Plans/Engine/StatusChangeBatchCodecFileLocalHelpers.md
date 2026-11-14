<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:39:51.200Z","dependsOn":[]} -->
# Make the uncompressed status-change batch codec file-local

## Context

Paths are under `Engine/Source/Network/` unless they start elsewhere. Verified
against commit `10d84ca6`; the lines cited are those at commit `c675b6b2`.

`SerializeStatusChangeBatch` and `DeserializeStatusChangeBatch` are written
as public entry points, but each is the helper of one function in its own
file. Three things follow, and a fourth observation was decided as a
non-change.

- Exported without an outside caller. They are declared at
  `NetworkSerialization.h:46` and `:49`; the only calls are
  `NetworkSerialization.cpp:146` and `:173`. Outside callers use only
  `CompressStatusChangeBatch` (`Server/ServerBufferedFrames.cpp:65`) and
  `DecompressStatusChangeBatch` (`Client/Client.cpp:608`).
  `Documents/C++StyleGuide.txt` rule 66 gives file-local functions `static`.
- The serialized-size bound is derived twice. `NetworkSerialization.cpp:139-140`
  uses `kiTypeCount * 3 + iCount * kiMaxStatusChangeBytesPerItem`;
  `MaxSerializedStatusChangeBatchBytes` (`NetworkSerialization.h:27-30`) uses
  `iCount * (kiMaxStatusChangeBytesPerItem + 3)`. Both are valid upper bounds
  today, because a group header is written only for a non-empty group
  (`NetworkSerialization.cpp:18-21`). `kiMaxCompressStatusChangeWorkbufferBytes`
  (`NetworkSerialization.h:34`) bounds the Workbuffer high-water
  (`Server/ServerBroadcaster.cpp:120`) with the header formula while the
  reservation it bounds uses the other one. No overflow is reachable; this is
  drift risk.
- A guard for a state the caller already handled. The zero-count return at
  `NetworkSerialization.cpp:66-69` cannot run: the only caller returned at
  `:132-135` for an empty batch. Without the guard an empty span still
  returns 0, since no group is emitted (`:82-87`).
- No build guards, which stays. `NetworkSerialization.cpp` is compiled by
  both executables with no `BT_CLIENT` or `BT_SERVER` guard, though encode
  has only a server caller and decode only a client caller; the source says
  "Only the client decodes a batch" (`:94-95`). Root `AGENTS.md`
  `## Client/Server Targets` says to guard client/server-only code at the
  narrowest practical scope. The user chose in the authoring session to add
  no guard: `AGENTS.md` `## Ownership` names `NetworkSerialization` as owner
  of the whole codec, and `NetworkMessages.h` keeps its paired writer and
  reader unguarded in both builds, so guards would split a mirrored pair.

Originating gap: the architecture phase (module cohesion, and client/server
and data-shape lenses) and the in-function refactoring phase of an analysis
of `Engine/Source/Network` at that commit. Unmet criteria: the two
uncompressed functions have internal linkage and no header declaration; the
serialized reservation and the published bound come from one function; no
check exists for a state no caller produces.

## Design

Three edits:

- Give both functions `static` (`NetworkSerialization.cpp:62`, `:90`) and
  remove their declarations and comments (`NetworkSerialization.h:44-49`).
- Replace `NetworkSerialization.cpp:139-140` with a call to
  `MaxSerializedStatusChangeBatchBytes(iCount)` and drop `kiMaxGroupHeaders`.
  This only changes the size of a scratch reservation that the high-water
  constant already assumes.
- Delete the zero-count guard at `NetworkSerialization.cpp:66-69`.

The encode-only helpers and constant (`SerializeGroup` at `:14`,
`kiTypeCount` at `:37`, `GroupIndicesByType` at `:39`) are not touched; the
decode half (`:90-126`) uses none of them. The wire format does not change,
so `kiProtocolVersion` stays.

## Critical files

- `Engine/Source/Network/NetworkSerialization.cpp`
- `Engine/Source/Network/NetworkSerialization.h`

## In scope

- `SerializeStatusChangeBatch`: linkage, declaration, and the zero-count
  guard.
- `DeserializeStatusChangeBatch`: linkage and declaration.
- `CompressStatusChangeBatch`: the `iMaxSerializedSize` computation only.

## Out of scope

- `BT_CLIENT` and `BT_SERVER` guards around the encode half, the decode
  half, or their declarations: decided against in the authoring session, so
  the pair stays unguarded together.
- The batch wire format, `GroupIndicesByType`, `SerializeGroup`, and the LZ4
  framing.
- The values of `kiMaxStatusChangeBytesPerItem`,
  `kiMaxCompressStatusChangeWorkbufferBytes`, and the compressed bound.
- Both outside callers, and project membership of the two files.
- The `Max` and `Maximum` spellings of these names.

## Risk tier

Tier 2 — scoped behavior. Trigger: it removes two public declarations and
changes the size of one server scratch reservation, which Tier 1 excludes.
No determinism/CRC, wire, serialization-format, version, or threading
exposure.

## Acceptance criteria

1. The two uncompressed functions are `static` and absent from the header;
   `CompressStatusChangeBatch` sizes its reservation with
   `MaxSerializedStatusChangeBatchBytes`; the zero-count guard is gone.
2. The BrokenEngineSandbox client and server and ModelViewer, which includes
   the header through `Engine.h`, build through `/compile`.
3. Through `/agent-harness`, a client connected to a local server keeps
   applying status-change batches with no CRC mismatch. The originating
   evidence records no harness setup; the executing session sets it up from
   the harness documentation.

## Coordination

No directional dependencies or mandatory nondirectional coordination.
