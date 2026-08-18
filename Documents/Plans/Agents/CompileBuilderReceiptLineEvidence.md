<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-17T22:03:05.601Z","dependsOn":[]} -->
# Fix: /compile builder report contract — the build-reported receipt summary line is required as evidence but is never reported verbatim and survives nowhere durable

## Context

`/verify-changes` requires a verbatim artifact that `/compile`'s builder report
contract never asks for and that nothing in the repository persists.

Producer side, proven from the current tree:

- `.agents/skills/compile/scripts/Invoke-CompileBuild.ps1:296` and `:319` compose
  the `"<mode> receipt issued: path=... sha256=... dataRoot=... baseline=...
  aggregate=..."` and `"<mode> receipt verified: ..."` lines into
  `$script:Summary`.
- `Write-CompileDiagnostic` (`:78-80`) writes every summary line to stderr only,
  and the summary is flushed there in `Complete-CompileBuild` (`:393`), in the
  post-build path (`:706`), and nowhere else.
- Stdout is not available for it: the comment at `:387` records that stdout is
  left unredirected so WorktreeCli's `broken-engine-build-result/v1` envelope is
  the caller's stdout byte-verbatim, so the wrapper cannot add the line to the
  envelope. The retained log is WorktreeCli's own MSBuild log and likewise never
  receives it.

Reporting side, also in the current tree: `.agents/skills/compile/SKILL.md`
`## Report results` requires, for game builds, "each selected oracle's exact
receipt path, SHA-256, Data path, mode, baseline, and aggregate digest ... from
the invocation's own stderr summary lines" — the *fields*, not the line as
written. `.claude/agents/builder.md` adds nothing of its own; its description
defers to this same contract.

Consumer side: `.agents/skills/verify-changes/SKILL.md` requires the verbatim
line in both its `## Required inputs` conditions and its build-evidence
paragraph.

The consequence is that a compliant builder handoff can satisfy `/compile`'s
contract in prose and still leave the manager unable to assemble a
`/verify-changes` scope file, because the only copy of the line was in a
transient stderr stream of a finished child process.

Observed consequence (evidence, not the root cause, which is proven above): in
an earlier landed session the landing-gate `/verify-changes` review blocked at
`20:19:21Z` solely on the missing line, although the builder handoff at
`20:06:06Z` had already reported every receipt field as prose exactly as the
contract asks. Recovery cost roughly 50,000 subagent tokens, about three minutes
of landing-gate time, and one extra Codex review round, and was possible only
because the builder child happened to still be resumable.

This session already fixed the consumer-documentation side (naming the line in
`/verify-changes` `## Required inputs`). This Plan is the evidence-production
side only. It is not an acceptance failure of any active change: `/compile`
behavior, the `broken-engine-build-result/v1` envelope shape, and the receipt
summary line's own text are named explicitly in the `## Out of scope` section of
the Plan the recording session had claimed
(`Documents/Plans/Agents/VerifyChangesBuildReceiptDispatchInputs.md`).

Verify every cited line number against the working tree before editing — the
numbers above may have moved since.

Session provenance (machine-local; not reproducible after cleanup). The root
cause above is proven from tracked source, so no transcript review is required;
these fields only identify where the observed consequence was recorded:
- Client: claude
- Conversation session ID: b8b4eb54-6e35-498b-ab11-b4e67d65ecc1
- Worktree/branch UUID: 895e715f-5bb7-4307-95f5-f89d330e185c
- Session branch: claude/895e715f-5bb7-4307-95f5-f89d330e185c
- Worktree: .claude\worktrees\BrokenEngine\895e715f-5bb7-4307-95f5-f89d330e185c
- Observed in an earlier session: the timings above belong to the observing
  session (client claude, conversation session ID
  d69b06f3-6e77-4a9d-95d1-ebca3ed6aa73), whose landed commit is
  `d7b04de3e664afbc539555a20698902b738bb14d`.
- Landing ref: the session branch above, whose tip is the recording session's
  final commit.

## Design

