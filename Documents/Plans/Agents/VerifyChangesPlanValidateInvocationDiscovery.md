<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-16T19:58:05.743Z","dependsOn":[]} -->
# Fix: verify-changes / codex-review plan-validate evidence — the landing-gate invocation took three attempts to discover

## Context

At a landing gate the manager assembled the `/verify-changes` dispatch, whose
prompt assembly refuses to build a prompt when the reviewed diff touches
`Documents/Plans/**` and the scope file carries no `"operation":"validate"`
artifact (`.agents/skills/codex-review/scripts/New-CodexReviewPrompt.ps1:439`,
blocked code `prompt.typed-artifacts-required` documented at
`.agents/skills/codex-review/SKILL.md:90-92`). Producing that artifact took
three separate `WorktreeCli.exe plan validate` invocations:

1. the first returned `invalid-context`
   (`Tools/WorktreeCli/PlanScheduler.cpp:495-501`, which fails context
   resolution with no usage text and no hint about the missing switch);
2. the second returned a `usage` error; and
3. only the third, adding `--lint-only`, returned a valid result.

The three attempts plus the search needed to find the switch cost roughly
thirty seconds of landing-gate time and forced the manager to hunt for the
working form instead of reading it at the point of use.

Both surfaces that already carry the correct form were present in the tree at
the time of the observation and neither was reached from the entry point the
manager was actually working from: `.agents/skills/verify-changes/SKILL.md`
`## Executable Plan check` (`:109-123`) spells out
`plan validate --lint-only --repo <absolute Git common directory> --worktree
<adopted checkout>`, and `Tools/WorktreeCli/WorktreeCli.cpp:18` lists
`[--lint-only]` in its usage block. Whether the real gap is the codex-review
prompt-assembly entry point naming the required artifact without naming the
invocation that produces it, the `invalid-context` failure carrying no usage
hint, or something else, is left to the review below rather than assumed here.

Session provenance (machine-local; not reproducible after cleanup):

- Client: claude
- Conversation session ID: `ccaa8c39-19a7-4b6c-b2b6-3b6fb1bb0de1`
- Worktree/branch UUID: `ec7d0a29-fe8c-4d31-b355-17406342ca2e`
- Session branch: `claude/ec7d0a29-fe8c-4d31-b355-17406342ca2e`
- Worktree: `.claude\worktrees\BrokenEngine\ec7d0a29-fe8c-4d31-b355-17406342ca2e`
- Landing ref: the session branch above, whose tip is the session's final
  commit and which survives exactly as long as the worktree recorded above.
  Fallback once that branch is gone:
  `git log --diff-filter=A --format=%H -- Documents/Plans/Agents/VerifyChangesPlanValidateInvocationDiscovery.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to this
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes this worktree: Codex
  transcript discovery requires the producing worktree to remain registered,
  and Claude review requires the exact conversation session ID above.

## Design

In a new session, run `/next-plan-review <landing ref>` supplying the recorded
client and the recorded conversation session ID. Root-cause the friction from
the proven transcript, then make the smallest fix inside the `## In scope`
boundary below. If root-causing shows the fix lies outside that boundary — in
the WorktreeCli scheduler's own failure or usage text, for one — surface it for
re-planning instead of expanding scope.

## Critical files

- `.agents/skills/codex-review/SKILL.md` — step 1's required-evidence sentence
  for a `verify-changes` dispatch and the `prompt.typed-artifacts-required`
  blocked-code description (`:52-92`).
- `.agents/skills/verify-changes/SKILL.md` — `## Executable Plan check`, which
  already carries the `--lint-only` invocation (`:109-123`).

## In scope

- Root-cause investigation via /next-plan-review, run with the recorded landing
  ref, client, and conversation session ID.
- The smallest resulting fix, confined to the two files named above: the
  codex-review required-evidence and blocked-code wording that a manager reads
  while assembling a `verify-changes` dispatch, and the verify-changes
  `## Executable Plan check` invocation text it points at.

## Out of scope

- The landed change the reviewed session produced.
- `Tools/WorktreeCli/**` behavior, failure codes, and usage text; the scheduler
  guard, claim, and landing-lock surfaces.
- Unrelated skills and scripts; any transcript path or transcript text in the
  repository.

## Risk tier and invariants

Expected Tier 2 (scoped tool behavior); escalate if the fix reaches
build/bootstrap coordination or WorktreeCli source. The documented landing-gate
validation must stay the read-only `--lint-only` form that takes no scheduler
guard, creates no storage, and heals nothing, so the read-only review sandbox
can still run it. Never embed transcript paths or home paths.

## Acceptance criteria

- A manager following the codex-review dispatch instructions for a
  `verify-changes` review of a `Documents/Plans/**` change reaches the exact
  working `plan validate` invocation without trial and error; the recorded
  three-attempt symptom no longer reproduces.
- /validate-skill passes for any changed SKILL.md; plan validate exits 0.

## Coordination

`Documents/Plans/Agents/CodexReviewVerifyChangesPlanValidateArtifact.md` is keyed
to a different symptom of the same gate — the prompt assembly blocks on a
host-run plan-validate receipt that no `## Required inputs` condition names — and
it edits the same two skill files (`codex-review/SKILL.md` required-evidence and
blocked-code wording, `verify-changes/SKILL.md` `## Executable Plan check`). The
two are not directional: either may land first, and whichever lands second
re-reads those sections as they then stand and keeps the other's text intact.
This Plan owns making the required invocation discoverable at the point of use;
that Plan owns whether the host receipt is required at all and where that
requirement is stated. If that Plan's review drops the gate condition entirely,
this Plan's remaining work may shrink to nothing — surface that for re-planning
rather than deleting it from within that Plan.
