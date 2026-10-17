<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-17T20:39:46.375Z","dependsOn":[]} -->
# Give every first-party C array the rule 3 p prefix

Where a statement here and the code disagree, the code wins; report the contradiction instead of matching one side to the other.

## Context

`Documents/C++StyleGuide.txt` rule 3 reads "Pointer or C array - p", with the
example `char pcHex[20] {};`, and ends "Use only the prefixes listed in this
rule; never invent a new one (such as o for optional or a for array)." Before
that wording landed, rule 3 named only pointers, and many first-party C arrays
were named without `p`. They now violate rule 3. Recorded as out of scope by
the session that changed rule 3, which renamed only its own changed lines.

A single-line search over `Common/`, `DataPacker/`, `Engine/`, `Projects/`,
and `Tools/` finds about 125 candidate declarations whose name, after any
`k`, `s`, `g`, or `m` prefix, does not start with `p`. Examples:

- Members: `BufferManager::mWindOccupancyVkBuffers[2]`, `mWaterMeshLods`, `miMeshDataOffset` and its three siblings (`Engine/Source/Graphics/Managers/BufferManager.h`); `GameBase::mModalMessage[256]` (`Engine/Source/GameBase.h`); `mfButtonHoverAnimations` (`Engine/Source/Ui/Screens/MainMenuScreen.h`, `PauseMenuScreen.h`); the `StreamingVoice` slot arrays (`Engine/Source/Audio/StreamingVoice.h:109-115`).
- Struct fields: `uint8_t uiPadding[...]` and `uiPad[...]`
  (`Engine/Source/Ui/*Settings.cpp`, `Projects/BrokenEngineSandbox/Source/Game.h`,
  `Projects/BrokenEngineSandbox/Source/ClientSettings.cpp`, `Common/DataFile.h`);
  `std::chrono::duration<float> times[...]` and `keyframes[...]`
  (`Engine/Source/Frame/Collections/CollectionController.h:43-44`,
  `Puffs/Puffs.h:43-44`, `WindRadials/WindRadials.h:34-35`);
  `XMVECTOR vecVisiblePositions[4]` (`AreaLights/AreaLights.h:37`);
  `XMFLOAT4A data[...]` and `float data[...]`
  (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/Omnis.h:46,51`);
  `XMFLOAT4 normalMatrix[3]` and `rows[3]` (`Common/DataFile.h`, `MeshData` and
  `JointMatrix`); `FixedString::data` and `ConstexprCrcArray::array`
  (`Common/Crc.h`).
- Globals and locals: `gLogRingBuffers`, `gLogRuntimeLevels` (`Common/Log/Log.cpp:16`, `Common/Log/Log.h`); `snapshots` (`Engine/Source/GameBase.h`); `iBlockRgba`, `uiBc4Pixels` (`DataPacker/Source/ExportJobs/Texture/MigrateLegacyIntermediates.cpp`); `markers` (`Tools/WorktreeCli/LandingLockLifecycle.cpp`).
- Constants: `kName[]` (`Blasters.h:29`, `Omnis.h:58`), `kColorNames` and siblings (`Engine/Source/Graphics/Managers/RenderTargetTexturesLighting.cpp`), `kfSizeMultipliers`, and `kfTerrainShadowsRenderMultipliers`.

Unmet condition: rule 3 holds for every first-party C array declaration.

## Design

A rename-only sweep. For each C array object (local, static, global,
`extern`, member, or `constexpr`) whose name lacks `p`, insert `p` after the
scope and constant prefixes (`k`, `s`, `g`, `m`, in the order the name already
uses) and before the element type's rule 3 prefix, then update every
reference. Author's recommendations, with rationale:

- Element prefix: keep or supply the element type's rule 3 prefix, so
  `uiPadding` becomes `puiPadding`, `times` becomes `pTimes`,
  `mfButtonHoverAnimations` becomes `mpfButtonHoverAnimations`, and
  `kColorNames` (`std::string_view`, no prefix) becomes `kpColorNames`. Where
  the existing element prefix is wrong for the element type (for example
  `float f2CoordinatePixels[2]`, whose `f2` belongs to `XMFLOAT2`), correct it
  in the same rename; the line is being renamed anyway.
- `constexpr` arrays count: rule 3 stacks prefixes, and the repository's already-compliant constants (`kpcClientStatePath[]`, `kpcPadding[]`, `kppfDamages[][3]`) show `k` followed by `p`. So `static constexpr char kName[]` becomes `kpcName`. `kName` is not a framework contract: it is read only by its own collection (`kCrc = common::CrcConsteval(kName)`, `OmnisRender.cpp:47-48,189`), and the CRC hashes the string value, which does not change.
- One `p` per array, whatever its rank: rule 3 names the array kind, not its dimensions. Existing names that already start with `p` stay as they are, including multi-`p` names such as `sppiPushersPerZone`.

Format and wire exposure: none expected. Identifiers do not reach any
serialized byte, so no `kiVersion` changes. Before renaming, confirm each
candidate is not stringized into a key: the agent command channel turns
member tokens into JSON keys through `FRAME_COLUMN`
(`Engine/Source/Frame/FrameUtils.h:27`) and `INJECTED_FIELD`
(`Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationHarnessRigs.cpp:266`).
When this Plan was recorded, no candidate appeared in either macro. If one
does, rename it and update its `Projects/BrokenEngineSandbox/Documents/AgentHarness/`
entry in the same change.

Shader mirrors: `Engine/Data/Shaders/Model/ModelCommon.h` mirrors
`MeshData::normalMatrix` and `JointMatrix::rows` from `Common/DataFile.h`.
Rename the GLSL members in parallel (mirrored patterns stay parallel). Field
names do not affect the scalar layout, but the shader source changes, so the
build re-exports shaders.

Risk tier: Tier 1. The trigger is style: rename-only, with no behavior,
layout, format, determinism, or public-contract change. The files span
subsystems, but each rename is a local identifier change with its references,
not a behavior integration.

## Critical files

- `Documents/C++StyleGuide.txt` (rule 3, the authority; read only)
- The declaring files the search finds under `Common/`, `DataPacker/`, `Engine/`, `Projects/`, and `Tools/`, and their reference sites
- `Engine/Data/Shaders/Model/ModelCommon.h` (mirror members only)

## In scope

- The names of first-party C array object declarations lacking `p`, and every reference to them, including references in `Engine/Source/Frame/FrameUtils.h:23` and `ServerSimulationHarnessRigs.cpp:262` to `FixedString::data`.
- The GLSL `MeshData::normalMatrix` and `JointMatrix::rows` mirror members and their GLSL uses.
- Comments that name a renamed identifier.

## Out of scope

- `ThirdParty/`; declarations whose names a third-party or Windows SDK contract fixes.
- `std::array`, `std::vector`, and other containers; pointers and references to arrays, including array-reference function parameters such as `const char (&str)[N]`.
- GLSL-only declarations (for example `meshData[]`, `jointMatrices[]` in `ModelCommon.h`) and every other shader identifier.
- Names that already start with `p` after their scope prefixes, including any multi-dimensional `p` count.
- Every other rule 3 or style finding, and any non-rename edit.
- Every other Plan under `Documents/Plans/`.

## Acceptance criteria

- A repository search for C array object declarations under the five first-party roots, covering multi-line and multi-declarator forms as well as single-line ones, finds none whose name lacks `p` after its scope prefixes, except names `## Out of scope` excludes.
- `/compile` builds DataPacker, WorktreeCli, AgentHarness, and `BrokenEngineSandbox` client and server `Debug|x64` with no new diagnostics. `Release|x64` is checked too, apart from any pre-existing failure an open Plan already owns.
- `git diff` shows identifier and comment changes only.

## Notes

- The candidate count comes from a single-line pattern. The executing session re-runs a fuller search, because multi-line and multi-declarator declarations escape a single-line search.
