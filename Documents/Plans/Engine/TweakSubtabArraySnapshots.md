<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T18:30:07.238Z","dependsOn":[]} -->
# Use std::array for runtime tweak subtab snapshots

## Context

`TweaksScreenBase` stores active subtabs and their pre-audit snapshot in two fixed C arrays. `RunSliderAuditFrame()` copies the whole active array into the snapshot at audit frame zero and restores it after each synthetic render. These operations express whole-value snapshots but currently use `memcpy` and a byte count. Replacing only these runtime members with `std::array` expresses that intent directly, retaining inline storage and element-wise value semantics without allocation or indirection. No speedup is claimed.

Source inspection at baseline `d29fed456d3ede935c5e672f95f13d6733f0660c` establishes:

- `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenBase.h`: `kiMaxTweakSections` is 31; `miActiveSubtab` and `miPreAuditSubtab` are zero-initialized `int8_t` arrays. The separate serialized `TweakSectionState::iActiveSubtab` is a C array inside a trivially copyable 296-byte structure with an explicit size assertion.
- `TweaksScreenBase.cpp`: `RunSliderAuditFrame()` snapshots at frame zero and restores every frame, before normal UI rendering; `SaveState()` and `LoadState()` copy all 31 active subtab values across the persistence boundary. `LoadState()` first rejects a mismatched layout CRC.
- Repository searches found all uses of the two runtime members in these two files and no existing Plan owning this conversion.
- `Common/ExternalHeaders.h` already includes `<array>`. `Documents/C++StyleGuide.txt` rule 21 already permits `std::array` where value semantics help.

## Design

1. Declare both runtime members as `std::array<int8_t, kiMaxTweakSections>` and retain `{}` initialization and their existing names.
2. Replace the frame-zero snapshot with `miPreAuditSubtab = miActiveSubtab;` and the per-frame restore with `miActiveSubtab = miPreAuditSubtab;` at their existing positions.
3. Preserve the serialized C array. In `SaveState()`, use `std::memcpy(rState.iActiveSubtab, miActiveSubtab.data(), sizeof(rState.iActiveSubtab));`. In `LoadState()`, use `std::memcpy(miActiveSubtab.data(), rState.iActiveSubtab, sizeof(rState.iActiveSubtab));`. The byte count belongs to the serialized array, never to the `std::array` wrapper.
4. Keep existing indexed reads and writes unchanged. Add no includes, helpers, aliases, conversions, or bounds checks.

## Critical files

- `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenBase.h` — the two runtime member declarations; inspect `TweakSectionState` as an unchanged persistence invariant.
- `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenBase.cpp` — `SaveState()`, `LoadState()`, and `RunSliderAuditFrame()`.

## In scope

- Change only the declarations of `TweaksScreenBase::miActiveSubtab` and `TweaksScreenBase::miPreAuditSubtab` to the specified zero-initialized `std::array` type.
- Change the four copy statements in the three named functions exactly as specified above.
- Verify every reference to these two members still compiles and preserves its existing meaning.

## Out of scope

- Every member of `TweakSectionState`, its field order, padding, size assertion, layout CRC, game settings embedding, and settings version.
- `mWindowPositions`, other C arrays, other UI classes, and any repository-wide container migration.
- Audit scheduling, frame count, allocation suppression, section registration, tab selection, persistence validation, UI layout, or simulation behavior.
- New tests, benchmarking infrastructure, or compatibility paths.
- Style-guide and AGENTS.md amendments: none are warranted because the existing style rule allows this use and persistence, allocation, and audit contracts do not change. Future workflow documentation checks should confirm that conclusion without adding adoption policy.

## Risk tier and invariants

Future implementation is Tier 2: it changes publicly declared runtime member types and their persistence-boundary call sites within one subsystem, so use scoped plan review rather than treating the declarations as an unexposed local mechanical edit. It does not change the serialized representation or what the boundary carries. Any proposal to alter that representation is outside this Plan and requires reclassification.

- Both runtime arrays begin with all 31 elements zero and keep their existing extent and `int8_t` element type.
- Both audit assignments copy the full extent; snapshot and restore remain at the same control-flow points, including restore on every audit frame.
- Save/load copy exactly `sizeof(rState.iActiveSubtab)` bytes through `.data()`; serialized structure bytes, layout CRC checks, and settings version remain unchanged.
- Storage stays inline, with no allocation, extra indirection, or asymptotic cost change. This is a clarity change, not an optimization claim.

## Acceptance criteria and verification

| Criterion | Future verification |
|---|---|
| Exactly the two runtime members adopt `std::array`, remain zero-initialized, and retain all indexed behavior | Inspect the final diff and search every reference to both members across Engine and Projects. |
| Snapshot and restoration still copy every element at the original audit points | Inspect both assignments and unchanged surrounding control flow in `RunSliderAuditFrame()`. |
| Persistence retains its exact C-array representation and extent | Inspect the unchanged `TweakSectionState`, its existing trivial-copy and 296-byte assertions, unchanged settings version, and both `.data()`/serialized-array-size copy statements. |
| Changed declarations and callers compile | Run `/compile` for BrokenEngineSandbox client, `Debug|x64` (`Client`, `Debug`), using Shared runtime data: no asset or data-packer input changes are in scope. |
| No new allocation or copy-cost regression is introduced | Review fixed inline `std::array<int8_t, 31>` storage and direct full-value assignments; reject dynamic containers, wrappers, or additional copies. |

No new unit tests or `/agent-harness` scenario is required: the acceptance evidence is the bounded representation/copy diff and client compilation, with no user-visible behavior change. Server compilation is not required because the entire type and implementation are client-only. No build or runtime verification has been performed during Plan authoring.
