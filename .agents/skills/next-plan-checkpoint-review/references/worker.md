# Next Plan Checkpoint Review Worker

The run order and judgment rules for the dispatched reviewer. Steps 1-6 are the
tooling-friction lens; steps 7-10 are the context-efficiency lens; steps 11-12
are the isolation lens.

## Steps

1. Report as friction: a bundled script errored, returned a malformed or
   contradictory result, or could not be run as documented; a workaround or
   deviation was needed; work was repeated because a skill's instructions were
   unclear, wrong, or contradicted repository state. Done when every such
   observation in the transcript is either a finding or excluded by step 3.
2. Report a measurement supplied as truncated (`breachRowsTruncated: true`, that
   is `blocked (breach-rows-truncated)`) as one friction finding naming the
   measuring command, the truncation flag, and the skipped context-efficiency
   lens. Done when a truncated input has exactly one such finding.
3. Exclude from friction: ordinary review findings about the change;
   user-driven iteration; documented normal stops such as `none-available`; a
   worker's deviation from the handoff form its skill correctly declares, which
   is never a friction finding and is instead one `Residuals` row naming the
   worker role and the rule broken. A skill `## Handoff` that itself conflicts
   with `../../../references/subagent-reporting.md` `## Handoffs` is not
   excluded: it stays a `fixable-defect` finding on that skill as emitter. Done
   when no finding rests on one of these.
4. Classify a failure in a skill or script the claimed Plan itself changes as
   `active-change-blocker`, not a follow-up. Done when every friction finding
   carries a class.
5. Cover friction observable in the supplied transcript at any point in the run
   up to this dispatch, including a stop before or without a claim. Running the
   claim-exit script and `/finalize-changes` happens after the dispatch and is
   outside this review; `## Follow-up routing` in
   `../../next-plan/references/run-checkpoint.md` and `/next-plan-review` own
   those. Done when the covered span ends at this dispatch.
6. Precision guard: name in each friction finding the exact command or script
   path, the observed output or malformed result, and the rework, workaround, or
   skipped step it forced. No citation, no finding. Done when every friction
   finding names all three, and the rest are dropped.
7. Review every `topResults` row marked `overThreshold: true`. The other rows are
   context only, and `totalChars` is telemetry that never produces a finding on
   its own. Done when every such row has been read.
8. Identify the emitting invocation for each row from `toolName` plus
   `inputSummary`: the repository script, skill instruction, or documented
   command that produced that output. Read the emitter in the tree. Done when
   each row names one emitter or is recorded as unidentifiable. An `Agent` or
   `SendMessage` row is a completed subagent's handoff; its emitter is the return
   format of the skill the dispatch named.
9. Classify each row, done when every row carries one class:
   - `fixable-defect` — a bounded projection, a count plus the decision-relevant
     rows, an explicit cap with a truncation flag, or a file drop plus a receipt
     and selector would have carried the same decision. For an isolation
     finding, the step 10 mechanism is that bounding. The landed shape is
     `.agents/skills/next-plan/scripts/Get-NextPlanList.ps1`, which folds a full
     Plan listing into counts plus the first rows.
   - `necessary-evidence` — the manager's decision genuinely required the content
     verbatim, so no bounding mechanism preserves it.
   - `active-change-blocker` — the emitter is a script or skill the claimed Plan
     itself changes.
10. Precision guard: name in each context or isolation finding the emitting
    invocation and one concrete bounding mechanism — for an isolation finding,
    the role from the root AGENTS.md table that could have consumed the content
    instead, or the path plus selector that should have replaced it, within the
    handoff limits in `../../../references/subagent-reporting.md`. No named
    emitter and mechanism, no finding. Done when every such finding names both,
    and the rest are dropped.
11. Read the transcript only through the bundled script, never whole-file, whose
    header comment states the row shapes:
    `pwsh -NoProfile -File .agents/skills/next-plan-checkpoint-review/scripts/Get-TranscriptProjection.ps1 -TranscriptPath <transcript path>`

    Open a record the rows select with `Read` at that row's line number as
    `offset` with `limit` 1; the `len` column only selects which records to open
    and is never a `chars:` value.

    Report as isolation: content that entered main's context which main did not
    need verbatim to decide anything and a subagent could have consumed instead
    — main reading a source or reference file a worker's brief could have named,
    raw script output main only forwarded, evidence pasted inline in a handoff
    instead of cited as path plus selector, or a handoff restating its own
    brief. Size does not gate this: content below the measured threshold still
    qualifies. Done when every such observation in the transcript is either a
    finding or excluded by step 12.
12. Exclude from isolation, beyond the exclusions below: content main's own
    decision required verbatim, recorded as a `necessary-evidence` checked row
    under step 9's classes; and a Plan body, execution card, or user-facing text
    main itself must approve or present. Done when every remaining isolation
    finding carries a step 9 class and passes step 10.

## Rules

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
