<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-31T01:57:48.366Z","dependsOn":[]} -->
# Reject nested Attribution destination reparse points

## Context

Final survivor `S001-C004` is a promoted HIGH trust-boundary finding. `BuildPendingCopies` constructs nested `Attribution/<library>/<license>` destinations and checks existence and timestamps without validating nested reparse status (`DataPacker/Source/Attribution.cpp:131-163`). `FileManager::EnsureLocal` returns for a root already classified local, and `PublishPendingCopies` then calls `create_directories` and `copy_file(..., overwrite_existing)` through the unchecked path (`DataPacker/Source/Attribution.cpp:172-176`; `DataPacker/Source/FileManager.cpp:532-539`). The final disposition verified that Windows copy behavior can overwrite a destination link target, so a newer license can write outside the output tree.

The final disposition is recorded in `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-001.md` under `S001-C004 — FINAL — PROMOTE_HIGH`; the source report is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-001.md:135` and the consolidated selector is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:83`. The target was unchanged from baseline `a20acf1e31a24a0f61ae638e8976602b49655788`.

## Design

The author's recommendation is to validate every nested destination and its existing ancestors for reparse status before directory creation or copying, including when the Attribution root is already local. Reject the whole pending-copy operation through the existing DataPacker diagnostic path before opening or replacing a destination. Preserve normal local Attribution updates, newer-license selection, and the current recognized-root materialization behavior.

## Critical files

- `DataPacker/Source/Attribution.cpp:131-176` — nested destination construction and publication.
- `DataPacker/Source/FileManager.cpp:532-539` — local-root fast path that currently skips nested validation.
- `DataPacker/Source/AGENTS.md` — fail-closed reparse and output-writer contract.

## In scope

- Reparse validation for nested Attribution destination paths before `create_directories` or `copy_file`.
- Existing failure reporting when a nested destination or ancestor is unsafe.
- Valid local Attribution copy selection and publication behavior.

## Out of scope

- ThirdParty license contents, stale Attribution cleanup, recognized-root materialization, or linked-output identity.
- Changing copy timestamps, output naming, or the DataPacker output format.
- New filesystem compatibility modes, source fixes elsewhere, or unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 2 (scoped DataPacker filesystem behavior). Trigger: an opaque filesystem/reparse result reaches an offline output writer, but the correction is confined to the existing Attribution publication boundary and changes no serialized, wire, deterministic, or threading contract.

Preserve these invariants:

- No nested reparse path is followed or overwritten during Attribution publication.
- Unsafe output fails through the existing diagnostic path before any destination bytes change.
- Valid newer-license and already-current local outputs retain their current behavior.

## Acceptance criteria

- A local Attribution tree containing a nested directory or file link to an outside sentinel is rejected before copy, and the sentinel remains unchanged.
- A valid local Attribution tree copies a newer ThirdParty license and leaves the expected generated file in place.
- DataPacker `Debug|x64` builds pass through `/compile`.

## Notes

Origin: `S001-C004`; disposition selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-001.md` (`S001-C004 — FINAL — PROMOTE_HIGH`), source selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-001.md:135`, and consolidated selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:83`. No source fix or build was performed during routing.
