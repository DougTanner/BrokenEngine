<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-22T13:53:37.821Z","dependsOn":[]} -->
# Keep the code-quality history fixture aligned with its prefix and live-suffix contract

## Context

The originating acceptance expectation was that the documented/current-checkout
metrics history fixture invocation passes:

```powershell
pwsh -NoProfile -File .agents/skills/code-quality-metrics/scripts/Test-CodeQualityMetricsHistoryFixtures.ps1 -RepositoryRoot <session worktree root>
```

In the current checkout it exits `1` with:
`CodeQualityMetricsHistoryFixtures: History fixture failed: tracked JSONL is not the immutable 133323-byte prefix.` The false required condition is in
`Test-PrefixAndSuffix` at
`.agents/skills/code-quality-metrics/scripts/Test-CodeQualityMetricsHistoryFixtures.ps1:20-28`:
it requires the complete tracked file to be exactly 133323 bytes, to have the
prefix digest as its full-file digest, and to contain exactly 648 LF-terminated
lines.

The source contract at
`.agents/skills/code-quality-metrics/references/HistoryContract.md` says the
source begins with the immutable 133323-byte/648-line prefix and may have a
live suffix beginning at index 647. At the originating session baseline, the
tracked JSONL was 138691 bytes with 683 LF lines: its first 133323 bytes still
had digest
`5a39debf4be41abebd8496b9f25ee4023d109813788e95b30da8f74474fe75ed`, followed
by a live suffix. Those baseline size and suffix values are failure evidence,
not fixture acceptance constants; receipt-verified overlays may add later rows.
The file's blob was byte-identical to the baseline path blob at commit
`774a1def513887aae849cc1c7d4a380f9c983711` (`29170f3a3964cd38e9872a1ddca6c570723b4c69`).
The active implementation boundary is the two metrics scripts being changed by
that session; this follow-up Plan is a separate planning artifact. The mismatch
therefore predates and is outside the active implementation boundary.

The landing contract also explicitly permits a receipt-verified JSONL/SVG
history overlay after confirmation (`AGENTS.md:83`,
`.agents/skills/finalize-changes/references/scripts.md:146-166`), which is how
the tracked live suffix can legitimately grow. The fixture's whole-file exact
size check therefore contradicts the current overlay contract rather than
exposing a changed immutable prefix.

The live `Documents/Plans/Skills/CodeQualityHistoryCppOnly.md` owns the
producer/finalizer no-overlay behavior. It explicitly leaves history data and
the immutable prefix untouched. This Plan records the separate fixture-contract
repair rather than expanding that Plan's history behavior.

The same suite has two scratch builders that currently assume the source ends at
the immutable prefix: `Test-CarryDateContainmentAndRepeat` at lines 92-98 reads
the complete tracked source and appends a synthetic row at index 647, and
`Test-BaseCommitHistoryBinding` at lines 128-135 does the same before adding a
working row at 648. When the complete source already has a live suffix, those
builders construct an invalid duplicate suffix before the producer can validate
it. Prefix-only cases must start from the 133323-byte prefix; complete-source
cases must derive the next row from the committed source's decoded final row
(`lastIndex + 1`) at fixture execution.

## Design

The author's recommendation is to make the fixture suite distinguish the
immutable prefix from the complete tracked source and to exercise suffix
validation through the existing production `Contract` path:

1. `Test-PrefixAndSuffix` should require at least 133323 bytes, extract exactly
   that prefix, and keep the digest, LF-count, strict UTF-8, 648-line, and
   legacy-PNG assertions against the prefix. It should leave the complete-file
   suffix untouched and accept either zero suffix bytes or any valid live suffix
   present in the committed source. It must not duplicate the production suffix
   schema validator.
2. `Test-CarryDateContainmentAndRepeat` should remain a prefix-only synthetic
   fixture: build its one-time `catch-up` row from the immutable prefix and
   give it index 647, then retain the existing date/containment/repeat checks.
   It must not append index 647 to the complete tracked source. Any later row
   used by this fixture is derived from the fixture's actual final row.
