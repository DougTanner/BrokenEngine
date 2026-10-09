<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-08T23:18:34.344Z","dependsOn":[]} -->
# Remove over-engineered checks and durability protection in the agent tools

## Context

A repository-wide over-engineering sweep looked for useless hashing, excessively defensive checks, fallbacks for cases that cannot happen, and ultra-rare edge-case protection (power-loss durability, retries) that a plain operation plus a hard error would replace. A low-effort first-pass model produced the findings; a second model validated each one once against the code (about 95% confirmed), and the dispatching session spot-checked only two of the sweep's 27 groups. This Plan carries the 25 `Tools/` candidates (AgentHarness, ToolCommon, WorktreeCli) that survived that validation, plus the entries that must not change.

User direction, as relayed by the dispatching session: do not trust these recommendations blindly. Every listed finding is a candidate, not an approved change. Line numbers were taken on 2026-10-08 and will drift.

## Design

Recommended procedure for the executing session:

1. For each candidate below, find the current code by symbol, not by line number. Re-derive the claim yourself by reading every caller and the invariant it relies on.
2. Apply a candidate only when you can prove it from current code: cite the invariant or every caller in the change summary. Drop any candidate that does not hold, whose proof needs more than a local reading, or whose code has changed shape. Report each dropped one with the reason.
3. These tools have no `ASSERT` macro (no `ASSERT` or `assert(` exists under `Tools/`), so the simpler form is deletion or a hard error. Do not add an assert facility for this Plan.
4. Return a per-ID table: applied (with its proof) or dropped (with its reason).

Boundaries that must never be removed, even when a candidate seems to propose it:

- Win32 API result checks.
- Checks on genuinely external input: Git output (`RunGit`, `ls-files`, `ls-tree`, `symbolic-ref`), MSBuild output, user and agent command-line arguments, Plan file bytes, and coordination files read back from disk (lock, claim, and lease metadata, including `ValidateMetadataEnvelope` and the claim parse `try`/`catch` in `ReadClaim`).
- The scheduler and coordination record formats and their schema checks.
- The atomic replace (`MOVEFILE_REPLACE_EXISTING` rename) that concurrent readers of coordination files rely on. T2 and T3 remove only power-loss durability, never atomicity.
- The cross-process `.guard` mutex in `CoordinationStore`.

These trees are shared tool infrastructure. Building, landing, and promoting them follows `/compile` and `.agents/skills/finalize-changes/references/agenttools.md`. Every live worktree runs the same promoted binaries, so a change to a CLI contract or to coordination-file write behavior means pausing to warn the user before landing.

### Candidates

AgentHarness

- **T1** `Tools/AgentHarness/AgentHarness.cpp:120` in `ReadAllStandardInput`, the `std::ssize(rInput) > kiMaxRequestBytes` disjunct is never true. The only caller (:638) passes an empty `request`, because `ParseSocketCommandArguments` (:410-431) accepts exactly one request source, and each append (:125) runs only after the second disjunct proves `size + iRead <= kiMaxRequestBytes`. Form: `if (iRead > kiMaxRequestBytes - std::ssize(rInput))`.

ToolCommon

- **T2** `Tools/ToolCommon/CoordinationStore.cpp:344` `FlushFileBuffers` on the staged temp file protects only against power loss. Atomic visibility comes from the rename in `CommitStagedBytes` (:357). The callers write machine-local lock and claim metadata (HarnessLockCommands.cpp, LandingLockCommands.cpp, PlanScheduler.cpp:879) and Plan rewrites inside a Git worktree (PlanScheduler.cpp:1129-1146). Form: `bSucceeded = bSucceeded && std::cmp_equal(iWritten, contents.size());`, and change "Durable byte replacement" to "Atomic byte replacement" in the comment at CoordinationStore.h:53.
- **T3** `CoordinationStore.cpp:357` `MOVEFILE_WRITE_THROUGH` only forces the rename to disk before the call returns. Form: pass `MOVEFILE_REPLACE_EXISTING` alone. Apply together with T2 or not at all.
- **T4** `Tools/ToolCommon/ToolCliCommon.h:57` nothing ever sets `RunProcessOptions::bCaptureOutput` to false. The only `RunProcess` callers are BuildCommand.cpp:86-90 and :300-310 (both set it to true) and `RunGit` (ToolCliCommon.cpp:199-205, which uses the default). Form: delete the field and the redundant `options.bCaptureOutput = true;` lines at BuildCommand.cpp:87 and :301. Apply together with T5 and T6.
- **T5** `ToolCliCommon.cpp:99-115` the pipe creation guarded by `bCaptureOutput` always runs. Form: make the `CreatePipe`/`SetHandleInformation` block unconditional, set `hStdOutput = hPipeWrite.Get()` (:121), and set `hStdError = rOptions.bMergeStdError ? hPipeWrite.Get() : ::GetStdHandle(STD_ERROR_HANDLE)` (:122).
- **T6** `ToolCliCommon.cpp:152-181` the read loop guarded by `bCaptureOutput` always runs. Form: remove the wrapper and keep the body.

