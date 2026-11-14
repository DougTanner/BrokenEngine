<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:53:37.552Z","dependsOn":["Documents/Plans/Engine/RemovePipelinePushConstantBytes.md"]} -->
# Compute pipeline layout reuses the set-split and multi-set helpers

## Context

`PC` is `Engine/Source/Graphics/Objects/PipelineCreator.cpp`. Verified against
commit `c675b6b2`; the line numbers below are from that commit and move when
the dependency lands.

`PipelineCreator::CreateComputePipeline` builds its global-Set-0 plus
per-pipeline-Set-1 layout inline, repeating two helpers the graphics path
already calls:

- `PC:661-671` is the Set 1 half of `SplitBindingsBySet` (`PC:155-173`), with
  the same `Pipeline::ResolveBindingSetIndex` filter.
- `PC:675-681` matches the Set 1 layout creation in
  `CreateMultiSetPipelineLayout` (`PC:182-188`) statement for statement.
- `PC:683-697` matches that helper's pipeline-layout assembly (`PC:203-215`)
  with a fixed two-set array and the compute stage flag.

The helper's preconditions hold for compute: `PC:640` asserts the pipeline is
not `kMultiSet`, so the helper takes its two-set path and creates no Set 2,
and no compute producer sets `vkExternalSet1DescriptorSetLayout` (its only
writer is `ModelPipeline::Create`), so the helper creates Set 1 itself.

Origin: an architecture review and an in-function refactor review of
`Engine/Source/Graphics/Objects`, independently verified. Unmet criterion:
root `AGENTS.md` `## Directives` DRY — reuse existing mechanisms.

## Design

Recommended: in `CreateComputePipeline`, build the compute-stage
`VkPushConstantRange`, call `SplitBindingsBySet` on the filtered bindings, and
under the existing condition (`mExternalVkDescriptorSetLayout` non-null and a
Set 1 count above zero) call `CreateMultiSetPipelineLayout`. The `else` arm
stays as it is: it clears `mExternalVkDescriptorSetLayout` and calls
`CreateSingleSetPipelineLayout`, because
`Engine/Source/Graphics/Managers/AGENTS.md` `## Shared Contracts` keeps
standalone compute pipelines. `SplitBindingsBySet` also fills a Set 2 array;
compute passes scratch storage for it and an empty span onward.

One fact is not established and is reported, not acted on: whether the
graphics single-set branch of `CreateDescriptorSetLayouts` (`PC:238-241`) is
reachable. It runs only when the global Set 0 layout is null at
`Pipeline::Create`; `Engine/Source/Graphics/Managers/TextureDescriptors.cpp`
creates that layout and `Engine/Source/Graphics/AGENTS.md`
`## Frame and Resource Lifecycle` says the texture manager is constructed
before the pipeline manager. The executing session reads the construction
order and states in its handoff whether the branch is reachable. This Plan's
edit is the same either way; an unreachable branch is a residual for a
separate change, not a removal here.

## Critical files

- `Engine/Source/Graphics/Objects/PipelineCreator.cpp`

## In scope

- `PipelineCreator::CreateComputePipeline`: the Set 1 partition loop, the
  inline Set 1 layout creation, and the inline two-set pipeline-layout
  assembly, replaced by calls to `SplitBindingsBySet` and
  `CreateMultiSetPipelineLayout`.
- The comment above the partition loop, if the edit makes it incorrect.
- The read-only reachability check of the single-set branch in
  `CreateDescriptorSetLayouts`.

## Out of scope

- The standalone-compute `else` arm and the `iSet1Count > 0` condition.
- The signatures and bodies of `SplitBindingsBySet`,
  `CreateMultiSetPipelineLayout`, and `CreateSingleSetPipelineLayout`,
  including giving the single-set helper a `VkPushConstantRange` parameter or
  moving `setLayoutCount` into it.
- Removing the graphics single-set branch.
- The indirect-buffer setup and the empty-binding filter at the top of
  `CreateComputePipeline`.

## Risk tier

Tier 2 — scoped behavior. Trigger: layout creation for every non-standalone
compute pipeline moves to shared helpers, one subsystem's runtime path. The
create-info contents are meant to be unchanged. Client-only; no
determinism/CRC, wire, serialization, `.pack`, replay, threading, or shader
exposure.

## Acceptance criteria

- `CreateComputePipeline` holds no inline `vkCreateDescriptorSetLayout` or
  `vkCreatePipelineLayout` call.
- The client and ModelViewer build.
- Through `/agent-harness`, a world frame renders with the validation layer
  reporting no error; shadow, lighting, smoke, and particle compute all run in
  it. The supplied evidence records no harness setup.
- The handoff states whether the graphics single-set branch is reachable.

## Coordination

`Documents/Plans/Engine/RemovePipelinePushConstantBytes.md` is the metadata
dependency: it rewrites the push-constant size statement this Plan moves.
`Documents/Plans/Engine/RemoveUnproducedGraphicsFlagValues.md` edits another
region of `PipelineCreator.cpp` (`ConfigureRasterization`); neither depends on
the other, and whichever lands second re-derives its regions by symbol from
the landed file.

## Notes

- Nothing was built or run by the authoring session; the equivalence claims
  come from reading.
