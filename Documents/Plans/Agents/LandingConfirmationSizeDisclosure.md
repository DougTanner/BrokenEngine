<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-16T17:49:57.715Z","dependsOn":[]} -->
# Fix: landing confirmation hides reviewer-recorded size observations from the user

## Context

In the Codex session reviewed by `/next-plan-review` for landing commit
`04807415125611a53b199c20ddf36f1e70a42565` (parent session
`01a00722-4f92-7202-94a6-6f26053bded7`), the first C++ review recorded a Size
Observation at 00:36 UTC: the changed file measured 4,897 `bt-token-v1`, having
grown from 179 to 1,061 lines. As the contract directs, that observation was
routed to a follow-up Plan. The user first saw a size figure at 15:16 UTC and
immediately invalidated the approach, discarding roughly 355 active minutes of
work that the earlier disclosure would have stopped.

The gap is visible in current source. `.agents/skills/repo-code-review/SKILL.md:282-283`
defines the `### Size Observations` output purely as a manager follow-up
candidate, and no user-facing presentation contract requires it afterwards:

- `.agents/skills/next-plan/SKILL.md:98-119` presents the resolved Plan and
  execution card for implementation approval, which happens before
  implementation, so no post-implementation review observation can appear there.
- `.agents/skills/finalize-changes/SKILL.md:134-165` is the only user-facing gate
  that follows C++ review. It enumerates exactly two mandatory disclosure lines
  main answers from the whole session record — Superseded decisions and
  Substituted approaches — and neither covers size or complexity growth.

So a reviewer can measure a file's growth hours before the user sees any figure,
and the workflow has no place that shows it.

Evidence provenance (machine-local; not reproducible after cleanup):

- Client: claude
- Conversation session ID: 731a9400-fe85-40e5-aa8c-1a5a84157ee1
- Worktree/branch UUID: e6aec63b-d896-4432-a476-4da999162e12
- Session branch: claude/e6aec63b-d896-4432-a476-4da999162e12
- Worktree: .claude\worktrees\BrokenEngine\e6aec63b-d896-4432-a476-4da999162e12
- Source of the timeline and measurement: the `/next-plan-review` report for
  commit `04807415125611a53b199c20ddf36f1e70a42565`.

## Design

The home is decided: `/finalize-changes`, in its `## Landing confirmation`
disclosure block. Add exactly one further mandatory line, parallel in shape and
wording rules to the two lines already there, which main answers itself from the
whole session record:

- **Size and complexity observations:** every size observation a reviewer
  recorded on code this session changed, giving the file, its measured
  `bt-token-v1` size, and how far it grew, in plain words the user can act on.
  Like the existing two lines it always appears and states `none` when the
  session record holds nothing to disclose.

The line is disclosure only: it adds no gate, no blocker, and no new check, and
it changes nothing about who records the observation or when. `/repo-code-review`
already emits the observation in its handoff, so its contract is unchanged, and
`/next-plan`'s pre-implementation approval presentation is unchanged because the
observation does not exist yet at that point.

## Critical files

- `.agents/skills/finalize-changes/SKILL.md` — the `## Landing confirmation`
  disclosure block: the sentence naming what main answers from the session record
  (`:139-141`) and the two-line list with its `none` rule (`:143-151`).

## In scope

- Adding the third mandatory disclosure line described in `## Design` to the
  `## Landing confirmation` block, and extending the surrounding sentence and
  `none` rule so the new line is covered by them.

## Out of scope

- `/repo-code-review`'s Size Observations threshold, format, and routing;
  `/reduce-file`; `/next-plan`'s approval presentation.
- The finalizer scripts and every JSON result contract under
  `.agents/skills/finalize-changes/scripts/`; the landing summary the finalizer
  worker returns is main's input, not the place this line is added.
- Any new gate, blocker, approval round, or automatic measurement run; already
  authored follow-up Plans that recorded past size observations.

## Risk tier and invariants

Expected Tier 1 (documentation wording in one skill file, no public signature or
invariant exposure); escalate to Tier 2 if the smallest fix turns out to require
changing the finalizer worker's scripted result contract rather than main's
presentation prose. Preserve the single explicit landing confirmation, the exact
confirmation question text, the rule that the whole summary is rendered message
text with the question as its last line and no tool call after it, and the
plain-language no-jargon requirement.

## Acceptance criteria

- When a C++ review in the session recorded at least one size observation on
  session-changed code, the landing confirmation message shows that file, its
  measured `bt-token-v1` size, and its growth in plain words, before the
  confirmation question.
- When the session record holds none, the line still appears and states `none`,
  matching the existing two lines.
- Landing gains no new blocker: the line only discloses, and the confirmation
  question and its single-confirmation contract are unchanged.
- `/validate-skill` passes for `.agents/skills/finalize-changes/SKILL.md`, and
  WorktreeCli `plan validate` exits `0` with `status: valid` and `code: ok`.

## Notes

This Plan is keyed to the pair (`/finalize-changes` landing confirmation
disclosure lines, a reviewer-recorded size observation that never reaches the
user before the work is finished). A later observation of that same pair is a
duplicate, not a new residual. No transcript path or transcript text is embedded.
