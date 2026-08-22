<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-21T22:37:53.442Z","dependsOn":[]} -->
# Update the code-quality history files only for C++ changes

## Context

The code-quality history pair
`.agents/skills/code-quality-metrics/references/history/CodeQualityMetricsHistory.jsonl`
and `.../CodeQualityMetricsHistory.svg` currently gains one new row on **every**
landing, including landings that change no C++ at all. The live series proves it:
of the rows after the immutable 648-line prefix, 1 is `catch-up`, 13 are
`cpp-change`, and 19 are `carry-forward` — rows that only copy the previous row's
values. The last two rows are byte-identical in every metric field:

```
{"index":678,"date":"2026-08-21","captureMode":"carry-forward","verbosity":0.03786124447,"structuralErosion":0.523999375544,"supported":526,"parsed":525}
{"index":679,"date":"2026-08-21","captureMode":"carry-forward","verbosity":0.03786124447,"structuralErosion":0.523999375544,"supported":526,"parsed":525}
```

Row 679 came from a documentation-only landing: the producer classified the patch
as `cppChanged: false`, chose `carry-forward`, and the landing still appended the
row, regenerated the SVG, and committed both files as a post-confirmation overlay.

User direction (binding): both history files must change **only** for C++ changes.
Documentation-only landings — and any landing with no metric-supported change —
must leave both files byte-identical. The rule must be enforced **in the scripts**,
from facts the scripts compute themselves (`patch.cppChanged`,
`patch.metricSupportedChanges`, `decision.captureMode`), and the no-overlay outcome
must be verified mechanically. No part of the rule may depend on an agent reading
prose and behaving correctly; prose only describes what the scripts already enforce.

Tree note (verified): `.claude/skills` is a **symlink** to `.agents/skills`
(`git check-ignore` reports "beyond a symbolic link"; `git ls-files .claude` lists
only `.claude/agents`, `.claude/claude-worktree.sh`, `.claude/output-styles`,
`.claude/settings.json`). Every skill file below is edited at its tracked
`.agents/...` path; no `.claude/skills/...` path is ever edited.

## Design (settled)

The `broken-engine-code-quality-history-contract/v1` schema and the
`carry-forward` decision value both stay exactly as they are. The producer keeps
classifying identically. What changes is what happens **downstream** of a
`carry-forward` decision: nothing is generated, nothing is overlaid, and the
reviewed source commit itself becomes the landed commit.

### The enforcement chain

1. **Producer classification (unchanged).**
   `Invoke-CodeQualityMetricsHistory.ps1` `Get-PatchEvidence` (:243-252) computes
   `metricSupportedChanges` and `cppChanged` from `git diff --name-status` filtered
   by `Test-MetricPath` (`.h`/`.cpp`, excluding `ThirdParty`, `.agents`, `.claude`,
   `Temp`, and pure-GLSL `Data/Shaders` headers). `Get-Plan` (:422-435) turns that
   into `decision.captureMode` = `catch-up` / `cpp-change` / `carry-forward`. No
   caller supplies the mode; it is always the producer's own computed fact.

2. **Producer refuses to produce docs-only history bytes.**
   `Invoke-History` (:453) refuses `-Mode Generate` when
   `$Plan.decision.captureMode -ceq 'carry-forward'`, exiting 2 before creating the
   output directory. After this, *no code path anywhere* can obtain a JSONL/SVG pair
   for a patch with no metric-supported C++ change.

3. **Receipt validators reject inconsistent or docs-only history claims.**
   In `.agents/scripts/FinalizeWorkflowCommon.psm1`:
   `Assert-FinalizeHistoryUpdateReceipt` (:635) drops `carry-forward` from the
   Generate-receipt mode enum, so a hand-made or stale receipt claiming outputs for
   a docs-only landing is rejected; `Assert-FinalizeHistoryContractReceipt` (after
   :608) gains a mode/patch consistency rule — `cpp-change` requires
   `patch.cppChanged` true, `carry-forward` requires it false — so no receipt can
   claim a capture mode its own patch evidence contradicts.

4. **Landing skips the whole overlay for a carry-forward contract.**
   `Invoke-FinalizeLanding.ps1` re-runs Contract under the landing lease
   (`Validate-HistoryContractForLanding`, :295-317) and branches on that
   **re-evaluated** mode, not on the frozen argument, because the approved
   `catch-up` → `carry-forward` narrowing is already permitted (:300-303). For
   `carry-forward` it never calls Generate, never builds an overlay tree, never
   creates a replacement commit: it lands the reviewed source commit itself
   (`final.replacement = false`).

5. **Landing proves the no-overlay outcome mechanically.**
   Before the compare-and-swap, the no-overlay route asserts that both reserved
   paths resolve to the *same Git blob IDs* in the base tree and in the tree about
   to become primary. A byte difference blocks the landing; the assertion is a
   script check, not a reviewer instruction.

