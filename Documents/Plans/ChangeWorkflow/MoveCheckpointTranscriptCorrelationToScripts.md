<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-21T22:11:13.329Z","dependsOn":[]} -->
# Move checkpoint transcript availability and bridge correlation to scripts

## Context

Root `AGENTS.md` (`## Directives`, instruction changes) requires putting
deterministic procedures in scripts and leaving instruction prose for judgment.
During the activation of `/codex-bridge`, `/progressive-disclosure-review`
flagged two deterministic transcript procedures carried in
`.agents/skills/next-plan-checkpoint-review/references/worker.md` that were
outside that change's implementation boundary (which left
`.agents/scripts/AgentTranscriptCommon.psm1` and
`.agents/skills/next-plan-checkpoint-review/scripts/Get-TranscriptProjection.ps1`
untouched):

1. **Antigravity CLI (`agy`) transcript availability**:
   `.agents/scripts/AgentTranscriptCommon.psm1:290-317`
   (`Resolve-AgentAgyParentSession`) already leaves `RootSessionId` and
   `RootTranscriptPath` `$null` when `$cycleDetected` is `$true`, and returns
   `SessionId`, `ParentSessionId`, `RootSessionId`, `RootTranscriptPath`,
   `CycleDetected`, and `ReadErrors` separately without one availability
   verdict. As a result,
   `.agents/skills/next-plan-checkpoint-review/references/worker.md:13-16`
   instructs the reviewer in prose to compute availability from three fields
   (`RootTranscriptPath` non-null, `CycleDetected` false, and empty
   `ReadErrors`, else `BLOCKED`).
2. **Bridged launch/`-Wait`/delivery correlation and checkpoint boundaries**:
   `.agents/skills/next-plan-checkpoint-review/scripts/Get-TranscriptProjection.ps1:60-192`
   emits flat per-record rows (`use`, `brief-missing`, `match`, `result`,
   `assistant-text`, and string-content records), capping `use` summaries at
   160 characters (`:63`). It does not correlate `Invoke-Codex.ps1` launches
   (`-Skill`/`-Mode`) with follow-up `-Wait <runId>` calls by `runId`, does not
   match bridged delivery records (paired `result` row, Claude
   task-notification/queued-attachment `<tool-use-id>` tag, or `agy`
   `SYSTEM_MESSAGE` `delivery=` carrying a `completed` bridge receipt and
   `--- findings ---` separator while skipping background-task launch stubs),
   and does not identify `/next-plan-checkpoint-review` dispatch groups or
   checkpoint span boundaries. Consequently,
   `.agents/skills/next-plan-checkpoint-review/references/worker.md:48-63`
   (Step 6), `:70-77` (Step 9), and `:97-118` (Step 12) instruct the reviewer
   in prose to scan truncated shell rows, open raw JSONL lines to recover
   `-Skill`/`-Wait`, trace `<runId>` back to the launch call, group `-NoRetry`
   re-dispatches and bridged `Write`/`write_file` prompt calls, and apply the
   three-shape delivery selector to establish checkpoint boundaries and
   identify emitters.

Existing callers of these scripts must stay intact:
`.agents/skills/next-plan-review/scripts/Find-AgentSessionTranscript.ps1:608-613`
reads `ReadErrors`, `CycleDetected`, and `ParentSessionId` from
`Resolve-AgentAgyParentSession`;
`.agents/skills/next-plan/references/follow-up-provenance.md:24-26` reads
`.RootSessionId`; and
`.agents/skills/next-plan-review/references/measurement.md:12-16` runs
`Get-TranscriptProjection.ps1` and depends on its documented row shapes.

## Design

