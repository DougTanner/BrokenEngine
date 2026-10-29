# Plan03 save rejection diagnosis

## Identity and scope

Read-only diagnosis at HEAD 429a5782767c920d620482aa032ea5b783779859, lighting-foundation-work, existing lighting worktree. Fixed foundation baseline b56f20fa44d0424c2fdf5003157563684ed747f5. No runtime run, claim, source/data edit, build, or commit. Only this requested durable evidence file was created. Initial git status --porcelain was empty.

## Reproducing signal

Existing execution: Temp/plan03-runtime-retry-evidence.md ## Setup identifies
Capture.ps1 invocation; ## Rejected baseline load records its failure.
Temp/lighting-strip-baseline/Capture.ps1:59 sends server load of
lighting-strip-baseline-t400 with pauseAfterLoad:true; line61 throws on
resetToFresh. Captured output reports resetToFresh:true and Save was
rejected. Temp/plan03-runtime-retry/server-agent.log:23 reports ReadGrid
lighting-strip-baseline-t400 failed: version 254 != 255. No
screenshot/comparison ran.

The static signal remains red: FileManager.h:277-283 reads two int64 header fields and requires riVersion == STRUCT_TYPE::kiVersion; GridSave.cpp:76-80 returns false immediately on failure. The actual save header is version254,size0; the launched binary expected255. This alone deterministically rejects the save before parsing its body.

## Header and artifact inspection

PowerShell read-only probe: [System.IO.File]::ReadAllBytes on each named save, [BitConverter]::ToInt64(bytes,0) and offset8, Get-FileHash -LiteralPath with SHA256. Enumerated only Temp/lighting-strip-baseline, Temp/plan03-runtime and Temp/plan03-runtime-retry plus named replay instructions; no AppData or unrelated-worktree search.

Original Temp/lighting-strip-baseline/AppData/Broken Engine Sandbox Server/lighting-strip-baseline-t400: 2547bytes,version254,size0,SHA256 DDC58C961B2EF2F396F4CAA614248E55BF005D6D8D1B471C02D4FCB5E42D6EDA.

Original ServerAutosave.save and copies of lighting-strip-baseline-t400 under Temp/plan03-runtime/AppData and Temp/plan03-runtime-retry/AppData have the same length/version/hash. Thus retry used the exact original save, not a damaged or wrong copy.

Retry Temp/plan03-runtime-retry/AppData/Broken Engine Sandbox Server/ServerAutosave.save: 886bytes,version255,size0,SHA256 5ECE58BAFFFDC5F9DDAF21A4CE8FA90B82901F58166EB5B3094E524EF8040412. This is compatible by header only, not an authoritative baseline. status-after-rejected-load.json shows tick0 after fallback; server-agent.log:23-29 shows rejection followed by fresh grid initialization. There is no captured tick400 workload or old-visual reference for this autosave.

## Version history and layout

git show -s --format='%h %p %s' bc69eea5 b50c0621 b56f20fa establishes: original baseline built at bc69eea5; merge b50c0621 parents are b56f20fa and f68c1cae. git log bc69eea5..b56f20fa returns f91254a9 and b56f20fa. Their merge base is aa47e4a3. The newer simulation change entered through merged main, not the lighting strip.

git show f91254a9 -- Engine/Source/Frame/Collections/Pushers/Pushers.h Engine/Source/Frame/Collections/Pushers/PushersUpdate.cpp proves PushersInterpolate::kiVersion changed4->5. Its comment explicitly gates both SOA layout AND push behavior. FrameBase.h:313 sums shared collection versions; Frame.cpp:106 includes that sum, causing254->255. The same pusher/save paths have no diff from b56f20fa to current HEAD.

Actual change: PushersUpdate.cpp:142-149 now treats XY distance <=0.0001m as coincident, adding signed0.001m X separation and skipping force. Previously only exact XY coincidence got separation and also unit-X force. Redundant correction resets were removed in ApplyPush and ShipBehaviours.cpp:605 onward. This changes deterministic simulation behavior and justifies invalidating replay/save version identity.

There is NO persisted field-layout change behind this increment. git diff
bc69eea5 HEAD -- Engine/Source/Frame/FrameBase.h
Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp is empty. Full
Engine/Source/Frame/Collections diff shows only pusher
behavior/version/comments; Pushers.h:68-77 member pointers and Members
tuple are unchanged. GridSave and game Save/FleetSerialization
serializers are unchanged. Other frame diffs are renderer changes,
comments, and removal of client IslandTemplate residency bounds. Do not
describe the rejection as an established byte-layout incompatibility:
it is an intentional simulation-version incompatibility.

