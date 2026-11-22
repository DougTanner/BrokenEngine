<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-22T17:50:02.519Z","dependsOn":[]} -->
# Align checkpoint transcript error and projection failure routing

## Context

During `/external-skill-creator` validation of
`.agents/skills/next-plan-checkpoint-review`, a pre-existing contradiction was
observed between checkpoint measurement-state routing and
`Get-TranscriptProjection.ps1` error handling:

1. **Measurement-state routing assumes friction and isolation can continue on
   transcript parse and read errors**:
   `.agents/skills/next-plan/references/run-checkpoint.md:27,29,35-39`
   (`## Measurement states`) and
   `.agents/skills/next-plan-checkpoint-review/references/worker.md:19-22`
   (Step 1) return whole-review `BLOCKED` only when the transcript is
   unresolvable, unreadable, or `Measure-SessionContext.ps1` returns
   `transcript.not-found`. Every other blocked or error envelope from
   `Measure-SessionContext.ps1` — including `transcript.malformed-line`
   (`Measure-SessionContext.ps1:130,157,168`), `transcript.unsafe-path`
   (`:93,117`), and `transcript.read-failed` (`:94,118`) — is routed to
   `skipped (<code>)` so the reviewer skips only the context-efficiency lens
   and continues the friction and isolation lenses.
2. **Projection fails fast without emitting `checkpoint-span`**:
   `.agents/skills/next-plan-checkpoint-review/references/worker.md:48-53`
   (Step 6) requires `checkpoint-span` from `Get-TranscriptProjection.ps1`
   before applying any lens, and Step 12 (`:94-99`) requires selecting records
   only through `Get-TranscriptProjection.ps1`. However,
   `Get-TranscriptProjection.ps1:753-764,820-822,909-914` throws on missing,
   unsafe, or unreadable paths (`:757-761`) and on malformed JSONL lines (via
   `Test-AgentAgyTranscript` at
   `.agents/scripts/AgentTranscriptCommon.psm1:360-364`,
   `Read-AgentAgyTranscript` at `:395-402`, or `ConvertFrom-Json` at
   `Get-TranscriptProjection.ps1:822`), catching at `:911-914` to exit `1`
   without emitting `checkpoint-span`. Neither `worker.md` nor
   `next-plan-checkpoint-review/SKILL.md` defines a `BLOCKED` route when
   projection fails.

Observed reproduction: passing
`.agents/skills/next-plan-checkpoint-review/references/worker.md` as malformed
JSONL input to both scripts makes `Measure-SessionContext.ps1` exit `1` with
`code: 'transcript.malformed-line'` (which `run-checkpoint.md` routes to
continue friction and isolation) while `Get-TranscriptProjection.ps1` exits `1`
without emitting `checkpoint-span`.

This defect is pre-existing (present in both `run-checkpoint.md` and
`Get-TranscriptProjection.ps1` before the checkpoint correlation script work)
and outside the boundary of moving correlation logic to scripts.

## Design

Recommended approach: keep transcript parsing fail-closed and route every
transcript-unusable measurement error and any `Get-TranscriptProjection.ps1`
failure to whole-review `BLOCKED`, aligning the measurement-state table,
`next-plan-checkpoint-review/SKILL.md`, and `worker.md`.

Rationale: skipping malformed lines inside `Get-TranscriptProjection.ps1` to
salvage partial spans would risk dropping a checkpoint dispatch, tool call, or
delivery record, and in `agy` transcripts (`AgentTranscriptCommon.psm1:386,
434,440`) `Read-AgentAgyTranscript` pairs `PLANNER_RESPONSE` tool calls to
`GENERIC` results through a FIFO queue that a skipped line would desynchronize.
Routing transcript-level errors and projection failures to whole-review
`BLOCKED` keeps one consistent fail-closed invariant across measurement and
projection.

Recommended changes:

1. **Align `.agents/skills/next-plan/references/run-checkpoint.md`
   (`## Measurement states`, `:18-41`)**:
   - Expand the whole-review `BLOCKED` row (`:29`) and explanatory prose
     (`:35-39`) so that transcript-unusable measurement errors
     (`transcript.not-found`, `transcript.malformed-line`,
     `transcript.unsafe-path`, `transcript.read-failed`) and a
     `Get-TranscriptProjection.ps1` failure (non-zero exit or absent
     `checkpoint-span`) return `none (BLOCKED handoff)` with no summary block,
     recording `blocked (transcript-unavailable)` (or `blocked (<code>)` if
     preferred) on both `Friction follow-ups:` and
     `Context-efficiency follow-ups:`.
   - Keep genuine context-efficiency-only skips (`breachRowsTruncated: true`
     and non-transcript measurement errors such as `measure.failed` when
     projection still succeeds) continuing the friction and isolation lenses.
2. **Align `.agents/skills/next-plan-checkpoint-review/SKILL.md` (`:37-42`,
   `:82-97`) and `references/worker.md` (Steps 1, 3, 6, 12)**:
   - Update `worker.md` Step 1 (`:17-22`) to return whole-review `BLOCKED` on
     those transcript-unusable measurement codes instead of skipping only
     context-efficiency, and ensure Step 3 (`:31-36`) applies only to states
     that actually skip only context-efficiency.
   - Update `worker.md` Steps 6 (`:48-53`) and 12 (`:94-99`) to return
     whole-review `BLOCKED` naming the projection failure when
     `Get-TranscriptProjection.ps1` exits non-zero or emits no
     `checkpoint-span` row.
   - Keep `next-plan-checkpoint-review/SKILL.md` `## Inputs` (`:37-42`) and
     `## Handoff` (`:82-97`) consistent with the updated `BLOCKED` condition.
