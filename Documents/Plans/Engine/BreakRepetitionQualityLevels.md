<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-08T17:01:01.318Z","dependsOn":[]} -->
# Break Repetition Quality Levels

## Context

The Graphics screen's persisted, default-on Break Repetition toggle (`gBreakRepetition`,
`Engine/Source/Ui/GraphicsSettingsWrappersBase.h`; byte `uiBreakRepetition` of `GraphicsSettings`,
`kiVersion` 27, in `Engine/Source/Ui/GraphicsSettings.cpp`) gates every water randomized patch
blend strength: `PopulateWaterReducedUv` (`Engine/Source/Graphics/Render/WaterUniforms.cpp`)
publishes each of the seven `GlobalLayout` strengths (`fWaterNormalPatchBlend{One,Two,Three}`,
`fWaterColorNoisePatchBlend`, `fWaterFoam{Texture,Breakup,SpawnNoise}PatchBlend`) as its Tweaks
wrapper value (default 1.0, `Engine/Source/Ui/WaterWrappersBase.h`) when the toggle is on and 0
when it is off. The user measured the water pass at 3200 us with the toggle off and 4700 us with it
on, judged the increase too large, and asked for this follow-up: today's behavior becomes the High
level, new Medium and Low levels must cost much less, and the user judges their quality. The user
rejected an earlier proposal that varied only the patch weight threshold per level (typical normal
fetches about 19, 21.5 and 24 against 18 off) as too little difference. This record of the user's
choices is the authoring agent's, not the user's words.

Cost facts at default settings, derived by code read and a Monte Carlo count of the weight formula,
not measured: all strengths 1; all three normal bands drawn; Skybox One sample count 2, so
`SampleWaterNormals` runs twice per fragment (the centre call in `Water/Water.frag` and the extra
call in `Water/WaterSpecular.h` `ComposeWaterSpecular`); foam on; both color-noise weights and the
foam breakup amount non-zero.

- A patch-blended read at strength 1 fetches up to three taps: `MakePatchTriangle`
  (`Engine/Data/Shaders/ShaderFunctions.h`) drops a vertex whose cubed barycentric is below
  `kfPatchWeightThreshold` (0.05) of the cube sum, for an area mean of 1.81 taps (1 tap 35.2 %, 2
  taps 49.0 %, 3 taps 15.8 %). A GPU wave pays the largest tap count among its lanes, so a lattice
  whose triangles are small on screen costs close to three taps and one whose triangles span many
  waves costs close to the mean. A zero-strength read is the original single fetch with no
  triangle built.
- Each blended read needs one `MakePatchTriangle` (lattice transform plus one pcg3d hash per kept
  vertex). Water normals build each band's triangle once per fragment
  (`MakeWaterNormalPatchTriangles`, `Water/WaterNormalSampling.h`) and the extra sample reuses
  them; only octave One of each band is blended.
- `Water.frag` fetches from normals, color noise, foam color and foam breakup (fixed unpatched reads
  excluded): Off 22 (normals 18); today maximum 42 and area mean 30.1 (normals 30 and 22.8), with
  seven triangles per fragment.
- Lattice steps at defaults (`PopulatePatchLattice` exponent `k=floor(log2(900 m * F)+0.5)`, step
  `900 m / 2^k`): normal bands One, Two, Three octave One 28 m, 56 m, 113 m (tiles 25 m, 67 m,
  125 m); color noise One and Two 3600 m and 900 m (tiles 3846 m and 769 m); foam color 3.5 m; foam
  breakup 7 m; foam spawn noise 28 m.
- Normal band weights (`gLightingSampledNormalsWeight{One,Two,Three}{Minimum,Maximum}`,
  `Engine/Source/Ui/WaterWrappersBase.cpp`) are 1.5 to 2.5, 0.25 to 0.75 and 0.05 to 0.1,
  interpolated by one framing factor, so band One carries 75 to 83 % of the summed normal weight
  and band Three at most 3 %.

## Design

### Setting

The author recommends keeping the toggle and adding a Low/Medium/High level row under it, the
pattern of the Graphics screen's Lighting and Smoke groups (toggle, `ImGui::BeginDisabled`, headed
`RadioRow`), whose dependent controls stay visible and dimmed while their parent is off
(`Documents/UserInterfaceDesign.txt`, the `Graphics: heavy features use Low/Medium/High rows`
bullet):

