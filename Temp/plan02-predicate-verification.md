# Capture predicate focused verification

## Result

PASS. CXX006 is RESOLVED: Lighting.h:78 and LightingComparison.cpp:103 now declare/define one bool parameter, bCaptureAllowed. The only caller, GameBase.cpp:942, computes paused && mbShowImGui. The repository convention prohibits multiple bool parameters; this signature no longer triggers it. No flags type, state member, global dependency or new abstraction was introduced. Scope PASS for authorization, minimality and KISS.

CXX005/ADV001 lifecycle equivalence is preserved. Independent examination of the exact substitution confirms identical capture failure/admission conditions and ordering, with the approved combined prerequisite diagnostic. The separate settings-change reason remains when prerequisites hold. No new finding. Source verification only; no build or runtime was executed in this review.

## Proof

- Boundary: all twelve paths in Temp/plan02-boundary-targets.json match their sections in immutable Temp/plan02-predicate-review.patch exactly against b01f2286e10144f36a8031f4daad51c060015151. Nine patch sections are identical to Temp/plan02-boundary-review.patch; only GameBase.cpp, Lighting.h and LightingComparison.cpp differ. The already reviewed foundation is not reopened.
- Snapshot preservation: decoded Base64 before-bytes match their recorded SHA256 and the materialized files. Applying only the documented
  signature/caller replacement, merged active guard and merged admission guard to those before-bytes reproduces current raw SHA256 exactly:
  Lighting.h 349AB626406B442A8D04D488BA2C29789820903368787587CB1FE217E66B12C6; LightingComparison.cpp
  EB0EF99CD68DE288B877C76CFC508B02148D4D4468402962DFD0ABEBCAF03520; GameBase.cpp
  8DCA332DB2ECD13DEA95402F5EFB95E9AB3BC9C0DF62B46C17487C44D601C918. Thus every other byte in those three files is preserved. Materialized
  before-files have Git blob IDs 71b3143a0dff49b3ff5bec42b918c7d12542ade3, df2ee40e07e8c2616c0ddf64c1e6dde07974a0e5 and
  fba50d30c867a0441526b9041d2d894113cdb2d7, matching the preceding patch endpoints.
- Caller and publication: current GameBase::Render was read in full. The argument is true exactly when paused and Tweaks is visible. ApplyComparisonSettings still precedes camera/interpolation, deferred-swapchain return and RenderGlobal publication.
- Active invalidation: IsCaptureRunning still covers receivers, tracing and composing. The former ordered hidden guard plus unpause/settings
  guard fail iff active && (!visible || !paused || settingsChanged). The new guard fails iff active && (!(paused && visible) ||
  settingsChanged), the same condition. Failure remains before the equal-settings return at LightingComparison.cpp:114. The settings-change
  diagnostic remains at :112 whenever capture is permitted; other prerequisite failures use the approved combined error. Full function
  context confirms no reordered wait, state assignment or publication operation.
- Hidden queued requests: settings are assigned at :139 and request identity is consumed at :144 before the new admission test at :154. A hidden or unpaused request is consumed and rejected; opening Tweaks later cannot restart it. Both equal-settings return and already-serviced identity return remain unchanged.
- Fresh permitted request: a new identity with paused && visible passes the merged prerequisite. The fixture-reference and framebuffer-extent checks retain their order and exact conditions. Successful admission still sets receivers and the tile count at :164-166.
- Composition and prior lifecycle proof: LightingCapture.cpp, Lighting.cpp, TweaksScreen.cpp, TweaksScreenLighting.cpp and all other target
  bytes match the earlier frozen boundary patch. Combined with equivalent active/admission conditions, this preserves
  Temp/plan02-boundary-verification.md ## Acceptance proof for same-frame composing, failure before visibility-driven publication changes,
  disabled input controls and zero-reference cleanup. Those unaffected paths were verified by snapshot identity rather than retraced.
- Integration: rg across Engine and Projects finds exactly the declaration, definition and sole caller. The Lighting implementation introduces no GameBase or game::gpGame reference, preserving ModelViewer compilation independence. No membership trigger.
- Static checks: git diff --check on the three assigned files returned exit 0 without diagnostics. Measure-Tokens returned GameBase.cpp 11653 tokens and LightingComparison.cpp 5270 tokens; the predicate change exposes no concrete cohesive split. git diff --no-index returned its ordinary difference exit code 1 while displaying the expected delta; all byte assertions passed.
- Intent: the accepted CXX006 correction and Temp/lighting-plans/02-reviews.md authorize exactly this three-site substitution. Binding capture decisions in Documents/Investigations/Engine/LightingFoundationHandoff.md ## Decisions remain satisfied. No pending external API claim, unrelated audit, source edit, build or runtime claim.
