# Foundation minimum/default zoom completion

## Bounded completion

The latest request, "Just use lowest and default zoom", is complete for the common lighting
prototype foundation. Lowest/Default use normal top-down gameplay framing150/300 and FoV15.
Observed eyeZ values are261.8525085/517.7050171; Default is unchanged at event119.
The source checkpoint is a66da68a plus the reviewed camera/documentation correction.
No prototype algorithm was added. kiTilesPerFrame remains120; no throughput tuning was attempted.

Fresh client/server/ModelViewer Debug x64 Local builds PASS with zero diagnostics and
RunDataPacker=true. Source was unchanged between that build and the smoke check.
Build envelope: Temp/AgentBuildEnvelopes/compile-20261029T220641317Z-14916.md
## Normal/minimum fixture camera builds.

The fresh-binary smoke check passed6/6 in approximately81s: patched save without fresh fallback,
paused server tick400/clientCount1, Lowest, stable Default/event119,1920x1080 PNGs, clean logs
and release. Both screenshots were inspected. There are no owned processes or harness locks.
Evidence: Temp/foundation-zoom-smoke-evidence.md ## Criterion results, ## Images,
## Logs and owned release.

Source reviews PASS: Temp/foundation-zoom-style.md (style/comments),
Temp/foundation-zoom-docs.md ## Review (coherence/progressive disclosure),
Temp/foundation-zoom-cpp-review.md ## C++ review and
Temp/foundation-zoom-adversarial.md ## Review.
Final static checks passed all four triggered checks.
No build or smoke check remains pending.

## Historical limits and next action

The original exhaustive Plan03 matrix remains incomplete and unchecked. No full reference
export, radiometry, skin/offscreen pixel verdict or exhaustive event validation is claimed.
The old6000m capture reached54960 tiles in905.680s before user cancellation. Its old deadline
FAIL remains historical; it is not a failure of the superseding bounded camera/smoke request.
Evidence: Temp/plan03-complete-runtime-evidence.md ## Supersession and historical deadline
and ## Final criterion disposition. Its ## Final logs and owned release records clean release.

Retain the memory investigation/link based on the historical wide-view GPU evidence in that
file's ## Final terrain GPU time (current1168.405ms/average1163.397ms/max1198.686ms).
The normal presets were not rebenchmarked; no current normal-preset performance claim follows.

Row3 retains its ordinary-gameplay matched-reference fallback PASS, with no added/missing local
glow observed. The numeric retake39.650%/19.892% over8 failed; no pixel-match PASS is claimed.
Evidence: Temp/lighting-matched-reference/reference-off-runtime-evidence.md
## Row3 comparison and visual verdict. This historical comparison was not repeated at new zooms.

Do not automatically restart long captures, benchmarks or reference switching. Wait for user
direction on future prototype validation or the next plan. Plans04/05 were not executed or claimed.
