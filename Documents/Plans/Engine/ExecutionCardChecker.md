<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-03T22:43:04.397Z","dependsOn":[]} -->
# Validate execution-card fields with a script instead of the substring gate

## Context
The only enforcement of the execution card today is a substring test.
`.agents/skills/codex-review/scripts/New-CodexReviewPrompt.ps1:336-345`
(`Test-PromptScopeEvidence`, called at `:470`, reading the scope text loaded at
`:431`) blocks a `/plan-audit` dispatch with `prompt.execution-card-required`
only when the scope text does not contain the literal words "execution card",
any casing. A card that carries those words but omits a field — `Roles`, for
one — passes the gate, so the omission is found by the reviewer instead, which
costs a full review round returning `BLOCKED`.

Two hand steps read the same document for the same reason.
`.agents/skills/prepare-change/references/worker.md:30-32` has the author
re-read the drafted file and confirm the two scope headings, the card heading,
and each filled card field. `.agents/skills/plan-audit/references/worker.md:11`
and `:14` run `Test-PlanCitations.ps1` from
`.agents/skills/plan-audit/scripts/`, which already emits
`headings.inScopePresent` and `headings.outOfScopePresent`, but
`/prepare-change` never runs it, and after this Plan two skills would own it.

This Plan is deferred on purpose. It was scoped inside
`Documents/Plans/Engine/ExecutionCardFile.md`, whose prose-deletion half the
user approved and completed; the user declined the script half for now. No
incomplete execution card has actually been observed costing a review round:
`git log -S"execution-card-required" --oneline` returns only plan-authoring
commits (`b047dc9a`, `1073acc1`, `38b46f0a`, `c5df654a`, `3ea3da65`,
`cd07f0b9`), never a session that hit the gate. With `dependsOn` empty this
Plan is claimable in the normal `(createdUtc, path)` order. A session that
claims it should confirm with the user that the deferral condition — an
incomplete card has cost a review round, or a dependent Plan needs the
machine-readable card — now holds before implementing.

Two live Plans already assume this checker exists:
`Documents/Plans/Engine/InventoryVcxprojAndAcceptanceTriggers.md:42` ("the
execution card that this Plan's prerequisite makes machine-readable"), whose
`dependsOn` edge now names this Plan, and
`Documents/Plans/Engine/DirectReviewerBriefAssembly.md:67-68`, which excludes
`Test-PromptScopeEvidence` as owned by `ExecutionCardFile.md`; that ownership
now rests on this Plan.

## Design
The design below is carried over verbatim from `ExecutionCardFile.md` plus the
refinements its plan audit accepted, and is decision-complete as written.

1. Add `.agents/scripts/Test-ExecutionCard.ps1`, taking the scope or plan file
   path and judging field presence only inside the card region — from the line
   carrying the words `execution card`, any casing, to end-of-file or the next
   heading of the same or higher level. It emits one
   `broken-engine-execution-card/v1` JSON object with `status`, `code`,
   `message`, a `cardPresent` row, and one presence row per field:
   `whatDoesThisPlanDo`, `whyGoodForCodebase`, `goal`, `outOfScope`,
   `tierTrigger`, `interfacesAndInvariants`, `acceptanceChecks`, `roles`.
   Surface forms: the two `###` headings need a non-empty body below them; the
   six `- <Label>:` bullets need non-empty text after the colon. Mirror the
   envelope shape of `.agents/scripts/Test-PlanSchedulerState.ps1:12-32`.
2. Have `Test-PromptScopeEvidence` run that checker as a child `pwsh` process,
   following the `Get-SessionChangeInventory.ps1` precedent already in the same
   script at `:269-321`. Derive the existing `prompt.execution-card-required`
   block from `cardPresent`, and block at exit 2 with a new code
   `prompt.execution-card-incomplete` naming the missing fields. The card must
   be inline in `-ScopeFile`; state that in one clarifying sentence in
   `.agents/skills/codex-review/references/worker.md:44-50`. Add the new code's
   row to `.agents/skills/codex-review/references/receipts.md:15-27`.
3. Move `.agents/skills/plan-audit/scripts/Test-PlanCitations.ps1` to
   `.agents/scripts/`, changing its `:13` `$PSScriptRoot` root derivation from
   four levels up to two (`:64`, `:71`, `:257`, and `:279` all resolve through
   it), and update `.agents/skills/plan-audit/references/worker.md:11` and
   `:14`. Replace `prepare-change/references/worker.md:30-32` with the two
   script runs.

## Critical files
- `.agents/scripts/Test-ExecutionCard.ps1` (new)
- `.agents/skills/plan-audit/scripts/Test-PlanCitations.ps1` (moved to
  `.agents/scripts/`)
- `.agents/skills/codex-review/scripts/New-CodexReviewPrompt.ps1`
- `.agents/skills/codex-review/references/worker.md`,
  `.agents/skills/codex-review/references/receipts.md`
- `.agents/skills/plan-audit/references/worker.md`
- `.agents/skills/prepare-change/references/worker.md`
- `.agents/scripts/Test-PlanSchedulerState.ps1` (read-only envelope precedent)

## In scope
- The new `Test-ExecutionCard.ps1`, its card-region rule, its emitted field
  names, and its `broken-engine-execution-card/v1` envelope
- `Test-PromptScopeEvidence` in `New-CodexReviewPrompt.ps1:336-345` and its
  child-process run of the checker
- The new `prompt.execution-card-incomplete` code and its receipts row at
  `.agents/skills/codex-review/references/receipts.md:15-27`
- The one clarifying inline-card sentence in
  `.agents/skills/codex-review/references/worker.md:44-50`
- The `Test-PlanCitations.ps1` move, its `:13` root derivation, and every path
  reference to it in `plan-audit/references/worker.md:11` and `:14`
- `prepare-change/references/worker.md:30-32`

## Out of scope
- Whether the goal is the right goal, whether the boundary is enforceable, and
  whether an acceptance check is decisive — all agent judgment, unchanged
- The `/plan-audit` review itself and the citation records
  `Test-PlanCitations.ps1` already emits (`:292`)
- The execution-card field list's owning locations (root `AGENTS.md` term
  definition and `.agents/skills/next-plan/SKILL.md`), whose prose
  `ExecutionCardFile.md` already settled