- `gBreakRepetitionLevel` in `Engine/Source/Ui/GraphicsQualityWrappersBase.h/.cpp`, an `int64_t`
  0/1/2 `GraphicsQualityLevel` wrapper, default Medium (1): the other heavy-feature levels except
  Water default to Medium, and the user found High's cost too large. Like `gWaterLevel` it is
  consumed directly by per-frame rendering: no Apply function, no `ApplyAllGraphicsQualityLevels`
  entry, no `Graphics::Refresh` poll.
- `GraphicsMenuScreen::Render`: an `ImGui::Separator()` before the existing Break Repetition
  toggle, so the toggle and its level form their own group, then under the toggle
  `ImGui::BeginDisabled(!gBreakRepetition.Get<bool>())`, `RadioRow("Break Repetition",
  &gBreakRepetitionLevel, ...)` with Low/Medium/High at 0/1/2, and `ImGui::EndDisabled()`. The
  header drops `Level` as every existing level row does (`gLightingLevel` is headed `Lighting`).
- Persistence: `uint8_t uiBreakRepetitionLevel = 1` at offset 57, taking the first byte of
  `puiBreakRepetitionPadding`, which shrinks to two bytes at offset 58; `sizeof` stays 60;
  `kiVersion` becomes 28, so every older file fails the current-format gate. Save,
  `FindInvalidGraphicsSetting` (`IsValidGraphicsQualityLevel`), load (`LoadGraphicsQualityLevel`)
  and `ResetGraphicsSettings` (`mfCurrent = mfDefault` beside the other levels) cover it.

### Water per level

The level selects which of today's blended reads keep their strength. Every other read publishes
strength 0 and takes the existing zero-strength path. Threshold, lattices, hash, seeds and weights
stay unchanged at every level.

| Blended read (published strength) | Low | Medium | High |
|---|---|---|---|
| Band One octave One, centre (`fWaterNormalPatchBlendOne`) | yes | yes | yes |
| Band Two octave One, centre (`fWaterNormalPatchBlendTwo`) | no | yes | yes |
| Band Three octave One (`fWaterNormalPatchBlendThree`) | no | no | yes |
| The Skybox One extra sample's octave One reads | no | no | yes |
| Color noise One and Two (`fWaterColorNoisePatchBlend`) | yes | yes | yes |
| Foam color (`fWaterFoamTexturePatchBlend`) | no | no | yes |
| Foam breakup (`fWaterFoamBreakupPatchBlend`) | no | no | yes |
| Foam spawn noise, compute (`fWaterFoamSpawnNoisePatchBlend`) | no | yes | yes |

`yes` publishes the wrapper value while `gBreakRepetition` is on; `no` publishes 0. Toggle off
publishes 0 for all seven, as today. High publishes exactly today's values. `PopulateWaterReducedUv`
applies the table beside its existing toggle gate, reading the level clamped to the valid range as
`WaterUniforms.cpp` already clamps `gWaterLevel`; the expression shape is the session's choice.

The author's rationale for the table:

- Band One carries most of the normal weight and band Three at most 3 %, so Low keeps band One and
  Medium adds band Two.
- The color-noise lattices are kilometres wide, so a wave rarely straddles a vertex boundary and
  the blend costs near its mean; it breaks the kilometre-scale color repetition seen from RTS
  height. Low and Medium keep it.
- The foam color and breakup lattices (3.5 m and 7 m) make waves pay all three taps on every water
  fragment while foam is on, for tiles a few metres wide; only High keeps them. Foam spawn noise
  runs in the foam compute pass (its own GPU timer, `kGpuTimerWaterFoamCompute`), not the water
  fragment pass; Medium keeps it.
- The extra sample's three blended reads are half the normal blend cost. Tradeoff: at the default
  Skybox One sample blend 1 the Skybox One lobe averages the centre and extra samples, so at Low
  and Medium half of that lobe's input keeps the repeating normals; skybox color, the other lobes,
  reflection projection and water lighting read the centre normal only.

