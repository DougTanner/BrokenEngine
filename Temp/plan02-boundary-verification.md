# Capture boundary focused verification

## Result

NEEDS_ACTION. CXX005/ADV001 is RESOLVED by independent source tracing of the frozen capture visibility correction. No reachable lifecycle
defect remains in the assigned path. CXX006 Required remains: Engine/Source/Graphics/Lighting/Lighting.h:78 and LightingComparison.cpp:103
grow ApplyComparisonSettings to two bool parameters, contrary to cpp-conventions.md Flags over booleans and
repo-code-review/references/checks.md Repository patterns. No human override was supplied; manager explicitly accepted this finding and
selected a single bCaptureAllowed prerequisite computed from paused and visible at the caller, with a combined prerequisite diagnostic. That
removes the multiple-boolean trigger without new types or global dependencies; preserve the validated ordering and independent
settings-change failure. No source edits, builds or runtime execution were performed here.

Scope PASS: the caller/declaration/definition and two visibility branches implement the explicitly authorized capture boundary without new state, identities, options or unrelated behavior. Convention compliance is independently Required.

All twelve target paths match their frozen Temp/plan02-boundary-review.patch sections exactly against round origin
b01f2286e10144f36a8031f4daad51c060015151. Nine paths have identical patch sections to Temp/plan02-fix-review.patch. For the other three,
reversing only the two visibility branches and signature/caller substitutions reproduces the exact Base64 original-byte snapshots in
Temp/plan02-capture-boundary-before.json. Thus prior reviewed bytes, including CXX001-004 and the Tweaks disable scopes, are preserved; this
review does not reopen the unchanged foundation.

## Acceptance proof

- Active invalidation: LightingComparison.cpp:75 defines receivers/tracing/composing as running. The new guard at :109 fails all three when hidden before both the older unpause/settings check and :119 equal-settings return. Failure is sticky until a fresh request is serviced.
- Input/publication ordering: Main.cpp:436 ProcessInput precedes :446 ClientUpdate and :455 Render. GameBase.cpp:127 toggles visibility; :942 passes its current value before camera/interpolation and :962 RenderGlobal. GlobalUniforms.cpp:509 then reads Camera::SunAngle. Sandbox Camera.cpp:81-92 selects the override from Tweaks visibility. The effective sun change cannot precede failure in this supported sequence.
- Graphics reachability: UiScreens.cpp:55-69 exposes Graphics only when Tweaks is hidden. Therefore the High-to-Low water path is preceded by failure, regardless of whether its refresh destroys comparison resources. Tweaks navigation remains enabled; Sun Angle and non-Lighting contents remain disabled during the running states at TweaksScreen.cpp:312-323,410-412.
- Hidden queued request: TweaksScreenLighting.cpp:60-76 increments and publishes the request during ImGui Prepare. That occurs after GameBase's check. On the next frame F3 can hide Tweaks first; LightingComparison.cpp:144 copies requested settings and :149 consumes the request before :159 rejects hidden admission. Reopening unchanged settings returns at :119; changing other settings returns at :147 when the request is already serviced. Neither path resurrects it.
- Fresh visible request: a new button increment changes settings equality, reaches the request service, and passes the added visibility test while visible. Existing reference-mode, pause and extent checks still gate receivers admission at :173. Existing active unpause/settings invalidation and publication-count checks are unchanged.
- Final composition ordering: Graphics.cpp:213 prepares UI, then :215 calls SubmitMainCommandBuffer. CommandBufferManager.cpp:202 calls
  RecordPreparation synchronously on the main thread, including Lighting.cpp:372 RecordCapture. Only that operation can transition tracing
  to composing and enable the descriptor (LightingCapture.cpp:50-51). Graphics.cpp:217 synchronously calls FinishCapture after UI
  submission; LightingCapture.cpp:112 disables the descriptor after successful or handled failed export. No subsequent ProcessInput can
  interleave between those operations. If hidden at the earlier boundary, RecordCapture's :24 status guard returns and cannot enable the
  reference. An ordinary composing state therefore cannot survive into the next input iteration; its inclusion in the new guard is
  consistent with the status contract.
- Gameplay zero: the sole true SetLightingReference call remains LightingCapture.cpp:50; initialization and teardown set false. LightingCompose.h:8-9 returns zero when disabled. Visibility failure before recording does not enable reference lighting, and normal composing cleanup still disables its framebuffer. No new descriptor path was added.
- Consumer caution for CXX006 correction: a direct game::gpGame read is not automatically available in LightingComparison.cpp. ModelViewer Pch.h includes Engine.h without GameBase.h or Game.h, and ModelViewer builds this source even though it has no ApplyComparisonSettings caller. Sandbox Pch.h supplies GameBase.h but no concrete Game.h. Preserve both compilation consumers when correcting the signature; visibility is owned by GameBase::mbShowImGui, not an evidenced UiManager member.
- Recorded checks: whole-tree symbol search found exactly declaration, definition and sole caller; focused git diff --check returned no diagnostics. Measure-Tokens reports GameBase.cpp 11652 tokens and LightingComparison.cpp 5329 tokens; the focused change exposes no concrete cohesive size split. Supplied build envelope records three successful rebuilt targets, but this review claims source verification only. Plan03 still owns runtime acceptance.
