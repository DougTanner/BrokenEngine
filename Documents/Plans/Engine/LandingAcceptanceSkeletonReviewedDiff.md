<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-05T17:36:13.575Z","dependsOn":[]} -->
# Derive the landing acceptance skeleton from the reviewed diff, not the baseline range

## Context

`.agents/scripts/Get-SessionChangeInventory.ps1 -Landing` produces the row set a
landing owes on its acceptance table. It computes those rows from the wrong path
set.

Root cause, read from the current script:

- `:769` — `$result.triggers = Get-RoutingTrigger ([object[]] $sorted.ToArray())`.
  `$sorted` is the ordinary inventory `entries` set, built by
  `Get-InventoryRawRow` at `:386` from `git diff --raw ... <baseline> <head>`
  through `Get-InventoryDiffArgument` (`:380-383`), which emits a two-dot
  range.
- `:788` — `$result.landing = Get-LandingState $baselineSha $headSha
  ([ref] $landingReference) $result.triggers`, so the landing state receives
  those two-dot triggers.
- `:529` — inside `Get-LandingState`, `$reviewed` is built from
  `"$BaselineSha...$HeadSha"`, a three-dot range: the session's own changes
  only.
- `:563-565` — `acceptanceSkeleton` adds one `BLOCKED` row per
  `$script:AcceptanceSkeletonChecks` entry whose trigger is true, reading the
  two-dot `$Triggers`, while `:566-571` decides the `Executable Plan check` row
  from the three-dot `$reviewed` rows. The two neighbouring loops therefore
  disagree about which diff the landing owns.

A two-dot range includes commits that landed on primary after the session's
baseline. Those are foreign bytes the session never changed and never reviewed.

