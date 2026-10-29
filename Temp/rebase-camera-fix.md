# Focused camera latch correction

## Before snapshot

Assignment: conformance, non_structural correction of accepted ADV001 in Temp/rebase-adversarial.md ## Findings.
Controlling instruction: preserve both ordinary main framing semantics and explicit fixture perspective behavior.
Baseline cd6d410c064a5a5f905bf68efaf28493ab6fc33a; rebased head 1c3608533ba58d1a1327b51928ed4dced5426c53 plus
pre-existing staged comments/docs.

Exact byte copies captured before edits: Temp/rebase-camera-before.cpp (SHA256
8BF6A44FF89F221BAE15000247AE885DB55C61619F1AA7F0700C3B8D32509D2D) and Temp/rebase-camera-before.h (SHA256
53F09C4CD8F4F34BBDBD5B88CC6A2E6213CB284E4F2774F91908BE3A6A4E18E7). Header snapshot includes another worker's
already-completed comment correction. Temp/rebase-camera-before-AGENTS.md holds HEAD's unchanged pre-fix graphics
guidance.

Focused source deltas against those snapshots: Temp/rebase-camera-fix-cpp.patch and Temp/rebase-camera-fix-header.patch.
Documentation delta: Temp/rebase-camera-fix-docs.patch. No stage, commit, primary-checkout changes, or delegation.

## Resolution

CameraBase.cpp:387-401 selects mfCameraFraming for ordinary visible-area LOD/bucket and explicit comparison eye Z for
fixtures; the projection key receives the effective projection FOV. The footprint plane is Z=0 and cell rebasing changes
XY only, so immutable fixture-local eye Z is the relevant extent and avoids basis-dependent subtraction.

CameraBase.cpp:473-509 consistently uses that selected value in LOD, bucket, and both hysteresis rules. Existing
ordinary framing, eye placement, orthographic dimensions, water eye, near/far clips, and zoom limits are unchanged.

CameraBase.cpp:514-524 retains the existing dimensional/projection key in the lower 32 bits and stores exact
fixture-height bits in the upper 32 bits. Ordinary uses zero in the upper half; every fixture eye height is positive.
CameraBase.h:213 widens the private latch key to uint64_t. Entry/exit cannot collide with fixtures through the
dimensional XOR. Every fixture zoom height changes the upper half, including a hypothetical within-bucket change;
immutable fixture pans retain the same footprint size.

Graphics/AGENTS.md ## World and Camera Contracts qualifies the ordinary framing rule and states the fixture LOD/bucket
exception and latch invalidation. No duplicate subsystem prose added. Existing doc-budget advisories remain:
Engine/Source/AGENTS.md 3846/3531 and Graphics/AGENTS.md 4198/2653 tokens at discovery; unrelated trimming is outside
scope.

## Verification

Source trace with settled ordinary framing 348, unchanged framebuffer, and unchanged user FOV: ordinary uses bucket 348,
LOD 0, upper key zero; close uses bucket 900, LOD 1, upper key bits of 900.0f; wide uses bucket 6000, LOD 2, upper key
bits of 6000.0f; returning ordinary restores bucket 348, LOD 0, upper key zero. Each transition changes the key and
recomputes both latched quad dimensions from the newly unprojected render area. Existing 5% LOD hysteresis does not
alter these representative values.

Source trace for all current immutable fixtures: LightingFixture.cpp:55-67 fixes target Z=6, up=+Z, and eye/target X
together; eye Y and Z both vary only with event zoom. MakeCamera:111-129 keeps fixed 45-degree perspective and clips
while normalizing cells. Event indices 0-59 change height and tilt and therefore relatch. Indices 59-119 retain height
and tilt while panning, so no extent relatch is necessary; both raw corners and the snapped origin translate. Cell
normalization and camera-basis rebasing preserve eye Z and footprint dimensions.

Ordinary zoom/FOV trace: selected visible-area framing remains mfCameraFraming; ordinary LOD/bucket hysteresis is
textually unchanged except for the equivalent local name. Ordinary FOV and orthographic controls still change the lower
projection key while ordinary eye/projection calculation remains untouched. Fixture matrix FOV remains
comparisonPose.fVerticalFieldOfView (XM_PIDIV4), independent of user FOV.

Propagation search: git grep over Engine/Source and Projects found the latch key and bucket only in CameraBase; public
LOD readers are DescribeScene, WaterUniforms and ProfileScreens. They consume the updated LOD unchanged.
GlobalUniforms.cpp:535 publishes the corrected snapped area; Water.vert consumes that area for water vertices, including
receiver draws. ModelViewer's derived camera still calls the generic calculation; server remains protected by BT_CLIENT.

Identity/layout search: no collection, CPU/GPU struct, serialized/save/replay record, CRC membership, or wire format
references the private latch cache. No version changes required. Get-SessionChangeInventory invocation completed
pass/nontruncated; Temp/rebase-camera-inventory.json contains the receipt. Filtering its vcxprojCandidates to the three
owned paths yields none: no project-membership trigger.

Recorded command: git diff --check exited 0. Invoke-StaticChecks.ps1 -RepositoryRoot <this worktree> -Baseline
cd6d410c064a5a5f905bf68efaf28493ab6fc33a -IncludeUntracked exited 0: status pass, truncated false; validate-skill,
markdown-links, file-hygiene and line-length all passed. This run occurred after the final source/docs edits and before
this evidence file was authored.

Self-audit: fixture height sufficiently identifies footprint changes -> inspect every camera/event producer -> resolved:
all current tilt changes accompany height changes, while fixed-height pan translates the footprint. Mode changes ->
inspect upper-key discriminator -> resolved: zero ordinary versus positive fixture eye heights. Ordinary invariants ->
compare focused patch -> resolved: no ordinary camera matrices or framing state changed.

Build requests for manager's builder, not executed here:
Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj Debug|x64;
Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandboxServer.vcxproj Debug|x64;
Projects/ModelViewer/Platforms/VisualStudio2026/ModelViewer.vcxproj Debug|x64. Client and ModelViewer consume
CameraBase.cpp and its changed header; server is requested by the assignment as guard/regression coverage. No data
regeneration or save-header change required by this correction.

Independent reviewer should try to disprove coverage on ordinary -> close -> wide -> ordinary, compare event 0/59/60/119
(including a normalized-cell crossing), and verify ordinary user FOV/orthographic transitions. No harness was run.
Manager owns deciding runtime verification and the still-pending runtime-baseline question.
