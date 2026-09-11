# Next Plan Checkpoint Review Worker

The run order and judgment rules for the dispatched reviewer. Step 1 is the
shared evidence step all three lenses read from; steps 2-7 are the
tooling-friction lens; steps 8-11 are the context-efficiency lens; steps 12-13
are the isolation lens.

## Steps

1. Resolve this run's transcript and measure it before applying any lens. The
   reviewer's shell carries the dispatching main session's ID, so resolve the
   transcript from the PowerShell tool:
   `(Get-ChildItem "$env:USERPROFILE/.claude/projects/*/$env:CLAUDE_CODE_SESSION_ID.jsonl").FullName`
   then measure it from the session worktree root:
   `pwsh -NoProfile -File .agents/skills/next-plan-checkpoint-review/scripts/Measure-SessionContext.ps1 -SessionId <id>`
   (or `-TranscriptPath <resolved path>`). Classify the returned envelope by the
   states `## Measurement states` in
   `../../next-plan/references/run-checkpoint.md` keeps. Return `BLOCKED` for the
   whole review when that glob resolves to nothing, the transcript file cannot be
   read, or the measurement returns `transcript.not-found`, because then no lens
   has evidence; every other blocked or error code skips only the
   context-efficiency lens, while the friction and isolation lenses still run.
   Done when one transcript path and one measurement state are in hand, or the
   review is `BLOCKED`.
2. Report as friction: a bundled script errored, returned a malformed or
   contradictory result, or could not be run as documented; a workaround or
   deviation was needed; work was repeated because a skill's instructions were
   unclear, wrong, or contradicted repository state. Step 1's own measurement
   state is in scope here even though it postdates step 6's span. Done when
   every such observation in the transcript span, plus that measurement state,
   is either a finding or excluded by step 4.
3. Report a step 1 measurement state that skips the context-efficiency lens — a
   blocked or error code, or a truncated measurement (`breachRowsTruncated:
   true`, that is `skipped (breach-rows-truncated)`) — as one friction finding
   naming the measuring command, the returned code or the truncation flag, and
   the skipped context-efficiency lens. Done when such a state has exactly one
   such finding.
4. Exclude from friction: ordinary review findings about the change;
   user-driven iteration; documented normal stops such as `none-available`; a
   worker's deviation from the handoff form its skill correctly declares, which
   is never a friction finding and is instead one `Residuals` row naming the
   worker role and the rule broken. A skill `## Handoff` that itself conflicts
   with `../../../references/subagent-handoff.md` `## Handoffs` is not
   excluded: it stays a `fixable-defect` finding on that skill as emitter. Done
   when no finding rests on one of these.
5. Classify a failure in a skill or script the claimed Plan itself changes as
   `active-change-blocker`, not a follow-up. Done when every friction finding
   carries a class.
6. Before applying any lens, establish this checkpoint's transcript span
   through step 12's projection. End immediately before this review's own
   dispatch. If the same transcript contains an immediately preceding
   `/next-plan-checkpoint-review` dispatch, start immediately after that
   dispatch and exclude only its correlated completed-handoff record; otherwise
   start at the transcript beginning. Retain every other record between the
   boundaries. Apply the span to friction and isolation records and, by
   `toolUseId`, to context-efficiency `topResults`. Running the claim-exit
   script and `/finalize-changes` happens after this dispatch and is outside
   this review; `## Follow-up routing` in
   `../../next-plan/references/run-checkpoint.md` and `/next-plan-review` own
   those. Done when no finding or checked telemetry row comes from outside the
   span or from the excluded handoff — except step 3's friction finding on step
   1's own measurement state, which the span does not exclude — including a run
   that stopped before or without a claim.
7. Precision guard: name in each friction finding the exact command or script
   path, the observed output or malformed result, and the rework, workaround, or
   skipped step it forced. No citation, no finding. Done when every friction
   finding names all three, and the rest are dropped.
8. Review every `topResults` row marked `overThreshold: true`. The other rows are
   context only. Done when every such row has been read.
