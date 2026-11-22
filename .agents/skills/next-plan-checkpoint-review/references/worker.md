# Next Plan Checkpoint Review Worker

The run order and judgment rules for the dispatched reviewer. Step 1 is the
shared evidence step all three lenses read from; steps 2-7 are the
tooling-friction lens; steps 8-11 are the context-efficiency lens; steps 12-13
are the isolation lens.

## Steps

1. Resolve, project, and measure this run's transcript first. On Claude, resolve
   `(Get-ChildItem "$env:USERPROFILE/.claude/projects/*/$env:CLAUDE_CODE_SESSION_ID.jsonl").FullName`.
   On `agy`, import `./.agents/scripts/AgentTranscriptCommon.psm1`, call
   `Resolve-AgentAgyParentSession -SessionId $env:ANTIGRAVITY_CONVERSATION_ID`,
   require `Status` `'available'` (else `BLOCKED` naming `Reason`), and take
   `RootTranscriptPath`. If resolved and readable, run
   `pwsh -NoProfile -File .agents/skills/next-plan-checkpoint-review/scripts/Get-TranscriptProjection.ps1 -TranscriptPath '<resolved path>' -IncludeContextEnvelope`
   once from the worktree root, capturing stdout in a PowerShell variable and
   reading `$LASTEXITCODE` before any other command (no unbounded raw stdout or
   temp files; sandbox is `read-only`). Classify via
   `../../next-plan/references/run-checkpoint.md` `## Measurement states`. On a
   whole-review `BLOCKED` state, stop immediately with no direct transcript
   read, synthesized span, findings, or summary block (safe reason in
   `Residuals`). Otherwise print only exit code, `checkpoint-span`,
   `context-envelope`, total in-span row count, and a bounded first page of
   in-span rows (`[start, end]` minus `exclude`). Fetch remaining `[start, end]`
   pages for steps 2-7 and 12-13 by rerunning `Get-TranscriptProjection.ps1`
   without `-IncludeContextEnvelope` or temp files, applying the same in-memory
   capture, `## Measurement states` `BLOCKED` gate, and bounded `[start, end]`
   minus `exclude` slice. Done when path, span, initial in-span rows, and state
   are in hand, or `BLOCKED`.
2. Report as friction: a bundled script errored, returned a malformed or
   contradictory result, or could not be run as documented; a workaround or
   deviation was needed; work was repeated because a skill's instructions were
   unclear, wrong, or contradicted repository state. Step 1's own measurement
   state is in scope here even though it postdates step 6's span. Done when
   every such observation in the transcript span, plus that measurement state,
   is either a finding or excluded by step 4.
3. For a `skipped` state (`## Measurement states` in
   `../../next-plan/references/run-checkpoint.md`), emit one friction finding
   naming `Get-TranscriptProjection.ps1 -IncludeContextEnvelope`, the state's
   code (noting a missing or malformed `context-envelope` row when applicable)
   or truncation flag, and the skipped lens. Done when such a state has one
   finding.
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
6. Before applying any lens, take this checkpoint's span `[start, end]`,
   excluded completed-handoff line (`exclude`), and excluded `toolUseId`
   (`exclude-id`) from step 1's `Get-TranscriptProjection.ps1`
   `checkpoint-span` row. Retain every other record in `[start, end]`. Apply
   the span and exclusions to friction and isolation records and, by
   `toolUseId`, to context-efficiency `topResults`. Running the claim-exit
   script and `/finalize-changes` happens after this dispatch and is outside
   this review; `## Follow-up routing` in
   `../../next-plan/references/run-checkpoint.md` and `/next-plan-review` own
   those. Done when no finding or checked telemetry row comes from outside the
   span or from the excluded handoff — except step 3's friction finding on
   step 1's own measurement state, which the span does not exclude — including
   a run that stopped before or without a claim.
7. Precision guard: name in each friction finding the exact command or script
   path, the observed output or malformed result, and the rework, workaround, or
   skipped step it forced. No citation, no finding. Done when every friction
   finding names all three, and the rest are dropped.
8. Unless step 1 skipped context-efficiency, review every `topResults` row
   marked `overThreshold: true` in step 1's `context-envelope` row. Other rows
   are context only. Done when every such row has been read or skipped.
9. Identify each row's emitter from `toolName` and `inputSummary` (script,
   skill, or command) and read it in the tree. An `Agent`/`SendMessage`
   (Claude), `invoke_subagent` (`agy`), or bridged `Invoke-Codex.ps1` row is a
   subagent handoff emitted by that skill's return format; for bridged
   `Invoke-Codex.ps1` launches and `-Wait` calls, identify the emitter directly
   from `Get-TranscriptProjection.ps1`'s `bridge-launch` and `bridge-delivery`
   (`findings=yes`) rows matching `toolUseId`, ignoring `bridge-stub` rows.
   Done when each row names an emitter or is unidentifiable.
10. Classify each row, done when every row carries one class:
    - `fixable-defect` — a bounded projection, a count plus the
      decision-relevant rows, an explicit cap with a truncation flag, or a file
      drop plus a receipt and selector would have carried the same decision. For
      an isolation finding, the step 11 mechanism is that bounding. The landed
      shape is `.agents/skills/next-plan/scripts/Get-NextPlanList.ps1`, which
      folds a full Plan listing into counts plus the first rows.
    - `necessary-evidence` — the manager's decision genuinely required the
      content verbatim, so no bounding mechanism preserves it.
    - `active-change-blocker` — the emitter is a script or skill the claimed
      Plan itself changes.
11. Precision guard: name in each context or isolation finding the emitting
    invocation and one concrete bounding mechanism — for an isolation finding,
    the role from the Change Workflow delegation role table that could have
    consumed the content instead, or the path plus selector that should have
    replaced it, within the field rules in
    `../../../references/subagent-handoff.md`. No named emitter and mechanism,
    no finding. Done when every such finding names both, and the rest are
    dropped.
12. Select records from step 1's in-span projection rows (script header owns row
    shapes; use step 1's paging rule for remaining pages), never reading the
    transcript whole-file. Consume `checkpoint-span` and `bridge-*` directly,
    ignore `bridge-stub`, and never assume the tool is named `Task`.

    Open one selected record line at a time with `Read` (`offset` = line,
    `limit` 1), `read_file` (`StartLine`/`EndLine` = line), or on Codex:
    `Get-Content -LiteralPath '<path>' -TotalCount <line> | Select-Object -Last 1`.
    `len` selects lines only and is never `chars:`.

    Report as isolation any content main did not need verbatim that a subagent
    could have consumed: reading a file a worker's brief could name, forwarding
    raw script output, pasting evidence inline instead of path plus selector, or
    a handoff restating its brief (regardless of size). Detect the first case
    via in-span `match` rows whose `read-at` line is also in span: emitter is
    main's active skill instruction; step 11 bounding mechanism is that brief's
    role. Done when every in-span observation is a finding or excluded by
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
