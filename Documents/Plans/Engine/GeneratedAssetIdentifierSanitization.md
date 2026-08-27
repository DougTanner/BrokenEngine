<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:30:45.407Z","dependsOn":[]} -->
# Make generated asset symbols valid and collision-free

## Context

The frozen audit retained `CAI/shard-0001/005`. `PathToCppVariable` removes
only a short punctuation list (`Common/StringUtils.cpp:26-30`), while
`DataPacker/Source/Main.cpp:71-74` writes its result directly into generated
identifiers. Legal names containing `+`/`#` remain invalid, and names such as
`foo-bar`/`foobar` collide. The baseline diff contains no source changes from
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`; the build-blocking behavior is
pre-existing.

## Design

Use a deterministic byte encoding in `PathToCppVariable`: preserve only ASCII
letters and digits, encode every other UTF-8 byte as `_xHH`, and keep the
generated `k` prefix. Track encoded names while writing each type header and
fail the export before writing on a collision. Keep the original path CRC as
the runtime identity; the generated spelling is only a valid source symbol.

## Critical files

- `Common/StringUtils.cpp` and `Common/StringUtils.h` — encoding contract.
- `DataPacker/Source/Main.cpp` — generated header emission and collision check.

## In scope

- `PathToCppVariable`'s complete identifier encoding and the generated-header collision gate.
- Diagnostics for invalid/colliding legal asset paths before header publication.

## Out of scope

- Path CRC identity, asset discovery, generated header layout, or unrelated C++ naming.
- Renaming existing hand-written symbols or adding compatibility aliases.

## Risk tier and invariants

Tier 3. Trigger: generated source is a build-facing output of DataPacker, and
identifier validity/uniqueness is an integration boundary. Every accepted
asset produces one valid unique symbol while path identity remains stable.

## Acceptance criteria

- Legal filenames containing punctuation generate a compilable header.
- Distinct legal paths always produce distinct generated symbol spellings, or the export fails before publication.
- Existing generated names and path CRC lookups remain compatible where their encoded spelling does not change.

## Notes

Origin: `CAI/shard-0001/005`, source selector
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0001.md:119`.
This Plan is a debt route; it contains no source fix or build result.