6. **The separately requested primary-commit route does the same.**
   `Invoke-FinalizeCandidateCommit.ps1` repeats the ceremony (:83-118) and gets the
   same branch, the same blob-identity assertion, and the same
   `final.replacement = false`.

7. **Recovery decides from the landed commit's own blobs, not from the frozen mode.**
   Today's entry guard (:1067-1068) tests whether the SVG file exists at the primary
   tip — a test that is effectively always true, so the overlay-recovery search runs
   for every landing and the plain recovery paths at :1075 and :1080 are unreachable.
   That is harmless only while every landing produces an overlay. Once no-overlay
   landings exist it is a live defect, in three distinct ways:
   - *The overlay search mis-identifies a no-overlay commit.* After an internally
     rebased no-overlay advance, primary holds a single-parent commit carrying the
     frozen metadata and the approved patch, whose history blobs equal its parent's.
     Every filter in `Test-HistoryReplacementRecovery` (:864-908) still passes —
     including the index check `[int]$last.index -ne ($parentRows.Count - 1)` (:903),
     which is trivially satisfied by an unchanged JSONL — so the search reports a
     replacement recovery with `final.replacement = true` and
     `historyUpdate.status = 'recovered'` for a landing that produced nothing.
   - *The plain paths cannot see the advance at all.* Both retained plain checks read
     the primary **checkout** head, `$script:PrimaryIdentity.Head` (:1076 and, inside
     `Test-LandingRebasedRecovery`, :772-773), which `Get-FinalizeGitIdentity` takes
     from `git rev-parse HEAD` in that worktree (`FinalizeWorkflowCommon.psm1`
     :107). A crash between the compare-and-swap and the primary checkout reset —
     exactly the `FixtureFailure -ceq 'post-update-ref'` point at :587 — advances only
     `refs/heads/<primary>`, so that checkout head is still the *old* tip and both
     checks return `$false` while primary has in fact already landed the commit.
   - *An `if/elseif` chain hides the remaining paths.* A `$false` from the first
     branch ends recovery instead of trying the next one.
   The fix is a sequential `-not $recovered` chain whose no-overlay member works from
   the primary **ref** and settles the shape question by blob identity, which is the
   only fact that distinguishes the two landed shapes. It runs before the overlay
   search, so a blob-identical commit is recovered as the no-overlay landing it is and
   never reaches the search that would mis-read it; an overlay commit fails the blob
   check immediately and falls through to the unchanged search. The frozen
   `$HistoryContractMode` is used only as an upper bound on what the overlay search
   may need to look for, never as the answer, because preparation branches on the mode
   re-evaluated under the lease (:315) and `catch-up` may narrow to `carry-forward`
   (:300-303).

Nothing in this chain asks an agent to decide anything. The producer decides from
its own diff classification; the landing decides from the producer's re-checked
receipt; the validators reject every inconsistent shape; the blob-identity
assertion proves the outcome.

### Why refuse rather than emit a no-output receipt

`Generate` is refused for `carry-forward` instead of returning a receipt with null
outputs because: (a) the user's direction names the producer as an enforcement
site, and a producer that *cannot* emit docs-only history bytes is the strongest
mechanical form; (b) it needs no schema branch — `outputs.jsonl`/`outputs.svg` stay
mandatory in `broken-engine-code-quality-history-update/v1`, and the validator
change is a one-value enum removal instead of a conditional-shape rule; (c) the
only production callers (landing and the primary-commit route) already skip Generate
for `carry-forward`, so the refusal is unreachable in normal operation and exists
purely as the backstop. Accepted cost: three carry-forward-only determinism
assertions in the metrics fixture suite lose their subject (see In scope, item 9) —
they assert behavior that will no longer exist.

## Critical files

| File | Region | Role |
| --- | --- | --- |
| `.agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetricsHistory.ps1` | `Get-Plan` :422-435; `Invoke-History` :453-503 | Classification (unchanged) and the Generate refusal |
| `.agents/scripts/FinalizeWorkflowCommon.psm1` | `Assert-FinalizeHistoryContractReceipt` :585-621; `Assert-FinalizeHistoryUpdateReceipt` :630-655 | Receipt validators |
| `.agents/skills/finalize-changes/scripts/Invoke-FinalizeLanding.ps1` | `Prepare-HistoryReplacementCommit` :551-575; `Advance-PrimaryExactCandidate` :579-586; recovery entry :1065-1081; recovery tuple :1006-1008; mode default :1049; `historyUpdate` skeleton :69 | Landing transaction |
| `.agents/skills/finalize-changes/scripts/Invoke-FinalizeCandidateCommit.ps1` | :88 mode re-check; `New-HistoryReplacement` :83; `Invoke-PrimaryHistoryAdvance` :90; :112-118 | Primary-commit route |
| `.agents/skills/finalize-changes/scripts/Test-FinalizeWorkflowFixtures.ps1` | stub producer :251-266; `New-PrimaryCandidateParameters` :467-473; `New-RetryLandingParameters` :806-808; landing assertions :493-494, :740-760, :839, :921, :997, :1108, :1156 | Finalize fixture suite |
| `.agents/skills/code-quality-metrics/scripts/Test-CodeQualityMetricsHistoryFixtures.ps1` | :77-78, :107, :110-124, :125-127 | Metrics fixture suite |
| `AGENTS.md` | Step 8, the "only postconfirmation source exception" sentence (line 83) | Root constraint |
| `.agents/skills/finalize-changes/SKILL.md` | :53-58, :134-137, :145-146, :212-221 | Skill prose |
| `.agents/skills/finalize-changes/references/scripts.md` | :24-25, :59-66, :67-77, :146-190, :223-232 | Script contract prose |
| `.agents/skills/verify-changes/SKILL.md` | :35-36, :57-60, :119-131 | Acceptance-review prose |
| `.agents/skills/code-quality-metrics/SKILL.md` | :24-28 (`## History`) | Producer prose + fixture invocation |
| `.agents/skills/code-quality-metrics/references/HistoryContract.md` | :47-56, :106-115 | Decision table and Generate contract |
| `.agents/skills/code-quality-metrics/references/history/README.md` | :3-9 | History data description |

