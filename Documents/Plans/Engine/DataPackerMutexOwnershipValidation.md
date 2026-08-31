<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-31T01:58:34.678Z","dependsOn":[]} -->
# Distinguish newly created and existing DataPacker mutexes

## Context

Final survivor `S002-C022` is a promoted HIGH startup-lifecycle finding. `main` calls `CreateMutex(..., TRUE, ...)`, then later uses `GetLastError() == ERROR_ALREADY_EXISTS` to choose the wait branch (`DataPacker/Source/Main.cpp:817-845`). The final locator verified that a successful newly created mutex does not guarantee a cleared last-error value. A stale `ERROR_ALREADY_EXISTS` can therefore make the creator wait forever on its own already-owned mutex.

The final disposition is in `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-002.md` under `S002-C022 — FINAL: PROMOTE_HIGH`; the source report is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-002.md:317` and the consolidated selector is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:109`. The target remained at baseline `a20acf1e31a24a0f61ae638e8976602b49655788`.

## Design

The author's recommendation is to capture the defined mutex-creation result immediately and branch on that result, rather than relying on a stale last-error value after successful creation. Preserve initial ownership for a newly created mutex, wait only for a genuinely pre-existing named mutex, keep null-handle failure on the existing diagnostic path, and release the mutex through the current RAII lifetime.

## Critical files

- `DataPacker/Source/Main.cpp:817-853` — mutex creation, ownership classification, and wait.
- `DataPacker/Source/AGENTS.md` — single-instance and startup-failure contract.

## In scope

- Correct classification of newly created versus pre-existing named mutexes.
- Existing wait, ownership, release, and null-result failure behavior.

## Out of scope

- DataPacker export ordering, mutex naming, multi-process policy, diagnostics redesign, or other Win32 calls.
- A compatibility mode or unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 2 (scoped DataPacker startup behavior). Trigger: a third-party Win32 resource result controls process serialization, but the correction is confined to the existing startup mutex branch and changes no serialized, wire, deterministic, or export contract.

Preserve these invariants:

- The first DataPacker process proceeds immediately with ownership of a newly created mutex.
- A second process waits for the existing owner according to the current single-instance policy.
- Mutex creation failure reports through the existing diagnostic path and no handle is misclassified.

## Acceptance criteria

- A newly created named mutex proceeds without waiting on itself even when last error initially contains `ERROR_ALREADY_EXISTS`.
- A genuinely pre-existing mutex waits and proceeds only after the prior owner releases it.
- DataPacker `Debug|x64` builds pass through `/compile`.

## Notes

Origin: `S002-C022`; disposition selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-002.md` (`S002-C022 — FINAL: PROMOTE_HIGH`), source selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-002.md:317`, and consolidated selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:109`. External claim `EXT-012` was VERIFIED. No source fix or build was performed during routing.
