<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T18:18:06.181Z","dependsOn":[]} -->
# Express replay activation-record size from its serialized fields

## Context

`Replay::SaveLoadReplay` in `Engine/Source/File/Replay.cpp:465` bounds the activation-record count before reserving storage with `common::ValidateDeserializedCount(iCoordinateCount, 16, manifestStream, "ReplayManifest records")`. The literal conceals the fieldwise serialized contract.

The same file declares `ReplayManifestRecord::iActivationTick` as `int64_t` (line 58). `Engine/Source/Frame/GridCoord.h:8–9` declares `iX` and `iY` as `int32_t`. The manifest payload writer appends these three fields individually and in that order (`Replay.cpp:144–149`); the reader consumes the same fields (`Replay.cpp:474`). The integral encoding loops consume exactly `sizeof(TYPE)` bytes per field (`Replay.cpp:28–51` and the payload writer). Thus the record occupies 8 + 4 + 4 = 16 bytes, irrespective of struct padding.

`Engine/Source/Frame/Alignments.cpp:96` already expresses a fieldwise record bound using qualified non-static member names in `sizeof`. This C++11 facility fits the existing C++23 codebase without introducing a convention.

## Design

Replace only the second argument of the activation-record `ValidateDeserializedCount` call with:

```cpp
sizeof(ReplayManifestRecord::iActivationTick) + sizeof(GridCoord::iX) + sizeof(GridCoord::iY)
```

Keep the call at its existing location before the empty-record check and reserve. Retain its other arguments and diagnostic text. Use surrounding formatting; introduce no named constant, object, helper, cast, assertion, or explanatory comment. `sizeof` does not evaluate or construct these members, and the sum is the same compile-time constant 16 passed to the helper's `int64_t iElementBytes` parameter. There is no added runtime work.

Do not substitute `sizeof(ReplayManifestRecord)` or `sizeof(GridCoord)`: the stream encodes fields, not object padding.

## Critical files

- `Engine/Source/File/Replay.cpp`: edit the activation-record size argument in `Replay::SaveLoadReplay`; record declaration, payload writer, reader, and format version are read-only evidence.
- `Engine/Source/Frame/GridCoord.h`: read-only coordinate member types.
- `Common/Serialization.h:31–36`: read-only `ValidateDeserializedCount` contract and signed byte-count parameter.
- `Engine/Source/Frame/Alignments.cpp:96`: read-only precedent.
- `Projects/BrokenEngineSandbox/Source/Pch.h`: read-only evidence that Debug enables `kbDebugInput` and Release/Profile disable it.

## In scope

- The single literal `16` passed as the activation-record size in `Replay::SaveLoadReplay`, replaced by the three member-size terms above.

## Out of scope

- The inventory-record size `57`, other size expressions, record/member declarations, serialization helpers, writer/reader field order, validation policy, replay lifecycle, and inventory identity comparison.
- Format or version changes, compatibility handling, new headers, helpers, tests, fixtures, instrumentation, and broader C++11 adoption.
- Style-guide or AGENTS.md edits: no new contract or policy is warranted. Perform the implementation workflow's required documentation review, with no amendment expected.

## Risk and invariants

Future implementation is Tier 1: a local behavior-preserving constant-expression substitution with no public signature or invariant change. Its placement in replay validation does not itself alter the replay surface. Any proposed change to accepted streams, serialized bytes, or replay compatibility exceeds this scope and requires reclassification.

- The effective element-byte count remains 16 and continues to exclude object padding.
- The same counts and stream lengths pass or fail; diagnostics and validation/reserve ordering are unchanged.
- Manifest version remains unchanged because the format is unchanged. No determinism, CRC, trust, allocation, or runtime-performance change occurs.

## Acceptance criteria

1. The focused C++ diff changes only the stated argument to the three qualified member-size terms.
2. Source inspection proves those terms match the three independently encoded/read fields and total 16; all surrounding behavior and the inventory size `57` remain unchanged.
3. A future Debug server build passes and confirms the expression compiles in the active replay path.

## Verification

- Review the focused diff and recheck member types, fieldwise writer/reader calls, and `ValidateDeserializedCount`'s parameter. This proves numerical equivalence and zero runtime work; no benchmark or disassembly comparison is necessary.
- During implementation, use `/compile` for BrokenEngineSandbox server `Debug|x64`, with Shared data mode because this is a source-only change. Debug is required to compile the enabled replay implementation. No client build or runtime `/agent-harness` scenario is needed for this constant-expression substitution. Do not add unit tests.
- These are future checks; no implementation build or runtime verification was performed when authoring this Plan.
