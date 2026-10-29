# Rebase propagation and documentation

## Scope and identity

Owned conflict contracts: `Temp/lighting-rebase-evidence.md` `## Conflicts and resolutions`.
Current baseline: cd6d410c064a5a5f905bf68efaf28493ab6fc33a.
Rebased source: 1c3608533ba58d1a1327b51928ed4dced5426c53.
Old baseline: b56f20fa44d0424c2fdf5003157563684ed747f5.
Old foundation: d170b18b843a3a96af227cef467b824b4aa214c5.
No source or AGENTS.md edits required by this pass. No primary changes, commit, build, or runtime.
Project/configuration verification belongs to the concurrent mechanic.

## Trigger outcomes

5/5 settled by focused inspection and old/new tracked searches; independent correctness review
and compilation remain manager work.

1. Camera rename/framing: REFUTED missing propagation. Compared old foundation and new main
   against the old baseline (`Temp/rebase-old-intent.diff` and `Temp/rebase-main-intent.diff`).
   Foundation removes legacy texel lighting and adds explicit comparison pose; main adds framing,
   true vertical FOV, orthographic projection and sun clock. Merged CameraBase retains both.
   GetRenderFraming feeds terrain detail normals, water normal weights, reduced UV speed and
   Gerstner amplitude. Ordinary framing, comparison eye-height tuning and shadow extent floor
   survive. ModelViewer uses framing/default shadow initialization and shared matrix calculation.
2. Camera CPU/GPU publication: REFUTED missing propagation. PopulateMainCamera writes projection,
   world/model eyes, water eye and orthographic W flag through both ordinary and empty-frame
   calls. Comparison disables shake, sets orthographic false and uses explicit eye/up/clips with
   LightingFixture::MakeCamera's XM_PIDIV4 FOV. Shared ShaderMainLayout is the single CPU/GLSL
   layout; ShaderFunctions::ToEyeLine consumes the W flag, Water.frag consumes water eye.
   ModelViewer's independent Main publisher writes the new fields too. No new descriptor,
   push-constant, enum or shader binding contract is introduced by these conflict resolutions.
3. Settings serialization: REFUTED missing propagation. Old foundation and main each used
   incompatible version20 layouts; merged GraphicsSettings version21 rejects both. Save, field
   validation, load, reset, wrapper defaults and assertions agree: size48, orthographic41,
   projection padding42, FOV44. Removed lighting flag/cadence/quality are absent.
4. Legacy and capture controls: REFUTED missing propagation. Model.frag retains ModelForward;
   HexShield.vert ordinary projection and deletion of WaterReflectionProjection match old
   foundation. TextureManager has no delta from old foundation. Tweaks retains main's live sun
   pointer while disabling it for receivers/tracing/composing. Current AGENTS guidance agrees.
5. Resume/identity: RESOLVED stale baseline and current version/build claims in the four edited
   files below. Old retry/review reports remain unchanged. Plan03 remains incomplete; no changed
   acceptance, runtime reference, plan04 or plan05 work.

## Save-version proof

`git diff --unified=0 d170b18b843a3a96af227cef467b824b4aa214c5 HEAD -- Engine/Source/Frame Projects/BrokenEngineSandbox/Source/Frame`
filtered to added/removed kiVersion/kiNavDataVersion lines has only PushersInterpolate 5 -> 6.
Frame.cpp keeps base144 and summed collection versions; FrameBase.h sums server collections
including PushersInterpolate. Thus the historically runtime-proven version255 becomes 256.
GridSave.cpp ReadAndValidateVersionHeader uses game::Frame::kiVersion before reading records;
FileManager.h accepts only the current exact version. Saved version254 remains incompatible.
No post-rebase rejection log was produced or claimed.

## Exact edits

- Documents/Investigations/Engine/LightingFoundationHandoff.md: current graphics version21 and
  check9 diff baseline cd6d410c; criterion wording unchanged.
- Temp/lighting-plans/00-context.md: Identity new baseline/source/safety ref and rebase evidence;
  Progress old verification marked historical, new builds/static checks pending and version256.
- Temp/lighting-plans/CODEX-HANDOFF.md: resume new baseline/source/rebase record and rebuild/static
  requirements; historical255 retained and current256/no authorized acceptance change explicit.
- Temp/lighting-plans/03-runtime-results.md: current Identity and rebase record, historical table
  clearly labeled, new build/static verification pending, current256 blocker and resume baseline.

## Decisive checks

- `git grep -n -E 'GetRenderEyeHeight|mfCameraEyeHeight|mfLightingTexelEyeHeight|mfLightingTexelFraming|mMatPerspective|WaterReflectionProjection' -- Engine/Source Projects/ModelViewer/Source Engine/Data/Shaders`: exit1, no retired references.
- Tracked new-name searches GetRenderFraming/mbOrthographicProjection plus water-eye and shader W
  flag searches: inspected definitions, consumers and ModelViewer counterpart; no missing sites.
- `git diff d170b18b HEAD -- Engine/Source/Graphics/Managers/TextureManager.cpp`: empty.
- Get-AffectedAgentsDocs.ps1 with 13 owned source paths: pass; 13 chains, omittedCount0, existing
  TextureManager chain over advisory target (16157), below warning; no new AGENTS prose needed.
- Get-SessionChangeInventory.ps1 baseline cd6d410c: pass; owning membership candidate from this
  conflict scope is removed WaterReflectionProjection.h, routed to concurrent project mechanic.
- Invoke-StaticChecks.ps1 baseline cd6d410c -IncludeUntracked: pass (validate-skill,
  markdown-links, file-hygiene, line-length); complete receipt Temp/rebase-propagation-static.json.
- `git diff --check`: exit0 after documentation changes.

## Required verification

Build required: DataPacker Release|x64; BrokenEngineSandbox Debug|x64;
BrokenEngineSandboxServer Debug|x64; ModelViewer Debug|x64. Consumers use Local generation and
RunDataPacker=true. Earlier envelopes cannot verify rebased source. Manager owns fresh review
of camera/shadow contracts, Main publication, settings layout and sun control. Static acceptance
rows2/9 need current evidence. Plan03 runtime remains blocked pending user reference decision.
