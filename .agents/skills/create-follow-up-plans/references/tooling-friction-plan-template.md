# Tooling-friction Plan template

Draft a tooling-friction Plan body from this template. Its `Worktree` line is a
profile-relative locator, never an absolute path, and no transcript path or
transcript text ever enters the body.

```markdown
# Fix: <skill or script> — <one-line symptom>

## Context
<Observed symptom: exact command run, script path(:line if known), observed
output/behavior, what was repeated or worked around. No transcript text.>

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude | codex
- Conversation session ID: <lowercase uuid on Claude, read from
  CLAUDE_CODE_SESSION_ID and used as /next-plan-review's override; none on
  Codex, which uses bounded commit-window discovery; the worktree UUID is
  never substituted>
- Worktree/branch UUID: <lowercase uuid, the same one the Session branch and
  Worktree lines carry — selection evidence only, never production proof>
- Session branch: <claude|codex>/<uuid>
- Worktree: <profile-relative locator, e.g. .claude\worktrees\<repo>\<uuid> —
  never an absolute path, so no home prefix enters the public repo>
- Observed in an earlier session: <omit this line entirely when the session
  recording this Plan is the session that observed the friction. Otherwise
  state that the fields above are the observing session's, and give that
  session's landed commit hash, never its branch, which worktree reuse moves
  past the landing — required on Codex, whose transcript discovery needs the
  commit window around it.>
- Landing ref: <a ref whose tree contains this Plan; never assume the session
  branch above contains it. When the observing session records and lands the
  Plan itself: the session branch above, which survives exactly as long as the
  worktree recorded above. When a later session records it: that later
  session's landing commit hash or session branch, because the branch above was
  landed before this Plan existed.>
  The review ref the invoker passes to /next-plan-review has two cases. When
  the `Observed in an earlier session` line is present, it is that line's
  landed commit hash, with no lookup. When that line is absent, the recording
  session is the observing session, and worktree reuse moves its branch past
  its landing, so the invoker never reviews the ref itself: it resolves the ref
  to the commit that added this Plan with
  `git log --follow --diff-filter=A -n 1 --format=%H <ref> -- <this plan path>`
  and passes that commit. When the recorded ref is gone or that command prints
  nothing (a later session moved this Plan), it runs the same command with
  `<ref>` omitted. A periodic Plan-history squash can make either command
  return an unrelated aggregate commit, and a landing that also deletes a
  similar file can make it return another session's commit, so review the
  result only when the commit is attributable to the observing session recorded
  above alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
First root-cause the friction from the current tree and this Plan's
`## Context`. Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <review ref>` in bounded friction mode, with the review ref
the provenance block above defines, supplying the recorded client and, on
Claude, the recorded conversation session ID; a Codex review supplies the
client and that review ref only. Then make the smallest fix inside the
`## In scope` boundary below. If root-causing shows the fix lies outside that
boundary, surface it for re-planning instead of expanding scope.

## Critical files
- <each concrete SKILL.md and/or script file involved — these files are the
  authorized fix boundary>

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to <the files named above, naming
  sections/functions when the observed symptom already identifies them>

## Out of scope
- The landed change the session produced
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior); escalate if the fix reaches
build/bootstrap coordination. Never embed transcript paths or home paths.

## Acceptance criteria
- The recorded symptom no longer reproduces under the documented invocation
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
```
