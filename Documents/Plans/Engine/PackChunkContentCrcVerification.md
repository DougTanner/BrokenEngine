<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:30:39.820Z","dependsOn":[]} -->
# Verify packed chunk content CRCs

## Context

The frozen audit retained `CAI/shard-0001/002`. DataPacker computes and stores
`ChunkLocation.contentCrc` at `DataPacker/Source/Main.cpp:418-424`, but runtime
startup hashes only the Islands location table at
`Engine/Source/File/PackChunks.cpp:194-198`; the eager/lazy readers at
`Engine/Source/File/PackChunks.cpp:213-226,614-720` never compare payload bytes
with that field. A payload-only pack edit therefore passes the current
handshake. No source/runtime file differs from baseline
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`, proving the gap is pre-existing.

## Design

Use the existing `contentCrc` definition to verify each emitted chunk's exact
stored bytes before publication. For Islands, recompute the CRCs while loading
the pack and derive `mPackIntegrityToken` from the verified content rows so the
handshake covers payload bytes; for other types, reject a mismatch before the
eager/lazy chunk is published. Keep path CRC checks, compression handling, and
failure policies intact; a mismatch must fail as corrupt data rather than
silently loading a different payload.

## Critical files

- `Engine/Source/File/PackChunks.cpp` — runtime verification and handshake token.
- `Common/DataFile.h` — `ChunkLocation::contentCrc` contract.
- `DataPacker/Source/Main.cpp` — producer CRC definition.

## In scope

- Runtime comparison of `contentCrc` with the exact bytes DataPacker records.
- Islands integrity-token/publication behavior needed to reject payload-only divergence.

## Out of scope

- Location range validation, producer CRC algorithm changes, or unrelated asset checks.
- Backward compatibility for packs whose content CRC is absent or stale.

## Risk tier and invariants

Tier 3. Trigger: CRC/determinism and `.pack` trust/handshake behavior are
affected. Identical manifest tables must not authenticate different chunk
bytes; valid packs must continue to load deterministically on client and
server.

## Acceptance criteria

- Changing one stored chunk byte without changing its manifest causes a clear corruption failure before adoption or handshake success.
- Client/server Islands integrity rejects payload-only divergence under an unchanged manifest.
- Unmodified packs retain current payloads and load paths.

## Notes

Origin: `CAI/shard-0001/002`, source selector
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0001.md:68`.
No source fix, build, or harness was performed while routing this residual.

## Coordination

`Documents/Plans/Engine/PackChunkRangeValidation.md` also changes
`PackChunks.cpp`; preserve both independent trust checks and re-derive line
citations from the current implementation.
