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
Current rows1/2 PASS; row9 static PASS against cd6d410c. Main's current exact check-2 searches
both returned no matches. Runtime rows3-9 remain incomplete; the last harness session
released its owned resources.
The current builds passed with zero diagnostics: DataPacker Release x64 and client/server/
ModelViewer Debug x64, all consumers using Local generation/RunDataPacker=true.
C++, shader, style/membership, DOC1 and comments pass; the camera ADV001 correction passes fresh
independent C++/adversarial retest. Final rebase static checks pass: InvokeStaticChecks on
cd6d410c passed 4 checks (2 packages, 93 links, 227 hygiene files, 78 line-length files).
Final rebase acceptance review PASS: criteria 8/8, findings none; plan03 runtime remains pending.
The baseline remains pinned to the user-request target despite main independently advancing to
40b82adfe90b447ee848120b4a26bad546b83a49; no further rebase is authorized here.

| Row | Result | Evidence |
| --- | --- | --- |
| 1 | PASS | Latest shader-validation build envelope indexed below. |
| 2 | PASS | Main's current exact check-2 searches both returned no matches. |
| 3 | PENDING | User approved matched cd6d410c reference; historical drift is not a pass. |
| 4 | FAIL; retest pending | HARNESS-F-001; readiness gate required. |
| 5 | PENDING | Current island speckle verification pending. |
| 6 | PENDING | Current radiometry verification pending. |
| 7 | FAIL; retest pending | HARNESS-F-001; capture did not export. |
| 8 | FAIL; retest pending | HARNESS-F-002; shader repairs built/reviewed. |
| 9 | Static PASS; runtime FAIL | HARNESS-F-002; complete-log retest pending. |

