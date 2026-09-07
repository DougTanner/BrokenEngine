<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-07T16:14:40.302Z","dependsOn":[]} -->
# Consolidate Graphics manager guidance into AGENTS.md

## Context

`Engine/Source/Graphics/Managers/AGENTS.md` is the automatically loaded owner
for the Graphics manager directory, but ten sibling `*.AGENTS.md` files hold
manager-specific constraints outside the ordinary ancestor `AGENTS.md` chain:
`BufferManager.AGENTS.md`, `CommandBufferManager.AGENTS.md`,
`DeviceManager.AGENTS.md`, `ImGuiManager.AGENTS.md`,
`InstanceManager.AGENTS.md`, `ParticleManager.AGENTS.md`,
`PipelineManager.AGENTS.md`, `SwapchainManager.AGENTS.md`,
`TextureManager.AGENTS.md`, and `TextureUploadManager.AGENTS.md`.

The owner and ten detail files total 4,969 `bt-token-v1` on baseline
`d992d64a740212e2a72b33328c3f14e4126329ad`, above the 4,000-token hub target,
so a verbatim merge is too large. The detail documents contain manager
lifetime, synchronization, validation, resource-identity, recreation, and
threading constraints that still change implementation decisions; those
constraints must enter the automatic document while inventories and repeated
context are trimmed.

Two automatically loaded documents link directly to detail files:
`Engine/Data/Shaders/Particles/AGENTS.md` links ParticleManager ownership, and
`Engine/Source/Ui/Screens/AGENTS.md` links ImGuiManager behavior. Two live
executable Plans also cite manager details:
`Documents/Plans/Engine/ExplosionParticleGraphicsRecovery.md` cites
`TextureManager.AGENTS.md`, while
`Documents/Plans/Engine/PointLightsGraphicsRecovery.md` cites
`TextureManager.AGENTS.md` and `PipelineManager.AGENTS.md`. No existing Plan
owns this consolidation boundary.

## Design

The author's recommendation is to replace the linked responsibility list in
`Engine/Source/Graphics/Managers/AGENTS.md` with one `## Manager Contracts`
section containing a `### <ManagerName>` subsection for each of the ten
managers. Move every decision-changing rule from each sibling document into
its matching subsection. Keep shared constraints once in `## Shared Contracts`
and shorten repeated introductions, source/member inventories, and prose whose
operative rule is already retained.

Preserve the reason and condition for every ownership, lifetime, ordering,
synchronization, validation, failure, affinity, and recreation requirement.
Preserve ordinary links to other owners where they prevent duplication,
including the ImGui links to UI screens, the TextureManager links to File and
Water, and the TextureUploadManager link to File. The File links remain on
`Engine/Source/File/AGENTS.md`, which is the automatically loaded File owner.

After the contract inventory proves the merged hub covers every operative
detail rule, delete all ten sibling `*.AGENTS.md` files. Retarget the Particle
shader and UI screen links to the matching anchors in
`Engine/Source/Graphics/Managers/AGENTS.md`. If the two executable consumer
Plans still exist, retarget their manager-detail links to the matching hub
anchors in the same change; absence because a Plan already completed is not an
error. Keep `Engine/Source/Graphics/Managers/CLAUDE.md` as the directory's only
stub.

Change Workflow tier: Tier 1, mechanical documentation-only work. It changes
instruction placement and wording without changing C++, GLSL, scripts,
project membership, runtime behavior, public signatures, determinism/CRC,
serialization, wire formats, threading behavior, or trust-boundary policy.

## Critical files

- `Engine/Source/Graphics/Managers/AGENTS.md`
- `Engine/Source/Graphics/Managers/BufferManager.AGENTS.md`
- `Engine/Source/Graphics/Managers/CommandBufferManager.AGENTS.md`
- `Engine/Source/Graphics/Managers/DeviceManager.AGENTS.md`
- `Engine/Source/Graphics/Managers/ImGuiManager.AGENTS.md`
- `Engine/Source/Graphics/Managers/InstanceManager.AGENTS.md`
- `Engine/Source/Graphics/Managers/ParticleManager.AGENTS.md`
- `Engine/Source/Graphics/Managers/PipelineManager.AGENTS.md`
- `Engine/Source/Graphics/Managers/SwapchainManager.AGENTS.md`
- `Engine/Source/Graphics/Managers/TextureManager.AGENTS.md`
- `Engine/Source/Graphics/Managers/TextureUploadManager.AGENTS.md`
- `Engine/Data/Shaders/Particles/AGENTS.md`
- `Engine/Source/Ui/Screens/AGENTS.md`
- `Documents/Plans/Engine/ExplosionParticleGraphicsRecovery.md`
- `Documents/Plans/Engine/PointLightsGraphicsRecovery.md`

## In scope

- Inventory every operative rule and exception in the ten manager detail
  documents and map it to semantically equivalent text in a manager-named
  subsection of `Engine/Source/Graphics/Managers/AGENTS.md`.
- Remove repeated introductions, file/member inventories, and duplicate shared
  context while keeping the merged owner below 4,000 `bt-token-v1`.
- Preserve ordinary links to separately owned File, Water, and UI guidance in
  the merged owner.
- Delete the ten `Engine/Source/Graphics/Managers/*.AGENTS.md` detail files only
  after their contract inventory is complete.
- Retarget the two automatic-document consumers and any still-live executable
  Plan consumers named under `## Critical files` to stable manager anchors in
  the merged owner.

## Out of scope

- Changing any documented Graphics, Vulkan, threading, validation, lifetime,
  affinity, or failure contract.
- Changing C++, GLSL, scripts, project files, runtime behavior, or build output.
- Changing the File subsystem documentation or creating another sibling
  `*.AGENTS.md` document.
- Changing `/update-claude-docs` policy or discovery behavior; the dependent
  Change Workflow Plan owns that work.
- Editing or adding a CLAUDE stub.

## Invariants

- Every operative rule, exception, and required outcome from the ten detail
  documents remains semantically present in the automatically loaded manager
  owner.
- Shared constraints appear once, and manager-specific constraints remain under
  the manager they govern.
- Ordinary links continue to point at their authoritative external owners.
- The manager directory keeps exactly one `AGENTS.md` and its existing
  `CLAUDE.md` import stub.

## Acceptance criteria

- A before/after contract inventory maps every operative detail-document rule
  and exception to semantically equivalent text in
  `Engine/Source/Graphics/Managers/AGENTS.md`, with no unmatched item.
- `pwsh -NoProfile -File .agents/scripts/Measure-Tokens.ps1 Engine/Source/Graphics/Managers/AGENTS.md -Json`
  reports no more than 4,000 `bt-token-v1`.
- `rg --files Engine/Source/Graphics/Managers -g '*.AGENTS.md'` returns no
  files, while `Engine/Source/Graphics/Managers/AGENTS.md` and its unchanged
  `CLAUDE.md` stub remain.
- The Particle shader, UI screens, and any still-live named executable Plans
  link to the matching manager anchors in the merged owner; all Markdown links
  and heading anchors resolve.
- The diff contains only the owner consolidation, ten detail deletions, and
  required link retargeting named in scope.

## Notes

The ten detail documents and their owner entered the repository together, so
the evidence establishes an operational automatic-loading gap rather than a
historically observed move. This Plan does not cover the rejected File
documentation split.
