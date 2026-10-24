<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-24T12:28:35.931Z","dependsOn":[]} -->
# Investigate whether back-to-back implementer steps should share one worker context

## Context
The user read Anthropic articles on multi-agent design and context engineering
and asked for this investigation. The articles warn that splitting sequential
phases of the same work across agents loses detail at each handoff and pays a
cold-start cost each time. They also say fresh-context review is a good split.
Reviewer steps are not in question here.

The Change Workflow (`.agents/references/change-workflow.md`
`### Steps`) runs several `implementer` steps one after another, each in a
fresh worker. Two groups are candidates for running in one worker context:

- Candidate A: the preparation `implementer` (the Prepare and explore
  alternatives step's `/prepare-change`, or `/next-plan` step 4's preparation
  `implementer` for a claimed Plan) and the Plan review step's fresh
  `implementer` that writes accepted Plan review findings into the plan
  snapshot from a continuation capsule.
- Candidate B: the Implement and propagate step's `/implement-plan` slice
  `implementer` and its `/update-affected-code` `implementer`, and the Apply
  the triggered cleanup step's `/update-claude-docs` `implementer`.

User constraint: tokens are limited. There are no replay evals and no
throwaway worktrees that re-run old Plans. Measurement means reading what real
runs already produced. Codex and OpenCode share the same workflow, so a
recommendation must hold for them too.

## Design
Measure from existing transcripts only. No model reads a transcript whole.

1. Pick the sample. List landed commits on `main`, newest first, that delete a
   `Documents/Plans/**/*.md` file (a completed or rejected `/next-plan` claim).
   Scan at most the 30 newest. For each, find its Claude transcript with
   `.agents/skills/next-plan-review/scripts/Find-AgentSessionTranscript.ps1`,
   run in the form `.agents/skills/next-plan-review/references/worker.md`
   step 2 documents, and build the parent transcript path from its result as
   steps 3 and 5 state. Keep the runs that have a proven Claude transcript. Take
   up to 5, choosing runs whose diff touches C++ (`.cpp` or `.h`) first and
   filling with other runs to reach at least 3. If fewer than 3 exist, use what
   exists and record the shortfall.
2. List the dispatches of each run. A one-off PowerShell query over the parent
   transcript writes to `Temp/` one row per `Agent` tool call: the subagent
   type, the brief's `Skill:` line, the description, the `agentId` from the
   matching tool result, and that tool result's handoff text. Derive each child
   transcript path from the `agentId` as `worker.md` step 8 states. Map each
   dispatch to its candidate step. A step that did not run in a sample (for
   example, no accepted finding, so no Plan review step writer) is recorded as
   not run.
3. Measure the cost of keeping steps separate, per consecutive pair in each
   candidate:
   - Per-dispatch tokens: run
     `.agents/skills/next-plan-review/scripts/Get-TranscriptUsage.ps1` on each
     child transcript as `worker.md` step 8 documents. Record input, cache
     read, cache creation, and output tokens.
   - Re-read overlap: a one-off query writes to `Temp/` the files each child
     read (`Read` and `Grep` paths with line ranges). The overlap is the set
     both steps of a pair read. Size it with `.agents/scripts/Measure-Tokens.ps1`
     on the read ranges.
   - Brief size: the later dispatch's brief, sized with `Measure-Tokens.ps1`
     from the `Temp/` copy.
   No new tracked script. Every query output stays in `Temp/`.
4. Measure the value of separation, from handoffs only:
   - What `/update-affected-code` changed that the slice `implementer` missed:
     its `Changed files` rows, each marked as a correctness site the slice did
     not update or not.
   - What `/update-claude-docs` changed: its `Changed files` rows.
   - What the Plan review step writer changed: its `Changed files` rows.
