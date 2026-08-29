<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-29T15:28:48.210Z","dependsOn":[]} -->
# Fix: codex-review New-CodexReviewPrompt.ps1 — typed-artifact gate reads only the first progressive-disclosure-review handoff

## Context
Observed during a `/verify-changes` landing round in this session.
`.agents/skills/codex-review/scripts/New-CodexReviewPrompt.ps1`, function
`Test-PromptScopeEvidence` (around lines 471-505), locates the
`/progressive-disclosure-review` typed artifact with
`[Regex]::Match($script:ScopeText, '(?m)^Skill: progressive-disclosure-review')`
and then validates only that FIRST matched block's `Baseline:` and `Status:`
lines. When a landing session's scope file accumulates several
`/progressive-disclosure-review` handoffs across rebase rounds — the manager
appends each round's handoff so the evidence trail stays complete — the gate
reads the oldest, superseded block and blocks with `prompt.typed-artifacts-required`,
reporting: "a /progressive-disclosure-review handoff whose 'Baseline:' is the
dispatch baseline or a resolvable commit whose reviewed instruction docs do not
differ from it" — even though a later block in the same scope file satisfies
that rule.

Observed twice in this session (verify rounds 3 and 4). The workaround each time
was manual scope-file surgery: hand-editing the superseded blocks' `Skill:` and
`Baseline:` marker lines so they no longer match the regex.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: be3ace8f-a9bc-483d-ba78-023df37fb393
- Worktree/branch UUID: c5a8ebf1-a533-4cce-bc53-e04e3fb140ab
- Session branch: claude/c5a8ebf1-a533-4cce-bc53-e04e3fb140ab
- Worktree: .claude\worktrees\BrokenEngine\c5a8ebf1-a533-4cce-bc53-e04e3fb140ab
- Landing ref: claude/c5a8ebf1-a533-4cce-bc53-e04e3fb140ab
  Fallback once the recorded ref is gone:
  `git log --diff-filter=A --format=%H -- Documents/Plans/Agents/CodexReviewProgressiveDisclosureFirstBlockOnly.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
In a new session, run `/next-plan-review claude/c5a8ebf1-a533-4cce-bc53-e04e3fb140ab`
— the landing ref above — supplying the recorded client (`claude`) and the
recorded conversation session ID. Root-cause the friction from the proven
transcript, then make the smallest fix inside the `## In scope` boundary below.
If root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

The author's recommendation, to be confirmed or replaced by that root-cause
investigation, is one of two shapes:
- Match every `^Skill: progressive-disclosure-review` block in the scope text and
  accept the scope when any one block satisfies the existing `Files checked:`,
  `Status: PASS`, and baseline rule, keeping each block's checks bound to that
  same block so a PASS and a baseline from two different handoffs cannot combine.
- Or, if reading only one block is the intended contract, document in
  `.agents/skills/codex-review/SKILL.md` that the gate reads the first such block
  and that later rounds must replace rather than append their handoff.

The first shape matches the observed manager behavior (appending each round's
handoff keeps the evidence trail complete) and removes the manual scope-file
surgery; the second is a smaller change but leaves the manager responsible for
destroying superseded evidence.

## Critical files
- `.agents/skills/codex-review/scripts/New-CodexReviewPrompt.ps1` — function
  `Test-PromptScopeEvidence`, the `progressive-disclosure-review` marker match
  and the block's `Files checked:` / `Status: PASS` / `Baseline:` checks
- `.agents/skills/codex-review/SKILL.md` — the documented typed-artifact scope
  contract, if the fix is documentary

## In scope
- Root-cause investigation via /next-plan-review, run with the recorded client
  (`claude`), the landing ref named in `## Design`, and the recorded conversation
  session ID
- The smallest resulting fix, confined to the two files named above — in
  `New-CodexReviewPrompt.ps1` to `Test-PromptScopeEvidence`'s
  `progressive-disclosure-review` marker match and that block's validation, and
  in `SKILL.md` to the typed-artifact scope-evidence contract

## Out of scope
- The landed change the session produced
- The other typed-artifact gates in `Test-PromptScopeEvidence` (`Validation: PASS`,
  the build envelope, the identity values)
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior: one skill's landing-gate script and its
documented contract); escalate if the fix reaches build/bootstrap coordination.
The gate must keep binding a PASS verdict and a baseline to the same handoff
block, so evidence from two different handoffs can never combine into an
acceptance. Never embed transcript paths or home paths.

## Acceptance criteria
- A scope file carrying a superseded `/progressive-disclosure-review` handoff
  followed by a satisfying one passes the typed-artifact gate without any manual
  editing of the scope file
- A scope file whose only `/progressive-disclosure-review` handoffs all fail the
  baseline or `Status: PASS` rule still blocks with
  `prompt.typed-artifacts-required`
- /validate-skill passes for any changed SKILL.md; plan validate exits 0
