# Zoom correction scope

## Changed files

Owned baseline: a66da68abfd942591d92c9d69489e5e240b927e4.
Four Temp/lighting-plans notes were already dirty; preserved their row3 corrections.

Engine/Source/Graphics/CameraBase.cpp — comparison framing consumers; shared clip constants.
Engine/Source/Graphics/CameraBase.h — expose existing clip constants for fixture reuse.
Engine/Source/Graphics/Lighting/Lighting.h — Lowest/Default enum names; explicit pose framing.
Engine/Source/Graphics/Lighting/LightingFixture.cpp — gameplay framing/eye/projection; bounded event zoom.
Engine/Source/Graphics/Lighting/LightingFixture.h — MakeCamera takes framing and existing pan.
Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenLighting.cpp — visible Lowest/Default zoom labels.
Engine/Source/Graphics/AGENTS.md — comparison framing contract.
Documents/Investigations/Engine/LightingFoundationHandoff.md — live presets; former recipe superseded.
Documents/Investigations/Engine/LightingPrototypeClustered.md — link current camera owner.
Documents/Investigations/Engine/LightingPrototypeDDGI.md — link current camera owner.
Documents/Investigations/Engine/LightingPrototypeDirectionalCache.md — link current camera owner.
Documents/Investigations/Engine/LightingPrototypeReGIRReSTIR.md — link current camera owner.
Documents/Investigations/Engine/LightingPrototypeSplitRadianceCascades.md — link current camera owner.
Temp/lighting-plans/00-context.md — current correction scope and canceled historical run.
Temp/lighting-plans/03-runtime-acceptance.md — current correction scope and canceled historical run.
Temp/lighting-plans/03-runtime-results.md — current correction scope; short smoke resume only.
Temp/lighting-plans/CODEX-HANDOFF.md — current correction scope; short smoke resume only.

## Verification

Static checks against a66da68a: PASS, markdown-links36, file-hygiene17, line-length11.
Get-SessionChangeInventory: PASS,17 modified paths, vcxprojCandidates empty.
Git diff --check: PASS. Old fixture poses, old camera enum names and UI labels: no code matches.
LightingCameraPose consumers: CameraBase, Lighting::GetComparisonCamera, MainUniforms shake suppression.
No GPU/serialized/CRC layout consumes this client-only CPU pose. Numeric capture camera slots remain0/1/2.
No capture throughput, cancellation, readiness, source/solid/random generation or event timing edits.
Expected eye-above-target at default FoV15: minimum255.85254m/default511.70509m; +lowest flight plane.
Event0 minimum; event59 onward default; event60..119 retains pan15..900m and original cell rebase.
User minimum/default instruction supersedes prior plan900/6000m choice. No foundation completion claim.

## Build scope

BrokenEngineSandbox client Debug|x64 and ModelViewer Debug|x64 consume all six changed C++ files.
Changed .cpp members: Engine/Source/Graphics/CameraBase.cpp, Engine/Source/Graphics/Lighting/LightingFixture.cpp,
Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenLighting.cpp.
BrokenEngineSandbox server Debug|x64: requested shared-header/guard verification; no server behavior changed.
Caller must run /compile and fresh-binary preset/save/readiness smoke at1920x1080; not executed here.
Independent C++, style/comments, coherence and progressive-disclosure review remain caller-owned.