Observed effect, second landing of the session that recorded this Plan
(`5829aab9c191d0f8678dae13422237ce20793252`, whose own reviewed diff touched
only `Documents/Plans/` files plus generated code-quality history artifacts):
foreign primary commit `6cea077c` ("Require BaseCommit in the code-quality
history script") had changed four `.agents/skills/code-quality-metrics/**`
files. Because those files fell inside the two-dot range, the inventory reported
`validateSkill: true` and `progressiveDisclosureReview: true` and emitted two
`BLOCKED` skeleton rows, `/validate-skill` and
`/progressive-disclosure-review`, for a change that touched no skill file at
all. The finalizer reconciled the rows by hand against `landing.reviewed` and
marked them not triggered. A literal reading of
`.agents/skills/finalize-changes/references/landing-acceptance-table.md` — which
states that `acceptanceSkeleton` carries "the rows this landing owes, each
emitted `BLOCKED`" and that any non-`PASS` row is a blocker — would instead have
blocked the landing on bytes belonging to another session.

`acceptanceSkeleton` was introduced by commit `b21e74d7` ("List Visual Studio
project candidates in the session change inventory"), confirmed with
`git log -S acceptanceSkeleton -- .agents/scripts/Get-SessionChangeInventory.ps1`.

Session provenance (machine-local; not reproducible after cleanup):
- Client: claude
- Conversation session ID: 552a615b-9166-4c5b-8991-7e9c279d2dcf
- Worktree/branch UUID: def6cf77-44da-402b-b5e4-a597e25a7971
- Session branch: claude/def6cf77-44da-402b-b5e4-a597e25a7971
- Worktree: .claude\worktrees\BrokenEngine\def6cf77-44da-402b-b5e4-a597e25a7971
- Landing ref: claude/def6cf77-44da-402b-b5e4-a597e25a7971 (the session branch
  that carries this Plan). The landings that exhibited the symptom are
  `60d78d125a977df1aa5f7e6a36a5ffe3cb21be84` and
  `5829aab9c191d0f8678dae13422237ce20793252`; neither contains this Plan.

## Design

Recommended fix, because it is the smallest change that makes the two
neighbouring loops agree and needs no new range plumbing: in `-Landing` mode,
compute the trigger set the skeleton reads from the three-dot `$reviewed` rows
inside `Get-LandingState`, instead of from the two-dot `$Triggers` argument.
Concretely, build a path/mode/class list from `$reviewed` in the shape
`Get-RoutingTrigger` already consumes, call `Get-RoutingTrigger` on it, and key
the `:563-565` loop off that result. The `Executable Plan check` loop at
`:566-571` already reads `$reviewed` and stays as it is.

Recommended non-change: the top-level `triggers` object stays computed from the
two-dot entries set, because
`.agents/skills/finalize-changes/references/landing-acceptance-table.md` states
that "a `-Landing` run still reports the complete top-level `triggers` object
like any other run", and non-landing callers depend on that meaning. Only the
skeleton's input changes. If the implementer concludes the top-level object must
change too, that is a documentation-contract change and should be re-planned
rather than absorbed.

Alternative considered and not recommended: recompute the whole two-dot range
from the merge-base of the session branch and primary instead of the supplied
`-Baseline`. That would fix the skeleton as a side effect, but it changes the
meaning of `entries`, `counts`, `regions`, and the top-level `triggers` for
every caller of the script, including the non-landing modes, which is a far
wider blast radius for the same benefit.

Alternative considered and not recommended: leave the script alone and document
in `landing-acceptance-table.md` that the finalizer must reconcile skeleton rows
against `landing.reviewed` by hand. That is the workaround this session already
performed; it leaves a correct-looking `BLOCKED` row that only a careful reader
knows to discard, which is exactly the failure mode observed.

Whichever route the implementer takes, verify the mechanism in the script first;
the line numbers above are from the tree at the time of writing and may drift.

## Critical files

- `.agents/scripts/Get-SessionChangeInventory.ps1` — `Get-LandingState`
  (`:527-586`), especially the skeleton loop at `:563-565`; the
  `$result.triggers` assignment at `:769`; the `Get-LandingState` call at
  `:788`; `Get-InventoryDiffArgument` (`:380-383`) and `Get-InventoryRawRow`
  (`:385-386`) read as the two-dot source; `$script:AcceptanceSkeletonChecks`
  (`:37-44`).
- `.agents/skills/finalize-changes/references/landing-acceptance-table.md` —
  the paragraph describing `acceptanceSkeleton` and the top-level `triggers`
  object, updated only if the fix changes what either reports.

## In scope

- The input path set that `acceptanceSkeleton` rows are derived from in
  `Get-LandingState` in `Get-SessionChangeInventory.ps1`.
- Any documentation sentence in `landing-acceptance-table.md` that the changed
  behavior makes inaccurate.

## Out of scope

- The top-level `triggers` object's own computation and its meaning for
  non-landing modes.
- `entries`, `counts`, `regions`, `porcelain`, `submodules`, `planTouched`,
  truncation handling, and the `-EmitTargets` path.
- The `Executable Plan check` row, which already reads the reviewed diff.
- Any change to `/finalize-changes` scripts, to the landing gate, or to the
  acceptance table's row format.

## Risk tier and invariants

Expected Tier 2 under the root `AGENTS.md` risk triggers: scoped behavior of one
tool script, with no determinism/CRC, wire, serialization, replay, threading, or
trust boundary exposure and no build/bootstrap coordination.

Invariants that must survive: the emitted JSON keeps its schema version and key
set, so existing consumers keep parsing it; truncation reporting still describes
the complete inventory rather than the emitted subset; a genuinely triggered
check still emits its `BLOCKED` row, so the fix must not silence a row the
session's own reviewed diff owes.

## Acceptance criteria

- For a session whose reviewed diff touches no skill file, a `-Landing` run
  whose baseline range also spans a foreign primary commit that did touch skill
  files emits no `/validate-skill` and no `/progressive-disclosure-review`
  skeleton row.
- For a session whose own reviewed diff touches a skill file, both rows are
  still emitted.
- The run's `status` is `pass` and its top-level `triggers` object is unchanged
  for the same inputs.

## Notes

No existing Plan owns this root cause: a search of `Documents/Plans/**` for
`SmartGit`, `acceptanceSkeleton`, and `SessionChangeInventory` returned no
match. The recently landed finalizer Plans
`Documents/Plans/Engine/FinalizeOwnedPathStagedThenModified.md`,
`Documents/Plans/Engine/FinalizeVerifiedCandidateRebaseReResolution.md`, and
`Documents/Plans/Engine/FinalizeVerifiedCandidateWorktreeIdentity.md` are bounded
to `/finalize-changes` scripts and prose and do not touch
`Get-SessionChangeInventory.ps1`, so no `dependsOn` edge and no reciprocal
`## Coordination` section is warranted.
