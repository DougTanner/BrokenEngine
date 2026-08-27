<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:40:33.675Z","dependsOn":[]} -->
# Fix: Normalize multisample settings before Vulkan render-pass creation

## Context

The accepted survivor `CAI/shard-0040/001` shows that graphics settings load an
arbitrary `VkSampleCountFlagBits` directly into `gSampleCount`
(`Engine/Source/Ui/GraphicsSettings.cpp:113-125`).  The wrapper's discrete
`GetIndex()` soft-falls but does not replace an off-grid value, while the
allowed set is only 2, 4, 8, 16, 32, and 64
(`Engine/Source/Ui/WrapperBase.h:142-149,183-202`; `GraphicsSettingsWrappersBase.cpp:6-10`).
Physical-device validation can then reset the value to one without disabling
`gMultisampling` (`Engine/Source/Graphics/Managers/InstanceManager.cpp:542-547`),
and SwapchainManager consumes the resulting value for MSAA attachments and a
resolve path (`Engine/Source/Graphics/Managers/SwapchainManager.cpp:53-70,95-153,353-386`).

The source report is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0040.md:79`;
the consolidated selector is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:1011`.
All 44 UI target rows matched frozen baseline
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`; no source changes were made in
this routing session.

Impact: a hand-edited current-format settings file or a device supporting only
one sample can drive Vulkan render-pass/image creation with an invalid MSAA
count and throw during startup or recreation.

## Design

Author's recommendation: normalize the persisted sample count by explicit
membership in the six allowed multisample values before storing it in the
wrapper.  During selected-device capability validation, if the color/depth
intersection supports no value greater than one, disable `gMultisampling` and
keep `gSampleCount` on an allowed default; otherwise choose the highest allowed
member not exceeding the intersection and use it consistently.  Add a final
render-pass guard so the MSAA/resolve attachments are built only when the
normalized parent toggle and count are both valid.  Do not use the wrapper's
soft-fall `GetIndex()` as validation.

## Critical files

- `Engine/Source/Ui/GraphicsSettings.cpp:113-125` — persisted admission.
- `Engine/Source/Ui/GraphicsSettingsWrappersBase.cpp:6-10` — allowed count set.
- `Engine/Source/Ui/WrapperBase.h:142-208` — discrete wrapper semantics.
- `Engine/Source/Graphics/Managers/InstanceManager.cpp:92-120,535-548` —
  supported-count selection and capability clamp.
- `Engine/Source/Graphics/Managers/SwapchainManager.cpp:53-70,95-153,353-386`
  — MSAA render pass and image consumers.
- `Engine/Source/Graphics/Graphics.cpp:510-515` — runtime capability clamp
  call path.

## In scope

- Explicit current-format sample-count membership normalization at settings
  load and device capability validation.
- Disabling multisampling when the selected device's shared color/depth support
  is `VK_SAMPLE_COUNT_1_BIT`, and selecting a valid allowed count otherwise.
- Keeping attachment, framebuffer, and pipeline sample counts paired and
  updating only comments needed for the final guard.

## Out of scope

- Present-mode normalization, other persisted setting fields, Vulkan device
  scoring, render-pass redesign, or adding sample-count enum values.
- Backward compatibility for a new settings layout (the current four-byte field
  remains); server/simulation/wire/save/replay formats and unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 2 (downgraded from Tier 3). Trigger: opaque
settings input controls Vulkan capability selection and render-pass/image
creation across UI and Graphics; trust-boundary and GPU integration are
higher-risk surfaces.

Preserve these invariants:

- A stored sample count is always one of 2/4/8/16/32/64 before wrapper consumers
  use it.
- With multisampling enabled, the count is greater than one and supported by
  both color and depth attachments; a max-one device uses the non-MSAA path.
- MSAA source, depth, framebuffer, and resolve relationships remain valid and
  use one normalized count.
- No simulation CRC, wire, save, replay, or `.pack` bytes change.

Tier rationale: the Design pre-specifies each edit — an explicit membership
check at settings load, a capability clamp that also clears the parent toggle,
and one guard before render-pass creation. The persisted field keeps its
current four-byte layout and a valid supported count produces exactly today's
attachments, so only already-invalid input changes behavior.

## Acceptance criteria

- A current-format settings file containing `3` or `VK_SAMPLE_COUNT_1_BIT`
  loads to a valid allowed/default state without a debug soft-fall break.
- On a max-one device, enabling the checkbox is normalized to non-MSAA before
  render-pass creation; no three-attachment resolve pass is attempted.
- On a device supporting 4x, a valid 4x setting creates paired 4x attachments
  and resolves successfully; an unsupported higher value selects a supported
  allowed member.
- Client Debug and Release builds pass `/compile`; a harness settings/recreate
  scenario has no Vulkan sample-count or resolve validation error.

## Notes

The audit catalog records external Vulkan claims `CAI-EXT-012` and
`CAI-EXT-013`; implementation review must preserve those sample-count and
resolve-operation rules.