Extra-sample mechanism: `SampleWaterNormals` gains a `bool bPatchBlend` parameter; when it is false
each octave One `SAMPLE_NORMAL_PRECISE` call passes strength `0.0f` instead of its band strength.
`Water.frag` passes `true`; `ComposeWaterSpecular` passes
`globalLayout.iWaterNormalExtraSamplePatchBlend != 0`, a new `int32_t` water `GlobalLayout` field
after `fWaterFoamSpawnNoisePatchBlend`, which `PopulateWaterReducedUv` publishes as 1 when the
toggle is on and the level is High, else 0. At High the extra call therefore receives today's
strengths. Bands whose strength is 0 build no triangle (`MakeWaterNormalPatchTriangles`), and
`SampleWaterNormals` reads a band's triangle only when the strength it passes is positive.

### Cost per level

Derived by code read under the Context defaults, strengths 1, not measured. Fetches are maximum /
area mean; triangles are `MakePatchTriangle` calls per water fragment.

| Level | Normals | `Water.frag` | Triangles | Foam spawn per texel |
|---|---|---|---|---|
| Off | 18 / 18 | 22 / 22 | 0 | 1 / 1 |
| Low | 20 / 18.8 | 28 / 24.4 | 3 | 1 / 1 |
| Medium | 22 / 19.6 | 30 / 25.2 | 4 | 3 / 1.81 |
| High (today) | 30 / 22.9 | 42 / 30.1 | 7 | 3 / 1.81 |

Of High's 20 extra fragment fetches at most, the 16 from the metre-scale normal and foam lattices
are the ones waves are most likely to pay in full; Medium keeps 4 of them and Low 2, while the 4
color-noise fetches cost near their mean at every level. No speedup is claimed; the user measures.

### Alternative considered for Low

A shared per-effect domain warp (two noise fetches per fragment, then each read once at a warped
UV). The author recommends against it: it is a second algorithm beside patch blending, which the
user chose over a shared domain warp for this feature; it needs its own amplitude and scale
controls and warped-gradient handling for every read; it shifts repeated motifs without replacing
them; and its two extra fetches per fragment equal Low's whole small-lattice excess (band One
centre, at most two).

### Terrain and smoke/wind stages

The level is the shared Break Repetition setting. The author recommends that the terrain and
smoke/wind stages blend only at High: each publishes its strengths as wrapper values when the
toggle is on and the level is High, and 0 otherwise. Their costs are unmeasured, and this keeps
Medium and Low cheap for the whole feature; a stage whose own cost measurement suggests a Medium
or Low subset returns that proposal to the user. Both stage Plans depend on this Plan and state
the rule (`## Coordination`).

## Critical files

- `Engine/Source/Graphics/Render/WaterUniforms.cpp`
- `Engine/Data/Shaders/Water/WaterNormalSampling.h`
- `Engine/Source/Ui/GraphicsSettings.cpp`
- `Engine/Source/Ui/Screens/GraphicsMenuScreen.cpp`

## In scope

- `Engine/Source/Ui/GraphicsQualityWrappersBase.h/.cpp`: declare and define
  `gBreakRepetitionLevel`; the header comment names it with Water as consumed directly by
  per-frame rendering.
- `Engine/Source/Ui/GraphicsSettings.cpp`: the `uiBreakRepetitionLevel` field, the padding and its
  `static_assert`s, `kiVersion` 28, save, validate, load and reset.
- `Engine/Source/Ui/Screens/GraphicsMenuScreen.cpp` `GraphicsMenuScreen::Render`: the separator and
  the dimmed level row around the existing Break Repetition toggle.
- `Engine/Data/Shaders/ShaderGlobalLayout.h`: `int32_t iWaterNormalExtraSamplePatchBlend` after
  `fWaterFoamSpawnNoisePatchBlend`, CPU/GLSL order aligned.
- `Engine/Source/Graphics/Render/WaterUniforms.cpp` `PopulateWaterReducedUv`: the per-level strength
  publication and the extra-sample flag.
