# Plan 03 current static acceptance

## Scope

Reviewed HEAD a49b61276f424515ff80f746c58ac89f4e73f520 in lighting-foundation-work.
Comparison base: cd6d410c064a5a5f905bf68efaf28493ab6fc33a, the merged main baseline.
`git merge-base --is-ancestor <base> HEAD` returned 0.
No runtime, rebuild, source modification, or primary-checkout operation was performed.
The historical plan03-static-evidence.md was not used as proof for this baseline.

## Row 1 current builds

PASS. Parsed recorded JSON build envelopes and checked their retained logs.
DataPacker Release x64: success, exitCode 0, started 19:01:15.451Z, elapsed 41.862 s.
Source: Temp/AgentBuildEnvelopes/compile-20261029T190055928Z-45844.md
Selector: ## Rebased lighting foundation builds
Log: Temp/AgentBuildLogs/datapacker-20261029T190115453Z-5528.log
The log ends with the successfully linked DataPacker.exe.

Latest Debug x64 builds all report status success, exitCode 0, no diagnostics:
- Sandbox client: started 19:21:21.848Z, elapsed 56.740 s.
- Sandbox server: started 19:22:31.791Z, elapsed 12.765 s.
- ModelViewer: started 19:22:57.806Z, elapsed 23.912 s.
Source: Temp/AgentBuildEnvelopes/compile-20261029T192103385Z-5736.md
Selector: ## Camera fix Debug x64 builds
Each carries /p:DataBuildMode=Local and /p:RunDataPacker=true.
GeneratedDataIncludeRoot and GameDataDirectory point into this worktree's Output tree.
Retained logs checked:
- Temp/AgentBuildLogs/brokenenginesandbox-20261029T192121850Z-16816.log
- Temp/AgentBuildLogs/brokenenginesandboxserver-20261029T192231792Z-45476.log
- Temp/AgentBuildLogs/modelviewer-20261029T192257807Z-3852.log
All end with their expected Debug executable; server data packing reports Success.

Freshness check: git status shows no source changes; only the baseline save is modified.
Enumerated git-tracked Engine/Common/DataPacker/Projects C++/header/shader/project inputs.
None has LastWriteTimeUtc later than the first latest Debug build's start time.
Thus current executable source predates all three latest Debug builds.
The save modification does not affect compiled source or establish runtime CRC acceptance.

## Row 9 static examination

PASS for the static portion only. Executed and read the complete scoped diff:
`git diff cd6d410c064a5a5f905bf68efaf28493ab6fc33a -- Engine/Source/Frame`
with Projects/BrokenEngineSandbox/Source/Frame supplied in that same comparison.
It contains 12 files, 64 insertions and 137 deletions:
- Four AGENTS.md updates remove descriptions of legacy diffuse deposit rendering.
- AreaLightsRender.cpp and PointLightsRender.cpp remove legacy deposit buffers and draws.
- HexShieldsRender.cpp removes the legacy lighting pipeline and GPU lighting fields.
- Collection.h renames texture registration inside existing BT_CLIENT guards.
- BattleshipsRender.cpp and FightersRender.cpp retain off-screen model poses for tracing.
- MissilesRender.cpp similarly writes visible-prefix and trace-only-tail GPU instances.
- OmnisRender.cpp skips fully destroyed visual instances and registers rendered models.

All seven changed Render.cpp files are enclosed by BT_CLIENT guards.
Model changes write GPU layouts, skinning reservations, render counters and lighting lists.
Their shared/interpolated input columns remain read-only.
Collection.h registration still requests source texture chunks and records client residency;
TextureManager.cpp's RegisterCollectionTextureCrc body confirms this client-only role.
No changed simulation phase, shared column, frame Values/Collections/Crcs, serialization,
identity, transfer, or CRC-participation definition appears in the scoped diff.
`git diff <base> -- '*Pusher*'` is empty: upstream pusher behavior remains in the baseline.
`git diff --check` passes.

## Limits and remaining acceptance

Full row 9 remains for the runtime worker: search both scenario --log-file paths for
SendDesyncReport, RecoverFromDesync, LogDifferences CRC and DEBUG_BREAK at.
No absence-of-desync or CRC-runtime result is inferred from this static examination.
The separately authorized SAVE header 254-to-256 edit is neither simulation evidence
nor permission for compatibility changes; no such source change was made here.
