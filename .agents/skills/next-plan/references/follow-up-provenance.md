# Follow-Up Provenance Sourcing

Read this reference when a `/next-plan` run records a tooling-friction or
context-efficiency follow-up and needs the provenance values. It covers only
where this run sources those values; `/create-follow-up-plans` owns the
provenance block's field shapes, and the routing rules stay in
[`/next-plan`](../SKILL.md).

## Where each field comes from

The block names the client (the session-branch `claude`/`codex` prefix), the
worktree/branch UUID, the session branch, a profile-relative worktree locator
with the user-profile prefix stripped, and, on Claude, the conversation session
ID. Take the client, worktree/branch UUID, session branch, and worktree locator
from the `Get-NextPlanContext` result already resolved in the Preconditions and
selection section, or from `git branch --show-current` and
`git rev-parse --show-toplevel`.

## Conversation session ID

Those sources name the worktree only, so none of them yields the conversation
session ID `/next-plan-review` needs to find a transcript. Any shell of the
session reads it from the `CLAUDE_CODE_SESSION_ID` environment variable, a
dispatched worker's shell included, because a subagent shell carries the parent
session's value; the value differs per conversation and resume, so read it when
the friction is recorded. Codex sessions record no conversation session ID,
because `/next-plan-review` discovers Codex transcripts by bounded commit
window.

## Friction observed in a different session

When the friction was observed in a different session than the one recording it
— the shape a `/next-plan-review`-driven follow-up takes — main supplies that
observing session's identity fields in place of the current session's, including
its landed commit or branch, labelled as the observing session's per the
provenance block in `/create-follow-up-plans`, which owns how the recording
session's own landing ref is stated.

## What never enters a Plan

Never record a transcript file path or transcript text; reference the session by
client, worktree/branch UUID, on Claude the conversation session ID, and, for an
earlier observing session, its landed commit or branch only.