5. Decide per candidate. The author's recommended rule: recommend keeping the
   steps separate when any sampled run shows the later step finding a
   correctness site the earlier step missed. Recommend a merge when no sampled
   run does and the re-read overlap is at least a quarter of the total tokens
   of all file content the later step read, sized with `Measure-Tokens.ps1` on
   that step's read ranges from step 3; both are one-time quantities. Otherwise
   recommend keeping. The `Get-TranscriptUsage.ps1` totals are reported as each
   dispatch's cost but are not this threshold's denominator: they sum every
   turn, so cache reads repeat. Rationale: a missed correctness site costs more
   than the tokens a merge saves.
6. Check each merge against existing constraints and record the result. Root
   `AGENTS.md` `### Directives` says a finished subagent is never resumed. The
   Plan review step's reviews sit between preparation and that step's writer,
   and the Run targeted pre-review checks and Review and resolve correctness
   steps sit between the Implement and propagate step and the Apply the
   triggered cleanup step. A merge that would need a worker to
   outlive one of those gates, or that changes step order, is recorded as such.
   It names the directive or step order it would change. It is not recommended
   silently.
7. Write the findings record
   `Documents/Investigations/ChangeWorkflow/MergingImplementerSteps.md` with
   these sections: `## Sample` (each run's landing commit short SHA, subject,
   and whether it touched C++); `## Candidate A` and `## Candidate B` (each with
   a per-run cost table, the observed value of separation, the constraint check
   from step 6, a merge-or-keep recommendation, and for a merge the exact
   sections of `change-workflow.md`, `.agents/skills/next-plan/SKILL.md`, and
   the skills a later change would edit); `## Codex and OpenCode` (whether each
   recommendation holds there); `## Limits` (sample size and any step not run).
   The record holds no transcript path, transcript text, session ID, or home
   path.

The user decides afterwards whether to change the workflow. This Plan does not
edit it.

## Critical files
- `.agents/references/change-workflow.md` — `### Steps`: the Prepare and
  explore alternatives, Plan review, Implement and propagate, and Apply the
  triggered cleanup steps (read only)
- `.agents/skills/next-plan/SKILL.md` — `## Steps` steps 4, 5, and 8 (read only)
- `.agents/skills/next-plan-review/scripts/Find-AgentSessionTranscript.ps1`
  and `Get-TranscriptUsage.ps1` (run only)
- `.agents/skills/next-plan-review/references/worker.md` — steps 2, 3, 5, and
  8 (read only)
- `.agents/scripts/Measure-Tokens.ps1` (run only)
- `Documents/Investigations/ChangeWorkflow/MergingImplementerSteps.md` — new

## In scope
- The sample selection, dispatch listing, cost and value measurement, and
  decision steps in `## Design`
- One-off queries whose output goes only to `Temp/`
- The new findings record
  `Documents/Investigations/ChangeWorkflow/MergingImplementerSteps.md`

## Out of scope
- Any edit to `change-workflow.md`, any skill, or any agent definition under
  `.claude/agents/`, `.opencode/agent/`, or `.codex/agents/`
- Replays, evals, or throwaway worktrees that re-run old Plans
- Reviewer steps and their dispatches
- Codex and OpenCode transcripts (not measurable with these scripts)
- Any new tracked script

## Risk tier and invariants
Expected Tier 1: one new documentation file and no behavior change. The
executing session classifies. Invariants:
- No model reads a transcript whole; only script and query output enters a
  worker's context.
- Every number in the record traces to a `Get-TranscriptUsage.ps1`,
  `Measure-Tokens.ps1`, or query output under `Temp/`.
- No transcript path, transcript text, session ID, or home path in the record.

## Acceptance criteria
- The findings record exists with the sections step 7 of `## Design` lists.
- Each candidate has a cost row for every sampled run in which its steps ran,
  and a recommendation that follows the rule in step 5.
- Each merge recommendation names the exact sections a later change would
  edit and states the step 6 constraint result.
- `## Codex and OpenCode` states whether each recommendation holds there.
- The diff against the baseline adds only the findings record.
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing.