1. **Single availability result for `agy` parent-session resolution**:
   Recommended shape: add an explicit availability status field (for example
   `IsAvailable` or `Status` such as `available`/`unavailable` with a concise
   `Reason`) to the object returned by `Resolve-AgentAgyParentSession` in
   `.agents/scripts/AgentTranscriptCommon.psm1:310-317`, computed from the
   existing `RootTranscriptPath` and `ReadErrors` checks (which already account
   for `CycleDetected` leaving `RootTranscriptPath` unset), while preserving
   every existing property (`SessionId`, `ParentSessionId`, `RootSessionId`,
   `RootTranscriptPath`, `CycleDetected`, `ReadErrors`). Rationale: adding a
   computed status field on the existing return object avoids breaking
   `Find-AgentSessionTranscript.ps1:608-613` and `follow-up-provenance.md:24-26`
   while letting `worker.md` Step 1 check one result instead of three fields.
2. **Correlated bridge records and checkpoint boundaries in
   `Get-TranscriptProjection.ps1`**:
   Recommended shape: extend
   `.agents/skills/next-plan-checkpoint-review/scripts/Get-TranscriptProjection.ps1`
   so one streaming pass over Claude and `agy` transcripts emits compact,
   deterministic correlation rows alongside the existing row shapes (updating
   its header comment at `:1-24` as the single owner of row shapes):
   - Parse `Bash`/`PowerShell`/`run_command` tool calls invoking
     `Invoke-Codex.ps1` from the full unclipped command string (avoiding the
     160-character summary truncation), extracting `-Skill`, optional `-Mode`,
     and `-NoRetry` on launches and `-Wait <runId>` on waits, plus any
     immediately preceding `Write`/`write_file` prompt-file call whose path
     matches `-PromptFile`.
   - Extract `runId` from bridge receipt output on `result` rows and async
     delivery records (Claude task-notification/queued-attachment
     `<tool-use-id>` and `agy` `SYSTEM_MESSAGE` `delivery=`), correlating each
     `-Wait <runId>` call back to its launch `-Skill`/`-Mode`, and flagging
     delivery records that carry both a `completed` bridge receipt and the
     `--- findings ---` separator while ignoring background-task launch stubs.
   - Emit explicit checkpoint boundary rows identifying the current
     `/next-plan-checkpoint-review` dispatch group (native or bridged,
     including `-NoRetry` re-dispatches and bridged prompt write), any
     immediately preceding distinct checkpoint dispatch group, its correlated
     completed-handoff line to exclude, and the resulting `[start, end]` span.
   Rationale: keeping the existing `use`, `brief-missing`, `match`, `result`,
   `assistant-text`, and text-record rows unchanged preserves
   `.agents/skills/next-plan-review/references/measurement.md:12-16` while
   removing deterministic correlation and truncation-recovery steps from
   reviewer prose.
3. **Simplify `next-plan-checkpoint-review/references/worker.md`**:
   - Step 1 (`:13-16`): check the single availability result from
     `Resolve-AgentAgyParentSession` instead of combining `RootTranscriptPath`,
     `CycleDetected`, and `ReadErrors` in prose.
   - Steps 6, 9, and 12 (`:48-63`, `:70-77`, `:97-118`): consume the script's
     emitted checkpoint span boundaries, excluded completed-handoff line, and
     correlated bridge emitter/delivery rows instead of performing manual
     `runId`, tool-use ID, truncation-recovery, and three-shape delivery
     matching in prose.

Change Workflow tier: Tier 2 — trigger: scoped behavior of one tooling
subsystem (`.agents/scripts/AgentTranscriptCommon.psm1`,
`.agents/skills/next-plan-checkpoint-review/scripts/Get-TranscriptProjection.ps1`,
and `.agents/skills/next-plan-checkpoint-review/references/worker.md`). No
determinism/CRC, wire, serialization, `.pack`/`kiVersion`, replay, affinity,
threading, allocation, shader, build, trust-boundary, or live `/agent-harness`
exposure.

## Critical files

- `.agents/scripts/AgentTranscriptCommon.psm1` — `Resolve-AgentAgyParentSession`
  (`:209-318`).
- `.agents/skills/next-plan-checkpoint-review/scripts/Get-TranscriptProjection.ps1`
  — header contract (`:1-24`), `Write-ToolCallProjection` (`:60-121`), and the
  `agy`/Claude record loops (`:139-192`).
