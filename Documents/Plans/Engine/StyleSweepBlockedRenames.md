<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-06T17:33:03.234Z","dependsOn":[]} -->
# Finish the style-sweep renames blocked on skill edits, and fix stale collection-member skill text

## Context

The `/sweep` `style-guide` run left these findings unfixed because the matching edits lay in `.agents/`, which the Codex sandbox could not write. The user directed (this session) that they become one mechanical Plan.

Verified gaps (current tree):

1. Rule 56 (complete words, `Documents/C++StyleGuide.txt:319`). `fSmokeObjectHeightInv` (`Engine/Data/Shaders/ShaderGlobalLayout.h:74`) and `fWaterColorHeightInv` (`:224`) abbreviate "Inverse"; sibling fields in the same header already spell it out (`fBaseHeightInverse` `:12`, `fSmokeEdgeDecayDistanceInverse` `:75`, `fSpreadHeightEndHeightInverse` `:169`). Every use: `Engine/Data/Shaders/Model/Model.frag:375`, `Engine/Data/Shaders/Particles/ParticlesRender.frag:51`, `Engine/Data/Shaders/Water/Water.frag:104`, `Engine/Source/Graphics/Render/SmokeUniforms.cpp:44`, `Engine/Source/Graphics/Render/WaterUniforms.cpp:172`, `.agents/skills/glsl-review/references/shader-footguns.md:122`, and the manual feature document `Documents/Features/Graphics/WaterSSS.md:48`. No shader-side alias names exist: every shader reads the field as `globalLayout.<name>`.
2. Stale skill text. `.agents/skills/add-collection-member/references/worker.md:97-99` says "Sounds copies in `AllocateAndCopy()`", but `Engine/Source/Frame/Collections/Sounds/` declares neither `AllocateAndCopy` nor `PersistentMembers()`. The live mechanism: `AllocateAndCopyCollections` (`Engine/Source/Frame/FrameUtils.h:332-343`) calls a collection's own `AllocateAndCopy` only when one is declared and otherwise calls `engine::AllocateAndCopyMembers`, which copies `PersistentMembers()` when declared or the full `Members()` tuple otherwise (`Engine/Source/Frame/Collections/AGENTS.md`, `## Core Contract`). So Sounds carries every column forward through the default `AllocateAndCopyMembers()` full-`Members()` copy.

Origin: `Documents/Investigations/ChangeWorkflow/StyleGuideSweepDeferredFixes.md` (`## Engine/` `### Not applied for other reasons`), deleted when this Plan is created. The `AddIndexableElementWithId` entry in that section is already resolved (deleted by commit `f60c4016`) and is not part of this Plan.

## Design

1. Rename `fSmokeObjectHeightInv` to `fSmokeObjectHeightInverse` and `fWaterColorHeightInv` to `fWaterColorHeightInverse` at the declaration, both C++ writers, all three shader reads, `shader-footguns.md:122`, and `WaterSSS.md:48`. Only the field name changes. The type, order, offset, and scalar layout of `GlobalLayout` stay the same, so the CPU/GPU contract is unchanged.
2. Replace "and Sounds copies in `AllocateAndCopy()`" in `add-collection-member/references/worker.md:98-99` with a sentence naming the live default, recommended wording: "and Sounds, which declares neither, carries its full `Members()` tuple through the default `AllocateAndCopyMembers()`." Recommended over dropping Sounds from the list because the full-copy default is a pattern the list does not otherwise show. The rest of the step 10 bullet list stays unchanged.

Risk tier: Tier 1. Trigger: mechanical renames and documentation text. No serialized or uploaded bytes change: the layout fields keep their position and type. No determinism/CRC, wire, save/replay, threading, or trust-boundary surface is touched.

## Critical files

- `Engine/Data/Shaders/ShaderGlobalLayout.h`
- `Engine/Data/Shaders/Model/Model.frag`
- `Engine/Data/Shaders/Particles/ParticlesRender.frag`
- `Engine/Data/Shaders/Water/Water.frag`
- `Engine/Source/Graphics/Render/SmokeUniforms.cpp`
- `Engine/Source/Graphics/Render/WaterUniforms.cpp`
- `.agents/skills/add-collection-member/references/worker.md`
- `.agents/skills/glsl-review/references/shader-footguns.md`
- `Documents/Features/Graphics/WaterSSS.md`

## In scope

- `GlobalLayout::fSmokeObjectHeightInv` and `GlobalLayout::fWaterColorHeightInv` and the single read or write of each in the listed shaders and Render populators.
- The name text at `shader-footguns.md:122` and `WaterSSS.md:48`.
- The final clause of the "Follow the target's live pattern" bullet at `add-collection-member/references/worker.md:97-99`.

## Out of scope

- Other `*Inv` abbreviations or other layout fields, and any change to `GlobalLayout` field order or type.
- The `ShaderLayoutsBase.h` file name in `shader-footguns.md:122`: that header includes `ShaderGlobalLayout.h` (`:182`), so the citation stays true.
- The other bullets of `add-collection-member/references/worker.md` step 10, including the generic `AllocateAndCopy()` bullet at `:89`.
- The other `## Engine/` entries of the deleted record. The user settled the rule 35 Win32 calls (`CrashReport.cpp:101`, `FileManager.cpp:322`) and the rule 62 split checks (`FileManager.cpp:58-66`). Other Plans own the rule 36 `AudioManager.cpp:219` and rule 12 `Puffs.h` entries. The rule 8 `DynamicPipelines.cpp` entry needs no change: the iterator is already named `sceneIt` (`:22`), a complete-word iterator name that rule 56 allows (its own example is `bufferIt`), and the record's proposed nested `it` would shadow the outer iterator, which C4456 rejects under `/W4 /WX`.

## Acceptance criteria

- A repository search outside `Documents/Investigations/` (`rg --hidden` excluding `ThirdParty/`) finds no `fSmokeObjectHeightInv` or `fWaterColorHeightInv`.
- `add-collection-member/references/worker.md` no longer claims Sounds copies in `AllocateAndCopy()`.
- `/compile` builds both the client and the server (`BrokenEngineSandbox`, Debug x64) with no new warnings. The client build uses Local generation, because shader sources change. This criterion requiring a shader repack is the Local generation authorization (`.agents/skills/compile/references/runtime-data-mode.md` `## Mode selection`).
- `/external-skill-creator` validation passes for the two changed skill packages (`add-collection-member`, `glsl-review`).

## Notes

- No runtime behavior changes, so no `/agent-harness` scenario is required.