WorktreeCli build

- **T7** `Tools/WorktreeCli/BuildCommand.cpp:443-447` the second deadline check after the wait message is redundant. Without it, a negative `remaining` makes `sleep_for` return at once, and the :437 timeout check handles the next pass with the same result. Form: delete it and compute `remaining = deadline - std::chrono::steady_clock::now()`.
- **T8** `BuildCommand.cpp:111-132` the retained log name gets up to 16 collision retries. Each process opens one retained log (`RunBuildExecution` :683; `RunBuildCommand` runs once per process, WorktreeCli.cpp:60), and the name already has the target stem, a millisecond UTC timestamp, and the process id. Form: one `CREATE_NEW` attempt on `baseName + L".log"`, and on failure `FailBuild` with the Windows error and return false.
- **T9** `BuildCommand.cpp:711-719` the code that honors a caller's `/nodeReuse` choice never runs: no skill, script, or tool passes `/nodeReuse`, `/nr`, or `-nr`, and the comment at :709-710 says reused nodes would stall the pipe drain anyway. Form: append `L"/nodeReuse:false"` unconditionally, delete the `bHasNodeReuse` scan, and drop "honor an explicit caller choice when present" from the comment.

WorktreeCli landing lock

- **T10** `Tools/WorktreeCli/LandingLockCommands.cpp:224-234` `HandleRecover` re-reads the lock file and compares it to `rMetadata`, which was read (:461) under the same `Guard` (:447). The guard keeps `.guard` open with share mode 0 (CoordinationStore.cpp:37), every lock write goes through a guarded verb, and `AllRegisteredWorktreesClear` (LandingLockLifecycle.cpp:107-193) only queries. The bounded-wait path already skips this re-read (:341-342). Form: delete the re-read, its failure branch, and the inequality conflict, and reword the :341-342 comment so it says both paths rely on the guard.
- **T11** `LandingLockCommands.cpp:369` `now >= deadline ||` is covered by `iSleepMilliseconds >= iRemainingMilliseconds`, because `iSleepMilliseconds >= 1` (poll at least 50, validated at :428, or the `+ 1` form at :332/:361). Form: `if (iSleepMilliseconds >= iRemainingMilliseconds)`.
- **T12** `LandingLockCommands.cpp:373` the `max(1, min(sleep, remaining))` clamp has no effect once :369 did not return. Form: `std::this_thread::sleep_for(std::chrono::milliseconds(iSleepMilliseconds));`.
- **T13** `LandingLockCommands.cpp:245-272, 413-417` the WorktreeCli `lock steal` verb always ends in `EmitLandingConflict` (:249, :257, :271) and has no caller. The only `lock steal` user is AgentHarness's separate verb, and `Test-AgentToolsCapabilities.ps1` checks only the `Usage: WorktreeCli.exe lock ` prefix. Form: remove `LandingReleaseOperation` (:131-135) and the steal branches, turn `HandleReleaseOrSteal` into `HandleRelease`, make the `--expect` check recover-only ("recover requires --expect"), and update the dispatch (:493), the usage line (WorktreeCli.cpp:13), and the verb list in `Tools/WorktreeCli/AGENTS.md`. This is a visible CLI contract change: before applying it, re-run a repository search for `lock steal` callers across `.agents/`, `.claude/`, `.codex/`, and the scripts.
- **T14** `Tools/WorktreeCli/LandingLockLifecycle.cpp:65-68` the `claimedAt` and `heartbeatAt` parse-failure arms cannot fire. `ValidateMetadataEnvelope` (CoordinationStore.cpp:418-419) already parsed both strings from the same `rMetadata` at :41, and `ParseUtcTimestamp` is pure. Form: unchecked calls for those two. Keep the `expiresAt` check, which has no earlier validation.
- **T15** `LandingLockLifecycle.cpp:165` `|| error` after `!is_directory(path, error)` is redundant: when the overload sets `error`, it returns false. Form: `if (!std::filesystem::is_directory(ExtendedLengthPath(rWorktree), error))`.
- **T16** `LandingLockLifecycle.cpp:180` the same redundant `|| error`. Form: `if (!std::filesystem::is_directory(extendedGitDirectory, error))`. Keep the separate `exists(...) || error` at :185.

WorktreeCli plan scheduler

