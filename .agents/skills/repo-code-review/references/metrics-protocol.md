# Metrics Protocol

Operating rules for the `## Workflow` step 1 Compare run in
[../SKILL.md](../SKILL.md).

## Running Compare

Compare writes the ignored `Temp/CodeQualityMetrics` cache, which the
`/codex-review` read-only sandbox denies. Under that sandbox the manager runs
Compare host-side and puts the verbatim
`broken-engine-code-quality-evidence/v2` digest in the scope file together with
the baseline and head it ran against. Consume that digest and validate it
yourself: the stated baseline and head must be the reviewed change set's, and
the digest's own `targetSelection` pairs must name the supplied targets file's
paths, whose per-side `sha256` identities are what bind the digest to those
exact file contents. An absent digest, a stated identity that does not match, or
a target set that does not match is a blocker for this step — report it and
leave the review incomplete with `NEEDS_ACTION`; never continue as if metrics
did not apply.

Outside that sandbox, run Compare yourself: `pwsh -NoProfile -File
.agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetrics.ps1 -Mode
Compare -Targets <supplied-targets-file> -Baseline <fixed-full-sha>
-RepositoryRoot <absolute-checkout-root> -Digest`. It takes roughly 2 to 3.5
minutes, so invoke it with a call timeout of at least `600000` ms. Omit
`-OutputPath` so this review retains no file.

Record the digest's `profile`, `targetSelection`, `coverage`, and `comparison`
evidence. Never reconstruct the digest's field selection or summarization
inline.

This findings-only review permits no changes except the entry point's validated
ignored `Temp/CodeQualityMetrics` cache and capture writes; do not write a
targets file, output file, source, or repository metadata.

Record `comparison.contextChanges` without widening the review scope.

## Failures

Every failure exits `2` with diagnostics on stderr and no digest on stdout. An
operational failure leaves the review incomplete with `NEEDS_ACTION`, not a
correctness finding.

## Advisory status and target failures

Metrics remain advisory. A regression or classification never becomes a finding
or changes a clean review to non-PASS. Only independent source inspection may
promote a substantial new near-copy under the duplication rule in
[../SKILL.md](../SKILL.md), or another reachable correctness violation. Changes
that quietly weaken the code's structure are advisory follow-up evidence.

A target failure arrives as one JSON line on stderr; classify it by that line's
`code` field. A `target-parse-failure` makes the review incomplete with
`NEEDS_ACTION`, not a finding: request a separate authorized implementer to apply the listed narrow
sanitizer spot-fix, then rerun Compare in focused review. A
`target-signature-extraction-failure` is also incomplete; investigate it and
rerun, without treating it as a sanitizer instruction. Do not return PASS until
every authorized target has complete parsing and signature extraction. A
corpus-only `upstream-omitted` row — a corpus file the analyzer's parser left
out — is an advisory residual and preserves PASS. Report coverage and the
outcome from `comparison`.