## In scope

1. **Producer refusal** —
   `.agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetricsHistory.ps1`,
   `Invoke-History` (:453). Immediately **after** `Assert-UniqueOutput $Repository $Output`
   (:454) and **before** `[IO.Directory]::CreateDirectory` (:455), throw when
   `$Plan.decision.captureMode -ceq 'carry-forward'` with a message naming the rule
   (history is written only for a metric-supported C++ change). Placing it after the
   containment check keeps the existing outside-`Temp` rejection meaningful and
   guarantees no directory or file is created. Then delete the now-unreachable
   carry-forward value-copy `else` branch (:468-471) and collapse the
   `if ($Plan.decision.forceSnapshot)` guard (:457), which is true for every mode
   that still reaches this point. `Get-Plan`, `New-Contract`, the schema strings,
   `Get-EffectiveRows` (:361-372), and `New-HistorySvg` (:384-412) are untouched;
   `-Mode Contract` still reports `carry-forward` exactly as today.

2. **Contract-receipt consistency rule** — `.agents/scripts/FinalizeWorkflowCommon.psm1`,
   `Assert-FinalizeHistoryContractReceipt`, after the `forceSnapshot` check (:608):
   reject a receipt whose `decision.captureMode` is `cpp-change` while
   `patch.cppChanged` is false, or `carry-forward` while `patch.cppChanged` is true.
   `catch-up` accepts either, because the one-time catch-up row is not patch-driven.

3. **Update-receipt rejection** — same module, `Assert-FinalizeHistoryUpdateReceipt`
   (:635): the accepted `captureMode` set becomes `@('catch-up','cpp-change')`, with
   a message stating that a carry-forward landing produces no history output. Delete
   the now-dead carry-forward coverage rule (:649); keep :650 as the unconditional
   "coverage is required" rule.

4. **Landing: split the prepare step** —
   `.agents/skills/finalize-changes/scripts/Invoke-FinalizeLanding.ps1`,
   `Prepare-HistoryReplacementCommit` (:551-575). Keep `Assert-HistoryPathsClean`
   (:553) and `Validate-HistoryContractForLanding` (:554) as the shared preamble and
   keep the existing `catch` restore wrapper (:565-574) around both routes. After the
   re-check, read the re-evaluated mode from `$result.historyContract.current.mode`
   (set at :315) and branch:
   - non-`carry-forward`: today's path, byte-for-byte unchanged (Generate :555,
     `Validate-HistoryUpdate` :556, `New-HistoryOverlayTree` :557,
     `New-FrozenReplacementCommit` :558, postcondition :561, `final.replacement = true`).
   - `carry-forward`: call the new `Assert-HistoryPathsUnchanged` (item 5), then set
     `$result.final = @{ commit = $script:LandingCommit; tree = $script:LandingTree;
     parent = $script:LandingPrimaryTip; replacement = $false }` and
     `$result.historyUpdate.status = 'skipped'` (receipt/rowDate/jsonl/svg stay null,
     matching the :69 skeleton), and return that commit/tree/parent tuple.
   `Advance-PrimaryExactCandidate` (:579-586) is otherwise unchanged: it consumes the
   returned tuple, so the CAS, checkout resets, session-branch advance, rollback, and
   every postcondition at :587-614 apply identically to both routes.

5. **Landing: mechanical no-overlay assertion** — same file, new
   `Assert-HistoryPathsUnchanged([string]$BaseRevision, [string]$LandedRevision)`
   placed beside `Assert-HistoryPathsClean` (:502). For each of the two reserved
   paths it resolves `git rev-parse <revision>:<path>` in the session worktree and
   blocks with `Throw-Landing 2 'history.no-overlay-invalid'` when the two blob IDs
   differ or either lookup fails. This is the mechanical verification the user's
   direction requires: a carry-forward landing cannot advance primary unless both
   history files are provably identical to the base tree's.