- Acceptance-table rows built from approved criteria, owned by
  `Documents/Plans/Engine/InventoryVcxprojAndAcceptanceTriggers.md`
- The `-ScopeFile` judgment text, `-RiskTier`, `/codex-review` routing and
  fallbacks, and the Codex prompt template wording
- `New-PlanFile.ps1` and Plan scaffolding, which are already complete
- Declined by the audit that produced the source Plan: a plan snapshot hash in
  `Test-PlanCitations.ps1` so parallel `/plan-audit` and
  `/plan-simplicity-review` provably read one snapshot — no divergence has been
  observed, and the load-once rule cannot leave the worker anyway

## Risk tier and invariants
Expected Tier 2 (scoped behavior of the review-dispatch tooling and the skill
prose that feeds it — no determinism/CRC, wire, serialization, threading, or
trust-boundary surface); this author's classification, to be confirmed by main
at Step 1. Invariants to preserve:
- `broken-engine-plan-citations/v1` keeps its schema name and every field across
  the move, so `/plan-audit` reads an unchanged result.
- The `broken-engine-codex-review-prompt/v1` receipt keeps every field
  (`New-CodexReviewPrompt.ps1:55-68`) and its 0/1/2 exit mapping.
- A complete card that dispatches today must still dispatch.
- The bundled-script rule's canonical
  `pwsh -NoProfile -File <repo-relative path>` invocation from the worktree root
  applies to both scripts.

## Acceptance criteria
- A card missing only `Roles` gets exit 2 `prompt.execution-card-incomplete`
  naming that field, with no review dispatched
- A scope file with no card at all still gets `prompt.execution-card-required`
- A complete card assembles the prompt exactly as today, with the receipt's full
  key set compared and unchanged; the prompt-assembly acceptance commands use the
  full 40-character baseline SHA and
  `-UntrackedPath .agents/scripts/Test-ExecutionCard.ps1`
- `Test-PlanCitations.ps1` at its new path returns byte-identical JSON for the
  same plan file as the old copy does, both run back-to-back on one tree state
- A repository search for the old script path, run before
  `Complete-NextPlan.ps1` deletes this Plan, returns no hits outside Git history
  and this Plan's own file `Documents/Plans/Engine/ExecutionCardChecker.md`
- `Validate-Skill.ps1` reports `VALID` for every changed skill package

## Notes
Origin: deferred by explicit user choice from
`Documents/Plans/Engine/ExecutionCardFile.md` during a Claude `/next-plan` run
in worktree/branch UUID `962f5a29-43da-4719-b31a-3ad80b588e5f`, branch
`claude/962f5a29-43da-4719-b31a-3ad80b588e5f`; that Plan was completed with the
prose-deletion half only; conversation session ID
`4330820e-7cd1-4169-8cc2-3e773d73034d`.
