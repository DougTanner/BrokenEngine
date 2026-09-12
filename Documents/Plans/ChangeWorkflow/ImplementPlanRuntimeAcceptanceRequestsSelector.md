<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-12T16:24:58.156Z","dependsOn":[]} -->
# Fix: implement-plan — `Runtime acceptance requests` invites per-criterion specs the cited plan file already holds

## Context
`.agents/skills/implement-plan/SKILL.md` `## Handoff` (lines 60-61) declares
`Runtime acceptance requests` as "setup, action, observation, and required
evidence per criterion; or none", with no alternative to writing those four
parts out for every criterion. The field's own text therefore invites the bulk
inline form even when the same handoff's shared `Evidence` row already cites the
claimed plan snapshot's `## Acceptance criteria` selector, where those criteria
already live in the shape a runtime verifier reads.

Observed symptom in this session: the implementation handoff returned by
`/implement-plan` carried an inline per-criterion `Runtime acceptance requests`
block of roughly 3 KB inside a 10.6 KB handoff, while that same handoff's
`Evidence` row cited the plan snapshot plus its `## Acceptance criteria`
selector — so main paid for the criteria twice in its own context, once as the
citation and once as the restatement, for a field the shared form intends to
stay one row per item.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: e99c34ab-c9ee-4261-a00a-9f9db0dfccc2
- Worktree/branch UUID: e5b6e0e4-e0c9-4bf8-ad8e-e9d0fa455e78
- Session branch: claude/e5b6e0e4-e0c9-4bf8-ad8e-e9d0fa455e78
- Worktree: .claude\worktrees\BrokenEngine\e5b6e0e4-e0c9-4bf8-ad8e-e9d0fa455e78
- Landing ref: the session branch above. The recording session had not landed its
  change when this Plan was written, so the branch tip named here contains this
  Plan only once that session's work is committed to it; until then the Plan
  exists as tracked content in the worktree above.
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- <this plan path>`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to one session alone
  (its diff limited to that session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`;
the cited lines should be sufficient, so the transcript is not expected to be
needed. Only when it genuinely is, in a new session run
`/next-plan-review <landing ref above>` in bounded friction mode, supplying
client `claude` and the recorded conversation session ID. Then make the smallest
fix inside the `## In scope` boundary below. If root-causing shows the fix lies
outside that boundary, surface it for re-planning instead of expanding scope.

The author's recommendation is a documentation-only fix to that field's
declaration: state that when the criteria already live in a plan file the
handoff cites, the field is a path plus `##` selector into that file — naming any
setup, action, observation, or evidence the plan does not already state — and
that the per-criterion four-part form is for criteria with no such cited home.
Whether the field keeps one row per criterion or collapses to a single citation
row when the whole set is covered is a wording choice for the fix session, as
long as the shared form's one-row-per-item rule is preserved.

## Critical files
- `.agents/skills/implement-plan/SKILL.md` — `## Handoff`, the
  `Runtime acceptance requests` bullet at lines 60-61

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the `Runtime acceptance requests`
  bullet of `.agents/skills/implement-plan/SKILL.md` `## Handoff`

## Out of scope
- The landed change the session produced
- The shared handoff form in `.agents/references/subagent-handoff.md`, and this
  skill's existing `Changed files` row-cap overflow paragraph, which already
  states the rule correctly
- The other extension fields this `## Handoff` declares
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Tier 1 (mechanical: skill documentation prose, no public signature or invariant
exposure); escalate only if the fix reaches the shared handoff reference. Never
embed transcript paths or home paths.

## Acceptance criteria
- The `Runtime acceptance requests` bullet directs a worker to a path plus
  selector when the cited plan file already holds the criteria, and keeps the
  per-criterion form for criteria with no cited home
- The recorded symptom no longer reproduces under the documented invocation
- The static-checks runner, invoked as `.agents/references/static-checks.md`
  documents it, reports every row the change triggers passing
