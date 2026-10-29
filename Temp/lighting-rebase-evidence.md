# Lighting foundation rebase evidence

## Result
Worktree: C:/Users/dougt/.claude/worktrees/BrokenEngine/0cb8b7b1-9695-4b87-ab78-8bf06eceacad
Branch: lighting-foundation-work
Old HEAD: d170b18b843a3a96af227cef467b824b4aa214c5
New HEAD: 1c3608533ba58d1a1327b51928ed4dced5426c53
Old baseline: b56f20fa44d0424c2fdf5003157563684ed747f5
Exact new baseline: cd6d410c064a5a5f905bf68efaf28493ab6fc33a
Safety ref: refs/heads/safety/lighting-foundation-pre-rebase-20261029, pointing to old HEAD.
Main was never switched, modified, pushed, or landed. No runtime launch or acceptance revision.
User's latest synchronization request supersedes the fixed-baseline wording retained unchanged in Temp/lighting-plans/00-context.md. Manager owns subsequent note updates.

## Replay method
Used git rebase --rebase-merges --onto pinned-target old-baseline lighting-foundation-work.
The existing side branch (forked before old-baseline) and its six commits remain unchanged; the
original merge and five subsequent commits were replayed. At the merge stop,
git merge-tree --write-tree --merge-base=old-baseline pinned-target b50c0621 recreated the original
merge's deliberate resolutions plus current main, avoiding re-resolution of already settled
pre-baseline differences. Installed that tree into index/worktree and resolved its remaining
conflict content. Resolution-only fixups were autosquashed into the recreated merge; no extra fixup
commits remain.

## Commit map
b50c0621 -> bdaae5825a4f2e3e9dcf59b01af23f3896f8020b (merge)
f713d7cf -> f33e0bb28f3f0d32fc631591c5e650350632b7a6
b01f2286 -> d74644db1a26bb59ee387c36d05b2e1ba5d749a0
ee53a2c5 -> c70defb1ec83a9ea761773a86d6f1f3c574e67ad
429a5782 -> 209fc4a81c9780974fca2cf78b472ac1af2cdc97
d170b18b -> 1c3608533ba58d1a1327b51928ed4dced5426c53

