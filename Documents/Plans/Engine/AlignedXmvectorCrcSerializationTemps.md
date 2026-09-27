<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-27T19:34:47.105Z","dependsOn":[]} -->
# Cleanup: Common — aligned DirectXMath temporaries in the XMVECTOR Crc and serialization overloads (style guide rule 44)

## Context
Style guide rule 44 prefers aligned DirectXMath storage (`XMStoreFloat4A`
over `XMStoreFloat4`). Three `XMVECTOR` overloads round-trip through an
unaligned `XMFLOAT4` stack temporary:

- `Common/Crc.h:121` `Crc(FXMVECTOR)` stores to `XMFLOAT4` and hashes it
- `Common/Serialization.h:112` `Write(std::ostream&, FXMVECTOR)` stores to
  `XMFLOAT4` and writes it
- `Common/Serialization.h:120` `Read(std::istream&, XMVECTOR&)` reads an
  `XMFLOAT4` and loads it

The whole-file scanner sweep
(`Documents/Plans/Engine/StyleGuideScannerRuleSweepCommonDataPackerTools.md`)
left all three as residuals: the fix changes the type whose object
representation is hashed or serialized, which its card forbade, and the three
were kept parallel. `XMFLOAT4A` derives from `XMFLOAT4` with only `alignas(16)`
added, so `sizeof` is 16 in both and the hashed and serialized bytes are
identical; the `Crc` (`Common/Crc.h:108-111`) and `Write`/`Read`
(`Common/Serialization.h:63-64`, `:87-88`) templates take any `T` and use its
object bytes.

## Design
The author's recommendation: change the three temporaries to `XMFLOAT4A` and
their store/load calls to `XMStoreFloat4A` / `XMLoadFloat4A`, all three in one
change so the overloads stay parallel. Update each overload's leading comment
if it names `XMFLOAT4`.

Rationale: aligned stores are the rule's form, the byte image is unchanged, and
no caller or format is touched.

## Critical files
- `Common/Crc.h`
- `Common/Serialization.h`

## In scope
- The bodies (and their one-line comments) of `Crc(FXMVECTOR)` in `Crc.h` and
  of the `XMVECTOR` `Write`/`Read` overloads in `Serialization.h`

## Out of scope
- Every other `XMFLOAT4` use, including stored members and serialized structs
- Any serialization format, `kiVersion`, or CRC definition change

## Risk tier and invariants
Tier 2 (scoped behavior): trigger is an edit to the CRC and serialization code
paths themselves; the per-tick CRC, save, replay and network bytes must stay
bit-identical, so no format or version bump applies. A reviewer may escalate to
Tier 3 if any produced byte differs.

## Acceptance criteria
- `/compile` passes for Client and Server in Debug and Release, and DataPacker
- An `/agent-harness` replay determinism check passes

## Notes
Originating record: the scanner sweep's rule 44 residuals, each deferred for the
hashed or serialized type change.
