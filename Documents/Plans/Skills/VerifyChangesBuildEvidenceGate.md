<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-20T22:07:10.481Z","dependsOn":[]} -->
# Fix: /verify-changes — missing build evidence blocks after a full review run, not at prompt assembly

## Context

At a landing gate, the manager assembled a `/verify-changes` reviewer prompt
with `.agents/skills/codex-review/scripts/New-CodexReviewPrompt.ps1` and a
`-ScopeFile` that omitted one evidence class it already held: the pre-rebase
`broken-engine-build-result/v1` build envelopes backing a build-based acceptance
criterion. Assembly accepted the scope, the headless review ran to completion,
and the reviewer returned BLOCKED solely because that evidence was absent — not
because anything in the change was wrong. Measured cost: about 186 seconds (a
roughly 151-second wasted headless run plus repair) and one extra review round,
after which the same review passed with the envelopes pasted in.

The script already blocks at assembly on the closely related cases:
`New-CodexReviewPrompt.ps1:431-432` blocks `/plan-audit` without an execution
card, and `:434-450` blocks `/verify-changes` when the scope omits the baseline
SHA, the head SHA, or, when the diff touches a `SKILL.md`, the `Validation:
PASS` line — the comment at `:423-428` states the intent as evidence "the
reviewer cannot recover from the diff", "without this check a scope that omits
it costs a whole review round". A build-evidence criterion is the same class of
prior-role evidence and has no such gate.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 16cd05b5-c7fa-490d-be9f-6e0ee2b68ca6
- Worktree/branch UUID: ba61731e-35c7-49ca-ad52-1b3499ca7172
- Session branch: claude/ba61731e-35c7-49ca-ad52-1b3499ca7172
- Worktree: .claude\worktrees\BrokenEngine\ba61731e-35c7-49ca-ad52-1b3499ca7172
- Observed in an earlier session: the fields above are the observing session's,
  not the session that recorded this Plan. That session landed as commit
  c088675f3adc40c52e4d8b9a0062019e285405d3.
- Landing ref: claude/2f71cdaf-8580-4d3e-984c-5f58b9e385ca — the recording
  session's branch, whose tree contains this Plan; the observing session's
  branch above was landed before this Plan existed.
  Fallback once the recorded ref is gone:
  `git log --diff-filter=A --format=%H -- Documents/Plans/Skills/VerifyChangesBuildEvidenceGate.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design

In a new session, run
`/next-plan-review c088675f3adc40c52e4d8b9a0062019e285405d3` — the commit on the
`Observed in an earlier session` line — supplying the recorded client `claude`
and the recorded conversation session ID
`16cd05b5-c7fa-490d-be9f-6e0ee2b68ca6`. Root-cause the friction from the proven
transcript, then make the smallest fix inside the `## In scope` boundary below.
If root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

Prefer the mechanism over the rule: extend the existing assembly-time gate in
`Test-PromptScopeEvidence` so a `/verify-changes` scope whose acceptance
criteria rely on build evidence blocks with the established
`prompt.typed-artifacts-required` outcome before the headless run starts,
falling back to tightening the `/verify-changes` required-inputs contract only
if root-causing shows the trigger cannot be detected at assembly.

## Critical files

- `.agents/skills/codex-review/scripts/New-CodexReviewPrompt.ps1` —
  `Test-PromptScopeEvidence` (lines 423-451), the existing typed-artifact gate.
- `.agents/skills/verify-changes/SKILL.md` — the required-inputs contract, the
  fallback location for the rule.
- `.agents/skills/codex-review/SKILL.md` — only if the blocking code the script
  returns needs its documented meaning extended.

## In scope

- Root-cause investigation via /next-plan-review, run with client `claude`, the
  review ref named in `## Design`, and the recorded conversation session ID.
- The smallest resulting fix, confined to the files named above — expected to be
  an added build-evidence condition inside `Test-PromptScopeEvidence`'s existing
  `/verify-changes` branch, reusing its `missing` list and its exit-2
  `prompt.typed-artifacts-required` outcome.

## Out of scope

- The landed change the observing session produced.
- The plan-audit execution-card gate, `Test-PromptReviewedTreeClean`, the diff
  and section assembly, and every other part of the prompt script.
- Any new script, new parameter, or new blocking code beyond the existing
  `prompt.typed-artifacts-required` outcome.
- Changing what `/verify-changes` reviewers decide, or `.codex/codex-review.ps1`.
- Unrelated skills/scripts; any transcript path or transcript text in the repo.

## Risk tier and invariants

Expected Tier 2 (scoped tool behavior); escalate if the fix reaches
build/bootstrap coordination. Never embed transcript paths or home paths. The
invariant that must survive: a scope carrying its required evidence still
assembles and reviews unchanged — the gate may not block a complete scope.

## Acceptance criteria

- The recorded symptom no longer reproduces: a `/verify-changes` scope whose
  acceptance criteria need build envelopes and omits them blocks at assembly
  with a nonzero exit and a message naming the missing evidence, before any
  headless review runs.
- A scope carrying that evidence assembles exactly as before.
- /validate-skill passes for any changed SKILL.md; plan validate exits 0.