6. **Landing: recovery entry guard and chain** — same file, :1065-1081. Replace
   `$historyArtifactPresent` (the `cat-file -e ...svg` probe at :1067) with
   `$overlayPossible = $HistoryContractMode -cin @('catch-up','cpp-change')`, and
   convert the `if/elseif/elseif` chain into four sequential `-not $recovered` checks,
   in this order:
   1. the existing plain identity/ancestor check (:1075-1079), unchanged — it is the
      cheapest and it cannot fire for an overlay landing, because that landing's
      `$script:LandingCommit` is the approved session commit while primary carries the
      replacement commit, which the approved commit is not an ancestor of;
   2. the new `Test-LandingNoOverlayRecovery` below;
   3. the overlay search, only when `$overlayPossible` and
      `$primaryRefTip -cne $ApprovedSessionCommit`; the WorktreeCli-path resolution at
      :1069-1072 moves with this branch;
   4. `Test-LandingRebasedRecovery` (:1080).
   `$overlayPossible` is only an upper bound: preparation branches on the re-evaluated
   mode, and an approved `catch-up` may narrow to `carry-forward` (:300-303), so a
   frozen `catch-up` landing can have either shape and step 2 is what tells them apart.
   `Test-HistoryReplacementRecovery` (:864-951), `Find-HistoryReplacementMatch`
   (:817-862), `Test-RecoveryHistoryFields` (:803-815), and `Test-LandingRebasedRecovery`
   (:771-789) keep their bodies unchanged; ordering the blob-identity check ahead of the
   search is what keeps a no-overlay commit out of it.

   **The new `Test-LandingNoOverlayRecovery`** — same file, a new function placed
   beside `Test-LandingRebasedRecovery` (:771). It reads the primary **ref**, never the
   possibly stale primary checkout head, and recovers only a commit it can prove is
   this landing's own no-overlay work:
   1. Use `$primaryRefTip` (already read at :1066). Return `$false` when it equals
      `$ExpectedPrimaryTip` — nothing landed, so the normal landing path must run.
   2. Identify the landed commit: `$primaryRefTip` qualifies when it is
      `$ApprovedSessionCommit` itself, or when it is a single-parent commit whose
      parent is `$ExpectedPrimaryTip` and whose `Get-LandingPatchIdentity` `PatchId`
      and `Changes` equal the approved identity — the same proof
      `Test-LandingRebasedRecovery` uses at :778-782, which covers the internally
      rebased no-overlay advance. Anything else returns `$false`.
   3. Call `Assert-HistoryPathsUnchanged` (item 5) between that commit's parent and the
      commit. Differing blobs mean the landing did produce an overlay, so return
      `$false` and let the unchanged overlay search handle it; identical blobs prove
      the no-overlay shape regardless of the frozen mode.
   4. `Acquire-RecoveryLandingLock` (:347), then re-read the primary ref under the
      claim and repeat steps 2-3 when it moved, mirroring the overlay route's
      post-claim re-check (:918-930).
   5. Reconcile both checkouts exactly as the overlay route does after its match:
      refresh `$script:PrimaryIdentity`, reset the primary checkout to the landed
      commit and prove it clean (:934-938 pattern — this is what repairs the stale
      checkout a `post-update-ref` crash leaves behind); require the session branch to
      be `$ApprovedSessionCommit` or an ancestor of the landed commit, then advance and
      hard-reset it to the landed commit and prove it clean (:939-944 pattern).
   6. `Assert-ApprovedCandidateTree`, set `$script:LandingCommit`, `$script:LandingTree`,
      `$script:LandingPrimaryTip`, `$script:FinalCommit`, `$script:FinalTree`, then
      `$result.final = @{ ...; replacement = $false }`,
      `$result.historyUpdate.status = 'skipped'`, `$result.primaryAdvanced = $true`,
      and return `$true`.
   Because step 5 leaves `$script:PrimaryIdentity` on the reconciled checkout, the
   shared `Assert-LandingSanity` at :1082 and the lock adoption and
   `Complete-LandedState` block at :1086-1103 apply unchanged.

7. **Landing: recovery-evidence tuple rule** — same file, :1006-1008. Keep the
   all-or-nothing tuple rule, and add: when `$HistoryContractMode -ceq 'carry-forward'`
   and any tuple element or byte count is supplied, block with
   `Throw-Landing 1 'input.history-recovery-unexpected'` — a caller cannot claim
   history artifacts for a landing whose contract mode produces none. Leave the
   omitted-mode default at :1049 as `carry-forward`: with this change the safe
   default is the route that writes nothing.

8. **Primary-commit route** —
   `.agents/skills/finalize-changes/scripts/Invoke-FinalizeCandidateCommit.ps1`. At
   the `-AdvancePrimary` mode re-check (:88) keep every existing guard. In
   `Invoke-PrimaryHistoryAdvance` (:90) branch on the re-checked contract mode: for
   `carry-forward`, skip the Generate call, `New-HistoryReplacement` (:83), the
   history index writes and the two `Copy-Item` history-file copies; assert both
   reserved blob IDs are identical between `$ExpectedPrimaryTip` and
   `$VerifiedCandidateCommit`; CAS primary to `$VerifiedCandidateCommit`; reconcile
   only the declared source paths; set
   `final = @{...; replacement = $false }` and `historyUpdate.status = 'skipped'`.
   The rollback path, postcondition checks, and `broken-engine-finalize-candidate/v3`
   property set are otherwise unchanged.