- **T17** `Tools/WorktreeCli/PlanMetadata.cpp:90`, `PlanMetadata.h:17` the SHA-256 `Plan::digest` is computed and never read. `Plan` is in-memory only, and `Tools/WorktreeCli/AGENTS.md` (Coordination State) says claims store no digest. Form: delete the assignment and the member.
- **T18** `PlanMetadata.cpp:45-48` the `value.empty()` check is redundant: the `Documents/Plans/` prefix check (:61-64) rejects it with the same `false`. Form: delete.
- **T19** `PlanMetadata.cpp:49-52` the backslash rejection is redundant on Windows: `generic_wstring()` converts `\` to `/`, so the :80-83 comparison fails. Form: delete.
- **T20** `PlanMetadata.cpp:53-60` the `has_root_name`/`has_root_directory` checks are redundant, because a value that passes the prefix check starts with `Documents/`. Form: delete.
- **T21** `PlanMetadata.cpp:65-68` the length check after the prefix check is redundant: a value equal to the bare prefix fails `ends_with(L".md")` (:69-72). Form: delete.
- **T22** `PlanMetadata.cpp:373` `it->second == 0` is never true: `colors` is written only with 1 (:336) and 2 (:366), and every `Visit` leaves the entries it touched at 2. Form: `if (rPlan.bValid && !colors.contains(rPath))`, replacing the :372-373 lookup.
- **T23** `Tools/WorktreeCli/PlanScheduler.cpp:987-991` `RenderDependencies`' `found == end()` branch cannot run: its only caller (`RunTerminal` :1089) calls it only after `std::ranges::contains(rPlan.dependencies, target)` (:1080) on the same vector. Form: make it `void`, drop `return true;` (:1001), and replace the caller's `if (!RenderDependencies(...)) return Failure(...)` (:1089-1092) with a plain call.
- **T24** `PlanScheduler.cpp:240-282` the `try`/`catch (nlohmann::json::exception)` around `ValidateClaim` guards accesses that are each checked first (`is_object` :243, `contains` :251, `JsonIntegerEquals` in CoordinationStore.cpp:381-388, `ReadRequiredString` :53-61). Form: remove the `try`/`catch` and keep every check inside it. The disk-input parse `try`/`catch` in `ReadClaim` (:292-301) stays.
- **T25** `PlanScheduler.cpp:189-193` the `IsPathBelow(temporaryPath, rWorktree)` check repeats containment already established. `planPath` passed `IsPathBelow` (:168), the entry name is one separator-free name starting with `<plan>.tmp.` (:178), and a link entry is skipped by the non-following reparse-point test (:198-199) before `DeleteFileW`. Form: delete.

### Do not change (validated as REJECTED)

- `PlanMetadata.cpp:231-235` `iEnd == -1` in the `git ls-files -z` parse: this is a check on Git output, and removing it lets an unterminated record produce an out-of-bounds `string_view`.
- `PlanMetadata.cpp:268-272` `iEnd == -1` in the `git ls-tree -z` parse: same reason.
- `PlanMetadata.cpp:275-279` the missing-tab `continue` in `ls-tree` entries: this checks the shape of Git output.
- `PlanScheduler.cpp:377-381` (`ResolveGitBranch`) and `:392-396` (`ResolvePrimaryBranch`) the empty checks on `git symbolic-ref` output: these check Git output (`RunGit` returns it unvalidated, ToolCliCommon.cpp:195-213).

## Critical files

- `Tools/AgentHarness/AgentHarness.cpp`
- `Tools/ToolCommon/CoordinationStore.cpp`, `CoordinationStore.h`, `ToolCliCommon.cpp`, `ToolCliCommon.h`
- `Tools/WorktreeCli/BuildCommand.cpp`, `LandingLockCommands.cpp`, `LandingLockLifecycle.cpp`, `PlanMetadata.cpp`, `PlanMetadata.h`, `PlanScheduler.cpp`, `WorktreeCli.cpp`, `AGENTS.md`

## In scope

- Only the code regions named by candidates T1-T25, each changed only to the stated simpler form or a hard error, and only after the executor's own proof.
- For T13, the WorktreeCli usage line and the `Tools/WorktreeCli/AGENTS.md` verb list. For T2, the `CoordinationStore.h:53` comment. Any comment adjacent to a changed region that the change makes false.

## Out of scope

- Every boundary in `## Design`, the "Do not change" list, and any candidate the executor cannot prove.
- Any change to a coordination, lock, lease, claim, or Plan-metadata record format, or to the AgentHarness `lock` verbs (including AgentHarness `lock steal`).
- `.agents/scripts/` and skill text. If T13 turns out to need a change there, drop T13 and report it.
- Adding an assert facility to the tools.
- New findings beyond T1-T25. Report them instead of applying them.

## Acceptance criteria

- WorktreeCli and AgentHarness Release build through `/compile` with no new warnings.
- The change summary lists every ID T1-T25 as applied (with the cited invariant or callers) or dropped (with the reason).
- With the rebuilt WorktreeCli: `.agents/scripts/Test-AgentToolsCapabilities.ps1` passes, `pwsh -NoProfile -File .agents/scripts/Test-PlanSchedulerState.ps1` reports `status: valid`, and a read-only landing-lock `status` call returns a valid result. Do not take the shared landing lock just to test it.
- Landing follows `.agents/skills/finalize-changes/references/agenttools.md`, including the pause to warn the user when T2/T3 or T13 is applied.

## Notes

- Change Workflow tier: Tier 3. Trigger: build and bootstrap coordination that can block other sessions. T2/T3 change how every coordination artifact (landing lock, plan claims, harness lock) is written, T10-T13 change the landing-lock verbs, and T13 removes a CLI verb. The promoted binaries are shared by every live worktree.
- `/compile` governs the AgentTools policy for building these tools, and promotion happens only through `/finalize-changes`.
