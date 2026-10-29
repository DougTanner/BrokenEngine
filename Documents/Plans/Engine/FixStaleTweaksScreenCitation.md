<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-29T14:29:11.294Z","dependsOn":[]} -->
# Fix Stale TweaksScreenBase Citation in UpdateThirdPartySubmodules

## Context

`Documents/Features/Engine/UpdateThirdPartySubmodules.md`, in the imgui bullet's "Accepted visual-only changes" line, cites typed slider input at `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenBase.cpp:240,347`. That file no longer exists: commit `86a91acc` ("Make the profile manager an engine-owned engine::ProfileManager") renamed `engine::TweaksScreenBase` to `engine::TweaksScreen`, together with its files `TweaksScreenBase.cpp/.h` (now `TweaksScreen.cpp/.h`).

When commit `1e88528b` wrote the citation, those two lines were the two `ImGui::SliderFloat` calls in the tweaks screen:

- `TweaksScreen::WrapperSlider`, `if (ImGui::SliderFloat(pcImGuiLabel, &fValue, pWrapper->mfMin, pWrapper->mfMax, "%.6f"))`, now at `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreen.cpp:205`.
- The sun-angle slider, `if (ImGui::SliderFloat("##Sun Angle", &fSunAngle, gSunAngleOverride.mfMin, gSunAngleOverride.mfMax, "%.6f"))`, now at `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreen.cpp:312`.

The citation was already stale at baseline `aa47e4a3`. No live Plan owns this citation.

## Design

The fix is a citation correction in the Features document. It changes no code.

1. At implementation time, find the two `ImGui::SliderFloat` calls in `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreen.cpp` and read their line numbers again, because line numbers drift.
2. In the "Accepted visual-only changes" line, replace `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenBase.cpp:240,347` with `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreen.cpp:<WrapperSlider line>,<Sun Angle line>`. Leave the rest of the line unchanged.

Change Workflow tier: Tier 1. The trigger is a documentation-only edit with no public signature or invariant exposure (`.agents/references/risk-tiers.md`). It has no exposure to determinism, CRC, serialization, `.pack`, `kiVersion`, replay, wire, affinity, threading, allocation, shaders, builds, or live verification.

## Critical files

- `Documents/Features/Engine/UpdateThirdPartySubmodules.md`: the imgui bullet's "Accepted visual-only changes" line.
- `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreen.cpp`: read only, to get the line numbers of the two `ImGui::SliderFloat` calls.

## In scope

- The `TweaksScreenBase.cpp:240,347` citation in the "Accepted visual-only changes" line of `Documents/Features/Engine/UpdateThirdPartySubmodules.md`.

## Out of scope

- The line's other citations (`MenuUtils.cpp`, `HudScreen.cpp`), and every other citation in the document.
- Any source, shader, or project file.
- Any other edit to the Features document.

## Acceptance criteria

- `Documents/Features/Engine/UpdateThirdPartySubmodules.md` no longer contains `TweaksScreenBase`.
- The new citation names `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreen.cpp`, and each cited line is one of the two `ImGui::SliderFloat` calls in that file.

## Notes

- Found by a documentation propagation pass in an unrelated HUD and chat UI change. That change shares only this Features document.