## Conflicts and resolutions
23 initial merge conflicts, reduced to 16 new-main overlaps after preserving the original merge resolutions; one later Tweaks conflict. Unique conflict paths and decisions:
- Engine/Data/Shaders/Lighting/AGENTS.md: preserve resident-query documentation; removed legacy light-texture framing contract stays removed.
- Engine/Data/Shaders/Model/Model.frag: retain shared ModelForward include; main's changed legacy light projection lives in removed code.
- Engine/Data/Shaders/Objects/HexShield.vert: retain ordinary projection; removed light-deposit branch stays removed.
- Engine/Data/Shaders/Water/AGENTS.md: retain smoke and sun contracts without legacy emitter lighting.
- Engine/Data/Shaders/Water/WaterReflectionProjection.h: retain foundation deletion; main's orthographic changes target obsolete reflection implementation.
- Engine/Source/Graphics/AGENTS.md: combine main framing/FOV contract with explicit comparison camera and retained shadow contract.
- Engine/Source/Graphics/CameraBase.cpp: main framing, FOV, orthographic, water eye and sun time remain; legacy lighting texel reference removed. Comparison still uses explicit pose, fixed perspective projection, up vector and clipping; sets mbOrthographicProjection=false and water eye=explicit eye. Shadow uses renamed framing reference and retains comparison extent floor. GetRenderFraming returns ordinary framing or prior fixture eye-height tuning.
- Engine/Source/Graphics/CameraBase.h: retain main camera names and projection state; retain comparison helper with renamed GetRenderFraming; remove obsolete lighting texel state and correct associated local comments.
- Engine/Source/Graphics/Managers/TextureManager.cpp: legacy lighting texture sizing remains deleted.
- Engine/Source/Graphics/Render/AGENTS.md: restore prior merge resolution (no newer main edit); retain foundation render documentation.
- Engine/Source/Graphics/Render/GlobalUniforms.cpp: tune terrain normals through GetRenderFraming.
- Engine/Source/Graphics/Render/LightingUniforms.cpp: retain legacy publication deletion; retained water-normal controls use new framing constants and GetRenderFraming.
- Engine/Source/Graphics/Render/MainUniforms.cpp: retain helper and zero comparison shake; publish new projection matrix, water eye and orthographic flag in all helper paths.
- Engine/Source/Graphics/Render/WaterUniforms.cpp: main framing-based wave names/constants with GetRenderFraming to preserve explicit fixture tuning.
- Engine/Source/Ui/AGENTS.md: document combined settings version21 and new projection controls without removed lighting quality.
- Engine/Source/Ui/GraphicsSettings.cpp: preserve main FOV/orthographic fields, validation/load/save/defaults; remove foundation legacy lighting fields. Version21 rejects both incompatible version20 layouts; final size48, orthographic offset41, padding42, FOV44; assertions match.
- Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj: preserve original merge membership plus main configuration collapse.
- Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj.filters: preserve original merge membership plus main removals.
- Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandboxServer.vcxproj: same client/server membership intent with main configuration collapse.
- Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandboxServer.vcxproj.filters: preserve original merge membership plus main removals.
- Projects/ModelViewer/Platforms/VisualStudio2026/ModelViewer.vcxproj: original merge resolution preserved (no newer main edit).
- Projects/ModelViewer/Platforms/VisualStudio2026/ModelViewer.vcxproj.filters: original merge resolution plus main filter removal.
- Projects/ModelViewer/Source/Graphics/Camera.cpp: retain main default framing/shadow setup; omit removed lighting texel reference; shared calculation now establishes ordinary eye pose.
- Engine/Source/Ui/Screens/TweaksScreen/TweaksScreen.cpp (later cleanup replay): main live sun slider retained; foundation capture-state BeginDisabled/EndDisabled guard retained.
Settings/version and comparison-framing resolution were confirmed by manager during this dispatch.

## Decisive checks
- git merge-base --is-ancestor cd6d410c HEAD: exit0.
- git status --porcelain: empty after rebase and after evidence creation (evidence ignored).
- git ls-files -u: empty.
- git diff --check d170b18b HEAD: exit0.
- git diff --name-only d170b18b HEAD -- Temp: empty; every tracked Temp note/evidence remains byte-identical.
- Compared all 51 main-only changed paths (main delta minus old foundation delta) to pinned main: zero mismatches.
- git grep -I for retired camera identifiers and conflict markers in Engine source/shaders and ModelViewer source: no matches (exit1).
- git diff d170b18b HEAD for Model.frag, HexShield.vert and WaterReflectionProjection.h: empty; intended strip/deletion preserved.
- git range-diff b50c0621..d170b18b bdaae582..HEAD: four replayed commits patch-identical; only plan02 cleanup differs at its sun-slider context, preserving current main sun editing plus capture guard.
- Source inspection confirms ordinary main framing, explicit fixture perspective, new water eye and orthographic flag publication; no build or runtime verification performed.

## Required verification
Build through /compile: DataPacker Release|x64 (shader/data generation), BrokenEngineSandbox Debug|x64, BrokenEngineSandboxServer Debug|x64, ModelViewer Debug|x64; consumers need Local generation/RunDataPacker=true for changed shaders.
Focused fresh review: merged camera comparison/ordinary projection and shadow extent; render publication in Main/Global/Water/LightingUniforms; GraphicsSettings version21 layout and defaults; sun slider capture disable and actual sun edit; project membership/configuration retention. C++/GLSL/docs review routing belongs to manager.
No runtime run authorized. Plan03 remains blocked/incomplete because version254 baseline save is rejected by current version255; no acceptance or reference changes made.
