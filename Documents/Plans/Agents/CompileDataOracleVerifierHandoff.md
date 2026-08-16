<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-16T13:57:21.640Z","dependsOn":[]} -->
# Fix: verify-changes — remove the unobtainable typed data-oracle verifier receipt requirement

## Context

At a landing gate this session, the `/verify-changes` dispatch through
`/codex-review` was refused before the reviewer ever started. The prompt
assembler exited `2` with code `prompt.typed-artifacts-required` and the message
"/verify-changes needs evidence -ScopeFile does not carry:
broken-engine-data-oracle-verifier-result/v1". The gate is
`.agents/skills/codex-review/scripts/New-CodexReviewPrompt.ps1:451`, which fires
whenever a quoted `broken-engine-build-result/v1` envelope names a
`BrokenEngineSandbox` target (`:442-448`), and it enforces
`.agents/skills/verify-changes/SKILL.md:29-31`, which requires "the passing
`broken-engine-data-oracle-verifier-result/v1` result" for a change set
including a game build.

No documented route produces that typed result for the manager to hand off:

- `.agents/skills/compile/references/runtime-data-mode.md:22` states the build
  invocation runs the oracle scripts itself and closes with "Never issue or
  verify a receipt by hand as a separate agent command; the harness handoff
  reads the identities the build reports"; `:44` repeats "Hand both receipt
  identities from the build's reported summary to the harness." So the agent may
  not run `Test-DataOracleReceipt.ps1` itself.
- `.agents/skills/compile/scripts/Invoke-CompileBuild.ps1` consumes the typed
  JSON internally and discards it: `Test-OracleReceiptEntry` (`:309-322`) parses
  the child result at `:316` and then adds only a derived prose line at `:319`,
  `"<mode> receipt verified: path=... sha256=... dataRoot=... baseline=...
  aggregate=..."`; `New-OracleReceiptEntry` (`:290-307`) does the same at `:296`
  for issuance. The schema name
  `broken-engine-data-oracle-verifier-result/v1` exists only inside
  `.agents/skills/compile/scripts/Test-DataOracleReceipt.ps1:163` and never
  reaches the build's stdout envelope or its stderr summary.

A builder inspected all three captured build outputs from this session and
confirmed none of them contains the schema string. The manager worked around the
block by pasting the derived `compile-build:` receipt summary lines into the
scope file with a provenance note explaining that the typed artifact is
unobtainable through the documented route — a substituted artifact the gate was
written to reject.

The claimed Plan this session executed,
`Documents/Plans/Agents/CompilePrefastAnalysisRebuildEvidence.md`, confined its
`## In scope` to the `-Prefast` rebuild in `Invoke-CompileBuild.ps1` and the
analysis text in `references/prefast-mode.md`, and its `## Out of scope`
explicitly names "Data-mode selection, oracle receipts, lock handling, and every
other compile skill protection". The receipt handoff is therefore outside the
active change, not an in-scope blocker of it.

