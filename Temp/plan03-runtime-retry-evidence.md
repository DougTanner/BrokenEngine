# Plan03 runtime retry evidence

## Identity

Worktree: C:/Users/dougt/.claude/worktrees/BrokenEngine/0cb8b7b1-9695-4b87-ab78-8bf06eceacad. Branch lighting-foundation-work. HEAD429a5782767c920d620482aa032ea5b783779859. Fixed foundation baseline b56f20fa44d0424c2fdf5003157563684ed747f5. Initial and final git status --porcelain were empty. No tracked changes, diagnosis, builds, or commits.

Build input: Temp/AgentBuildEnvelopes/compile-20261029T154650516Z-22496.md ## Build envelopes; Sandbox client/server Debug x64 Local. All artifact paths below are relative to this worktree. Runtime artifacts are under Temp/plan03-runtime-retry/.

## Claim

Exact invocation: pwsh -NoProfile -File .agents/skills/agent-harness/scripts/Invoke-HarnessClaim.ps1 -RepositoryRoot 'C:\Users\dougt\.claude\worktrees\BrokenEngine\0cb8b7b1-9695-4b87-ab78-8bf06eceacad' -Session 'plan03-runtime-retry' -Project 'BrokenEngineSandbox'.

Result: broken-engine-harness-claim/v1; status blocked; code claim.foreign-owner; claim null; packVersion null. Stale owner5ed3275b-828a-42b2-b117-5dded5af30fb, session normal-launch, foreign worktree3b12877a-3aff-4f6d-adc6-4dcde9046e42. claimedAt/heartbeatAt2026-10-29T16:07:36.453Z; hold1340.466s; wait0.101s/1attempt. Provision exit0. Minted unclaimed token d7263816-33f5-4b73-ab1d-9a6a963c278b.

No sandbox client, ModelViewer, sandbox server, or listeners on27100/27101/27102 remained at takeover inspection. No foreign process was quit or stopped. Worker steps11-15 conditional steal with --expect5ed3275b-828a-42b2-b117-5dded5af30fb succeeded. New owner a2534a80-d11e-4ec2-b9a8-794f9c850dd2; claimedAt2026-10-29T16:30:14.621Z; claimantPid27760; session plan03-runtime-retry; adopted worktree.

AgentHarness: Tools/AgentHarness/Platforms/VisualStudio2026/Output/AgentHarness.exe. Claim-selected data: Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/Output/Data beneath adopted worktree. No data fallback. prelaunch-lock.json and postserver-lock.json prove heartbeat advanced from16:30:14.621Z to16:30:45.271Z.

## Setup

Fresh roots Temp/plan03-runtime-retry/AppData and AppData-client were created with ErrorAction Stop, without Force; neither existed. Copied only the authoritative baseline save to the server game-name child and ClientGuid.bin to the client game-name child. No client state or tweaks settings copied. No old save conversion or regeneration attempted.

Server launch server-launch.json: pass/ok; PID4456; startTimeUtc2026-10-29T16:30:43.7640843Z; tick1. Client launch client-launch.json: pass/ok; PID29220; startTimeUtc2026-10-29T16:30:58.5959520Z; tick0. Each ping and processCheck passed with findings[]. Exact launch calls used Invoke-HarnessLaunch.ps1, own data, new per-role app-data roots and explicit --log-file paths.

Process state: Temp/plan03-runtime-retry/process-check-163014.json. Server log: Temp/plan03-runtime-retry/server-agent.log. Client log: Temp/plan03-runtime-retry/client-agent.log. Client isolation enumeration immediately before client launch found no client/viewer. status-after-rejected-load.json reports clientCount1, paused true, tick0, activeCoords[[0,0]].

restored.json: window_state minimized:false, width1600,height904. resize.json: width1920,height1080. Capture.ps1 invocation used OutDir absolute Temp/plan03-runtime-retry, owner a2534a80-d11e-4ec2-b9a8-794f9c850dd2, Prefix after, ClientStartUtc2026-10-29T16:30:58.5959520Z and default +60/+75s schedule.

