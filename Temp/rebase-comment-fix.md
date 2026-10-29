# Rebase comment fix

## Scope
Conformance + non_structural; accepted F1, F2, F3 only.
Session baseline: cd6d410c064a5a5f905bf68efaf28493ab6fc33a.
Rebased HEAD: 1c3608533ba58d1a1327b51928ed4dced5426c53.

## Root causes and corrections
F1: CameraBase.h:80 described twice-default framing as fully zoomed out, while development framing maximum is 2000 (line 89 after edit). Replaced with shared framing fade endpoint description.
F2: WaterUniforms.cpp:61 described camera height, while the adjacent lerp consumes GetRenderFraming(). Replaced with framing-driven speed lerp.
F3: LightingUniforms.cpp:93 described a near-camera height endpoint, while all three following lerps evaluate kfCameraFramingDefault. Replaced with default-framing endpoint.

## Verification
git diff --check: PASS (exit 0).
git diff --unified=0 limited to the three assigned files: PASS; every added/removed source line begins with // after indentation.
No code lines changed; build provenance is semantically unaffected by these comment edits.
No build or runtime verification requested for comment-only correction.

## Exact source diff
```diff
diff --git a/Engine/Source/Graphics/CameraBase.h b/Engine/Source/Graphics/CameraBase.h
index 010402de..2a934639 100644
--- a/Engine/Source/Graphics/CameraBase.h
+++ b/Engine/Source/Graphics/CameraBase.h
@@ -80,2 +80 @@ public:
-	// Camera-height zoom-factor fade endpoint (2x default framing = fully zoomed out). Single-sources the water
-	// (WaterUniforms) and lighting (LightingUniforms) LerpAtHeight calls -- both reference this constant.
+	// Framing fade endpoint shared by water speed and lighting weights.
diff --git a/Engine/Source/Graphics/Render/LightingUniforms.cpp b/Engine/Source/Graphics/Render/LightingUniforms.cpp
index 28f50da9..58c056f7 100644
--- a/Engine/Source/Graphics/Render/LightingUniforms.cpp
+++ b/Engine/Source/Graphics/Render/LightingUniforms.cpp
@@ -93,2 +93 @@ void RenderLightingMain(int64_t iCommandBuffer)
-	// Full-detail reference weights for WATER_SPEC_AA_FADE_HANDOFF: the same LerpAtHeight the live
-	// fWaterNormalWeight* uploads above use, evaluated at the near-camera endpoint height.
+	// Full-detail reference weights for WATER_SPEC_AA_FADE_HANDOFF, evaluated at the default-framing endpoint.
diff --git a/Engine/Source/Graphics/Render/WaterUniforms.cpp b/Engine/Source/Graphics/Render/WaterUniforms.cpp
index b8d06e77..af950550 100644
--- a/Engine/Source/Graphics/Render/WaterUniforms.cpp
+++ b/Engine/Source/Graphics/Render/WaterUniforms.cpp
@@ -61 +61 @@ static void PopulateWaterReducedUv(shaders::GlobalLayout& rGlobalLayout, std::ch
-	// Camera-height-driven speed lerp — single-sourced fade endpoint shared with LightingUniforms.cpp.
+	// Framing-driven speed lerp.
```
