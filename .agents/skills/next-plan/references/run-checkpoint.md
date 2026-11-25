# Run Checkpoint

On Claude and `agy`, one external review covers the run for tooling friction,
context efficiency, and content a subagent could have consumed instead. Codex
runs no live lens and leaves all three concerns to `/next-plan-review` after
landing. [The `/next-plan` steps](../SKILL.md#steps) own the run-level
obligation; this reference owns dispatch, join, and follow-ups.

## Dispatch

On Claude and `agy`, main dispatches one fresh `reviewer` subagent for
`/next-plan-checkpoint-review` with the claimed Plan path or `no claim` as its
only run-specific input, and that reviewer resolves and measures its own run
evidence per that package's
[`references/worker.md`](../../next-plan-checkpoint-review/references/worker.md).
A Codex main dispatches nothing.

For an accepted implementation, main may dispatch the checkpoint once edits,
Review and resolve correctness, and Apply the triggered cleanup settle and only
landing-gate `/compile` builds remain, overlapping them under
[step 9](../SKILL.md#steps)'s once-per-run rule; for other endings in
[post-checkpoint outcomes](../SKILL.md#post-checkpoint-outcomes), dispatch it
when it applies before claim exit, landing preparation, or stopping. Before
step 10 `Complete-NextPlan.ps1` (when it applies) and `/finalize-changes`
preparation, wait for:

- landing-gate `/compile` builds (including post-build checks or compile-fix
  rounds); and
- if a checkpoint reviewer was dispatched, its handoff, follow-up writes, and
  checkpoint-Plan `/coherence-review` and fix rounds (`## Follow-up routing`).

Once that join, step 10 (when it applies), later pre-preparation checkpoint
writes, and fix rounds finish mutating the worktree, obtain the landing
`Invoke-StaticChecks.ps1` `PASS` from a run after that last write before
`/finalize-changes` preparation, so both sets of edits ride the same landing.

## Measurement states

The table below maps each measurement state to the reviewer's summary line and
main's recorded handoff line.

| Measurement state | Reviewer summary line | Handoff line recorded |
| --- | --- | --- |
| `pass` envelope | `pass` | `Context-efficiency follow-ups: <routing result>` |
| `needs-review` envelope, untruncated | `needs-review` | `Context-efficiency follow-ups: <routing result>`, from the reviewer's findings |
| post-projection envelope error (`<code>`), or missing/malformed `context-envelope` after `checkpoint-span` (`measure.failed`) | `skipped (<code>)` | `Context-efficiency follow-ups: blocked (<code>), then <routing result>` |
| `breachRowsTruncated: true` envelope | `skipped (breach-rows-truncated)` | `Context-efficiency follow-ups: blocked (breach-rows-truncated), then <routing result>` |
| unresolvable/unreadable transcript, or `Get-TranscriptProjection.ps1` failure (non-zero exit or absent `checkpoint-span`) | `none (BLOCKED handoff)` — whole-review `BLOCKED`, no summary block | `blocked (transcript-unavailable)` on both `Friction follow-ups:` and `Context-efficiency follow-ups:`, routed through the post-checkpoint rule below |
| Codex main session | none — no reviewer is dispatched and no lens runs | `none (codex)` on both lines |

Untruncated `needs-review` runs context-efficiency. The two `skipped` rows skip
only context-efficiency while friction and isolation run. The `BLOCKED` row runs
no lens. [`## Handoff`](../../next-plan-checkpoint-review/SKILL.md#handoff) owns
the exact summary output for non-`BLOCKED` states.

The `Handoff line recorded` column is main's own record, written from what
`## Follow-up routing` produced, not reviewer output: `<routing result>` is
`logged <n>` for rows recorded, the Plan path(s) the routing produced, both, or
`none`. The `Friction follow-ups:` line records `<routing result>` in every row
that does not name it.

## Follow-up routing

Main records each accepted finding in
`Documents/Investigations/ChangeWorkflow/WorkflowIncidents.md` as its header
directs; a finding that header exempts keeps the Leftovers rule of the Verify
the acceptance table step in
[change-workflow.md](../../../references/change-workflow.md). The header's
`/create-follow-up-plans` dispatch routes the key as a tooling-friction proposal
with the provenance block sourced per
[follow-up-provenance.md](follow-up-provenance.md); checkpoint-authored Plans
follow that Leftovers rule for `/coherence-review` and `## Dispatch` above for
the final `Invoke-StaticChecks.ps1` order. An `active-change-blocker` finding is
never recorded: it returns to the current change as a blocker.

The [post-checkpoint outcome table](../SKILL.md#post-checkpoint-outcomes) owns
the authoritative claim disposition and whether a landing is followup-only.

Friction arriving after the `checkpoint-span` `end` line emitted by
[`Get-TranscriptProjection.ps1`](../../next-plan-checkpoint-review/scripts/Get-TranscriptProjection.ps1)
— including a concurrent build result arriving between bridge prompt write and
launch or after launch, claim exit, and `/finalize-changes` itself — follows
this rule: record it before `/finalize-changes` preparation to ride the same
landing, or land it at a later gate if observed after preparation. On deferral,
or when the run ends without a claim, the checkpoint's rows and Plans are
themselves the landed content and the landing gate applies to them.