9. **Metrics fixture suite** —
   `.agents/skills/code-quality-metrics/scripts/Test-CodeQualityMetricsHistoryFixtures.ps1`:
   - `:35-36` (classifier/decision-literal assertions): keep.
   - `:77-78` `Test-SchemasAndDeterminism`: the carry-forward value-copy assertion
     (`snapshotEvidence = [ordered]@{ verbosity = [double]$last.verbosity`) loses its
     subject; replace it with an assertion that `Invoke-History` refuses
     `carry-forward` before creating its output directory, and keep the
     `captureMode = if ($History.Suffix.Count -eq 0)` guard assertion (:78).
   - `:107` (carry Contract avoids runtime capture): keep unchanged — Contract
     behavior is unchanged.
   - `:110` (carry Generate emits a row without coverage): becomes "carry Generate
     exits 2 and creates no output directory and no file", asserting both the exit
     code and `-not (Test-Path Temp/out-a)`.
   - The same-date consecutive-row and SVG polyline-growth assertions that follow
     `:110` (they consume `Temp/out-a`/`Temp/out-b` carry-forward output) are deleted:
     they cover row appending that no longer occurs for carry-forward. Accepted
     coverage loss, recorded here; catch-up/cpp-change row generation stays covered by
     the `-RunCapture` path.
   - the outside-`Temp` containment case: keep, and add a short comment that
     containment is still checked before the mode refusal.
   - `:134` (`Test-BaseCommitHistoryBinding`) runs only Contract: unchanged.

10. **Documented invocation for the metrics fixture suite** —
    `.agents/skills/code-quality-metrics/SKILL.md`, `## History` (:24-28). The suite
    has **no documented invocation anywhere in the repository** (verified by grep
    across `*.md`, `*.ps1`, `*.psm1`, `*.yaml`), so its mandatory re-run is not
    runnable as documented. Add the canonical form and its run trigger:
    `pwsh -NoProfile -File .agents/skills/code-quality-metrics/scripts/Test-CodeQualityMetricsHistoryFixtures.ps1 -RepositoryRoot <absolute repository root>`,
    stating that it runs whenever `Invoke-CodeQualityMetricsHistory.ps1`,
    `Invoke-CodeQualityMetrics.ps1`, or the suite itself changes, and that
    `-RunCapture` adds the analyzer-backed cases.