- `Engine/Data/Shaders/Water/WaterNormalSampling.h` `SampleWaterNormals`: the `bPatchBlend`
  parameter gating the three octave One strengths; `Engine/Data/Shaders/Water/Water.frag` centre
  call passes `true`; `Engine/Data/Shaders/Water/WaterSpecular.h` extra call passes the flag. Comments
  there that the change makes false.
- Docs: `Engine/Source/Ui/AGENTS.md` `## Graphics Quality Levels` and `## Settings Persistence`
  (six persisted levels; Break Repetition's level is consumed per frame); the
  `Documents/UserInterfaceDesign.txt` Graphics heavy-features bullet (the Break Repetition group
  with its toggle and dimmed level row after Water Foam); any other owning `AGENTS.md` sentence the
  diff makes false.

## Out of scope

- High's and Off's output and cost; foam coverage-gated blending or any other change to High.
- `kfPatchWeightThreshold`, `MakePatchTriangle`, `PatchVertexOffset`, `TextureGradPatched`,
  `WaterNormalGradPatched`, `TextureLodPatched`, lattices, seeds, triangle reuse.
- Domain warp or any second algorithm; a continuous quality slider; per-read player controls; the
  Tweaks screen and the seven Tweaks strength wrappers and their defaults.
- Terrain, smoke and wind reads (their Plans apply the rule); the ModelViewer Render quality panel;
  the probe schema and command reference; other `GraphicsSettings` fields; water mesh, wave and
  foam-system quality behavior; simulation, CRC, network, save and replay formats.

## Risk triggers and invariants

Change Workflow Tier 3: `GraphicsSettings.bin` serialization layout and version
(`GraphicsSettings.cpp` `kiVersion`, offsets) and the CPU/GLSL `GlobalLayout` scalar layout
(`ShaderGlobalLayout.h`), spanning independently owned Ui, Render and shader code. Writing this
Plan is Tier 1. Invariants: at toggle on and level High every strength and the extra sample's
strengths equal today's; toggle off publishes 0 for all seven; a zero-strength read keeps its plain
read with no triangle; no unbuilt triangle is read at any level; `kiVersion` 28 rejects every older
file; CPU/GLSL field order; Graphics labels unique per window; render state stays outside the CRC;
Tweaks wrapper values are never written by the setting.

## Acceptance criteria

Live verification is the user's manual test on the launched client: the executing session runs no
`/agent-harness` capture unless it asks the user and the user agrees. The supplied evidence records
no harness setup for the user's 3200 us and 4700 us measurements.

1. Decisive code read: at toggle on and level High each of the seven published strengths equals its
   wrapper value and the extra-sample flag is 1, so the extra `SampleWaterNormals` call receives
   today's strengths; at toggle off all seven are 0 at every level.
2. Code read: per-level publication matches `### Water per level`; fetch and triangle counts per
   level match `### Cost per level`, reported without a speedup claim.
3. Code read: the struct, `static_assert`s, `kiVersion` 28, save, validate, load and reset cover
   `uiBreakRepetitionLevel`; a version 27 file fails the version gate and an out-of-range level
   rejects the file.
4. `/glsl-review`: the flag is read only at the extra-sample call; CPU/GLSL field order matches; no
   level reads an unbuilt triangle.
5. Shader pack, BES Debug client and server, and ModelViewer Debug x64 build.
6. User: the Graphics screen shows the toggle with a Low/Medium/High row dimmed while the toggle is
   off (absent at the session's baseline commit by read); the level survives a restart; Defaults
   restores toggle on and Medium; the user judges each level's quality.
7. User: the Water GPU timer (`kGpuTimerWater`) per level at one fixed camera, in Immediate present
   mode or with fixed GPU clocks: Off near the 3200 us baseline, High near 4700 us, Medium and Low
   materially below High. If the user finds a level too costly or too poor, the session returns to
   the user rather than retuning the table.

## Coordination

`Documents/Plans/Engine/RandomizedPatchBlendingTerrain.md` and
`Documents/Plans/Engine/RandomizedPatchBlendingSmokeAndWind.md` depend on this Plan (their
metadata) and publish their strengths only when the toggle is on and the level is High
(`### Terrain and smoke/wind stages`). This Plan changes neither stage's files.

## Notes

The default level Medium is the author's recommendation; the user may choose another default when
judging the levels.
