<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:31:21.884Z","dependsOn":[]} -->
# Initialize RDO sweep guard state before encoding

## Context

The frozen audit retained `CAI/shard-0008/001`. `RunCommand` dispatches
`--rdo-sweep*` before `MainThread` constructs `FileManager`
(`DataPacker/Source/Main.cpp:640-718`). All sweep paths call
`Texture::EncodeWithRdo`, whose first operation dereferences
`gpFileManager->mbForbidExpensiveExport` (`DataPacker/Source/ExportJobs/Texture/Texture.cpp:387-392`).
Source bytes match baseline `76d303f0eeeb86c1ed241edc81634e60070ba5a5`.

## Design

Route all RDO sweep modes through the same `MainThread` DataPacker
initialization that constructs and owns the live `FileManager`, while
preserving their log-only/no-file-write contract. The expensive-export
environment guard must be evaluated before any encode; normal export behavior
remains unchanged.

## Critical files

- `DataPacker/Source/Main.cpp` — command dispatch and initialization order.
- `DataPacker/Source/ExportJobs/Texture/RdoSweep.cpp` — sweep callers.
- `DataPacker/Source/ExportJobs/Texture/Texture.cpp` — shared guard dereference.
- `DataPacker/Source/FileManager.h` — global lifetime contract.

## In scope

- RDO sweep setup/lifetime ordering for `RunRdoSweep`, full, and validate modes.
- Guard-before-encode and log-only behavior.

## Out of scope

- RDO parameter/frontier logic, texture encoding quality, regular export setup, or FileManager redesign.

## Risk tier and invariants

Tier 2. Trigger: scoped DataPacker CLI runtime behavior; no wire, replay,
determinism/CRC, or serialized layout change is intended. Every advertised
sweep mode has a valid guard owner before encoding and writes no assets.

## Acceptance criteria

- All three RDO sweep modes complete or reject through the normal error path without a null `FileManager` access.
- `BT_DATAPACKER_FORBID_EXPENSIVE_EXPORT=1` blocks before encoding in each mode.
- Sweep output remains diagnostic/log-only and ordinary export behavior is unchanged.

## Notes

Origin: `CAI/shard-0008/001`, source selector
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0008.md:38`.
No source fix, build, or DataPacker run was performed here.