Close the contradiction on the producer side, with documentation only.

Amend the game-build bullet of `.agents/skills/compile/SKILL.md`
`## Report results` so that the receipt evidence a builder returns is the
build-reported `"<mode> receipt issued: ..."` and `"<mode> receipt verified:
..."` summary lines quoted verbatim, for every mode the invocation reported,
rather than the same values restated as prose fields. The existing requirement
to read the values from the invocation's own stderr summary lines and never
reconstruct them stays exactly as it is; only the reported form changes from
fields to the line as written. Keep the surrounding game-build requirements
(`DataBuildMode`, `RunDataPacker`, normalized paths, mode-selection triggers,
delta and Gaea outcomes, post-build verification results) unchanged.

`.claude/agents/builder.md` carries no independent statement of the receipt
fields — its description defers to this contract — so it is edited only if the
amended contract makes its one-line description inaccurate.

Deliberately rejected alternative: persisting the line into the retained log or
into the `broken-engine-build-result/v1` envelope. That envelope is WorktreeCli's
byte-verbatim stdout, so persisting the line means changing WorktreeCli or the
envelope schema — a serialization surface, and therefore Tier 3 — for a problem
a handoff-contract sentence solves completely. The verbatim line in every builder
handoff is the smallest complete fix, because the manager assembling the
`/verify-changes` scope file is exactly the party that reads the handoff.

## Critical files

- `.agents/skills/compile/SKILL.md` — the `## Report results` game-build bullet
  that lists the receipt fields (around `:160`)
- `.agents/skills/compile/scripts/Invoke-CompileBuild.ps1` — the summary-line
  composition (`:296`, `:319`) and stderr-only emission (`:78-80`, `:393`,
  `:706`), read to quote the exact line shape; never edited by this Plan
- `.claude/agents/builder.md` — the role description that defers to the contract,
  edited only if the amendment makes it inaccurate
- `.agents/skills/verify-changes/SKILL.md` — the consumer requirement, read for
  agreement; never edited by this Plan

## In scope

- The `## Report results` game-build receipt bullet in
  `.agents/skills/compile/SKILL.md`: require the verbatim build-reported receipt
  summary line(s) for every reported mode in every builder handoff
- The `.claude/agents/builder.md` description line, only where the amendment
  would otherwise leave it inaccurate

## Out of scope

- `Invoke-CompileBuild.ps1` and every other script: no change to how the receipt
  line is composed, emitted, or persisted
- The `broken-engine-build-result/v1` envelope shape, WorktreeCli, the retained
  log contents, and receipt issuance or verification semantics
- The receipt summary line's own text
- `.agents/skills/verify-changes/SKILL.md` and `.agents/skills/codex-review/SKILL.md`
- Every non-receipt part of the `## Report results` contract

## Risk tier and invariants

Expected Tier 2 (scoped tool behavior, documentation only). Escalate to Tier 3 if
the chosen fix reaches the envelope schema, WorktreeCli, or build/bootstrap
coordination — that is the boundary this Plan deliberately does not cross. The
evidence contract must stay fail-closed: the builder must still read every
receipt value from the invocation's own reported summary and never reconstruct or
infer it, and no change here may let a builder report receipt evidence it did not
observe.

## Acceptance criteria

- A builder following `.agents/skills/compile/SKILL.md` alone returns the
  verbatim receipt summary line(s) for a game build, so a manager can assemble a
  `/verify-changes` scope file from the handoff without resuming or re-running a
  builder
- The `/compile` reporting contract and the `/verify-changes` receipt requirement
  name the same artifact in the same form
- The never-reconstruct requirement remains stated for the receipt values
- `/validate-skill` passes for the changed `SKILL.md`; WorktreeCli
  `plan validate` exits `0` with `status: valid` and `code: ok`

## Notes

This Plan owns the producer half of the receipt-evidence gap: the reporting
contract asks for fields and the line survives nowhere durable. The consumer half
— naming the line among `/verify-changes` dispatch inputs — was fixed by the
recording session and is not repeated here.
