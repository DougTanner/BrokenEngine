<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-29T00:32:52.466Z","dependsOn":[]} -->
# Fix: Invoke-FinalizeLanding.ps1 — omitted-token retained-claim landing fails with git.rollback-failed

## Context
Run exactly as `.agents/skills/finalize-changes/references/scripts.md:374`
documents:

`pwsh -NoProfile -File .agents/skills/finalize-changes/scripts/Test-FinalizeWorkflowFixtures.ps1 -WorktreeCliExecutable '<worktreecli-exe>'`

Three consecutive runs at baseline `769cd615` each exited 1 at the same
scenario — two while another session was running the same suite on the machine,
one with no concurrent run — so the failure is deterministic, not the
intermittent abort recorded in `FinalizeFixtureSuiteIndexLockAbort.md`. All 657
result lines before the first failure are passes. Observed output, identical in
all three runs:

```text
FAIL landing-adopts-omitted-token-retained-claim exit=0 (was 1)
FAIL landing-adopts-omitted-token-retained-claim status=landed (was error)
FAIL landing-adopts-omitted-token-retained-claim code=ok (was git.rollback-failed)
  message: Exact candidate advance rollback did not restore the expected primary and session checkouts: Primary checkout did not update to the exact final commit: fatal: Unable to create '<scratch>/primary/.git/index.lock': File exists.
FAIL omitted-token adoption lands the deterministic final commit
```

The suite then aborted with
`Exception: git add retry-file.cpp failed: fatal: Unable to create '<scratch>/primary/.git/index.lock': File exists.`,
so every later scenario — `Test-FinalizeWorkflowFixtures.ps1:1568` onward,
including `landing-retries-identical-patch` at `1617-1630` — never ran. The
workaround was to stop using the suite as an acceptance signal for landing-script
scenarios and re-run it twice to confirm the failure was not transient.

Evidence in the current tree, none of it changed by the observing session (which
edited only `.agents/skills/finalize-changes/references/workflow.md` wording):

- `Test-FinalizeWorkflowFixtures.ps1:1556-1564` — the failing scenario claims a
  3600-second lease through WorktreeCli for a fresh owner (line 1558), runs
  `Invoke-FinalizeLanding.ps1` with the owner token omitted so the landing adopts
  the retained claim (line 1559), and asserts `0`/`landed`/`ok` (line 1560).
- `Invoke-FinalizeLanding.ps1:627-628` — the exact-candidate-advance try block
  calls `Invoke-LandingPrimaryCheckout $finalCommit` and throws
  `"Primary checkout did not update to the exact final commit: ..."` on non-zero
  exit. That throw text is the tail of the observed message, so the *first*
  failure was this checkout hitting `index.lock`; the reported
  `git.rollback-failed` is the second, masking failure.
- `Invoke-FinalizeLanding.ps1:646-650` — the catch path re-runs
  `Invoke-LandingPrimaryCheckout $expectedCheckout` and, when the primary head or
  status does not match, raises `git.rollback-failed` with the original reason
  appended, which is exactly the observed message shape.
- `Invoke-FinalizeLanding.ps1:455-473` — `Invoke-LandingPrimaryCheckout` waits out
  `index.lock` contention using the script-scoped stopwatch `$script:IndexLockWait`,
  which is started and stopped but never reset. The budget is therefore cumulative
  across every call in one landing, so the rollback checkout at line 646 can
  inherit a budget the line-627 checkout already spent. Whether that is what
  happened here is not proven.
- `Invoke-FinalizeLanding.ps1:161` — the same stopwatch is the only source of the
  `git.index-lock-wait` diagnostic, so a cumulative-budget defect would also
  under-report the wait.
- `Test-FinalizeWorkflowFixtures.ps1:1835` — the later
  `landing-fails-truthfully-on-persistent-primary-index-lock` case is the one
  scenario that legitimately expects `git.rollback-failed`; it runs after the
  abort point and never executed in these runs.
- `.agents/scripts/FinalizeWorkflowCommon.psm1:669` — `Invoke-ScratchGit` throws on
  the first non-zero git exit, which is what turned the leftover scratch
  `index.lock` into a whole-suite abort. That abort behavior is already owned by
  `Documents/Plans/Engine/FinalizeFixtureSuiteIndexLockAbort.md` and is not this
  Plan's subject; this Plan owns the scenario failure that precedes it and the
  stale lock the landing child leaves behind.