Session provenance (machine-local; not reproducible after cleanup):
- Client: claude
- Conversation session ID: 6a6f432e-7392-49bd-9cc3-50b7d2239a3d
- Worktree/branch UUID: 7bb47afc-da3b-4686-b67c-2afba2b6daa5
- Session branch: claude/7bb47afc-da3b-4686-b67c-2afba2b6daa5
- Worktree: .claude\worktrees\BrokenEngine\7bb47afc-da3b-4686-b67c-2afba2b6daa5
- Landing ref: the session branch above, whose tip is the session's final
  commit and which survives exactly as long as the worktree recorded above.
  Fallback once that branch is gone:
  `git log --diff-filter=A --format=%H -- <this plan path>`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to this session alone
  (its diff limited to this session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes this worktree: Codex
  transcript discovery requires the producing worktree to remain registered,
  and Claude review requires the exact conversation session ID above.

## Design

The root cause is already established and recorded in `## Context` above, and
the direction is fixed by explicit user decision: the requirement that a game
build's landing handoff paste a verbatim
`broken-engine-data-oracle-verifier-result/v1` receipt "seems like overly
complex ceremony that should be removed". So this Plan removes the typed
artifact requirement from the landing-verification contract instead of teaching
the build to produce it.

The rationale is that the requirement carries no information the handoff does
not already have. The build invocation runs the oracle scripts itself and fails
closed when verification fails, so a `broken-engine-build-result/v1` envelope
reporting `success` for a `BrokenEngineSandbox` target is itself proof that data
verification passed, and the build's reported `"<mode> receipt verified:
path=... sha256=... dataRoot=... baseline=... aggregate=..."` summary line
already names the exact receipt identity that was checked. The verbatim typed
JSON would only restate that, and — as `## Context` proves — no documented route
can produce it at all, so the gate blocks work it cannot let through.

The work is therefore: delete the requirement from
`.agents/skills/verify-changes/SKILL.md`, delete the matching marker gate and
its game-build detection from
`.agents/skills/codex-review/scripts/New-CodexReviewPrompt.ps1`, update the
fixture coverage that asserts the gate fires, and reword any remaining doc text
that names the typed receipt as the required game-build evidence so it names the
successful build envelope and its reported receipt identity instead. The other
two markers the same gate enforces — `"operation":"validate"` for a
`Documents/Plans/**` change and `Validation: PASS` for a changed `SKILL.md` —
are unaffected and must keep working exactly as they do now.

Explicitly rejected alternative, rejected by explicit user decision: making
`Invoke-CompileBuild.ps1` surface the verbatim
`broken-engine-data-oracle-verifier-result/v1` JSON it already receives from
`Test-DataOracleReceipt.ps1` so a caller could quote it. That was this Plan's
original direction; the user rejected it as unnecessary ceremony. Do not
re-adopt it.

Verify every cited line number against the working tree before editing — the
line numbers below are from this session and the files may have moved since.

## Critical files

- `.agents/skills/verify-changes/SKILL.md` — the `## Required inputs` clause at
  `:29-31` requiring the passing typed result on a change set including a game
  build, and the game-build sentence in the build paragraph at `:100-102`
  ("current passing data-oracle verification — the typed receipt recording that
  the data files still match their expected contents")
- `.agents/skills/codex-review/scripts/New-CodexReviewPrompt.ps1` — the marker
  gate at `:451`, the game-build detection it is the only consumer of
  (`$gameBuildClaimed`, `:438-448`), and the `prompt.typed-artifacts-required`
  block message at `:457-458`, which must keep reporting the remaining markers
- `.agents/skills/codex-review/scripts/Test-CodexReviewPromptFixtures.ps1` — the
  missing-marker assertion at `:452`, the fixture comments and game-build
  envelope at `:439-441`, the tool-only-build case at `:458-468`, and the
  present-artifacts scope text at `:470`
- `.agents/skills/compile/scripts/Invoke-CompileBuild.ps1` and
  `.agents/skills/compile/references/runtime-data-mode.md` — read-only reference
  for what the build already reports; not edited

## In scope

- Deleting the `broken-engine-data-oracle-verifier-result/v1` requirement from
  `.agents/skills/verify-changes/SKILL.md`'s `## Required inputs`, and rewording
  its game-build evidence sentence to name the successful
  `broken-engine-build-result/v1` envelope plus the build-reported receipt
  identity line as the data evidence
- Deleting the corresponding marker gate in `New-CodexReviewPrompt.ps1`, plus
  the game-build detection block that exists only to feed it, keeping the
  remaining plan-validate and skill-validation markers and the baseline/head SHA
  checks byte-for-byte equivalent in behavior
- Updating `Test-CodexReviewPromptFixtures.ps1` so its cases match the new gate:
  drop the data-oracle assertion and the now-meaningless game-build/tool-build
  distinction, and keep coverage that the remaining markers still block and that
  a satisfied scope still writes the prompt with byte-identical scope text
- Any other doc text inside these two skills that names the typed receipt as a
  required handoff artifact

## Out of scope

- `.agents/skills/compile/scripts/Invoke-CompileBuild.ps1`,
  `Test-DataOracleReceipt.ps1`, `New-DataOracleReceipt.ps1`, and
  `references/runtime-data-mode.md`'s oracle mechanics — the build's internal
  fail-closed data verification stays exactly as it is; only the handoff and
  gate ceremony is removed
- `.agents/skills/agent-harness/SKILL.md`'s pre-launch receipt verification,
  which is a different contract on a different route
- The `broken-engine-build-result/v1` envelope shape, WorktreeCli, data-mode
  selection, generation authorization, breach transitions, and the oracle state
  file's contents
- The remaining `"operation":"validate"` and `Validation: PASS` markers, the
  baseline/head SHA checks, and every other `/codex-review` routing behavior
- The landed change the session produced
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants

Expected Tier 2 (scoped tool behavior of the review-tooling gate); escalate if
the fix reaches build/bootstrap coordination such as WorktreeCli or the shared
build lock. Preserve the build's fail-closed data-oracle behavior, the
single-JSON-line stdout contract (`compile/SKILL.md:82-86`), the verbatim
WorktreeCli envelope passthrough, the prohibition on hand-running the oracle
scripts as separate agent commands, the `prompt.typed-artifacts-required` exit
code `2` and `blocked` status for the markers that remain, and the byte-identical
scope-text copy into the prompt. Never embed transcript paths or home paths.

## Acceptance criteria

- `New-CodexReviewPrompt.ps1` writes the prompt and exits `0` for a
  `/verify-changes` scope whose build envelope names a `BrokenEngineSandbox`
  target and carries no `broken-engine-data-oracle-verifier-result/v1` text,
  with the plan-validate and skill-validation markers and both SHAs present
- A scope still missing `"operation":"validate"` or `Validation: PASS` still
  exits `2` with `prompt.typed-artifacts-required` naming exactly the missing
  markers
- `.agents/skills/verify-changes/SKILL.md` no longer requires the typed
  data-oracle artifact and documents the successful build envelope plus the
  build-reported receipt identity as the game-build data evidence
- `Test-CodexReviewPromptFixtures.ps1` passes with cases matching the new gate
- `/validate-skill` passes for both `verify-changes` and `codex-review`;
  `plan validate` exits 0
