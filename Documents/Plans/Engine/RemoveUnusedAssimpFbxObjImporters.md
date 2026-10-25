<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-25T00:38:16.124Z","dependsOn":[]} -->
# Remove the unused Assimp FBX and OBJ importers from the ThirdParty build

## Context

DataPacker no longer decodes `.fbx` or `.obj` scenes through Assimp. Those
sources now go through the Blender conversion stage, like `.blend`:
- `IsAssimpScene` in `DataPacker/Source/ExportJobs/ExportScene.cpp:10-14`
  accepts only `.dae`, `.ply`, `.stl` and `.3ds`.
- The `AssimpSceneDecoder` constructor in
  `DataPacker/Source/ExportJobs/Scene/AssimpSceneDecoder.cpp` has no FBX or OBJ
  branch left.
- `DataPacker/Source/ExportJobs/AGENTS.md` `## Scene and Model` says
  `AssimpSceneDecoder` decodes four exchange formats.

The ThirdParty static library still compiles both importers, plus an RTTI
exception that only FBX needed:
- `ThirdParty/Prebuilts/Platforms/VisualStudio2026/ThirdParty.vcxproj:61`
  (`AssimpPreprocessorDefinitions`) disables every unused importer except FBX
  and OBJ.
- `ThirdParty.vcxproj:219-326` compiles the 15 `assimp\code\AssetLib\FBX\*.cpp`
  units and the 3 `assimp\code\AssetLib\Obj\*.cpp` units.
  `ThirdParty.vcxproj.filters:131-184` lists the same 18 units.
- 85 Assimp C++ units set `<RuntimeTypeInfo>true</RuntimeTypeInfo>`; the
  project default is off (`ThirdParty.vcxproj:92`, `:113`, `:136`).
- `ThirdParty/Prebuilts/Platforms/VisualStudio2026/AGENTS.md`
  `## Build Configuration` says "RTTI off except the Assimp-owned C++ units
  whose FBX object type checks require it". `## Source Organization` says "Only
  FBX, 3DS, OBJ, COLLADA, PLY and STL importers are enabled" and "Keep RTTI
  consistent across its common, importer and postprocessing C++ units".

No other compiled Assimp unit needs RTTI. A search of the 68 remaining
compiled Assimp `.cpp` units found no `dynamic_cast` or
`dynamic_pointer_cast`. The one `typeid` is in
`ThirdParty/assimp/code/Common/Importer.cpp:768-772`, guarded by
`defined _CPPRTTI`, with a non-RTTI fallback. Without the FBX and OBJ units, the
importer registry needs only the two disable defines:
`ThirdParty/assimp/code/Common/ImporterRegistry.cpp:85`, `:181`, `:242` and
`:364` guard them with `ASSIMP_BUILD_NO_OBJ_IMPORTER` and
`ASSIMP_BUILD_NO_FBX_IMPORTER`.

The change that moved FBX and OBJ to Blender could not make this edit itself.
It excluded changes under `ThirdParty/`, and ThirdParty builds run only in the
primary checkout (`.agents/skills/compile/SKILL.md` `## Inputs`).

## Design

1. In `AssimpPreprocessorDefinitions` (`ThirdParty.vcxproj:61`), add
   `ASSIMP_BUILD_NO_FBX_IMPORTER=1` and `ASSIMP_BUILD_NO_OBJ_IMPORTER=1`.
2. Remove the 18 FBX and OBJ `ClCompile` items from `ThirdParty.vcxproj`, and
   their entries from `ThirdParty.vcxproj.filters`.
3. Remove `<RuntimeTypeInfo>true</RuntimeTypeInfo>` from every remaining
   Assimp C++ unit, so all of them take the project default (off). The author
   recommends this because the Context search found no RTTI use left. Turning
   it off for all of them together keeps the RTTI-consistency rule.
4. Update `ThirdParty/Prebuilts/Platforms/VisualStudio2026/AGENTS.md`. In the
   compiler bullet, say RTTI is off for all units. In `## Source Organization`,
   list the enabled importers as 3DS, COLLADA, PLY and STL. Replace the
   RTTI-consistency sentence only if it no longer describes a distinct
   setting.

## Critical files

- `ThirdParty/Prebuilts/Platforms/VisualStudio2026/ThirdParty.vcxproj`
- `ThirdParty/Prebuilts/Platforms/VisualStudio2026/ThirdParty.vcxproj.filters`
- `ThirdParty/Prebuilts/Platforms/VisualStudio2026/AGENTS.md`

## In scope

- `AssimpPreprocessorDefinitions` in `ThirdParty.vcxproj`.
- The FBX and OBJ `ClCompile` items in `ThirdParty.vcxproj` and
  `ThirdParty.vcxproj.filters`.
- The `RuntimeTypeInfo` metadata on Assimp `ClCompile` items.
- The compiler bullet in `## Build Configuration` and the Assimp paragraph in
  `## Source Organization` of the ThirdParty `AGENTS.md`.

## Out of scope

- Upstream sources under `ThirdParty/assimp/` (a submodule; do not modify).
- DataPacker source and documentation: the FBX/OBJ routing to Blender has
  already landed.
- Removing any other enabled importer (3DS, COLLADA, PLY, STL) or changing
  contrib units, zlib, or the exporter setting.
- Project-wide RTTI or other compiler flags for non-Assimp units.

## Risk tier and invariants

Tier 3. The trigger is build/bootstrap coordination that can block other
sessions: every client, server and DataPacker links the primary
`ThirdParty.<Config>.lib`
(`ThirdParty/Prebuilts/Platforms/VisualStudio2026/AGENTS.md`
`## Consumer Provisioning`). Primary Debug, Profile and Release libraries must
stay nonempty while worktrees are active.

There is no runtime, CRC, wire, save, replay or `.pack` exposure: the
remaining Assimp decode is unchanged.

Building ThirdParty needs user-authorized primary maintenance through
`/compile`, because a session worktree cannot build `-Target ThirdParty`.

## Acceptance criteria

- ThirdParty Debug, Profile and Release build in the primary checkout through
  `/compile`, with no unresolved Assimp importer symbol.
- DataPacker Release builds and links against the rebuilt library.
- A DataPacker run over the current data completes with `Success`.
- No `AssetLib\FBX` or `AssetLib\Obj` unit remains in the ThirdParty project
  or filters, and no Assimp `ClCompile` item sets `RuntimeTypeInfo`.
- The static-checks runner reports every triggered row passing. Invoke it as
  `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents.

## Notes

- No tracked file under the repository has a `.dae`, `.ply`, `.stl`, `.3ds`,
  `.fbx` or `.obj` extension. That is why the DataPacker run criterion checks
  only that the export still completes.
