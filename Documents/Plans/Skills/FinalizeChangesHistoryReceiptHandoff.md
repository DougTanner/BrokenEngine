<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-22T12:17:47.757Z","dependsOn":[]} -->
# Fix: /finalize-changes — phase-1 handoff truncates the typed history Contract receipt

## Context

The false required condition was that a successful approval-preparation result
could be summarized in a delegated handoff while still preserving enough of its
history receipt for verification. During the `/next-plan` second
tooling-friction checkpoint, phase 1 ran
`pwsh -NoProfile -File .agents/skills/finalize-changes/scripts/Invoke-FinalizeApprovalPreparation.ps1`
with the documented phase-1 arguments
(`.agents/skills/finalize-changes/references/scripts.md:20`). It returned
`broken-engine-finalize-approval-preparation/v3`, `status: pass`, and candidate
`e52c28125637917585ba4c3175e9add44bcdac15`.

The nested `historyContract.receipt` was not delivered intact by the delegated
finalizer handoff. The first handoff abbreviated it, said the remaining manifest
was elsewhere, and included malformed or truncated digest text. A same-worker
artifact-recovery follow-up again returned JSON containing ellipses and malformed
hashes. Because `.agents/skills/finalize-changes/SKILL.md:92-104` requires every
typed receipt to reach `/verify-changes` verbatim, and
`.agents/skills/verify-changes/SKILL.md:35-36,120-123` requires the complete
preverification Contract receipt, the manager had to extract and parse the valid
`historyContract.receipt` from the already-produced approval-preparation stdout
before verification could run. This caused two malformed handoff attempts and
manager-side recovery work.

The producer itself is not implicated: its current path validates one Contract
JSON receipt before embedding it in the approval result
(`.agents/skills/finalize-changes/scripts/Invoke-FinalizeApprovalPreparation.ps1:180-195,441-443`),
and the observed command returned a passing result. The reporting guidance says
that large evidence should use an existing file or log plus a narrow selector
(`.agents/references/subagent-reporting.md:50-78`), but the handoff still failed
to preserve a usable complete receipt. The root cause is intentionally deferred
to the `/next-plan-review` session named below.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: codex
- Conversation session ID: none
- Worktree/branch UUID: 0eb03f7b-926f-4b63-b52d-133df13b467a
- Session branch: codex/0eb03f7b-926f-4b63-b52d-133df13b467a
- Worktree: .codex\worktrees\BrokenEngine\0eb03f7b-926f-4b63-b52d-133df13b467a
- Landing ref: codex/0eb03f7b-926f-4b63-b52d-133df13b467a
- Run the review before `/cleanup-worktrees` removes the worktree recorded above.

## Design

In a new session, run `/next-plan-review codex/0eb03f7b-926f-4b63-b52d-133df13b467a`,
supplying client `codex` and that review ref only. Root-cause the repeated
handoff failure from the proven approval-preparation result and the delegated
reporting path, then make the smallest fix inside the `## In scope` boundary
below. The author's recommendation is to preserve the complete typed receipt
through the finalizer handoff and resolve any file-or-selector evidence before
the verification handoff, because `/verify-changes` must validate the receipt
itself. Do not decide the mechanism until the review establishes where the
truncation occurs. If root-causing shows that the fix lies in the producer
script, the verification consumer, or another boundary, surface it for
re-planning instead of expanding this Plan.

## Critical files

- `.agents/skills/finalize-changes/SKILL.md` — `## Inputs and ownership` and
  `## Normal workflow` steps 2-3, which define the approval-preparation result
  and the typed-receipt handoff.
- `.agents/references/subagent-reporting.md` — `## Handoffs`, especially the
  typed-result and large-evidence return rules at lines 50-78.

## In scope

- Root-cause investigation via `/next-plan-review`, run with client `codex` and
  review ref `codex/0eb03f7b-926f-4b63-b52d-133df13b467a`.
- The smallest resulting fix, confined to the named sections of
  `.agents/skills/finalize-changes/SKILL.md` and
  `.agents/references/subagent-reporting.md`, so a successful approval result's
  complete `historyContract.receipt` reaches `/verify-changes` without
  abbreviated, ellipsis, or malformed JSON/hash content.

## Out of scope

- The landed ClientSession rename and the completed/deleted active Plan.
- `.agents/skills/finalize-changes/scripts/Invoke-FinalizeApprovalPreparation.ps1`;
  current evidence shows it produced and validated the successful result, not
  the malformed handoff.
- `.agents/skills/verify-changes/SKILL.md`, receipt schema or digest contents,
  the code-quality producer, finalizer scripts, landing transaction, history
  overlay, primary advancement, and unrelated skills or scripts.
- Any transcript path or transcript text in the repository.

## Risk tier and invariants

Expected Tier 2 (scoped tool behavior); escalate if the fix reaches
build/bootstrap coordination or the landing transaction. No determinism/CRC,
serialization/`.pack`, replay, wire, threading, allocation, shader, build, or
live-verification behavior is intended to change. The complete
`broken-engine-code-quality-history-contract/v1` receipt, its digests, and the
`broken-engine-finalize-approval-preparation/v3` producer result remain
unchanged; only their delegated handoff must become usable. Never embed
transcript paths or home paths.

## Acceptance criteria

- Under the documented approval-preparation invocation, a passing phase-1 result
  reaches the manager and `/verify-changes` with the complete typed
  `historyContract.receipt` verbatim, or with an existing evidence file and
  narrow selector that is resolved to that same complete receipt before the
  verification handoff; no abbreviation, ellipsis, or malformed digest is
  accepted as the receipt.
- The repeated artifact-recovery path no longer returns a second malformed or
  truncated receipt, and manager-side extraction from the original command
  stdout is unnecessary.
- The producer's successful JSON, receipt bytes, and digest values remain
  unchanged.
- `/validate-skill` passes for any changed `SKILL.md`,
  `/progressive-disclosure-review` passes for changed instruction prose, and
  `plan validate` exits 0.

## Notes

Root cause is deferred to `/next-plan-review`; this Plan records the observed
symptom and workaround only. Duplicate search found
`Documents/Plans/Skills/CodeQualityHistoryContractSandboxReproduction.md`, but
that Plan owns a different sandbox producer failure (missing `-RepositoryRoot`
and a boolean `True` diagnostic), not a successful finalizer handoff that
truncates a typed receipt. Related residuals: none.
