# Run Checkpoint

On Claude, one external review covers the run for tooling friction, context
efficiency, and content a subagent could have consumed instead. Codex runs no
live lens and leaves all three concerns to `/next-plan-review` after landing.
OpenCode has no supported live or retrospective transcript source, so no live
lens runs and the checkpoint records that limitation explicitly.
[The `/next-plan` steps](../SKILL.md#steps) own when it runs; this reference owns how.

## Dispatch

On Claude, main dispatches one fresh `reviewer` subagent for
`/next-plan-checkpoint-review` with the claimed Plan path or `no claim` as its
only run-specific input, and that reviewer resolves and measures its own run
evidence per that package's
[`references/worker.md`](../../next-plan-checkpoint-review/references/worker.md).
A Codex or OpenCode main dispatches nothing.

## Measurement states

The table below maps each measurement state to the reviewer's summary line and
main's recorded handoff line.

| Measurement state | Reviewer summary line | Handoff line recorded |
| --- | --- | --- |
| `pass` envelope | `pass` | `Context-efficiency follow-ups: <isolation-lens Plan path(s) or none>` |
| `needs-review` envelope, untruncated | `needs-review` | `Context-efficiency follow-ups:` Plan path(s) or none, from the reviewer's findings |
| blocked or error envelope | `skipped (<code>)` | `Context-efficiency follow-ups: blocked (<code>), then any isolation-lens Plan path(s)` |
| `breachRowsTruncated: true` envelope | `skipped (breach-rows-truncated)` | `Context-efficiency follow-ups: blocked (breach-rows-truncated), then any isolation-lens Plan path(s)` |
| transcript unresolvable, unreadable, or `transcript.not-found` | `none (BLOCKED handoff)` — the reviewer returns `BLOCKED` for the whole review and carries no summary block | `blocked (transcript-unavailable)` on both `Friction follow-ups:` and `Context-efficiency follow-ups:`, routed through the post-checkpoint rule below |
| Codex main session | none — no reviewer is dispatched and no lens runs | `none (codex)` on both lines |
| OpenCode main session | none — OpenCode transcript review is unsupported, so no reviewer is dispatched and no lens runs | `unsupported (opencode)` on both lines |

A `pass` envelope means the measurement completed with no rows at or over
threshold. An untruncated `needs-review` envelope runs the reviewer's
context-efficiency lens; a truncated one skips it and reports
`skipped (breach-rows-truncated)`, and blocked and error states, except the
transcript-unavailable one, skip it too while the other lenses still run. A
transcript the reviewer can neither resolve nor read, and a
`transcript.not-found` measurement, carry no lens at all, so that run's review
returns `BLOCKED` instead. The review skill's
[`## Handoff`](../../next-plan-checkpoint-review/SKILL.md#handoff) owns the
exact reviewer summary output for each measurement state.

The `Handoff line recorded` column is main's own record, written from what
`## Follow-up routing` produced, not reviewer output; the `Friction follow-ups:`
line records `<Plan path(s) or none>` in every row that does not name it.

## Follow-up routing

Each accepted finding follows the Leftovers rule of the Verify the acceptance
table step in [change-workflow.md](../../../references/change-workflow.md); an
`implementer` routes one that goes to `/create-follow-up-plans` as a
tooling-friction or isolation proposal, supplying the observed symptom with its
citation plus the provenance block sourced per
[follow-up-provenance.md](follow-up-provenance.md). An `active-change-blocker`
finding never becomes a follow-up Plan: it returns to the current change as a
blocker.

The [post-checkpoint outcome table](../SKILL.md#post-checkpoint-outcomes) owns the
authoritative claim disposition and whether a landing is followup-only.

Friction first observed after this checkpoint — including friction in running
the claim-exit script and in `/finalize-changes` itself — is recorded through
`/create-follow-up-plans` by an `implementer` and lands at a later gate as its
own content. On deferral, or when the run ends without a claim, the checkpoint's
Plans are themselves the landed content and the landing gate applies to them.