11. **Finalize fixture suite** —
    `.agents/skills/finalize-changes/scripts/Test-FinalizeWorkflowFixtures.ps1`. Today
    every scratch fixture commit is `.txt`-only and the stub producer hard-codes
    `carry-forward` in both receipts (:260, :265) while computing an unused
    `$captureMode` from the diff (:258) — so under the new rule every existing
    overlay/CAS/recovery case would flip to no-overlay and lose its coverage. Rework:
    - stub producer heredoc (:251-266): select the mode from
      `$env:BROKEN_ENGINE_FINALIZE_HISTORY_MODE`, defaulting to `cpp-change`, matching
      the suite's existing `BROKEN_ENGINE_FINALIZE_HISTORY_RECEIPT` convention. For
      `cpp-change` emit a receipt that passes the validators: one metric-supported
      `patch.changes` record with `metricSupportedChanges = 1`, `cppChanged = $true`;
      a `capture` block whose `manifest` has one sorted `100644` file entry and whose
      `manifestDigest` is that manifest's compressed-canonical-JSON SHA-256 (the
      `Assert-FinalizeHistoryCapture` self-consistency rule at :568); the fixed
      `snapshot` block for Contract; `series.coverage` with `corpusCounts`/
      `targetCounts` for Generate; and `captureDigest=`, `identityDigest=`,
      `scbDigest=` in the SVG `<desc>` matching the receipt (`Validate-HistoryUpdate`
      :268-271). For `carry-forward`, emit the Contract receipt as today and make
      Generate **exit 2**, mirroring the real producer so any regression that calls
      Generate on the no-overlay route fails the suite loudly.
    - `New-RetryLandingParameters` (:806-808) currently passes no `History*`
      arguments, so its landings would take the omitted-mode `carry-forward` default:
      add `HistoryContractMode='cpp-change'` plus the stub's capture and runtime
      digests (cache them once from a stub Contract run, as
      `$script:PrimaryHistoryContract` already does at :469) so
      `Validate-HistoryContractForLanding` :305-310 is satisfied.
    - existing overlay assertions (:493-494, :751-752, :839, :921, :997, :1108, :1156)
      keep asserting `final.replacement` true and stay otherwise unchanged.
    - new cases, each with `BROKEN_ENGINE_FINALIZE_HISTORY_MODE=carry-forward` and
      `HistoryContractMode='carry-forward'` except where the case says otherwise:
      (i) *session landing, no overlay*: exit 0 / `landed`; `final.replacement` false;
      `final.commit` equals the approved session commit; primary HEAD equals it;
      `historyUpdate.status` `skipped`; both reserved paths' blob IDs identical between
      the pre-landing primary tip and the landed commit; and
      `git diff --name-only <base> <landed>` names neither reserved path.
      (ii) *no-overlay crash recovery, plain*: run the landing with
      `-FixtureFailure 'post-update-ref'` so it advances `refs/heads/<primary>` and
      stops before the primary checkout reset (:587), then re-invoke the same landing
      and assert idempotent recovery: exit 0 / `landed`, `primaryAdvanced` true,
      `final.replacement` false, `historyUpdate.status` `skipped`, the landed commit
      unchanged, both reserved paths still byte-identical, and both the primary and the
      session checkout reconciled to the landed commit and clean. Without
      `Test-LandingNoOverlayRecovery`'s reconciliation (item 6) this case fails,
      because the stale primary checkout head defeats both retained plain checks.
      (iii) *no-overlay crash recovery, internally rebased*: the same
      `post-update-ref` crash and re-invocation for a no-overlay landing that had to
      rebase internally, reusing the suite's existing foreign-movement retry setup
      (`New-RetryLandingParameters` :806-808) with the carry-forward mode, asserting the
      same result plus that the recovered commit is the rebased one, not the approved
      session commit. This is the case the unchanged overlay search would otherwise
      mis-report as a replacement.
      (iv) *primary-commit route, no overlay*: the same replacement/blob assertions on
      `Invoke-FinalizeCandidateCommit.ps1 -AdvancePrimary`.
      (v) *frozen `catch-up` narrowed to `carry-forward`*:
      `HistoryContractMode='catch-up'` with
      `BROKEN_ENGINE_FINALIZE_HISTORY_MODE=carry-forward`, crashed at
      `post-update-ref` and re-invoked, asserting the no-overlay recovery result above
      even though the frozen mode made `$overlayPossible` true.
    - new validator cases, invoked **directly** against the functions the suite
      already imports from `.agents/scripts/FinalizeWorkflowCommon.psm1` (:18) rather
      than through a landing, because the carry-forward landing route never calls
      Generate and therefore can never reach `Assert-FinalizeHistoryUpdateReceipt`
      with a forged receipt:
      (vi) a forged `broken-engine-code-quality-history-update/v1` receipt with
      `captureMode = 'carry-forward'` and populated `outputs` is rejected by
      `Assert-FinalizeHistoryUpdateReceipt` (item 3), asserting the thrown message.
      (vii) a forged `broken-engine-code-quality-history-contract/v1` receipt with
      `decision.captureMode = 'cpp-change'` and `patch.cppChanged = $false`, and its
      mirror with `carry-forward` and `cppChanged = $true`, are rejected by
      `Assert-FinalizeHistoryContractReceipt`'s new consistency rule (item 2). Both
      forgeries must otherwise be valid receipts — `decision.forceSnapshot` consistent
      with `captureMode` (:608) and the `capture`/`snapshot` shapes valid for that mode
      (:611-619) — so the new rule is provably what rejects them.

12. **Prose (describes only; enforces nothing)** — all at tracked `.agents/...` paths:
    - `AGENTS.md` Step 8: the "only postconfirmation source exception" sentence gains
      that the overlay exists only for a C++-change (or catch-up) history contract, and
      that a carry-forward landing adds no generated byte and lands the reviewed commit
      itself.
    - `.agents/skills/finalize-changes/SKILL.md` :53-58 (Generate/overlay summary),
      :134-137 (step 5 overlay sentence), :145-146 (step 6 rebase/overlay sentence),
      :212-221 (crash-recovery paragraph: the recovery tuple applies only to an
      overlay-producing contract mode).
    - `.agents/skills/finalize-changes/references/scripts.md` :24-25 (Generate
      interface), :59-66 (primary-commit route result), :67-77 (Contract/Generate
      contract), :146-190 (landing contract, overlay, recovery), :223-232 (fixture
      trigger list — unchanged in content, re-read to confirm it still names the
      changed scripts).
    - `.agents/skills/verify-changes/SKILL.md` :35-36, :57-60, :119-131: the acceptance
      table carries the Contract receipt for every finalization, and the overlay
      exception applies only when the frozen contract mode produces one; a carry-forward
      landing must show no generated path at all.
    - `.agents/skills/code-quality-metrics/SKILL.md` :24-28: Generate refuses
      carry-forward; plus item 10's fixture invocation.
    - `.agents/skills/code-quality-metrics/references/HistoryContract.md` :47-56
      (decision table gains "no output, no history update" for carry-forward) and
      :106-115 (Generate emits a receipt only for catch-up/cpp-change).
    - `.agents/skills/code-quality-metrics/references/history/README.md` :3-9: a live
      point is generated only for a metric-supported C++ change; existing carry-forward
      rows remain valid history and are never rewritten.