3. **Keep `Get-TranscriptProjection.ps1` fail-closed and path-safe
   (`:753-764`, `:911-914`)**:
   - Preserve fail-closed exit `1` without emitting a synthetic
     `checkpoint-span` on malformed or unreadable transcripts, and ensure the
     `catch` block (`:911-914`) redacts any transcript path through
     `Protect-AgentTranscriptPath` before writing the error so a reviewer's
     `BLOCKED` `Residuals` row never inherits a raw transcript path.

Change Workflow tier: Tier 2 — trigger: scoped behavior of one tooling
subsystem (checkpoint review transcript error and projection failure routing
across `.agents/skills/next-plan/references/run-checkpoint.md`,
`.agents/skills/next-plan-checkpoint-review/SKILL.md`,
`.agents/skills/next-plan-checkpoint-review/references/worker.md`, and
`.agents/skills/next-plan-checkpoint-review/scripts/Get-TranscriptProjection.ps1`).
No determinism/CRC, wire, serialization, `.pack`/`kiVersion`, replay, affinity,
threading, allocation, shader, build, trust-boundary, or live `/agent-harness`
exposure.

## Critical files

- `.agents/skills/next-plan/references/run-checkpoint.md` —
  `## Measurement states` (`:18-47`).
- `.agents/skills/next-plan-checkpoint-review/SKILL.md` — `## Inputs`
  (`:37-42`) and `## Handoff` (`:82-97`).
- `.agents/skills/next-plan-checkpoint-review/references/worker.md` — Steps 1,
  3, 6, and 12 (`:10-23`, `:31-36`, `:48-60`, `:94-113`).
- `.agents/skills/next-plan-checkpoint-review/scripts/Get-TranscriptProjection.ps1`
  — error handling and `checkpoint-span` contract (`:753-764`, `:820-822`,
  `:909-914`).
- `.agents/skills/next-plan-checkpoint-review/scripts/Measure-SessionContext.ps1`
  — error codes emitted (`:92-94`, `:116-118`, `:127-133`, `:155-160`,
  `:167-168`, `:248`) (read-only reference unless a code contract needs
  alignment).
- `.agents/skills/next-plan-review/references/measurement.md:12` — existing
  caller of `Get-TranscriptProjection.ps1` to keep compatible.

## In scope

- `.agents/skills/next-plan/references/run-checkpoint.md`: align
  `## Measurement states` so transcript-unusable measurement errors and
  projection failures map cleanly to whole-review `BLOCKED`.
- `.agents/skills/next-plan-checkpoint-review/SKILL.md` and
  `references/worker.md`: align `BLOCKED` routing in `## Inputs`, `## Handoff`,
  and Steps 1, 3, 6, and 12 with `run-checkpoint.md` and add the explicit
  `BLOCKED` branch when `Get-TranscriptProjection.ps1` exits non-zero or emits
  no `checkpoint-span` row.
- `.agents/skills/next-plan-checkpoint-review/scripts/Get-TranscriptProjection.ps1`:
  preserve fail-closed exit `1` without `checkpoint-span` on unreadable or
  malformed transcripts and ensure error output redacts transcript paths via
  `Protect-AgentTranscriptPath`.

## Out of scope

- Changing `Measure-SessionContext.ps1` envelope schemas
  (`broken-engine-context-efficiency*/v1`) or adding partial-line recovery in
  `AgentTranscriptCommon.psm1`.
- Changing valid-transcript row shapes of `Get-TranscriptProjection.ps1`
  consumed by `.agents/skills/next-plan-review/references/measurement.md:12`.
- Any C++, shader, Visual Studio project, or `WorktreeCli` file.

## Acceptance criteria

- Passing a malformed JSONL file (such as `worker.md` or a disposable `Temp/`
  fixture containing invalid JSON) to both `Measure-SessionContext.ps1` and
  `Get-TranscriptProjection.ps1` produces a `transcript.malformed-line` error
  envelope from measurement and a path-redacted exit `1` without
  `checkpoint-span` from projection, and both outcomes map unambiguously to
  whole-review `BLOCKED` in `run-checkpoint.md`,
  `next-plan-checkpoint-review/SKILL.md`, and `worker.md`.
- Every error code `Measure-SessionContext.ps1` can emit (`transcript.not-found`,
  `transcript.malformed-line`, `transcript.unsafe-path`,
  `transcript.read-failed`, `measure.failed`, `input.*`) and a
  `Get-TranscriptProjection.ps1` failure (non-zero exit or missing
  `checkpoint-span`) has one consistent, non-contradictory route across
  `run-checkpoint.md` `## Measurement states`,
  `next-plan-checkpoint-review/SKILL.md`, and `worker.md`.
- Valid Claude and `agy` transcripts continue to project unchanged with exit
  `0` and a trailing `checkpoint-span` row.
- `pwsh -NoProfile -File .agents/skills/external-skill-creator/scripts/Validate-Skill.ps1 -Path .agents/skills/next-plan`
  and `-Path .agents/skills/next-plan-checkpoint-review` both exit `0`
  (`VALID`), and `Invoke-StaticChecks.ps1` passes with zero failures.

## Notes

- Keep any disposable malformed-JSON test fixtures under `Temp/` and remove
  them before completing the change; never commit transcript paths or
  transcript text.
- Live `/agent-harness` verification does not apply.
