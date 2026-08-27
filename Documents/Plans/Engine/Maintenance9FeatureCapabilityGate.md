<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:39:36.446Z","dependsOn":[]} -->
# Fix: Gate maintenance9 by queried feature support

## Context

The accepted survivor `CAI/shard-0029/002` shows that
`DeviceManager` treats the presence of `VK_KHR_maintenance9` as support for
the `maintenance9` feature.  It appends the extension during enumeration and
sets `.maintenance9 = VK_TRUE` in the device-create chain
(`Engine/Source/Graphics/Managers/DeviceManager.cpp:51-65,109-137`), while
`InstanceManager` never queries a maintenance9 feature structure
(`Engine/Source/Graphics/Managers/InstanceManager.cpp:475-487`).  The existing
QFOT fallback is probed only after logical-device creation
(`DeviceManager.cpp:280-281,486-510`), so an extension-advertising device with
the feature bit clear is rejected before fallback can run.

The source report is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0029.md:111`;
the consolidated selector is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:824`.
The frozen manager rows matched baseline
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`; this routing session made no
source edits.

Impact: a valid device that can run with explicit transfer ownership is
rejected during boot or Graphics recovery because an optional feature was
requested without support.

## Design

Author's recommendation: retain the extension-presence scan, but when the
extension is present first query a `VkPhysicalDeviceMaintenance9FeaturesKHR`
probe through `vkGetPhysicalDeviceFeatures2`.  Append the extension and the
create-chain structure with `maintenance9 = VK_TRUE` only when the probe bit
is true.  Pass that same enabled result to
`ProbeTransferQueueOwnershipTransfer`; when false, leave the extension feature
out and use the existing release/acquire ownership-transfer fallback.  Do not
make the optional feature required or change queue selection.

## Critical files

- `Engine/Source/Graphics/Managers/DeviceManager.cpp:51-65,109-137,168-229` —
  extension scan, feature probe, and logical-device chain.
- `Engine/Source/Graphics/Managers/DeviceManager.cpp:280-281,486-510` —
  QFOT capability probe and fallback gate.
- `Engine/Source/Graphics/Managers/DeviceManager.h:10-46` — capability helper
  declaration if the enabled-result name or contract needs adjustment.
- `Engine/Source/Graphics/Managers/InstanceManager.cpp:475-487` — selected
  physical-device feature-query boundary (reference; do not duplicate the
  probe chain there unless the implementation requires it).
- `Engine/Source/Graphics/Managers/TextureUploadManager.cpp:554-594` —
  release/acquire fallback consumer.

## In scope

- Querying maintenance9 support before adding the extension feature to
  `VkDeviceCreateInfo`.
- Enabling QFOT only for the queried-supported case and retaining explicit
  transfer ownership barriers otherwise.
- Keeping the existing optional extension, queue-family selection, shader-clock,
  line-rasterization, and device-feature behavior unchanged.

## Out of scope

- Vulkan feature requirements unrelated to maintenance9, device scoring,
  queue-family selection, or a new capability abstraction.
- Texture barrier stage masks, transfer staging, or any shader change.
- Server code, deterministic simulation, wire/save/replay formats, and unit
  tests.

## Risk tier and invariants

Expected Change Workflow Tier 2 (downgraded from Tier 3). Trigger: optional
Vulkan feature negotiation controls logical-device creation and a cross-queue
upload fallback; graphics device integration and queue ownership are
higher-risk surfaces.

Preserve these invariants:

- `maintenance9 = VK_TRUE` is requested only when the selected physical device
  reports the feature as supported.
- An extension-advertising but feature-unsupported device still reaches a valid
  logical device and uses explicit transfer release/acquire barriers.
- An enabled maintenance9 device continues to use the existing QFOT fast path.
- No simulation CRC, wire, save, replay, or `.pack` bytes change.

Tier rationale: the fix is fully specified and confined to one subsystem's
device-creation path — probe the feature before appending it to the create
chain and pass that result to the existing QFOT fallback gate. No serialized,
wire, save, or `.pack` bytes change, and devices that already report the
feature keep their current behavior.

## Acceptance criteria

- On a test or validation device exposing `VK_KHR_maintenance9` with the
  feature bit clear, logical-device creation succeeds without the feature and
  uploads use the explicit ownership-transfer path.
- On a device with the feature bit set, the extension/feature chain is enabled
  and QFOT probing preserves current behavior.
- Client Debug and Release builds pass `/compile`; Graphics boot/recovery has no
  unsupported-feature validation error.
- The device-create pNext chain contains no maintenance9 structure when its
  query bit is false.

## Notes

The audit catalog identifies this as `CAI-EXT-006`; the Vulkan feature-query
and-enable rule is an external API contract to preserve during implementation.
