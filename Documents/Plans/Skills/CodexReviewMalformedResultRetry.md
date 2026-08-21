<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-20T17:49:23.080Z","dependsOn":[]} -->
# Fix: /codex-review — a malformed `completed` result blocks on the user instead of one retry

## Context
During a Tier-3 preparation dispatch of `plan-simplicity-review`, the run
`pwsh -NoProfile -File .codex/codex-review.ps1` returned a `completed` receipt
(runId `81a9ffeba6d64d67a14e79ec25a60564`, exit `0`), but the findings it
published were the model's internal drafting notes rather than a review. The
sanitized output began:

```text
Plan snapshot: `cdc9070?` Wait wrong. Must exact cdcadb26... Ensure.
```

and ended:

```text
Ensure no accidental wrong hash. final only.
```

It carried no skill-native status line and no verdict token, which
`.agents/skills/codex-review/SKILL.md:121-122` already names as the structured
shape a success must have. The wrapper's own success criterion is CLI exit `0`
alone, so a leaked-draft final message still publishes as `completed` and every
caller has to detect the malformity itself.

Having detected it, the caller is then stuck: `## Fallback`
(`.agents/skills/codex-review/SKILL.md:153-166`) classifies malformed output as
genuine failure, requires reporting `CODEX-UNAVAILABLE`, and states "Do not
retry Codex". That forced a stop and a user-authorization round-trip mid
preparation. The user authorized an identical immediate retry in the same
session; it succeeded and returned a well-formed review, which shows this
failure mode is transient model output, not Codex being unavailable.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 04f068be-6ddc-4469-b861-1a3eb99d0e66
- Worktree/branch UUID: 8361cc63-a609-4d97-9163-f445b6215d99
- Session branch: claude/8361cc63-a609-4d97-9163-f445b6215d99
- Worktree: .claude\worktrees\BrokenEngine\8361cc63-a609-4d97-9163-f445b6215d99
- Landing ref: claude/8361cc63-a609-4d97-9163-f445b6215d99
  Fallback once the recorded ref is gone:
  `git log --diff-filter=A --format=%H -- Documents/Plans/Skills/CodexReviewMalformedResultRetry.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
In a new session, run `/next-plan-review claude/8361cc63-a609-4d97-9163-f445b6215d99`
— the landing ref above — supplying the recorded client `claude` and the
recorded conversation session ID. Root-cause the friction from the proven
transcript, then make the smallest fix inside the `## In scope` boundary below.
If root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

The mechanism this Plan authorizes is the wording-only one: amend `## Fallback`
in `.agents/skills/codex-review/SKILL.md` so that a `completed` result failing
the `SKILL.md:121-122` structured-shape check permits exactly one re-dispatch of
the identical prompt before it counts as genuine failure, with a second
malformed result reported as `CODEX-UNAVAILABLE` under the existing blocking
rules. Every other genuine-failure cause — `failed` wait status, non-zero script
exit — keeps its current no-retry behavior. Adding shape validation or retry
logic to `.codex/codex-review.ps1` is deliberately not this Plan's mechanism:
the caller-side check the skill already documents makes the script change the
larger of the two options for the same outcome.

## Critical files
- `.agents/skills/codex-review/SKILL.md` — the authorized fix boundary,
  `## Fallback` section, with the `## Method` malformity wording at lines
  118-123 available to reference but changed only if the retry rule cannot be
  stated in `## Fallback` alone.

## In scope
- Root-cause investigation via /next-plan-review, run with client `claude`, the
  landing ref named in `## Design`, and the recorded conversation session ID.
- The smallest resulting fix, confined to `.agents/skills/codex-review/SKILL.md`
  `## Fallback` (and, only if that section cannot carry the rule by itself, the
  malformity wording in `## Method`).

## Out of scope
- The landed change the session produced.
- `.codex/codex-review.ps1` — no shape validation, retry loop, exit-code, or
  receipt change; the script stays byte-unchanged.
- The Opus/`general-purpose` authorization path, the `## Manager evaluation`
  rules, and any change to how findings are decided.
- More than one retry, retrying any cause other than a malformed `completed`
  result, or changing the prompt on the retry.
- Unrelated skills/scripts; any transcript path or transcript text in the repo.

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior); escalate if the fix reaches
build/bootstrap coordination. Never embed transcript paths or home paths. The
invariant that must survive: a genuinely unavailable Codex still blocks on
explicit user authorization and never auto-substitutes a reviewer.

## Acceptance criteria
- The recorded symptom no longer reproduces under the documented invocation: a
  malformed `completed` result triggers exactly one identical re-dispatch, and
  only a second malformed result reports `CODEX-UNAVAILABLE`.
- `failed` status and non-zero exit still report `CODEX-UNAVAILABLE` with no
  retry.
- /validate-skill passes for `.agents/skills/codex-review/SKILL.md`; plan
  validate exits 0.
