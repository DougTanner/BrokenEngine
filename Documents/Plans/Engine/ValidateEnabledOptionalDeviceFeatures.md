<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T23:21:26.422Z","dependsOn":[]} -->
# Validate three optional Vulkan features DeviceManager enables

## Context

A path without a directory is under `Engine/Source/Graphics/Managers/`. Lines
are those at commit `401e4b75`.

`DeviceManager` enables four features that `InstanceManager` never checks:
`scalarBlockLayout` (`DeviceManager.cpp:174`), `hostQueryReset` (`:175`),
`drawIndirectFirstInstance` (`:220`), and `fragmentStoresAndAtomics`
(`:227`). A repository search finds those names nowhere else, so neither the
startup-error table (`InstanceManager.cpp:475-484`) nor the ASSERTs
(`:520-528`) cover them.

Verified against the Vulkan 1.2 specification:

- `scalarBlockLayout`, `fragmentStoresAndAtomics`, and
  `drawIndirectFirstInstance` are optional on a conformant 1.2 device.
- `hostQueryReset` is mandatory in Vulkan 1.2.
- `vkCreateDevice` fails with `VK_ERROR_FEATURE_NOT_PRESENT` when it is asked
  to enable a feature the physical device does not report.

So on a device that lacks one of the three optional features,
`vkCreateDevice` (`DeviceManager.cpp:242`) returns that error into
`CHECK_VK` instead of the named `ShowStartupError` path the table gives
every other required feature. The engine uses all three: the smoke and wind
deposit shaders call `atomicOr` on storage buffers in the fragment stage
(`Engine/Data/Shaders/Smoke/Smoke.frag:44`,
`Engine/Data/Shaders/Wind/WindDeposit.frag:51`), the comment at
`DeviceManager.cpp:217-219` records the nonzero `firstInstance` use in
indirect records, and the shared layouts use `scalar` blocks.

`AGENTS.md` `### InstanceManager` states "`DeviceManager` must enable every
feature required here", and that direction holds. The gap is the converse:
features enabled without being validated.

Originating gap: the architecture phase (data shape lens and cohesion lens,
which reported the same gap) of an analysis of
`Engine/Source/Graphics/Managers` at that commit. Unmet criterion: every
optional feature `DeviceManager` enables unconditionally is rejected by
`ValidatePhysicalDeviceCapabilities` with the startup error when the device
lacks it.

## Design

Recommended: add three rows to `pRequiredFeatures`
(`InstanceManager.cpp:475-484`), one per optional feature, each with its
name and a reason in the style of the existing rows.
`mVkPhysicalDeviceVulkan12Features` and `mVkPhysicalDeviceFeatures2.features`
are already filled at `:467`, so the rows read existing state.

`hostQueryReset` gets no row: it is mandatory.

The enable list in `DeviceManager` is not derived from a shared table.
Rationale: one table driving both steps would be a new abstraction over two
different Vulkan structs for no duplication beyond the list itself.

## Critical files

- `Engine/Source/Graphics/Managers/InstanceManager.cpp`

## In scope

- Three new rows in `pRequiredFeatures` in
  `ValidatePhysicalDeviceCapabilities`.
- `Engine/Source/Graphics/Managers/AGENTS.md` `### InstanceManager` or
  `### DeviceManager`, only if the documentation sync finds a sentence the
  new rows make incomplete.

## Out of scope

- `DeviceManager.cpp`: the enabled features and their order.
- A row for `hostQueryReset`.
- A table shared by validation and enabling.
- The `maxPerStageResources` ASSERT (`InstanceManager.cpp:463`) and every
  format check.

## Risk tier

Tier 2 — scoped behavior. Trigger: it adds checks inside one unit at an
existing boundary (physical-device capability validation), with the format
and the trust unchanged, which `.agents/references/risk-tiers.md` places in
Tier 2. A device that lacks one of the three features now fails at the named
startup error instead of at `vkCreateDevice`; a device that has them is
unaffected. No determinism, layout, wire, or threading exposure.

## Acceptance criteria

1. `pRequiredFeatures` has a row for each of `scalarBlockLayout`,
   `fragmentStoresAndAtomics`, and `drawIndirectFirstInstance`, and none for
   `hostQueryReset`.
2. The BrokenEngineSandbox client and ModelViewer build through `/compile`.
3. Through `/agent-harness`, the client still starts on the development
   device. The rejection path is verified by code reading plus the build:
   the development device reports all three features, and no harness
   command or temporary edit is added to trip it. The supplied evidence
   records no harness setup.

## Coordination

`Documents/Plans/Engine/BufferManagerDeadStateAndDuplicates.md` turns the
multisample member written at `InstanceManager.cpp:529-531` into a local, in
the same function as the table. Neither Plan depends on the other; whichever
lands later re-derives its lines by symbol.