3. `Test-BaseCommitHistoryBinding` should retain its historical base-binding
   subcase on prefix-only bytes (base row 647 and working row 648), so that the
   test still distinguishes the committed base blob from the working tree. Add
   a default-suite Contract subcase that commits the complete tracked source as
   both BaseCommit and TipCommit, reads the exact committed JSONL bytes, derives
   the total rows, live-row count, final index, and complete-source SHA-256 from
   those bytes, and asserts the receipt's `series` fields against those derived
   values. If that subcase appends a working synthetic row, derive it from the
   decoded complete-source final row (`lastIndex + 1`) rather than using 647 or
   648.
4. In the same existing fixture flow, record an explicit zero-suffix case from
   the exact prefix before adding a synthetic live row. Add three malformed
   source variants—strict UTF-8, live-row schema, and noncontiguous live index—
   and invoke the production `Contract` child for each. Assert its existing
   exit-2 diagnostics (`History JSONL is not strict UTF-8.`, the relevant
   `History row ...` property error, and `Live history index is not contiguous
   at row ...`); the fixture performs no parallel schema validation.

This is the smallest repair because it keeps source-shape construction in the
existing fixture functions, validates the committed complete source by the same
producer used in production, derives all volatile suffix expectations at
execution, and keeps all history behavior and data unchanged. No compatibility
format or history-data rewrite is recommended.

## Critical files

- `.agents/skills/code-quality-metrics/scripts/Test-CodeQualityMetricsHistoryFixtures.ps1` —
  `Test-PrefixAndSuffix` lines 20-28, `Test-CarryDateContainmentAndRepeat`
  lines 86-123, `Test-BaseCommitHistoryBinding` lines 124-138, and the default
  suite invocation near the bottom; these are the authorized fixture regions.
- `.agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetricsHistory.ps1` —
  read-only production `Read-History`/`Contract` behavior exercised by the
  complete, zero-suffix, and malformed-source fixtures; no edits are owned.
- `.agents/skills/code-quality-metrics/references/HistoryContract.md` —
  read-only source contract for the immutable prefix and live suffix.
- `AGENTS.md` and `.agents/skills/finalize-changes/references/scripts.md` —
  read-only landing-overlay contract evidence.
- `.agents/skills/code-quality-metrics/references/history/CodeQualityMetricsHistory.jsonl` —
  read-only evidence; no tracked history bytes are authorized to change.
- `Documents/Plans/Skills/CodeQualityHistoryCppOnly.md` — `## Context` live-suffix
  summary; a future prose-only correction replaces volatile row/count/index
  examples with the source contract's non-stale wording.

## In scope

- Update `Test-PrefixAndSuffix` to validate the immutable 133323-byte prefix
  independently of the optional live suffix, preserving the expected prefix
  digest, 648 LF lines, strict UTF-8, and legacy-PNG check.
- Correct `Test-CarryDateContainmentAndRepeat` and
  `Test-BaseCommitHistoryBinding` so every synthetic row is appended either to
  an explicitly prefix-only fixture or at the index derived from that fixture's
  decoded final row; retain the base-binding assertions.
- Add the default-suite complete-source `Contract` success case, explicit
  zero-suffix evidence, and strict-UTF8/schema/noncontiguous-index rejection
  cases by invoking the existing production validator and checking its
  diagnostics, without adding a second schema implementation.
- In the complete-source `Contract` case, derive the expected raw-byte hash,
  total rows, live-row count, and final index from the fixture's committed
  complete source and compare those values with the receipt; do not hard-code a
  complete-source suffix row count, final index, or full-file hash.
- Update only the `## Context` prose in
  `Documents/Plans/Skills/CodeQualityHistoryCppOnly.md` to describe a live
  suffix that may grow instead of freezing row counts or indices; leave that
  Plan's producer/finalizer behavior and other sections unchanged.
- Adjust only the related assertion wording, byte slicing, scratch history
  construction, and child invocations required by those fixture contracts.
- Re-run the documented fixture suite and `plan validate`.

## Out of scope

- `.agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetricsHistory.ps1`
  behavior, including capture classification, Contract/Generate output,
  `Read-History`, and error branches; it is invoked only as the existing
  validator under test and is not edited by this Plan.
- The no-overlay producer, finalizer, receipt, recovery, or history-contract
  redesign owned by `Documents/Plans/Skills/CodeQualityHistoryCppOnly.md`.
