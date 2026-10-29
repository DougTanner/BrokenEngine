# Plan 03 runtime checkpoint

## Identity

Worktree: C:/Users/dougt/.claude/worktrees/BrokenEngine/0cb8b7b1-9695-4b87-ab78-8bf06eceacad.
Branch: lighting-foundation-work. Session start: ee53a2c5986a363b94adc82789c634e1dbe312e3.
Retry checkpoint: 429a5782767c920d620482aa032ea5b783779859.
Current foundation baseline: cd6d410c064a5a5f905bf68efaf28493ab6fc33a.
Rebased source checkpoint: 1c3608533ba58d1a1327b51928ed4dced5426c53.
Prior baseline b56f20fa44d0424c2fdf5003157563684ed747f5 and retry identities describe history.
Rebase record: `Temp/lighting-rebase-evidence.md`
`## Commit map` and `## Conflicts and resolutions`.
User instruction: do plan03 only in this existing worktree. No runtime/source changes here.

## Acceptance results

Plan03 is incomplete. No acceptance criterion was waived or marked complete without evidence.
Row1 records fresh rebase builds. Rows2/9 static revalidation is pending; their historical PASS
results cannot pass the rebased source. Runtime rows3-9 remain blocked; no runtime since rebase.
The current builds passed with zero diagnostics: DataPacker Release x64 and client/server/
ModelViewer Debug x64, all consumers using Local generation/RunDataPacker=true.
C++, shader, style/membership, DOC1 and comments pass; the camera ADV001 correction passes fresh
independent C++/adversarial retest. Final rebase static checks pass: InvokeStaticChecks on
cd6d410c passed 4 checks (2 packages, 93 links, 227 hygiene files, 78 line-length files).
Final acceptance review PASS: criteria 8/8, findings none. Plan03 rows2/9 remain pending.
The baseline remains pinned to the user-request target despite main independently advancing to
40b82adfe90b447ee848120b4a26bad546b83a49; no further rebase is authorized here.

| Row | Result | Evidence |
| --- | --- | --- |
| 1 | PASS | Fresh rebase build envelope indexed below. |
| 2 | PENDING | Historical PASS; both exact check-2 patterns need revalidation. |
| 3 | BLOCKED | Baseline save rejected before timed comparison; retry `## Criterion results`. |
| 4 | BLOCKED | Fixture visibility not run; retry `## Criterion results`. |
| 5 | BLOCKED | Island speckle inspection not run; retry `## Criterion results`. |
| 6 | BLOCKED | Radiometry not run; retry `## Criterion results`. |
| 7 | BLOCKED | Capture completion/export validation not run; retry `## Criterion results`. |
| 8 | BLOCKED | Full scenario not run; startup log scan alone is insufficient. |
| 9 | Static PENDING; runtime BLOCKED | Historical static PASS; revalidation pending. |

Historical row2 searched Engine and explicit Projects/*/Source directories using both patterns from
`Documents/Investigations/Engine/LightingFoundationHandoff.md` check 2, unchanged.
Historical row9 inspection found no foundation simulation/CRC changes. Retry logs scanned clean,
but the planned scenario did not run; this does not pass rows8-9.

## Current save-version blocker

Current source requires version256: main advances PushersInterpolate version5 to version6.
The prior version255 rejection below remains historical evidence; the version254 save still
fails the current exact-version gate. No new runtime attempt was made after rebase.

Retry acquired the harness, launched client/server, restored the client and resized to 1920x1080.
Loading the authoritative save returned resetToFresh:true before screenshots or comparisons.
Server log: `ReadGrid lighting-strip-baseline-t400 failed: version 254 != 255`.
The copied save matches the original hash; no compatible authoritative baseline is available.

Diagnosis proves an intentional merged-main pusher simulation behavior/version change, with no
persisted field-layout change. Preserve the current required version256 base;
no source fix or conversion was authorized. The literal same-save criterion remains unsatisfied.

User decision pending: matched current-version pre-strip/current reference, or a weaker fresh-scene
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

- `Temp/AgentBuildEnvelopes/compile-20261029T190055928Z-45844.md`
  `## Rebased lighting foundation builds`: current row1 DataPacker Release x64 build.
- `Temp/AgentBuildEnvelopes/compile-20261029T192103385Z-5736.md`
  `## Camera fix Debug x64 builds`: current row1 client/server/ModelViewer Debug x64 builds.
- `Temp/lighting-plans/rebase-20261029.md` `## Verification`: completed reviews and final checks.
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
Preserve completed plan02 history; use the current Identity baseline for new diffs and retain
old evidence as historical. Do not start plans04/05.
