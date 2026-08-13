<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-13T12:20:06.251Z","dependsOn":[]} -->
# Add a lock-free lint mode to WorktreeCli `plan validate`

## Context

`plan validate` does two independent things in one command, and welds them together in a way that forces
read-only callers to take a mutating scheduler lock they have no use for.

In `Tools/WorktreeCli/PlanScheduler.cpp`, `RunValidate` first lints the tracked Plan tree — `BuildPlans` at
`:501-505`, which reads only the Git worktree — and only afterwards resolves the scheduler root, creates its
storage, and acquires the scheduler guard (`:506-521`). The one operation that actually changes machine-local
state is `HealClaims` (`:527-532`). So the lint result is already complete before the guard is taken, and the
guard exists solely for the heal.

The consequence is that a caller that only wants the lint must still have a usable
`%LOCALAPPDATA%\BrokenEngineLocks` and must still win the guard. A `/codex-review` read-only sandbox cannot
write there at all, so the command reports `guard-unavailable` and exits `1` even though nothing is wrong with
the Plan tree. That is documented as accepted friction today:
`.agents/skills/verify-changes/SKILL.md:112-117` tells the read-only reviewer not to run `plan validate` and to
validate a host-side result supplied through the scope file instead, and
`.agents/skills/codex-review/SKILL.md:60-64` cites `plan validate` as the example of a mechanical check the
sandbox cannot run. Under contention the same caller instead gets `busy` and must retry, again for a heal it
did not ask for.

`plan list` is the existing proof the two halves are separable: it "takes no scheduler guard, heals nothing,
creates no storage" (`Tools/WorktreeCli/AGENTS.md`, `plan` bullet) while reading the same tracked metadata.

This was proven while fixing how the scheduler's `busy` and `guard-unavailable` codes are reported to callers,
and left out of that change because it changes WorktreeCli behavior and its command surface.

## Design

Add a `--lint-only` switch to `plan validate` in `RunValidate`.

Reorder `RunValidate` so the guard-free work is contiguous: after `BuildPlans`, run the requested-plan
existence check (currently `:522-525`, which reads only `plans`) and `MarkCycles` (`:526`), then take the
scheduler block. Both checks keep their current results and exit codes.

The scheduler block — `SchedulerRoot`, `EnsureParentDirectory`, the `coordination::Guard`, its
`busy`/`guard-unavailable` failures, `BuildPrimaryTipPlans`, and `HealClaims` — runs only when `--lint-only`
is absent. With `--lint-only` the command touches no scheduler storage, acquires no guard, and heals nothing;
`healedClaims` is emitted as an empty array so the output stays one shape.

Everything else is unchanged: without the switch the command behaves exactly as it does today, including
`local-app-data-unavailable`, `storage-failed`, `busy` as a state conflict (exit `2`), and `guard-unavailable`
as an OS failure (exit `1`). The lint envelope, its `status`/`code`/`message`/`diagnostics`/`notices`/`plans`
fields, and `--plan` filtering are untouched.

Adopt the switch at the one documented read-only caller. In `.agents/skills/verify-changes/SKILL.md`
`## Executable Plan check`, the read-only reviewer runs
`WorktreeCli.exe plan validate --lint-only --repo <common-dir> --worktree <checkout>` itself instead of
validating a host-supplied result, so the `busy` retry rule and the `guard-unavailable` BLOCKED rule no longer
apply to this check. In `.agents/skills/codex-review/SKILL.md:60-64`, drop `plan validate` as the example of a
check the sandbox cannot run and leave the general host-side mechanism itself unchanged. Document the switch
in `Tools/WorktreeCli/AGENTS.md`'s `plan` bullet next to the existing `list`/`validate` descriptions.

## Critical files

- `Tools/WorktreeCli/PlanScheduler.cpp` — `RunValidate` (`:489-533`) and the `plan validate` argument parsing
  that must accept `--lint-only`.
- `Tools/WorktreeCli/AGENTS.md` — the `plan` command bullet.
- `.agents/skills/verify-changes/SKILL.md` — `## Executable Plan check`.
- `.agents/skills/codex-review/SKILL.md` — the host-side mechanical-check paragraph.

## In scope

- The `--lint-only` switch: argument acceptance, the `RunValidate` reordering described above, and skipping the
  scheduler root, storage creation, guard, and `HealClaims` when it is set.
- The four documentation/skill updates named above.

## Out of scope

- Default `plan validate` behavior, its exit codes, its envelope fields, and its diagnostics text.
- `plan list`, `claim-next`, `claim-status`, `unclaim`, `complete`, `reject`, landing locks, and the
  `coordination::Guard` implementation.
- Claim healing rules, claim record schema, and any change to when healing happens in the guarded path.
- `.agents/scripts/New-PlanFile.ps1`, `Invoke-NextPlanClaim.ps1`, `/create-follow-up-plans`, and every other
  caller not named above.
- The existing scheduler fixture suites: they must pass unchanged, not grow new cases.

## Risk tier and invariants

Tier 3: this changes WorktreeCli, whose executable is shared AgentTools requiring the `/compile`
candidate/promotion path and a landing gate, and it touches the plan scheduler that other sessions coordinate
through. Invariants: every scheduler state change still runs under the scheduler guard — `--lint-only` changes
no state, so it needs none; the output stays one JSON object on stdout with human progress on stderr; exit `0`
success, `2` state conflict, `1` usage/transport/OS failure.

## Acceptance criteria

- `plan validate --lint-only --repo <common-dir> --worktree <checkout>` on a valid tree exits `0` with
  `status: valid`, `code: ok`, the same `plans` inventory the guarded run reports, and an empty `healedClaims`.
- With `%LOCALAPPDATA%` pointed at an unusable location, `--lint-only` still exits `0` on a valid tree, while
  the same command without the switch still reports `guard-unavailable` and exits `1`.
- `--lint-only` creates no file or directory under the scheduler root, and leaves existing claim records
  byte-identical, including an expired one that a guarded run would have healed.
- An invalid Plan marker still reports `status: invalid`, `code: invalid-plans`, and names the file under
  `--lint-only`; `--plan <path>` naming an absent Plan still reports `plan-not-found` with exit `2`.
- `.agents/scripts/Test-WorktreeCliPlanScheduler.ps1` returns unchanged results against the built executable.