9. Identify the emitting invocation for each row from `toolName` plus
   `inputSummary`: the repository script, skill instruction, or documented
   command that produced that output. Read the emitter in the tree. Done when
   each row names one emitter or is recorded as unidentifiable. An `Agent` or
   `SendMessage` row is a completed subagent's handoff; its emitter is the return
   format of the skill the dispatch named.
10. Classify each row, done when every row carries one class:
    - `fixable-defect` — a bounded projection, a count plus the decision-relevant
      rows, an explicit cap with a truncation flag, or a file drop plus a receipt
      and selector would have carried the same decision. For an isolation
      finding, the step 11 mechanism is that bounding. The landed shape is
      `.agents/skills/next-plan/scripts/Get-NextPlanList.ps1`, which folds a full
      Plan listing into counts plus the first rows.
    - `necessary-evidence` — the manager's decision genuinely required the content
      verbatim, so no bounding mechanism preserves it.
    - `active-change-blocker` — the emitter is a script or skill the claimed Plan
      itself changes.
11. Precision guard: name in each context or isolation finding the emitting
    invocation and one concrete bounding mechanism — for an isolation finding,
    the role from the Change Workflow delegation role table that could have
    consumed the content instead, or the path plus selector that should have
    replaced it, within the field rules in
    `../../../references/subagent-handoff.md`. No named emitter and mechanism,
    no finding. Done when every such finding names both, and the rest are
    dropped.
12. Select the transcript records to read only through the bundled script, whose
    header comment states the row shapes, never by reading the transcript
    whole-file:
    `pwsh -NoProfile -File .agents/skills/next-plan-checkpoint-review/scripts/Get-TranscriptProjection.ps1 -TranscriptPath <transcript path>`

    In the projection, inspect candidate `use Agent` rows and open only the
    candidate records needed to identify this review's
    `/next-plan-checkpoint-review` dispatch and, when present, the immediately
    preceding matching dispatch and their tool-use IDs. These locate step 6's
    boundaries. Locate the preceding dispatch's correlated completed-handoff
    record by matching its tool-use ID to a `result` row or to the tool-use-ID
    tag in an opened task-notification/queued-attachment string record. Do not
    assume the delegation tool is named `Task`.

    Open a record the rows select with `Read` at that row's line number as
    `offset` with `limit` 1; the `len` column only selects which records to open
    and is never a `chars:` value.

    Report as isolation: content that entered main's context which main did not
    need verbatim to decide anything and a subagent could have consumed instead
    — main reading a source or reference file a worker's brief could have named,
    raw script output main only forwarded, evidence pasted inline in a handoff
    instead of cited as path plus selector, or a handoff restating its own
    brief. Size does not gate this: content below the measured threshold still
    qualifies. Detect the first case from the projection's `match` rows, keeping
    only those whose own line and `read-at` line both fall inside the step 6
    span: each names a read path a brief lists. Such a read is a finding whose
    emitter is the skill instruction main was executing when it made that read
    and whose step 11 bounding mechanism is that brief's dispatched role. Done
    when every such observation in span is either a finding or excluded by
    step 13.
13. Exclude from isolation, beyond the exclusions below: content main's own
    decision required verbatim, classified `necessary-evidence` under step 10
    whether or not a measured row exists; and a Plan body, execution card, or
    user-facing text main itself must approve or present. Done when every
    remaining isolation finding carries a step 10 class and passes step 11.

## Rules

- No finding this review returns carries content that `## What never enters a
  Plan` in `../../next-plan/references/follow-up-provenance.md` keeps out of a
  Plan.
- Exclusions:
  - Post-landing token-efficiency retrospectives — `/next-plan-review`.
  - Subagent-internal context: sidechain output never entered the main session
    and is absent from the envelope.
  - Content the user pasted or asked to display.
- `## Follow-up routing` in `../../next-plan/references/run-checkpoint.md` owns
  what main does with each class.
- The manager decides each finding on whether the failure is concrete,
  reachable, and meaningful under the standard defaults; this review adds no
  extra rounds.