Which process held the scratch primary `index.lock`, and whether the landing
script's own child git left it, is not proven and is deferred to the review
session named in `## Design`.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 9f85240a-7a26-41c4-a062-c99566fe2456
- Worktree/branch UUID: c7d95ad1-49a1-449d-b4eb-45ab35b95173
- Session branch: claude/c7d95ad1-49a1-449d-b4eb-45ab35b95173
- Worktree: .claude\worktrees\BrokenEngine\c7d95ad1-49a1-449d-b4eb-45ab35b95173
- Landing ref: claude/c7d95ad1-49a1-449d-b4eb-45ab35b95173
  Fallback once the recorded ref is gone:
  `git log --diff-filter=A --format=%H -- Documents/Plans/Engine/FinalizeFixtureOmittedTokenIndexLock.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID above.

## Design
In a new session, run `/next-plan-review claude/c7d95ad1-49a1-449d-b4eb-45ab35b95173`,
supplying client `claude` and conversation session ID
`9f85240a-7a26-41c4-a062-c99566fe2456`. Root-cause the friction from the proven
transcript, then make the smallest fix inside the `## In scope` boundary below.
If root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

The author's recommendation, offered as a starting hypothesis rather than a
decision, is to check the cumulative `$script:IndexLockWait` budget at
`Invoke-FinalizeLanding.ps1:455-473` first, because it is the one cited code fact
that would make the rollback checkout fail on a lock the forward checkout was
still allowed to wait out; if the transcript instead proves the landing child's
own git process left the lock, fixing that holder is equally acceptable. Either
fix must also leave the scratch primary without a stale `index.lock`, since that
leftover is what took the rest of the suite down.

## Critical files
- `.agents/skills/finalize-changes/scripts/Invoke-FinalizeLanding.ps1`
- `.agents/skills/finalize-changes/scripts/Test-FinalizeWorkflowFixtures.ps1`

## In scope
- Root-cause investigation via /next-plan-review, run with client `claude`, the
  review ref named in `## Design`, and the recorded conversation session ID
- The smallest resulting fix, confined to the two files named above — within
  them, `Invoke-LandingPrimaryCheckout` and the `$script:IndexLockWait` budget
  (`Invoke-FinalizeLanding.ps1:455-473`), the exact-candidate-advance try/catch
  and its `git.rollback-failed` paths (`Invoke-FinalizeLanding.ps1:627-650`), the
  `git.index-lock-wait` diagnostic (`Invoke-FinalizeLanding.ps1:161`), and the
  `landing-adopts-omitted-token-retained-claim` scenario
  (`Test-FinalizeWorkflowFixtures.ps1:1556-1564`)

## Out of scope
- The landed change the observing session produced
- `Invoke-ScratchGit`'s throw-on-first-failure abort behavior
  (`FinalizeWorkflowCommon.psm1:669`) and the fixture retry helpers, both owned by
  `Documents/Plans/Engine/FinalizeFixtureSuiteIndexLockAbort.md`
- The landing lock lease and claim-adoption logic in WorktreeCli
- Unrelated skills and scripts; any transcript path or transcript text in the
  repository

## Coordination
- `Documents/Plans/Engine/FinalizeFixtureSuiteIndexLockAbort.md` covers the same
  suite and the same scratch `index.lock` family from the fixture-helper side.
  Whichever Plan is implemented second re-runs the full suite before claiming its
  acceptance criteria, because a fix on either side changes what the other
  observes; neither Plan may edit the other's named regions.

## Risk tier and invariants
Expected Tier 3 (the candidate fix is in the production landing script, which is
build and landing coordination that can block other sessions); a fix proven to be
confined to the fixture scenario alone is Tier 2. `Invoke-LandingPrimaryCheckout`
is called on both the forward and the rollback path, so any budget change must
keep a genuinely persistent foreign lock still failing the landing truthfully, as
`Test-FinalizeWorkflowFixtures.ps1:1835` requires. Never embed transcript paths or
home paths.

## Acceptance criteria
- `landing-adopts-omitted-token-retained-claim` passes under the documented
  invocation, and the suite runs past `Test-FinalizeWorkflowFixtures.ps1:1568`
  through to its final scenario
- `landing-fails-truthfully-on-persistent-primary-index-lock`
  (`Test-FinalizeWorkflowFixtures.ps1:1835`) still reports exit 1 with
  `git.rollback-failed`
- No `index.lock` remains in the scratch primary after the omitted-token scenario
- /validate-skill passes for any changed SKILL.md; plan validate exits 0
