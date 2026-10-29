# Plan 03 runtime evidence

## Claim

Run date: 2026-10-29 UTC. Adopted worktree: C:/Users/dougt/.claude/worktrees/BrokenEngine/0cb8b7b1-9695-4b87-ab78-8bf06eceacad. Branch lighting-foundation-work. No tracked edits, game launches, harness socket commands, captures, or acquired lock.

Command from adopted worktree root:
`pwsh -NoProfile -File .agents/skills/agent-harness/scripts/Invoke-HarnessClaim.ps1 -RepositoryRoot 'C:\Users\dougt\.claude\worktrees\BrokenEngine\0cb8b7b1-9695-4b87-ab78-8bf06eceacad' -Session 'plan03-runtime' -Project 'BrokenEngineSandbox'`

Claim result: schemaVersion broken-engine-harness-claim/v1; status blocked; code claim.wait-timeout; claim null; project BrokenEngineSandbox; packVersion null. Message: "Harness lock 'default' is held by owner 5ed3275b-828a-42b2-b117-5dded5af30fb since 2026-10-29T16:07:36.453Z (614.295 seconds) and was still held after waiting 480.598 seconds over 97 attempt(s)."

Minted but unclaimed token: e80232a1-72d9-4e28-9690-578459a4be3f. AgentHarness: adopted worktree Tools/AgentHarness/Platforms/VisualStudio2026/Output/AgentHarness.exe. Data directory: adopted worktree Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/Output/Data. Provision exitCode 0, output: "Shared worktree dependencies validated for 'C:\Users\dougt\.claude\worktrees\BrokenEngine\0cb8b7b1-9695-4b87-ab78-8bf06eceacad' using primary 'C:\Users\dougt\Documents\BrokenEngine'."

Foreign owner: 5ed3275b-828a-42b2-b117-5dded5af30fb; session normal-launch; worktree C:/Users/dougt/.claude/worktrees/BrokenEngine/3b12877a-3aff-4f6d-adc6-4dcde9046e42; claimedAt and heartbeatAt 2026-10-29T16:07:36.453Z; claimantPid 28388. Final lock metadata: Temp/plan03-runtime/final-lock.json. Initial lock metadata: Temp/plan03-runtime/preclaim-lock.json.

The enclosing exec command returned exit1 while printing the explicit blocked claim JSON; classification follows the JSON, not an invented process failure. Claim process PID25800 was created at 16:09:43.903875Z and had exited by final inspection after 16:17:50Z. Manager instructed stopping at this bounded claim result, without re-invoking the wait.

## Criterion results

0/7 passed. All criteria blocked by the Claim section; no acceptance was waived.

| Row | Result | Exact unexercised check |
| --- | --- | --- |
| 3 | BLOCKED | No owned client; 1920x1080 resize, timed baseline captures, over8 comparison and side-by-side inspection not run. No retake used. |
| 4 | BLOCKED | Isolated panel shadow, skinned events 0/119 and off-screen traceOnlyInstances not run. |
| 5 | BLOCKED | No wide capture; island self-shadow speckle inspection not run. |
| 6 | BLOCKED | No isolated-point raw export; three-pixel radiometry not run. |
| 7 | BLOCKED | No capture requested; wide1000source256sample completion and seven-file validation not run; 15-minute capture clock never started. |
| 8 | BLOCKED | No owned client log opened; full-scenario Vulkan log scan unavailable. |
| 9 runtime | BLOCKED | Neither owned process log opened; desync/CRC scan unavailable. Static portion belongs to manager. |

Baseline save, screenshots, Capture.ps1 and Compare.ps1 were present. Prepared only Temp/plan03-runtime/AppData/Broken Engine Sandbox Server/lighting-strip-baseline-t400 and Temp/plan03-runtime/AppData-client/Broken Engine Sandbox/ClientGuid.bin in fresh app-data roots. Copied GUID matches authoritative baseline GUID SHA256 2C089A51FE21DF3AD3A7D987A4640E319AD84A10753676A8CF6F6BF528648BB0. No ClientState/TweaksSettings copied.

## Terrain GPU timer

BLOCKED: no comparison-enabled owned client or island view; gpuTimers Lighting Terrain was not queried. No timing value inferred. Memory investigation and its handoff link retained. The runtime 45-minute budget begins only on successful claim, so never started.

## Release proof

No release call applies: claim null proves no lock acquired and no server/client launch call
occurred. Final `AgentHarness.exe lock status --key default` still reports the foreign owner in
Temp/plan03-runtime/final-lock.json. Final Win32_Process inventory in
Temp/plan03-runtime/final-processes.json reports exactly foreign server PID31016 and client
PID14100, both under worktree3b12877a-3aff-4f6d-adc6-4dcde9046e42, both without --agent-port. No
game process belongs to the adopted worktree. Foreign resources left untouched. Exact Get-Process
PID25800 returned no process, proving the claim waiter ended.

The first isolation enumeration also listed foreign client PID14100; its output appeared with the completed claim command. An earlier progress message incorrectly said none were listed before the buffered output was available; final inventory settles the actual environment.

## Residuals

Foreign normal-launch client/server and lock block isolation and ownership. Rerun /agent-harness after the user closes that pair and lock policy permits a claim. Rows3-9 and memory timing remain wholly incomplete. No HARNESS-F finding: setup limitation only, with no owned process failure or observed criterion failure.
