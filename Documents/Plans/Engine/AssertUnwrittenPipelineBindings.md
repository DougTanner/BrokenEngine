<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T23:20:27.350Z","dependsOn":["Documents/Plans/Engine/RemoveInertLightingDescriptorEntries.md"]} -->
# Assert that a pipeline writes every binding of its own sets

## Context

`O/` is `Engine/Source/Graphics/Objects/` and `S/` is `Engine/Data/Shaders/`.
Lines are those at commit `401e4b75`.

Most Set 1 binding numbers are positional in C++ and literal in GLSL. Set 0
has five constants used by both languages (`S/ShaderLayoutsBase.h:123-127`);
Set 1 has nine (`:133-149`), scoped by their own comment to the model and
water sites (`:129-132`). Every other Set 1 number is the entry's position in
`descriptorInfos` (`O/PipelineDescriptorWriter.cpp:477`), annotated by hand
(`Engine/Source/Graphics/Managers/WorldLightingShadowPipelines.cpp:321-345`).

Every number compared agrees today. The hazard is a future edit: inserting
or removing a mid-list entry moves every later positional entry, the moved
writes are dropped by `FilterWritesByShaderLayout` (`:321-342`) or
`RouteWritesBySet` (`:344-364`) or land on a neighbour, and the intended
bindings stay unwritten. Neither drop logs or asserts, so nothing reports
it at build or at pipeline creation.

`S/AGENTS.md` `## Shared Contract` says "Binding constants live in the
shared layout header and must match C++ layout/writes." That sentence is not
contradicted; it is incomplete: it does not say that most Set 1 numbers are
list position or that a write to a wrong number is dropped silently.

Originating gap: the architecture phase (data shape lens) of an analysis of
`Engine/Source/Graphics/Managers` at that commit. Unmet criterion: a
`descriptorInfos` list that leaves one of the pipeline's own reflected Set 1
or Set 2 bindings unwritten is reported at pipeline creation.

## Design

Recommended: one check in `PipelineDescriptorWriter::Write`, after the
filter and routing steps and before `vkUpdateDescriptorSets` (`:530-538`).
For each binding number the pipeline's shaders reflect in a set this call
writes, a surviving write names that number; otherwise `ASSERT`.

- Direction: declared but unwritten. The opposite direction (every entry
  resolves to a declared binding) cannot hold: model pipelines deliberately
  write numbers the shadow variant does not declare (the comment at `:529`,
  `WriteModelDescriptor` `:75-173`).
- Sets covered: Set 1 only when the pipeline has no external Set 1 layout
  (with one, no Set 1 set is allocated, `:417`, and Set 1 writes are
  dropped, `:357`); Set 2 when the pipeline is multi-set; never Set 0, which
  `TextureDescriptors::WriteGlobalDescriptorSets` writes. A pipeline with no
  external Set 0 layout writes its one set whole.
- This is not a useless ASSERT under
  `.agents/references/cpp-conventions.md`: the failure it reports is
  otherwise silent.

If an existing pipeline trips the check for a binding it leaves unwritten on
purpose, the executing session reports that pipeline and binding and stops
for re-planning; it does not add an exemption list.

## Critical files

- `Engine/Source/Graphics/Objects/PipelineDescriptorWriter.cpp`

## In scope

- One check in `PipelineDescriptorWriter::Write`, with a file-local helper
  if the check needs one.

## Out of scope

- Moving the terrain, water, or model lists to shared binding constants.
- The wording of `S/AGENTS.md` `## Shared Contract`.
- `FilterWritesByShaderLayout` and `RouteWritesBySet` behavior.
- A check on written-but-undeclared entries.

## Risk tier

Tier 2 — scoped behavior. Trigger: it adds a check inside one unit at an
existing boundary, with formats and trust unchanged; a debug-build boot can
now stop at pipeline creation. No determinism, layout, wire, or threading
exposure.

## Acceptance criteria

1. `Write` asserts when a reflected binding of a set it writes has no
   surviving write.
2. The BrokenEngineSandbox client and ModelViewer build through `/compile`.
3. Through `/agent-harness`, BrokenEngineSandbox and ModelViewer each boot
   and render with the check active and it does not fire, including after
   one pipeline-tier recreation. The supplied evidence records no harness
   setup.
4. The failing case is verified by code reading plus the build; no
   temporary harness command is added to trip it.

## Coordination

This Plan depends on
`Documents/Plans/Engine/RemoveInertLightingDescriptorEntries.md` (metadata),
which removes the entries whose writes are dropped today and removes two
writer branches next to the check.

## Notes

- Two external questions were raised and not settled, so this Plan states
  neither as fact: whether a descriptor write whose type differs from the
  layout binding's type is reported at the update call, and whether a
  partially-bound array binding left unwritten produces a validation error
  only when used.