Historical row2 searched Engine and explicit Projects/*/Source directories using both patterns from
`Documents/Investigations/Engine/LightingFoundationHandoff.md` check 2, unchanged.
Historical row9 inspection found no foundation simulation/CRC changes. Retry logs scanned clean,
but the planned scenario did not run; this does not pass rows8-9.

## Current save input and authorization

The user's literal correction, "Update the *save file", authorized this one developer baseline
header254-to-current256 update and supersedes the prior header-patch prohibition.
No runtime compatibility code or acceptance waiver is authorized.
Current input under `Temp/lighting-strip-baseline/AppData/`:
`Broken Engine Sandbox Server/lighting-strip-baseline-t400`.
Original sibling backup: `lighting-strip-baseline-t400.version254.bak`.
Both files are 2547 bytes; only offsets 0,1 changed and bytes 8:end remain identical.
Updated SHA256: 42DD5DA00A924D39B95D985ACD09D9F104972BBD0946DBEF2FFCB1154E0AAF55.
Backup identity and byte proof: `Temp/plan03-save-update.md` `## Recorded byte verification`.
The runtime worker reports resetToFresh:false, paused tick400, clientCount1, counts2/3/4/0/0.
Latest owned release passed; runtime acceptance remains incomplete; no visual criterion settled.

## Earlier save-version blocker history

Current source requires version256: main advances PushersInterpolate version5 to version6.
The prior version255 rejection below is historical; the authorized update above resolves loading.

Retry acquired the harness, launched client/server, restored the client and resized to 1920x1080.
Loading the authoritative save returned resetToFresh:true before screenshots or comparisons.
Server log: `ReadGrid lighting-strip-baseline-t400 failed: version 254 != 255`.
The copied save matched the original hash at that retry.

Diagnosis proves an intentional merged-main pusher simulation behavior/version change, with no
persisted field-layout change. Preserve the current required version256 base;
no source fix or runtime conversion is authorized. Current acceptance remains pending above.

The prior reference-choice proposals were superseded by the user's save-file instruction;
no weaker visual criterion or acceptance waiver is approved.
Retry release passed; both processes exited normally, final lock held:false, no owned resources.
This in-scope acceptance blocker does not create a follow-up Plan.

## Earlier claim history

Harness claim timed out after 480.598 seconds over 97 attempts; claim was null.
Foreign normal-launch server PID31016 and client PID14100 and their lock remained untouched.
No process or lock was owned by this session; the claim waiter exited, so no release applied.
Runtime evidence `## Claim` and `## Release proof` hold the claim and final inventories.
The later retry resolved this claim blocker; it is history, not the current obstacle.

## Memory-cost question

Prior patched-run final-profile recorded Lighting Terrain current675.723ms, average588.736ms
at1920x1080 with comparison active and islands present; this is retained measured evidence,
not a completed capture or final memory disposition. The earlier worker's added4K gate is
superseded by the plan03 timing contract, not an acceptance requirement. Keep the document/link
until main applies that contract and records disposition alongside the completing runtime run.
Evidence: `Temp/plan03-patched-runtime-evidence.md` `## Terrain GPU timing` and
`Temp/plan03-capture-diagnosis.md` `## Timing contract`.
## Evidence index

- `Temp/AgentBuildEnvelopes/compile-20261029T190055928Z-45844.md`
  `## Rebased lighting foundation builds`: current row1 DataPacker Release x64 build.
- `Temp/AgentBuildEnvelopes/compile-20261029T192103385Z-5736.md`
  `## Camera fix Debug x64 builds`: prior camera-validation builds; superseded below.
- `Temp/lighting-plans/rebase-20261029.md` `## Verification`: completed reviews and final checks.
- `Temp/plan03-current-static.md` `## Row 1 current builds`: current build/freshness proof.
- `Temp/plan03-current-static.md` `## Row 9 static examination`: current static-only row9 PASS.
- `Temp/plan03-save-update.md` `## Scope and authorization`: user's save-header authorization.
- `Temp/plan03-save-update.md` `## Recorded byte verification`: updated save and backup identity.
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
- `Temp/plan03-save-diagnosis.md` `## Recovery choices and acceptance`: historical proposals/checks.

## Approved matched reference and current failures

User selected "Replace with matched current-main reference" after the baseline drift diagnosis.
Use exact cd6d410c064a5a5f905bf68efaf28493ab6fc33a in the same existing worktree on a temporary
reference branch, then restore foundation. No new worktree or newer-main target is authorized.
Matched save/identity, fresh app data, FoV15 perspective, sun1.8 and projected close/wide framing
replace historical sun schedule/exact height assertions only; all visual thresholds stay exact.
Recipe plus verified copied save/ClientGuid/comparator survive normal branch switching in ignored
`Temp/lighting-matched-reference/`; originals and historical images/logs remain unchanged.

HARNESS-F-001: capture failed when publication changed; proven incomplete subscribed-cell setup.
Require all subscribed cells confirmed/rendered before every capture and keep scene/profile
records through capture. Quiet publication counts alone do not prove GPU residency completion.
Diagnosis: `Temp/plan03-capture-diagnosis.md` `## Existing settling signals and next check`.
HARNESS-F-002: validation callbacks/debug breaks; three shader repairs built and independently
reviewed PASS. Full runtime/log retest is pending, not a passed failure.
Latest build: `Temp/AgentBuildEnvelopes/compile-20261029T200004779Z-19644.md`
`## plan03 shader validation builds`; review: `Temp/plan03-shader-review.md` `## Scope and verdict`.
Historical failure/log/release evidence: `Temp/plan03-patched-runtime-evidence.md`
`## Criterion results`, `## Log checks` and `## Release proof`. Both processes quit normally,
release PASS, held:false; no active owned harness remains from that run.
Plan03 timing contract is 1920x1080, without an added4K gate; retain memory investigation pending
plan03 disposition (`Temp/plan03-capture-diagnosis.md` `## Timing contract`).

## Resume

Pre-switch checkpoint: branch lighting-foundation-work,
HEAD a49b61276f424515ff80f746c58ac89f4e73f520,
three pending shader repairs, patched baseline save and session notes. Preserve them before switch.
Preparation performed no switch, stage, commit, build or runtime and did not modify primary.
Main owns checkpoint preservation and temporary branch transition. Next workers consume ignored
`Temp/lighting-matched-reference/reference-recipe.md` without relying on lighting-plans on main.
Build exact reference Sandbox client/server Debug x64 Local through /compile, capture matched
reference through /agent-harness, release, restore foundation and rebuild before candidate run.
Then settle row3 and unchanged rows4-9 plus 1920x1080 Lighting Terrain timing
and memory disposition.
Keep plan03 unchecked until every row passes; do not advance04/05. Update actual evidence here
and in `00-context.md` `## Progress`. Never alter foreign processes or resources.
