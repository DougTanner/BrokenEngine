<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-03T23:21:39.102Z","dependsOn":[]} -->
# Fix: /resolve-findings — its handoff contract invites the verbatim quoting the shared handoff rule forbids

## Context
The `/resolve-findings` handoff contract and the shared handoff rule disagree
about quoting rewritten text, and fix workers follow the skill.

- `.agents/skills/resolve-findings/SKILL.md:76-79` allows "Per-finding
  application detail" to stay inline until it "would exceed the size limits" in
  the shared reference, so the only stated brake is a size cap.
- `.agents/references/subagent-reporting.md:95-96` states the rule for every
  handoff row: "Every row is one line. Do not quote code and do not repeat a row
  from another field."

Observed symptom in this session's `/next-plan` run: one `/resolve-findings`
handoff of roughly 5,400 characters pasted the rewritten Markdown table row and
the rewritten bullet back into the main session verbatim, as multi-line
`Resulting line:` and `Resulting bullet:` quotes; a second `/resolve-findings`
handoff quoted a full rewritten table row the same way. Each was under the
20,000-character cap, so nothing in the skill's stated brake fired, and main
paid for content it had to re-read from the file anyway. No rework was forced;
the cost was oversized content in the main context on every fix round.

The skill's own item table already has a `Fixed region` column
(`.agents/skills/resolve-findings/SKILL.md:54-56`) that carries the same
information as a path plus location, which is why quoting the resulting text
adds bytes without adding decision value.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 23de3f7f-74a1-4fd3-ab32-c2332e7043a8
- Worktree/branch UUID: 6645b854-41ec-4677-bd6b-1674b4ff8a2e
- Session branch: claude/6645b854-41ec-4677-bd6b-1674b4ff8a2e
- Worktree: .claude\worktrees\BrokenEngine\6645b854-41ec-4677-bd6b-1674b4ff8a2e
- Landing ref: claude/6645b854-41ec-4677-bd6b-1674b4ff8a2e — the recording
  session is the observing session, so its own session branch tip carries this
  Plan, and it survives exactly as long as the worktree recorded above.
  Fallback once the recorded ref is gone:
  `git log --diff-filter=A --format=%H -- Documents/Plans/Engine/ResolveFindingsHandoffQuoting.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review claude/6645b854-41ec-4677-bd6b-1674b4ff8a2e` in bounded
friction mode, supplying the recorded client `claude` and the recorded
conversation session ID above. Then make the smallest fix inside the `## In
scope` boundary below. If root-causing shows the fix lies outside that boundary,
surface it for re-planning instead of expanding scope.

The author's recommendation, offered as a starting point rather than a binding
decision: bound the `## Handoff` section of
`.agents/skills/resolve-findings/SKILL.md` so a per-finding row cites the
`Fixed region` as `path:line` only, and any resulting text is cited by path plus
a selector rather than quoted, matching
`.agents/references/subagent-reporting.md` `## Handoffs`. The rationale is that
the shared rule already forbids quoting and main can open the cited file, so the
skill only needs to stop inviting the quote; a size-cap-only brake cannot, since
the observed handoffs were well under the cap. An alternative the fix session
may prefer is to have the skill point at the shared rule for quoting and keep
only its size-overflow route, if that reads as less duplicated prose.

## Critical files
- `.agents/skills/resolve-findings/SKILL.md`
- `.agents/skills/resolve-findings/references/worker.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the `## Handoff` section of
  `.agents/skills/resolve-findings/SKILL.md` and any matching handoff
  instruction in `.agents/skills/resolve-findings/references/worker.md`

## Out of scope
- The landed change the observing session produced
- `.agents/references/subagent-reporting.md` and the shared handoff rule itself
- Other skills' handoff sections; any transcript path or transcript text in the
  repo

## Risk tier and invariants
Expected Tier 1 (documentation/instruction prose in one skill package, no
behavior or public surface exposure); escalate if the fix reaches the shared
handoff reference or another skill's contract. Never embed transcript paths or
home paths.

## Acceptance criteria
- The `/resolve-findings` handoff contract no longer permits quoting rewritten
  or resulting text inline, and directs the per-finding row to cite the fixed
  region as a path plus location
- `/validate-skill` passes for the changed `SKILL.md`; plan validate exits 0
