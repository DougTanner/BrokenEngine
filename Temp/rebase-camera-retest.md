# Camera correction independent retest

## Disposition

PASS. ADV001 is closed by source proof for current reachable camera producers.
No new runtime verification is claimed. No source edits, builds, or harness operations were performed.
Scope is the four authoritative targets in Temp/rebase-camera-review-targets.json, baseline
1c3608533ba58d1a1327b51928ed4dced5426c53, with the exact focused pre-fix snapshots and patches
identified in Temp/rebase-camera-fix.md ## Before snapshot. Authorized integration baseline is
cd6d410c064a5a5f905bf68efaf28493ab6fc33a. The snapshot SHA256 values independently match that record.

## Coverage proof

CameraBase.cpp:387-405 independently selects ordinary framing or fixture eye Z without writing
mfCameraFraming. The latter is height above the footprint plane Z=0, not height above target Z=6.
GridCoord.h:70-73 rebases only XY. LightingFixture.cpp:111-129 normalizes eye and target into the same
cell, preserves Z, and fixes FOV at XM_PIDIV4, near at 0.1 and far at 20000.

CameraBase.cpp:473-510 uses this selection consistently for LOD, bucket, and both hysteresis rules.
At settled ordinary framing 348, ordinary has LOD 0, bucket 348 and upper key word zero. Close has
height 900, LOD 1, bucket 900 and upper key 0x44610000. Wide has height 6000, LOD 2, bucket 6000 and
upper key 0x45BB8000. These LOD transitions are outside the 5% hysteresis bands. Returning ordinary
returns the upper key to zero, even if its zoom bucket or LOD happens to match a fixture.

CameraBase.cpp:514-524 puts the fixture height bits in the upper 32 bits of a uint64_t key. All
current fixture heights are finite and positive (900 through 6000), so entry/exit cannot collide
with ordinary zero. The existing lower-word dimensional XOR cannot cancel these upper bits.
The cast to uint64_t precedes the shift; the uint64_t member retains the entire value. Float bits
are compared for cache identity, never numerically ordered or converted through a truncating cast.
The bucket's integer truncation and hysteresis cannot hide a changed fixture height from the key.
Every mismatched key recomputes both quad dimensions from the newly unprojected, expanded raw area.

LightingFixture.cpp:55-68 is the complete current fixture pose producer, selected without additional
pose modifications by Lighting.cpp:412-419. Events 0 through 59 change eye Y and Z together, with
target Z=6 and up=+Z unchanged. Their height increments are much larger than float precision, so
every changed zoom/tilt relatches. Event 0 equals close; event 59 equals wide. Events 60 through 119
retain wide height/tilt and translate eye and target equally in X (15 through 900). Their unchanged
height correctly retains quad dimensions: intersection with Z=0 translates without changing span.
MakeCamera cell normalization and CameraBase's render-basis rebasing are also planar translations.
Thus events 59/60 and a normalized-cell crossing cannot require a new extent solely from the pan.
Switching between equal-height wide and event poses is likewise only translation.

LightingComparison.cpp:103-140 applies settings; GameBase.cpp:945-954 applies settings before the
camera update and calls UpdateComparisonCamera for an absent interpolate frame. CameraBase.cpp:540-546
reaches the same calculation for active comparison poses. No reset of ordinary framing is required.
Default bucket zero (CameraBase.h:212) differs from every real ordinary/fixture bucket, forcing first
initialization. ResetForSession preserves the existing cache, but a changed fixture state or return
to ordinary still changes the upper key. An identical pose/extent after reset may safely reuse it.

GlobalUniforms.cpp:535 publishes the newly snapped area. Water/Water.vert:41 derives mesh positions
from that area. RenderWaterMain (WaterUniforms.cpp:545-558) selects the same LOD table and dimensions
as the camera latch; BufferManager.cpp:742-774 guarantees positive quad counts for every LOD.
The stale close-sized span underlying ADV001 therefore cannot survive close-to-wide or mode entry.

## Ordinary invariants and target coverage

The focused patch leaves ordinary eye distance, view, perspective and orthographic matrices,
water eye, near/far clipping, zoom limits, and framing interpolation untouched. Local
fVisibleAreaFraming equals the same mfCameraFraming formerly read directly by the LOD and bucket
code, including both hysteresis comparisons. The upper key word remains zero for ordinary cameras.
The lower key now uses the effective FOV in radians, the same value used for projection. A changed
ordinary FOV still invalidates it; the orthographic discriminator remains present. The wrapper's
ordinary supported FOV is 5 through 25 degrees (GraphicsSettingsWrappersBase.cpp:16). Fixture user
FOV/orthographic changes cannot override its explicit perspective calculation.

All four target paths were traced. CameraBase.h changes one private cache field width and the
already-reviewed framing comment. LightingUniforms.cpp changes only the full-detail reference
comment, whose default-framing LerpAtHeight inputs were checked in function context. WaterUniforms.cpp
changes only the framing-speed comment, whose GetRenderFraming input was checked in context.
No new collection, serialized layout, shader layout, CRC state, public signature, or build-affinity
change exists. ModelViewer/Source/Graphics/Camera.cpp:26-30 still uses the ordinary calculation;
its independent model orbit matrices are unaffected. All CameraBase member definitions remain in
CameraBase.cpp. The changed region exposes no concrete file-size split requiring a residual.

Authorization, minimality and KISS passes: PASS against the brief's preserve-both-intents correction.
No external API verification request and no project membership trigger.

## Recorded checks

git diff --check: exit 0.
Measure-Tokens.ps1, documented array invocation: CameraBase.cpp 6073 tokens / 551 lines;
LightingUniforms.cpp 2929 / 129; WaterUniforms.cpp 8303 / 574.
Read existing Temp/AgentBuildEnvelopes/compile-20261029T192103385Z-5736.md
## Camera fix Debug x64 builds: client, server, ModelViewer each exitCode 0, status success,
diagnostics empty, diagnosticsTruncated false, retainedLog complete. These are supplied recorded
build results, not builds performed by this reviewer.

Reviewed target SHA256 values:
CameraBase.cpp AB7A644BC9A7145FFB69821BD6DD527A83A6BBA1936E2A788E8478DDCDB88545
CameraBase.h AEFB3CD7FB125C2CF387319C6F6313846B8A853109E7023572003E468CE6FC18
LightingUniforms.cpp 0A97012E173515D75DA974489AD94C1AAE0496FA5DF29F7F47F052F249170175
WaterUniforms.cpp 22C861B4D9B2090C7EC6D7C38A496387CDFA9412359B3E7D29F3E63A54584EFE