- Any edit, rewrite, pruning, renumbering, or digest change to
  `CodeQualityMetricsHistory.jsonl` or its SVG, including the 133323-byte
  prefix.
- New compatibility formats, new history schema, unit tests, a parallel suffix
  schema validator, or unrelated fixture coverage.
- Any `CodeQualityHistoryCppOnly.md` change outside its `## Context` prose,
  including its producer, finalizer, receipt, recovery, or fixture behavior.

## Risk tier and invariants

Expected **Tier 2** (scoped tool behavior): the change adjusts a code-quality
history fixture's acceptance logic without changing production runtime,
landing, receipt, determinism/CRC, serialization/`.pack`, replay, wire,
threading, allocation, shader, or build behavior. Escalate if implementation
reaches the producer, finalizer, tracked history data, or build/bootstrap
coordination.

The prefix bytes, digest, 648 LF lines, and strict UTF-8 contract remain
unchanged. A valid live suffix remains accepted, including suffix growth after
later receipt-verified overlays; malformed suffix bytes are rejected by the
production validator. The complete-source Contract proof derives its expected
row count, final index, and full-file hash from the exact committed source, so
no later valid suffix is rejected by a frozen value. The fixture does not write
or modify tracked history. Existing diagnostics and generated-output cases
remain covered.

## Coordination

`Documents/Plans/Skills/CodeQualityHistoryCppOnly.md` is a related live Plan,
not a duplicate. It depends on this Plan because its documented fixture
acceptance must run against a valid prefix/full-source fixture contract. The
same root cause also affects that Plan's `## Context`: its live-suffix example
must not freeze row counts or indices that receipt-verified overlays can grow.
This Plan owns that future prose-only correction as well as the source-shape
and validator coverage in
`Test-PrefixAndSuffix`, `Test-CarryDateContainmentAndRepeat`, and
`Test-BaseCommitHistoryBinding`: prefix-only construction, complete-source
Contract acceptance, zero-suffix evidence, and malformed UTF-8/schema/index
rejection. `CodeQualityHistoryCppOnly.md` owns the producer/finalizer no-overlay
behavior and its overlapping carry-forward/Generate and landing assertions in
the same fixture file. Implement the source-shape contract first, then rebase
the no-overlay fixture edits and preserve both regions; re-derive line numbers
after either edit. Neither Plan edits the tracked history data or the immutable
prefix. Its reciprocal `## Coordination` section records the Context-prose
ownership; no separate Plan is needed for this same root cause and boundary.

## Acceptance criteria

- The documented default invocation exits `0` on the current checkout and
  reports the fixture schema/status pass object; the suffix proof does not
  depend on optional `-RunCapture`.
- The fixture still fails if the first 133323 bytes, their SHA-256, their 648
  LF lines, or their strict UTF-8 boundary changes.
- The default suite's production `Contract` scenario commits the complete
  source as both `BaseCommit` and `TipCommit`, derives the expected raw-byte
  SHA-256, total rows, live-row count, and final index from that exact committed
  blob, and asserts `series.historyBytesSha256`, `series.rows`,
  `series.liveRows`, and `series.lastIndex` against those values. It also
  records a valid zero-suffix prefix before any synthetic row is added. No
  complete-source suffix row count, final index, or full-file hash is
  hard-coded in this Plan's Design, In scope, Invariants, or Acceptance.
- Production `Contract` rejects the malformed strict-UTF8, suffix-schema, and
  noncontiguous-index variants with the existing validator diagnostics; the
  fixture contains no duplicate suffix schema logic.
- The prefix-only carry/date and base-binding fixtures use valid contiguous
  synthetic rows, while any complete-source working row is derived as
  `lastIndex + 1`.
- The `CodeQualityHistoryCppOnly.md` `## Context` describes the live suffix with
  non-stale contract language rather than fixed row counts or indices; its
  producer/finalizer behavior remains unchanged.
- Existing diagnostic and generated-output fixture cases retain their current
  behavior, and neither tracked history file is modified.
- `plan validate` exits `0` with `status: valid` and `code: ok`.

## Notes

This Plan records the pre-existing/out-of-scope residual from the active
metrics-script session. It does not claim or land the Plan, and it does not
authorize changing the active implementation's two scripts beyond the
separate fixture region named above.
