<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-16T16:14:32.475Z","dependsOn":[]} -->
# Fix: codex-review — the documented poll-until-complete wait cannot be followed on the Claude host

## Context

`.agents/skills/codex-review/SKILL.md:95-104` (step 3) documents waiting for a
Codex review run by re-invoking
`pwsh -NoProfile -File .codex/codex-review.ps1 -Poll <runId>` "until its `status`
is `completed` or `failed`". It documents no way to wait between those
re-invocations, and on the Claude host none of the available ways works as
documented:

- A standalone wait between polls is refused by the host. Observed verbatim:
  `Blocked: standalone Start-Sleep 45. To wait for a condition, use Monitor with
  an until-loop`.
- The host's suggested Monitor until-loop would run the bundled
  `.codex/codex-review.ps1` inside a loop construct, which the root `AGENTS.md`
  `## Directives` "Bundled scripts as documented" rule forbids: one script
  invocation per shell call, never wrapped.
- Back-to-back polls with no wait satisfy both rules but burn calls on a run that
  is still `running`.

Workaround used three times in the observing session: a filesystem Monitor
watching the launch receipt's `-OutFile` path until the file appears, then a
single confirming `-Poll <runId>` invocation. The three runs were launched from
prompts `Temp/CodeQualityMetricsCompareHostTimeout-plan-audit-prompt.md`,
`Temp/CompareTimeoutFix-coherence-prompt.md` with
`Temp/CompareTimeoutFix-scope-review-prompt.md`, and
`Temp/CompareTimeoutFix-validate-skill-prompt.md`. That workaround is documented
nowhere, and step 3 states only a `completed` status guarantees `<out>` is fully
written, so watching the file is not obviously safe on its face.

The friction is the contradiction itself: the skill's documented wait procedure
and the host plus root-rule constraints cannot both be honored, forcing an
undocumented workaround on every Codex review dispatch from Claude.

`.agents/skills/codex-review/SKILL.md` is outside the `## In scope` boundary of
the Plan claimed in the observing session
(`Documents/Plans/Agents/CodeQualityMetricsCompareHostTimeout.md`), so this is
not an in-scope blocker of that change.

Session provenance (machine-local; not reproducible after cleanup):
- Client: claude
- Conversation session ID: 191c7c85-d653-4f80-a68f-fffb605e8d5c
- Worktree/branch UUID: c12510c8-8f98-4bc5-9ec1-c35c31501885
- Session branch: claude/c12510c8-8f98-4bc5-9ec1-c35c31501885
- Worktree: .claude\worktrees\BrokenEngine\c12510c8-8f98-4bc5-9ec1-c35c31501885
- Landing ref: the session branch above, whose tip is the session's final
  commit and which survives exactly as long as the worktree recorded above.
  Fallback once that branch is gone:
  `git log --diff-filter=A --format=%H -- Documents/Plans/Agents/CodexReviewPollWaitPattern.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to this
  session alone (its diff limited to this session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes this worktree: Claude review
  requires the exact conversation session ID above.

## Design

In a new session, run `/next-plan-review <landing ref>` supplying the recorded
client and the recorded conversation session ID. Root-cause the friction from
the proven transcript, then make the smallest fix inside the `## In scope`
boundary below — expected to be either a sanctioned wait pattern written into
step 3 (for example, blessing the out-file watch followed by a single confirming
`-Poll`) or an explicit statement there that repeated `-Poll` re-invocation may
be driven by the host's wait mechanism, stating why that does not wrap the
script. If root-causing shows the fix lies outside that boundary — in the root
`AGENTS.md` bundled-script rule, for one — surface it for re-planning instead of
expanding scope.

## Critical files

- `.agents/skills/codex-review/SKILL.md` — step 3 under `## Method`, lines 95-104
- `.codex/codex-review.ps1` — the `-Poll` path, read for evidence; changed only
  if root-causing proves the fix belongs there

## In scope

- Root-cause investigation via `/next-plan-review`, run with the recorded landing
  ref, client `claude`, and the recorded conversation session ID
- The smallest resulting fix, confined to the files named above, and within
  `.agents/skills/codex-review/SKILL.md` to step 3 of `## Method`

## Out of scope

- The landed change the observing session produced
- The `## Fallback` `CODEX-UNAVAILABLE` contract and the manager-evaluation rules
- Unrelated skills and scripts; any transcript path or transcript text in the
  repository

## Risk tier and invariants

Expected Tier 2 (scoped tool behavior in one skill); escalate if the fix reaches
build or bootstrap coordination, or the root `AGENTS.md` bundled-script rule.
Preserve the existing step-3 guarantees: launch and poll stay separate calls, no
call waits for Codex, and only a `completed` status is treated as proof that
`<out>` is fully written. Never embed transcript paths or home-directory paths.

## Acceptance criteria

- The recorded symptom no longer reproduces: a Claude manager can wait for a
  Codex review run by following `.agents/skills/codex-review/SKILL.md` step 3
  alone, without a host block, without wrapping a bundled script, and without
  back-to-back no-wait polls
- `/validate-skill` passes for `codex-review`; `WorktreeCli plan validate` exits
  `0` with `status: valid`