## Out of scope

- **`Documents/Plans/Skills/CodeQualityHistoryContractSandboxReproduction.md`** owns
  the `Invoke-GitBytes` rethrow defect (`throw (($stderr.Trim()) -or 'git command failed.')`
  at :74 of the same producer, which yields a useless boolean message inside a
  read-only sandbox). Sequencing note only: that plan and this one both edit
  `Invoke-CodeQualityMetricsHistory.ps1`, so whichever lands second rebases onto the
  first. Do not absorb or fix it here.
- `Documents/Plans/Skills/FinalizeCandidateStaleBaseHistoryBlock.md` remains the
  surviving owner of the baseline-aware session-route scope for candidate-creation
  blocks caused by history-overlay commits on primary. This change makes overlay
  commits rare but does not remove them. Sequencing note only.
- No rewrite, pruning, or renumbering of the existing 19 `carry-forward` rows or of
  any tracked history byte. `Get-EffectiveRows` already inherits values across such
  rows, so they keep rendering correctly.
- No schema version bump: `broken-engine-code-quality-history-contract/v1`,
  `broken-engine-code-quality-history-update/v1`, `broken-engine-finalize-landing/v4`,
  and `broken-engine-finalize-candidate/v3` all keep their identifiers, and the
  immutable 133323-byte / 648-line prefix contract is untouched.
- No C++, GLSL, or WorktreeCli change; the whole history path is in-tree PowerShell.
  WorktreeCli is used only for the landing lock and scheduler, which this change does
  not touch.
- No change to catch-up classification (see Settled decisions) and no change to
  `cpp-change` behavior.

## Risk tier and invariants

**Tier 3.** Triggers: the post-confirmation landing transaction (landing lock lease,
compare-and-swap, rollback, crash recovery); a receipt schema shared across the
`code-quality-metrics`, `finalize-changes`, and `verify-changes` skills; and an edit
to the root `AGENTS.md` Step 8 constraint. The change also spans independently owned
areas (metrics producer, finalize scripts, shared validator module).

Invariants that must hold unchanged:

- **Landing atomicity.** The compare-and-swap advance, rollback on postcondition
  failure, session-branch/worktree restore, lock lease acquisition and release, and
  Plan-claim deletion behave exactly as today for both routes.
- **Frozen-contract re-check.** `Validate-HistoryContractForLanding` (:295-317) keeps
  every existing guard, including the generator-digest check, the permitted
  `catch-up` → `cpp-change`/`carry-forward` narrowing, the capture/runtime identity
  checks, the patch-digest check, and the aggregate-digest check.
- **`cpp-change` and `catch-up` are byte-identical to today** end to end: same
  Snapshot, same row, same SVG, same overlay tree, same frozen replacement commit,
  same recovery search.
- **Recovery still handles every crash shape**: overlay landings recover through the
  unchanged replacement search; no-overlay landings — including an internally rebased
  one, and including a crash between the compare-and-swap and the primary checkout
  reset — recover through the plain identity/ancestor path or the new
  `Test-LandingNoOverlayRecovery`, which works from the primary ref and reconciles a
  stale primary checkout; and a `$false` from one path no longer suppresses the
  others. No recovery path reports a replacement for a commit whose reserved blobs
  equal its parent's, or a no-overlay result for a commit whose blobs differ.
- **No history byte reaches primary without a metric-supported C++ change**, proven
  by the producer refusal, the validator rejections, and the blob-identity assertion —
  never by prose. The one permitted exception is the user-granted exception recorded in
  Settled decisions: the single empty-suffix `catch-up` bootstrap row, which a
  documentation-only landing may still write.
- **Reserved paths stay source-forbidden**: `Assert-NoHistorySourceChanges`
  (:495-497) still blocks a session patch that edits either history path.

## Roles

- `implementer` — the script slices (items 1-8), the two fixture suites (items 9-11),
  and the prose (items 10 and 12), split into disjoint slices where possible.
- `reviewer` — the Tier-3 review set for the changed artifact types: a coherence
  review of the changed scripts and prose, `/scope-review` over the whole diff, and
  `/adversarial-review`. No C++ or GLSL changes, so no `/repo-code-review`,
  `/glsl-review`, `/code-style-review`, `/update-vcxproj`, or `/update-claude-docs`.
- `reviewer` — one fresh `/validate-skill` dispatch per changed skill package. This
  change edits three `SKILL.md` files — `.agents/skills/code-quality-metrics/SKILL.md`
  (items 10 and 12), `.agents/skills/finalize-changes/SKILL.md` and
  `.agents/skills/verify-changes/SKILL.md` (item 12) — so all three packages are
  validated, and each returns its complete `/validate-skill` result block as the
  acceptance evidence.
- `reviewer` — one fresh `/progressive-disclosure-review`, ordered after the prose
  edits, because the session changes `AGENTS.md` and `.agents/skills/**/*.md`.

