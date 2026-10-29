# Plan 03 runtime checkpoint

## Identity

Worktree: C:/Users/dougt/.claude/worktrees/BrokenEngine/0cb8b7b1-9695-4b87-ab78-8bf06eceacad.
Branch: lighting-foundation-work. Session start: ee53a2c5986a363b94adc82789c634e1dbe312e3.
Retry checkpoint: 429a5782767c920d620482aa032ea5b783779859.
Fixed foundation baseline: b56f20fa44d0424c2fdf5003157563684ed747f5.
User instruction: do plan03 only in this existing worktree. No runtime/source changes here.

## Acceptance results

Plan03 is incomplete. No acceptance criterion was waived or marked complete without evidence.

| Row | Result | Evidence |
| --- | --- | --- |
| 1 | PASS | Retained current builds; static evidence `## Builds`. |
| 2 | PASS | Main ran both exact handoff check-2 patterns; exit 1/no matches for each. |
| 3 | BLOCKED | Baseline save rejected before timed comparison; retry `## Criterion results`. |
| 4 | BLOCKED | Fixture visibility not run; retry `## Criterion results`. |
| 5 | BLOCKED | Island speckle inspection not run; retry `## Criterion results`. |
| 6 | BLOCKED | Radiometry not run; retry `## Criterion results`. |
| 7 | BLOCKED | Capture completion/export validation not run; retry `## Criterion results`. |
| 8 | BLOCKED | Full scenario not run; startup log scan alone is insufficient. |
| 9 | Static PASS; runtime BLOCKED | Static `## Simulation`; scenario CRC evidence unavailable. |

Row2 searched Engine and explicit Projects/*/Source directories using both patterns from
`Documents/Investigations/Engine/LightingFoundationHandoff.md` check 2, unchanged.
Row9 static inspection found no foundation simulation/CRC changes. Retry process logs scanned clean,
but the planned scenario did not run; this does not pass rows8-9.

## Current save-version blocker

Retry acquired the harness, launched client/server, restored the client and resized to 1920x1080.
Loading the authoritative save returned resetToFresh:true before screenshots or comparisons.
Server log: `ReadGrid lighting-strip-baseline-t400 failed: version 254 != 255`.
The copied save matches the original hash; no compatible authoritative baseline is available.

Diagnosis proves an intentional merged-main pusher simulation behavior/version change, with no
persisted field-layout change. Preserve the required version255 base; no source fix or conversion
was authorized. The literal same-save criterion remains unsatisfied.

User decision pending: matched version255 pre-strip/current reference, or a weaker fresh-scene
visual criterion. Neither is approved; do not implement either or infer an acceptance waiver.
Retry release passed; both processes exited normally, final lock held:false, no owned resources.
This in-scope acceptance blocker does not create a follow-up Plan.

## Earlier claim history

Harness claim timed out after 480.598 seconds over 97 attempts; claim was null.
Foreign normal-launch server PID31016 and client PID14100 and their lock remained untouched.
No process or lock was owned by this session; the claim waiter exited, so no release applied.
Runtime evidence `## Claim` and `## Release proof` hold the claim and final inventories.
The later retry resolved this claim blocker; it is history, not the current obstacle.

## Memory-cost question

No comparison-enabled island view was available; Lighting Terrain GPU timing is unavailable.
Retain `Documents/Investigations/Engine/LightingTerrainPreparationMemoryCost.md` and its
handoff link until the plan03 timing rule can be evaluated. No timing value inferred.

## Evidence index

- `Temp/plan03-static-evidence.md` `## Builds`: row1 envelopes, artifacts and freshness.
- `Temp/plan03-static-evidence.md` `## Simulation`: row9 static diff and CRC inspection.
- `Temp/plan03-runtime-evidence.md` `## Claim`: bounded claim timeout and foreign ownership.
- `Temp/plan03-runtime-evidence.md` `## Criterion results`: unexercised rows3-9.
- `Temp/plan03-runtime-evidence.md` `## Terrain GPU timer`: unavailable timing and retained doc.
- `Temp/plan03-runtime-evidence.md` `## Release proof`: no owned processes/lock; foreign untouched.
- `Temp/plan03-runtime-retry-evidence.md` `## Setup`: successful launches and 1920x1080 resize.
- `Temp/plan03-runtime-retry-evidence.md` `## Rejected baseline load`: version254/255 rejection.
- `Temp/plan03-runtime-retry-evidence.md` `## Criterion results`: rows3-9 remain blocked.
- `Temp/plan03-runtime-retry-evidence.md` `## Log scans`: clean opened logs, incomplete scenario.
- `Temp/plan03-runtime-retry-evidence.md` `## Release proof`: clean release, no owned resources.
- `Temp/plan03-save-diagnosis.md` `## Reproducing signal`: deterministic header-version rejection.
- `Temp/plan03-save-diagnosis.md` `## Version history and layout`: behavior version bump.
- `Temp/plan03-save-diagnosis.md` `## Recovery choices and acceptance`: pending choices and checks.

## Resume

Resume `Temp/lighting-plans/03-runtime-acceptance.md` only in this worktree and branch.
Await the user's baseline/reference decision before another runtime attempt or baseline change.
Use the diagnosis `## Recovery choices and acceptance` for the scoped action after approval;
neither choice is currently authorized. Do not patch save headers or lower the required version.
After the chosen prerequisite is approved and satisfied, use /agent-harness for a fresh claim.
Keep original checks exact wherever the user has not changed them, including rows4-9 and timing.
Run rows3-9 in one owned client/server session, obtain Lighting Terrain timing, apply the plan's
memory investigation disposition, and release owned resources. Retain static/build evidence only
while inputs stay valid; do not stop or alter foreign resources.
Update this checkpoint and `00-context.md` `## Progress` with actual results and evidence.
Keep plan03 unchecked until every row passes and the memory-cost question is settled.
Preserve completed plan02 history and the fixed baseline; do not start plans04/05.