## Hypotheses ruled out

Wrong or damaged save copy: ruled out by identical SHA256 across original and both retry copies; header is a valid old header rather than missing/truncated zeros.

Lighting changed the persisted collection schema: ruled out by version history, unchanged Members/Values/serializer diffs, and merged-main pusher behavior increment.

Unnecessary unexplained version-only bump: ruled out by concrete force/separation behavior change in f91254a9 and the explicit behavior-version contract at Pushers.h:36.

Harness ownership, resize, or capture fault caused rejection: ruled out for this symptom by server log's explicit version gate and code returning before body reads; resize had already returned1920x1080.

## Workload reproduction limits

Temp/lighting-strip-baseline/Setup.ps1 and setup-output.txt record reset+pause tick1, eight injected entries, pause tick358, exact stepping to400 at1/32, restoring1/1, save. Three Omnis and three Fighters spawn at200; two explosions at400. Current ServerSimulationHarnessRigs.cpp still accepts these payload types; it and StatusChange.h have no diff b56f20fa..HEAD. The mechanism is available on the approved current simulation base; no runtime reproduction was authorized or performed here.

Exact original state is not reproducible by simply rerunning setup. temp-replay-steps.md ## Setup that produced the save explicitly records two runs with different battleship1 positions. ServerFleetManager.cpp:19-21 TimeSeeds its random engine; ResetState:356-358 deliberately does not reseed. Saved RNG is restored by ServerFleetSerialization.cpp:300-306. The setup hardcodes an expired owner token; a future harness worker must use its current claim, not run that historical driver unchanged.

Recorded visual workload: temp-replay-steps.md ## Recorded camera and sim state has origin cell, fleet battleship globalId1, two battleships, three Omnis, four blasters, no fighters/missiles/explosions at rendered tick413; confirmed400 and client415. New setup uses equivalent injection workload but counts/positions at400/413 require observation, not assumption.

## Recovery choices and acceptance

No route within the existing literal same-save criterion can pass on version255. Do not patch header bytes, accept254, lower kiVersion, change simulation behavior, or use the fallback autosave. Those would bypass the repository's no-backward-compatibility rule or change approved base/criterion.

Recommended decision: authorize replacement of the obsolete version254
row3 reference with a matched version255 before/after reference on the
fixed merged simulation base. A pre-strip renderer source exists at
b56f20fa (WorldLightingShadowPipelines retains spread/combine
pipelines); generate and capture an equivalent workload there, then
reuse that exact new save and ClientGuid on current foundation. This
requires explicit approval of baseline identity/provenance changes and
a separate scoped build/capture action; no checkout, restoration, or
extra worktree is authorized by this diagnosis.

Alternative decision: retain old screenshots as historical context and approve a revised row3 visual-inspection criterion against an equivalent freshly generated current workload. This weakens causal before/after evidence and changes acceptance; do not silently treat it as satisfying the current row3. Original baseline identity cannot be preserved without reverting the merged simulation base or permitting conversion, both outside the assigned scope and contrary to fixed decisions.

Acceptance for recommended recovery: document chosen version255 pre-strip
reference commit and input provenance; save at tick400, retain exact new
ClientGuid, record scene counts/positions and captures. On current binaries
load that exact save with pauseAfterLoad:true, require resetToFresh:false
and server tick400. Use fresh client state, resize1920x1080,
+60/+75s close/wide schedule and eye heights156/665.117. Compare matched
shots using unchanged over8<=2% and existing single-retake/side-by-side
rule; no added/missing local glow. Then complete unchanged rows4-9, timer
observation, and normal resource release. Exact scene-state equivalence
must be demonstrated by saved state/scene records, not regeneration alone.

## Residual decision and verification

Proven root cause; recovery blocked on the user's acceptance/reference
decision. Existing row3 demands version254 same save while approved merged
base rejects254 intentionally; trust fixed base and version contract,
report row3 unsatisfied. No claim about matching screenshot output or
actual successful version255 load is made. No temporary instrumentation
or scratch scripts added. Build required for diagnosis:none; choosing
matched reference needs manager-scoped /compile work for Sandbox
client/server Debug x64 with matching Local data before captures. Source
remains untouched.
