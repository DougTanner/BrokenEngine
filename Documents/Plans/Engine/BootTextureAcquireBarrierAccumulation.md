<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-31T01:59:19.098Z","dependsOn":[]} -->
# Preserve boot texture acquire barriers across repeated polls

## Context

Final survivor `S009-C002` is a retained HIGH Graphics synchronization finding. `WaitForTextures` repeatedly calls `ProcessPendingTextures(0)` while boot textures complete asynchronously. Each call clears `mbHasPendingAcquireBarriers`; after an early texture adoption ends its acquire command buffer, a later poll can clear the flag or re-record the same buffer, so the final flush submits only later barriers (`Engine/Source/Graphics/Managers/TextureManager.cpp:507-523,659-715`). The final locator verified that acquire work must be submitted before first use and that a new command-buffer recording does not retain prior commands.

The final disposition is in `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-011.md` under `S009-C002 — FINAL: RETAIN_HIGH`; the source report is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-009.md:48` and the consolidated selector is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:189`. The target remained at baseline `a20acf1e31a24a0f61ae638e8976602b49655788`.

## Design

The author's recommendation is to retain every boot adoption's acquire work until it is submitted, or submit each ended acquire command before that image can be used. Keep the existing command-buffer ownership, separate queue-family path, render-loop one-call behavior, and final boot wait; do not rely on a new recording to preserve prior barriers.

## Critical files

- `Engine/Source/Graphics/Managers/TextureManager.cpp:507-523,589-593,630-715` — barrier flag, command recording, and boot flush.
- `Engine/Source/Graphics/Managers/TextureManager.h` — per-frame acquire state.
- `Engine/Source/Graphics/Managers/TextureUploadManager.cpp` — transfer completion ordering.
- `Engine/Source/Graphics/Managers/AGENTS.md` and `Engine/Source/Graphics/AGENTS.md` — queue ownership contract.

## In scope

- Accumulation/submission of acquire barriers across repeated `WaitForTextures` polls.
- Separate-transfer and graphics queue-family ownership ordering for boot textures.
- Existing render-loop adoption, command-buffer reuse, and valid boot behavior.

## Out of scope

- Texture formats, upload release masks, descriptor registration, queue selection, or shader stages.
- A new synchronization abstraction, blocking wait for every texture, or unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 3. Trigger: queue-family ownership barriers and reusable command-buffer submissions cross asynchronous boot polling and first GPU use; synchronization and resource-lifetime ordering are invariant surfaces.

Preserve these invariants:

- Every adopted boot image has its acquire barrier submitted before first graphics use.
- Repeated boot polls never clear or overwrite pending acquire work that has not been submitted.
- Same-family/no-acquire behavior and ordinary render-loop barrier publication remain unchanged.

## Acceptance criteria

- With distinct transfer/graphics queue families and staggered completion of two boot IBL textures, every adopted image has a submitted acquire before first use.
- A later poll that adopts nothing cannot erase an earlier ended acquire command; valid same-family boot still completes normally.
- Client Debug and Release builds pass through `/compile`; synchronization validation reports no ownership/stale-barrier error.

## Coordination

`Documents/Plans/Engine/TextureAcquireComputeVisibility.md` owns release/acquire stage/access masks, while this Plan owns boot-poll lifetime/submission. Keep the first-consumer mask and repeated-poll accumulation independent, with no duplicate command-buffer path.

## Notes

Origin: `S009-C002`; disposition selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-011.md` (`S009-C002 — FINAL: RETAIN_HIGH`), source selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-009.md:48`, and consolidated selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:189`. External claim `EXT-041` was VERIFIED. No source fix or build was performed during routing.