## Acceptance criteria

1. `pwsh -NoProfile -File .agents/skills/finalize-changes/scripts/Test-FinalizeWorkflowFixtures.ps1 -WorktreeCliExecutable '<worktreecli-exe>'`
   — the full documented invocation from
   `.agents/skills/finalize-changes/references/scripts.md` :212, with
   `<worktreecli-exe>` replaced by the session worktree's provisioned
   `Tools/WorktreeCli/Platforms/VisualStudio2026/Output/WorktreeCli.exe` (:217-221),
   never the placeholder — exits 0 with every existing case passing plus the new
   no-overlay landing, crash-recovery, primary-commit, and validator cases of item 11,
   and reports no failed case. Mandatory: `references/scripts.md` :223-232 names every
   script this change edits.
2. `pwsh -NoProfile -File .agents/skills/code-quality-metrics/scripts/Test-CodeQualityMetricsHistoryFixtures.ps1 -RepositoryRoot <root>`
   passes, exercising the Generate refusal and the containment case. Mandatory
   because the producer changes.
3. A fixture docs-only landing (`carry-forward`) ends with: `final.replacement` false,
   primary at the reviewed source commit, `historyUpdate.status` `skipped`, and both
   reserved paths' blob IDs byte-identical between base and landed tree.
4. A fixture `cpp-change` landing still produces the overlay: `final.replacement`
   true, `historyUpdate.status` `pass`, both reserved paths changed and only those.
5. Both validators are proven by the direct-invocation fixture cases (item 11, (vi)
   and (vii)), not by inspection and not through a landing:
   `Assert-FinalizeHistoryUpdateReceipt` rejects a `carry-forward` Generate receipt
   that carries outputs, and `Assert-FinalizeHistoryContractReceipt` rejects both a
   `cpp-change` receipt whose `patch.cppChanged` is false and a `carry-forward`
   receipt whose `patch.cppChanged` is true, in each case with the new rule's own
   message.
6. `Invoke-CodeQualityMetricsHistory.ps1 -Mode Generate` on a docs-only patch exits 2
   and leaves no output directory; `-Mode Contract` on the same patch still returns a
   valid `carry-forward` receipt.
7. No surviving prose claims an unconditional per-landing history overlay. Search for
   `overlay`, `carry-forward`, and `CodeQualityMetricsHistory` in exactly the Markdown
   prose regions item 12 edits and nowhere else — `AGENTS.md` Step 8;
   `.agents/skills/finalize-changes/SKILL.md`;
   `.agents/skills/finalize-changes/references/scripts.md`;
   `.agents/skills/verify-changes/SKILL.md`;
   `.agents/skills/code-quality-metrics/SKILL.md`;
   `.agents/skills/code-quality-metrics/references/HistoryContract.md`;
   `.agents/skills/code-quality-metrics/references/history/README.md`. A hit fails only
   when it is a prohibited claim, defined as a sentence stating or implying that a
   landing (or every landing, or the landing overlay) writes, appends, regenerates, or
   commits the history files without naming the C++-change condition — that is, an
   unconditional per-landing overlay statement. Everything else passes: path literals,
   schema and mode names, descriptions of the `carry-forward` decision value itself,
   and any sentence that states the C++-only rule or is explicitly conditioned on the
   contract mode. `.ps1`/`.psm1` sources and the tracked `.jsonl`/`.svg` history data
   are out of this check's scope by construction — the scripts must keep their
   `carry-forward` literals, and the 19 existing `carry-forward` rows are preserved
   history (see Out of scope).
8. `git status` shows both tracked history files unmodified by this change itself
   (this session is documentation and scripts only — it must not append a row).
9. Each of the three changed skill packages — `code-quality-metrics`,
   `finalize-changes`, `verify-changes` — has a passing `/validate-skill` result block
   from a fresh reviewer, and `/progressive-disclosure-review` reports no accepted
   finding against the changed instruction prose.

## Settled decisions

The user settled both of this plan's remaining judgment calls on 2026-08-21. Both are
binding and already reflected throughout the sections above.

1. **Catch-up bootstrap row is an explicit exception.** `catch-up` classification and
   behavior are untouched, so the empty-live-suffix bootstrap row may still write both
   history files on any landing, including a documentation-only one. This is a
   user-granted exception to the C++-only rule stated in Context, narrowed to exactly
   that empty-suffix bootstrap row and to nothing else. The rule therefore reads: no
   history byte reaches primary without a metric-supported C++ change, except the single
   bootstrap row that starts an empty series. Because catch-up survives, the permitted
   `catch-up` → `carry-forward` narrowing survives with it, and fixture case (v) in
   item 11 is a required case.
2. **The carry-forward fixture coverage loss is accepted.** The producer refuses
   `-Mode Generate` for a carry-forward decision, and the three carry-forward-only
   assertions in the metrics fixture suite are deleted as covering behavior that ceases
   to exist (item 9). Equivalent row-generation coverage remains behind the
   analyzer-backed `-RunCapture` cases.
