<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:29:31.455Z","dependsOn":[]} -->
# Bound packed-header path strings before logging or naming assets

## Context

The accepted finding `CAI/shard-0013/003` identifies an unterminated fixed
field at the packed-asset boundary. `ChunkHeader::pcPath` is a fixed
`char[MAX_PATH]` field (`Common/DataFile.h:382-401`), but eager and lazy
`PackChunks` paths construct an unbounded `std::string_view` from it
(`Engine/Source/File/PackChunks.cpp:327-350,576-722`). Deferred texture metadata
also copies the field into a `std::string`
(`Engine/Source/Graphics/Managers/TextureManager.cpp:181`). A readable header
whose 260 bytes contain no NUL can read into adjacent storage during logging or
name construction, contrary to the packed-asset trust-boundary contract.

The source shard's frozen/live target hashes match audit baseline
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`; current status contains only the
six pre-existing `Documents/Plans/Agents/Cpp*Audit.md` edits. The missing
termination check is unresolved, pre-existing, and outside the approved audit
work.

## Design

The author's recommendation is to reject a chunk header when no NUL occurs
within `std::size(pcPath)`, before the header can be published to eager or lazy
consumers. For accepted headers, derive log and texture-name views with an
explicit bounded length rather than relying on a C-string sentinel. Route a
failed check through the existing per-chunk corruption handling, preserving
the current required-boot versus optional-lazy failure distinction.

## Critical files

- `Common/DataFile.h:382-401` — fixed packed path field (read-only layout authority).
- `Engine/Source/File/PackChunks.cpp:327-350,576-722` — eager/lazy header logging and publication.
- `Engine/Source/Graphics/Managers/TextureManager.cpp:165-193` — deferred texture-name construction.
- `Engine/Source/File/AGENTS.md` — packed-asset trust-boundary and failure rules.

## In scope

- NUL validation for `ChunkHeader::pcPath` at eager and lazy pack-header
  ingestion.
- Bounded path views/copies at the cited logging and deferred texture metadata
  consumers.
- Routing an unterminated path through the existing asset-corruption result.

## Out of scope

- Pack row offset/size validation, audio wave metadata, cross-pack CRC
  generation, manifest hashing, or DataPacker producer changes.
- Changing path CRC semantics, `ChunkHeader` layout, `DataHeader::kiVersion`,
  or the required/optional asset policy.
- Any texture rendering or GPU residency behavior beyond rejecting the unsafe
  name source.

## Risk tier and invariants

Expected Change Workflow Tier 2 (downgraded from Tier 3). The changed code
consumes opaque packed bytes at a trust boundary and its eager path runs during
asynchronous boot.

Preserve these invariants:

- No log or runtime asset name reads beyond `pcPath[MAX_PATH]`.
- A malformed optional chunk reports failure and leaves loaders/waiters live;
  required boot corruption retains its existing fatal policy.
- Valid path bytes, CRC identity, eager/lazy publication, and texture metadata
  remain unchanged.

Tier rationale: the Design fully specifies one NUL check at header ingestion
plus bounded views at the three named consumers, all routed through the
existing asset-corruption result. `ChunkHeader` layout, the data version, and
path CRC semantics are explicitly out of scope, so a valid terminated path
produces byte-equivalent output.

## Acceptance criteria

- A readable header with every path byte nonzero is rejected without an
  out-of-bounds read or unrelated suffix appearing in a log/name.
- A valid terminated path produces byte-equivalent logs and texture names to
  the current path.
- Client and server `Debug|x64` builds clean through `/compile`.

## Coordination

`Documents/Plans/Engine/LazyAudioMetadataValidation.md` and
`Documents/Plans/Engine/CrossPackReferenceResolution.md` also touch
`PackChunks.cpp`; they own audio-header semantics and missing CRC references.
Keep the path-field checks in their named regions separate, and re-derive line
numbers before implementation. No dependency is required.

## Notes

The consolidated index records no duplicate-family hint or external claim for
this candidate.