## Rejected baseline load

Temp/plan03-runtime-retry/capture-output.txt contains: load: {"file":"lighting-strip-baseline-t400","paused":true,"resetToFresh":true}. Capture.ps1 exited1 at line61 with "Save was rejected (resetToFresh:true)." This happened before any timed screenshot or comparison. Retake count0; no repeat load, alternative save, conversion, or source modification.

Temp/plan03-runtime-retry/server-agent.log line23: [Tick: 757] ReadGrid lighting-strip-baseline-t400 failed: version 254 != 255. This is the exact observed setup rejection, not a diagnosis or permission to revise the baseline. Main must resolve the required baseline precondition before retesting.

## Criterion results

0/7 passed. Setup prerequisite blocked; no acceptance relaxed and no runtime criterion failure observed. No HARNESS-F finding or unexpected process exit.

Row3 BLOCKED: baseline load rejected; no timed screenshot, over8 comparison, or side-by-side inspection ran. Restore and resize passed. No retake used. Evidence: ## Rejected baseline load.

Row4 BLOCKED: shared scenario prerequisite failed before isolated point/event0/119 panel checks, skinned direct.r comparison, or traceOnlyInstances query. Evidence: ## Rejected baseline load.

Row5 BLOCKED: no wide1000source capture or finished.png exists for island self-shadow speckle inspection. Evidence: ## Rejected baseline load.

Row6 BLOCKED: no isolated-point capture or three-pixel hand-computed radiometry executed. Evidence: ## Rejected baseline load.

Row7 BLOCKED: no capture request issued, no exports, and 15-minute capture clock never started. Seven-file size/schema validation unavailable. Evidence: ## Rejected baseline load.

Row8 BLOCKED: whole planned scenario did not execute. Full actually-opened client log scan passed with zero matches, but this cannot settle the full criterion. Evidence: ## Log scans.

Row9 runtime BLOCKED: whole planned scenario did not execute. Both actually-opened process logs scan clean, but full-scenario CRC evidence is unavailable. Static portion remains in Temp/plan03-static-evidence.md ## Simulation. Evidence: ## Log scans.

## Log scans

After release, rg -n 'DebugUtilsCallback|CheckVk failed|VK_ERROR_DEVICE_LOST|DEBUG_BREAK at|SendDesyncReport|RecoverFromDesync|LogDifferences CRC' Temp/plan03-runtime-retry/client-agent.log Temp/plan03-runtime-retry/server-agent.log returned no output (no matches). Both entire opened logs preserved. No get_logs ring buffer was used. No DEBUG_BREAK at in either log.

## Terrain GPU timer

BLOCKED: no comparison-enabled island view established and no Lighting Terrain gpuTimers observation obtained. No timing inferred. Memory-cost document and link untouched; manager decides disposition. The 45-minute run clock began16:30:14.621Z and ended upon the successful release approximately16:31Z, well within budget.

## Release proof

Temp/plan03-runtime-retry/release.json: broken-engine-harness-release/v1; status pass; code ok; crashFindings[]; processCheckNote null. Both quits accepted with exitCode0 and empty result. Server PID4456 already-absent, stopAttempted:false. Client PID29220 exited-after-quit, stopAttempted:false. survivingPids[]. lockRelease attempted:true, exitCode0, detail empty. No forced stop.

Temp/plan03-runtime-retry/final-lock.json: {"held":false}. Release completed through Invoke-HarnessRelease.ps1 with exact launched PIDs and registered process-check state. No owned process/lock remains. No foreign resource was modified. All evidence retained.

## Residuals

The authoritative baseline save was rejected with version254 versus255. Rows3-9 and terrain timing remain incomplete. Diagnosis/fix or acceptance changes belong to main; this runtime-only worker did not attempt them. No new claim or runtime retry after the failed prerequisite.
