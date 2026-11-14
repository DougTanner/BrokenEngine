<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:53:19.515Z","dependsOn":[]} -->
# Remove the Write-Only Graphics Debug-Names Set

## Context

Found by a deep analysis of `Engine/Source/Graphics` (direct files only) and
verified independently. Pre-existing at commit `401e4b75`; line numbers are
from that commit, so locate by symbol if they moved. `G/` abbreviates
`Engine/Source/Graphics/`.

`Graphics::mDebugNames` (`G/Graphics.h:158`) is written and never read. The
only uses in the tree are the declaration and `SetVkObjectName`
(`G/GraphicsUtils.cpp:81, 84`), which emplaces each formatted name so that
`pObjectName` (`:91`) points at NUL-terminated storage that lives as long as
the renderer. The set is never read or cleared, and the insert forces the
`// Heap:` allocation-tracking suppression at `:81-82`: a heap allocation per
distinct name in debug-layer builds.

Separately, `SetVkObjectName` has one caller, `VkName`
(`G/GraphicsUtils.h:20`), already inside
`if constexpr (kbVulkanDebugLayers)` (`:18`), so the same test at
`GraphicsUtils.cpp:66` and the three `[[maybe_unused]]` parameter attributes
at `:64` repeat it.

Still open (the investigation portion): whether
`vkSetDebugUtilsObjectNameEXT` copies `pObjectName`, so the string need not
outlive the call. The verification could not settle it from an authoritative
source. If the implementation keeps the pointer, the set is the string's
owner and is not removable. Also established: `common::Workbuffer::View()` is
not NUL-terminated (`Common/Workbuffer.cpp:72-76`), so the current workbuffer
contents cannot be passed as they are.

Unmet criterion: no renderer-lifetime container that nothing reads, and no
`// Heap:` suppression for a temporary. Authority:
`.agents/references/cpp-conventions.md` (workbuffer for temporaries; `// Heap:`
suppression only when unavoidable).

## Design

1. Settle the open fact first with `/verify-external-claims`: does
   `vkSetDebugUtilsObjectNameEXT` (`VK_EXT_debug_utils`,
   `VkDebugUtilsObjectNameInfoEXT`) copy `pObjectName` during the call?
2. If verified, recommended: in `SetVkObjectName`, append a terminator to the
   workbuffer frame opened at `GraphicsUtils.cpp:76` (`Append` takes a
   `std::string_view`, `Common/Workbuffer.h:42`), pass the frame's data
   pointer as `pObjectName` while that frame is open, and delete the
   `mDebugNames` member, the emplace, and the suppression with its `// Heap:`
   comment.
3. If refuted or still unresolved, leave `mDebugNames`, the emplace, and the
   suppression unchanged and report that result; the set then owns storage the
   driver may read.
4. In either case, remove the repeated `if constexpr (kbVulkanDebugLayers)`
   test and the three `[[maybe_unused]]` attributes in `SetVkObjectName`.

## Critical files

- `Engine/Source/Graphics/GraphicsUtils.cpp`
- `Engine/Source/Graphics/Graphics.h`

## In scope

- The investigation of step 1.
- The body and parameter attributes of `SetVkObjectName`.
- The `mDebugNames` member of `Graphics`.

## Out of scope

- `VkName` and its callers.
- `common::Workbuffer`.
- Any other allocation-tracking suppression.

## Risk tier

Tier 2 — scoped behavior. Trigger: one subsystem's tool behavior (Vulkan
object naming in debug-layer builds) changes how the name string is stored.
Debug-layer builds only: no determinism/CRC, serialization, wire, replay, or
threading exposure.

## Acceptance criteria

- The verdict of step 1 is reported with its source.
- If verified: no `mDebugNames` identifier remains in the tracked tree,
  `SetVkObjectName` holds no allocation-tracking suppression, and the string
  passed as `pObjectName` is NUL-terminated.
- `SetVkObjectName` no longer tests `kbVulkanDebugLayers`.
- The BrokenEngineSandbox client builds in a debug-layer configuration, and a
  RenderDoc capture taken through `/agent-harness` shows object names intact.
  The supplied evidence records no harness setup.

## Coordination

No directional dependencies or mandatory nondirectional coordination.

## Notes

- `Documents/Plans/Engine/IsPointVisibleCallsInVisibleArea.md` edits another
  function of `GraphicsUtils.cpp`.