- `.agents/skills/next-plan-checkpoint-review/references/worker.md` — Steps 1,
  6, 9, and 12 (`:10-23`, `:48-63`, `:70-77`, `:97-118`).
- `.agents/skills/next-plan-review/scripts/Find-AgentSessionTranscript.ps1:608-613`
  and `.agents/skills/next-plan-review/references/measurement.md:12-16` —
  existing callers to keep compatible (read-only unless a contract change
  requires propagation).

## In scope

- `.agents/scripts/AgentTranscriptCommon.psm1`: add one deterministic
  availability result field to `Resolve-AgentAgyParentSession` while keeping
  all existing return properties unchanged.
- `.agents/skills/next-plan-checkpoint-review/scripts/Get-TranscriptProjection.ps1`:
  correlate bridged `Invoke-Codex.ps1` launches, `-Wait <runId>` calls, paired
  prompt writes, three-shape completed bridge deliveries (skipping launch
  stubs), and checkpoint span boundaries for both Claude and `agy` transcripts,
  and document the new row shapes in the script header.
- `.agents/skills/next-plan-checkpoint-review/references/worker.md`: replace
  the prose availability check in Step 1 and the prose correlation/boundary
  mechanics in Steps 6, 9, and 12 with references to the script outputs.

## Out of scope

- `.agents/scripts/Invoke-Codex.ps1` and `.agents/references/codex-bridge.md`
  (CLI, `$routes`, receipt schemas, or bridge reference prose).
- `.agents/skills/next-plan-checkpoint-review/scripts/Measure-SessionContext.ps1`
  envelope schema (`broken-engine-context-efficiency*/v1`).
- `.agents/scripts/Get-ChangeWorkflowSteps.ps1`, `.codex/agents/*`, or OpenCode
  transcript support.
- Changing existing row formats consumed by
  `.agents/skills/next-plan-review/references/measurement.md`.
- Any C++, shader, or Visual Studio project file.

## Acceptance criteria

- Probing `Resolve-AgentAgyParentSession` against disposable `Temp/` brain
  fixtures (root session, child session, parent cycle, missing root transcript,
  and read error) returns the single availability result as available only when
  a readable root transcript exists with no cycle or read errors, while
  preserving all existing fields consumed by `Find-AgentSessionTranscript.ps1`
  and `follow-up-provenance.md`.
- Probing `Get-TranscriptProjection.ps1` against disposable Claude and `agy`
  JSONL transcripts containing:
  - a bridged `Invoke-Codex.ps1` launch whose `-Skill` would fall past the
    160-character `use` summary cap,
  - a follow-up `Invoke-Codex.ps1 -Wait <runId>` call,
  - a background-task launch stub sharing the tool-use ID with a later
    `completed` + `--- findings ---` delivery record, and
  - two distinct `/next-plan-checkpoint-review` dispatches (including a
    `-NoRetry` re-dispatch and paired `Write`/`write_file` prompt call)
  emits correlated emitter rows, identifies the true completed delivery record
  while skipping the launch stub, and emits exact checkpoint span boundaries
  and excluded handoff line without altering existing `use`/`match`/`result`
  row shapes.
- `.agents/skills/next-plan-checkpoint-review/references/worker.md` no longer
  instructs the reviewer to combine `Resolve-AgentAgyParentSession` fields,
  scan truncated shell summaries for `Invoke-Codex.ps1`, trace `<runId>` in
  prose, or manually apply the three-shape delivery selector.
- `pwsh -NoProfile -File .agents/skills/external-skill-creator/scripts/Validate-Skill.ps1 -Path .agents/skills/next-plan-checkpoint-review`
  exits `0` (`VALID`), and `Invoke-StaticChecks.ps1` passes with zero failures.

## Notes

- Keep any disposable test transcripts under `Temp/` and remove them before
  completing the change; never commit transcript paths or transcript text.
- Live `/agent-harness` verification does not apply.
