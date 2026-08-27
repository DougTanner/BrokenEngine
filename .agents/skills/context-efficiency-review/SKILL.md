---
name: context-efficiency-review
description: >-
  Review the main session's measured context-efficiency envelope for oversized
  tool results that indicate a fixable tooling or skill defect. Use only when
  dispatched from a `/next-plan` context-efficiency checkpoint with a
  `needs-review` envelope. Findings only; never edits.
allowed-tools: [Read, Grep, Glob]
---

# Context Efficiency Review

Judge whether an oversized tool result that entered the main session's context
came from tooling that could have carried the same decision in a bounded form.
The measurement is already done; this review supplies only the judgment the
measurement cannot make.

## Inputs

- The `broken-engine-context-efficiency/v1` envelope text, measured and supplied
  by main. Never run the measuring script, read a transcript, or read
  `CLAUDE_CODE_SESSION_ID`, for the reason `../../references/subagent-reporting.md`
  gives under `## Handoffs`.
- The checkpoint name main measured at.
- The claimed Plan path, or `no claim`.

If the envelope is missing, if its `verdict` is `pass`, if its
`breachRowsTruncated` is `true`, if the checkpoint name is missing, or if
neither a claimed Plan path nor `no claim` is supplied, return `BLOCKED` naming
the invalid input.

## Execution Context

Run in the delegated execution context of
`../../references/subagent-reporting.md`, dispatched via `/codex-review`;
inline review is prohibited.

## Review

1. Review every `topResults` row marked `overThreshold: true`. The other rows are
   context only, and `totalChars` is telemetry that never produces a finding on
   its own.
2. Identify the emitting invocation for each row from `toolName` plus
   `inputSummary`: the repository script, skill instruction, or documented
   command that produced that output. Read the emitter in the tree. Done when
   each row names one emitter or is recorded as unidentifiable. An `Agent` or
   `SendMessage` row is a completed subagent's handoff; its emitter is the return
   format of the skill the dispatch named.
3. Classify each row:
   - `fixable-defect` — a bounded projection, a count plus the decision-relevant
     rows, an explicit cap with a truncation flag, or a file drop plus a receipt
     and selector would have carried the same decision. The landed shape is
     `.agents/skills/next-plan/scripts/Get-NextPlanList.ps1`, which folds a full
     Plan listing into counts plus the first rows.
   - `necessary-evidence` — the manager's decision genuinely required the content
     verbatim, so no bounding mechanism preserves it.
   - `active-change-blocker` — the emitter is a script or skill the claimed Plan
     itself changes.
4. Precision guard: a finding names the emitting invocation and one concrete
   bounding mechanism. No named emitter and mechanism, no finding.

`/next-plan`'s `## Context-efficiency review` section owns what main does with
each class.

## Exclusions

- Post-landing token-efficiency retrospectives — `/next-plan-review`.
- A measuring script that errored or reported a malformed transcript — the
  `/next-plan` friction review.
- Subagent-internal context: sidechain output never entered the main session and
  is absent from the envelope.
- Content the user pasted or asked to display.

## Output

One line per finding:

```text
<toolName> | selector: <inputSummary> | chars: <n> | class: fixable-defect|necessary-evidence|active-change-blocker | emitter: <script or skill path> | bounding: <mechanism>
```

Example:

```text
PowerShell | selector: worktreecli plan list | chars: 41208 | class: fixable-defect | emitter: .agents/skills/next-plan/scripts/Get-NextPlanList.ps1 | bounding: state counts plus the first 10 rows
```

Then the summary block:

```text
Checkpoint: <checkpoint name>
Claimed Plan: <path or no claim>
Rows at or over threshold: <count>
Findings: <count or none>
Status: PASS | NEEDS_ACTION
```

Use `PASS` when no row yields a finding under the precision guard. Report every
`necessary-evidence` row as a checked row rather than a finding. The handoff
extends the block in `../../references/subagent-reporting.md`, keeping
`Build required` and `Residuals` last.

The manager decides each finding on whether the failure is concrete, reachable,
and meaningful under the standard defaults; this review adds no extra rounds.
