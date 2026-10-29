## C++ review

PASS: all six targets traced. Fixture and gameplay use
framing*tan(25.3125deg/2)/tan(15deg/2), giving eye
distances255.85254/511.70509 and valid +Y up. Clips share
unchanged constants; ModelViewer orbit clips remain independent.
Explicit framing feeds LOD, shadow reference and render
framing; eye/FOV/preset changes invalidate the visible-area
latch. Fixed presets ignore event zoom; event interpolation
remains150..300 with unchanged pan/rebase. Settings equality
cancels changed captures; metadata preserves indices0/1/2.
Pose remains CPU/client-only, outside save/CRC/wire/GPU
layouts. No scope or membership findings.

## Checks

Token counts: CameraBase.cpp6016; LightingFixture.cpp2397; TweaksScreenLighting.cpp821. No qualifying cohesive split. Git diff --check passed. Runtime smoke remains manager-owned.
