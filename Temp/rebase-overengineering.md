# Rebase C++ over-engineering finder

## Scope and inventory
Pass: finder. Worktree:
C:/Users/dougt/.claude/worktrees/BrokenEngine/0cb8b7b1-9695-4b87-ab78-8bf06eceacad. Baseline:
d170b18b843a3a96af227cef467b824b4aa214c5. Reviewed tip: 1c3608533ba58d1a1327b51928ed4dced5426c53.
Get-SessionChangeInventory.ps1 returned pass ok; output is
Temp/over-engineering-review-inventory.json. Scope restricted to C++ conflicts in
Temp/lighting-rebase-evidence.md ## Conflicts and resolutions; main-only changes excluded using
comparison to cd6d410c064a5a5f905bf68efaf28493ab6fc33a. The nine changed C++ conflict paths were
CameraBase.cpp/.h, Render/GlobalUniforms.cpp, LightingUniforms.cpp, MainUniforms.cpp,
WaterUniforms.cpp, Ui/GraphicsSettings.cpp, Ui/Screens/TweaksScreen/TweaksScreen.cpp, and
Projects/ModelViewer/Source/Graphics/Camera.cpp. TextureManager.cpp had no oldtip-to-newtip diff.

## Consumer proofs
- CameraBase.cpp:388-401 comparison branch retains the existing explicit fixture camera behavior while overriding the newly merged ordinary projection/water-eye state. Both disabled and enabled modes are reachable in Lighting::GetComparisonCamera (Lighting.cpp:412). Shadow framing floor feeds PopulateShadowArea (GlobalUniforms.cpp:301). No introduced unused state or fallback.
- CameraBase.cpp:526-539 GetRenderFraming is consumed by GlobalUniforms.cpp:487, LightingUniforms.cpp:26-28, and WaterUniforms.cpp:62,516. UpdateComparisonCamera is consumed by GameBase.cpp:953 for comparison rendering without an ordinary interpolate. Ordinary framing versus fixture eye-height tuning is an explicit fixed decision.
- MainUniforms.cpp:231-263 camera publication helper has calls at lines285 and336, including absent camera-interpolate rendering. New matrix, water eye, and projection flag publication updates current shader-facing state. Comparison shake suppression is retained existing behavior.
- GraphicsSettings.cpp new projection settings have save/load/reset consumers; FindInvalidGraphicsSetting reads file input and rejects through LoadGraphicsSettings. Layout/version checks are protected format checks. Version21 explicitly resolves the two incompatible version20 layouts.
- TweaksScreen.cpp:312-322 capture guard covers reachable receivers/tracing/composing states written in LightingComparison.cpp:164 and LightingCapture.cpp:34,51. It protects a current capture workflow and was already present before rebase.
- ModelViewer Camera.cpp ordinary pose setup moved into the shared calculation; no extra helper, state, or guard was introduced by the resolution. Remaining uniform edits rename the existing framing input and preserve current water/terrain consumers.

## Removal candidates
None. No scoped resolution machinery meets the impossible-state, unreachable-fallback, ultra-rare, unread-hash, or speculative-generality proof standard.

## Design suggestions
None. No tracked source edits or build/runtime operations performed. Concurrent documentation changes were observed in status and left untouched.
