<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:30:50.942Z","dependsOn":[]} -->
# Preserve atomic DataPacker publication across volumes

## Context

The frozen audit retained `CAI/shard-0005/002`. `RunDirtyExport` writes
temporary manifest, pack, and header files under the LocalAppData cache at
`DataPacker/Source/Main.cpp:468-480`, then renames them into the independently
chosen output tree at `:501-521` and `:140-165`. The cache is derived under
LocalAppData by `FileManager.cpp:405-424`; ordinary output can be on another
volume, where the pinned MSVC implementation permits copy/delete replacement.
The source tree matches baseline `76d303f0eeeb86c1ed241edc81634e60070ba5a5`.

## Design

Create the dirty-export staging files as siblings on the destination volume,
write and close each complete file there, then use the existing atomic replace
operation and manifest/header-before-pack commit ordering. Preserve retry,
cancel, cleanup, and output-root ownership; do not expose cache-to-output
cross-volume copy as the commit operation.

## Critical files

- `DataPacker/Source/Main.cpp` — temporary paths and publication order.
- `DataPacker/Source/FileManager.cpp` — cache/output roots and ownership.

## In scope

- Destination-volume staging and atomic replacement for generated headers, manifests, and packs.
- Failure cleanup/retry behavior when publication is interrupted or denied.

## Out of scope

- Dirty-set detection, pack range/content validation, or changing manifest-before-pack ordering.
- Cross-volume behavior of unrelated file-copy features.

## Risk tier and invariants

Tier 2 (downgraded from Tier 3). Trigger: generated asset publication is an
opaque trust/recovery boundary and spans cache/output ownership. A failed
publication must leave the previous complete output recoverable; a successful
one exposes complete files.

Tier rationale: the change moves staging-file creation to the destination
volume inside one offline DataPacker publication function and keeps the
existing atomic replace and commit ordering. No file format, manifest layout,
or runtime consumer changes, and same-volume exports behave as before.

## Acceptance criteria

- With cache and output roots on different volumes, termination or replacement failure leaves no partial destination header/manifest/pack.
- A successful publication makes each complete file visible with existing commit ordering.
- Same-volume export, cancel prompts, and retry cleanup retain current behavior.

## Notes

Origin: `CAI/shard-0005/002`, source selector
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0005.md:68`.
The audit's external API claim was accepted by the manager; this Plan records
the repository fix boundary only.
