<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-08T13:08:42.822Z","dependsOn":[]} -->
# Add Optional Observed Transcript Usage Summary

## Context

`/next-plan-review` judges main-session token efficiency from character sizes: `.agents/skills/next-plan-review/references/measurement.md` `## Measure main-session token efficiency` measures each context entry's chars through `.agents/skills/next-plan-checkpoint-review/scripts/Get-TranscriptProjection.ps1` and states that a subagent's own context is out of scope. The checkpoint envelope from `.agents/skills/next-plan-checkpoint-review/scripts/Measure-SessionContext.ps1` measures oversized tool results in characters. No repository script reads the usage counters Claude records on each assistant transcript record (`message.usage`); a search of `.agents/` and `Documents/Plans/` for `input_tokens`, `output_tokens`, `cache_read`, and `cache_creation` finds none. So the question "did moving work out of the main context reduce total observed model activity, or only move it into child transcripts?" cannot be answered today.

The idea comes from a static evaluation of `glitchwerks/claude-prospector` at commit `2e03a07e5fa997c6f1f684b9f571005e58ee7d15` (`src/claude_prospector/parser.py`, message-ID deduplication of the four usage fields). It is inspiration only: do not install, run, or copy it.

Local Claude transcripts observed on 2026-10-08 differ from the upstream parser's assumption that repeated fragments carry the final usage snapshot. Assistant records sharing one `message.id` carried equal `message.model`, `input_tokens`, `cache_read_input_tokens`, and `cache_creation_input_tokens`, but `output_tokens` differed across fragments in most multi-fragment groups (33 of 33, 72 of 73, and 0 of 67 in three large transcripts), and never decreased in file order. Keeping the first fragment would undercount output; requiring all four counters to be equal would mark most groups unresolved. Child transcripts live in separate files under `<sessionId>/subagents/` and their records carry `isSidechain: true`; main transcripts sampled carried no sidechain records.

These values are observed transcript usage only. They are not billed cost, not main-context size, and not elapsed time.

## Design

The author recommends one read-only PowerShell helper, `.agents/skills/next-plan-review/scripts/Get-TranscriptUsage.ps1`, taking exactly one `-TranscriptPath` to a Claude JSONL transcript and writing one compact JSON object.

Supported envelope: a top-level object with `type = "assistant"` and an object at `message`. Read `message.id` (nonempty string), `message.model` (nonempty string), and `message.usage.input_tokens`, `output_tokens`, `cache_read_input_tokens`, and `cache_creation_input_tokens`, each a non-negative JSON integer; a missing field, string, fraction, or negative value is invalid. Do not filter on `isSidechain`, since child transcripts mark every record sidechain. Other parsed object records are ignored.

Group supported envelopes by `message.id` within the one file. A group resolves when every fragment is valid and all fragments share the same model and the same input, cache-read, and cache-creation counters; it contributes one message with those three counters and the maximum `output_tokens` across its fragments, because output is the only counter observed to grow across fragments. Any invalid field or a differing model or input-side counter leaves the group unresolved, contributing nothing. A supported envelope without a usable `message.id` is unresolved on its own. Identical IDs in separate files are never merged.

Output properties, in order: `status`, `reason`, `assistantFragmentCount`, `uniqueMessageCount`, `duplicateFragmentCount`, `unresolvedMessageCount`, `malformedLineCount`, `models`. A nonblank line is malformed when it fails JSON parsing, parses to a non-object, or has `type = "assistant"` without an object `message`. Each `models` row carries `model`, `messageCount`, `inputTokens`, `outputTokens`, `cacheReadInputTokens`, and `cacheCreationInputTokens` from resolved groups only, sorted by ordinal model name. `status` is `complete` when supported envelopes exist and the unresolved and malformed counts are zero, `partial` when supported envelopes exist and either is nonzero, and `unsupported` otherwise; `reason` is null except for `unsupported`, where it is `no-supported-envelope` or `read-failed`, with zero counts (except an observed `malformedLineCount`) and an empty `models` array. Output never carries message text, paths, or other payload.

`/next-plan-review` gains an optional input, the user's explicit request for usage analysis. Only then, after Step 6 has proved the parent and fixed the child inventory, the worker runs the helper once per proven Claude transcript (the parent and each Claude child row Step 6 inventoried) and reports one row per transcript and model, or one model-less row per `unsupported` transcript, in an optional `Observed usage` handoff field. The finder never returns Claude child locators, so each child's path is derived as `<directory containing the proven parent transcript>/<parent session id>/subagents/agent-<agentId>.jsonl`, where `<agentId>` is the agent ID the parent's delegation event recorded; the directory is never globbed, and a child whose file is absent gets an `unsupported` / `read-failed` row. The field labels each row with the session ID and status, keeps child rows separate from the parent row, labels `partial` rows as partial, and states that the counters are observed transcript usage, distinct from main-context chars, elapsed time, and billed cost. The helper is never run on a Codex transcript; the worker writes an `unsupported` row for it without invoking the helper. Without the request, nothing runs and the field is absent.

## Critical files

- `.agents/skills/next-plan-review/scripts/Get-TranscriptUsage.ps1` — new helper.
- `.agents/skills/next-plan-review/SKILL.md` — `## Inputs` optional request and `## Handoff` optional `Observed usage` field.
- `.agents/skills/next-plan-review/references/worker.md` — the conditional invocation after Step 6.
- `.agents/skills/next-plan-review/references/measurement.md` — a short section on reading and reporting observed usage.

## In scope

- Create `Get-TranscriptUsage.ps1` with the envelope, grouping, output, and status rules in Design.
- Add the optional usage-analysis input and the optional `Observed usage` handoff field to `SKILL.md`, keeping the handoff under its 16,000-character cap.
- Add the conditional per-transcript invocation to `references/worker.md` after provenance and inventory are fixed.
- Add the interpretation rules (separate named quantities, partial labeling, per-transcript rows) to `references/measurement.md`.

## Out of scope

- `/next-plan-checkpoint-review`, its scripts, and the `/next-plan` checkpoint.
- Transcript discovery beyond the existing finder and the one derived child path in Design (including any directory globbing), hooks, persisted history, dashboards, billing or price tables, dollar estimates, scores, budgets, rankings, automatic findings, and any mandatory invocation.
- Changing the existing six main-context checks or concern 4's char-based measurement.
- Codex or OpenCode usage parsing.
- Python, copied upstream code, and unit tests.

## Risk triggers and invariants

- Change Workflow Tier 2, trigger: one subsystem's tool behavior (one review skill's optional script and measurement text); no determinism, wire, serialization, threading, scheduler, or trust-boundary surface changes.
- Transcripts remain untrusted data: the helper parses JSON only and never executes, opens, or echoes their content.
- The existing finder and Step 6 proof stay the only authority for which transcripts are read; the Step 6 child inventory alone selects which child paths are derived.
- An ordinary retrospective performs no added work.

## Acceptance criteria

- A scratch transcript with one valid assistant envelope returns `complete`, one unique message, and exact per-model counters.
- Fragments sharing one ID with equal model and input-side counters and growing `output_tokens` count once, report the maximum output, and raise `duplicateFragmentCount`; two IDs with identical snapshots count twice.
- A differing model or input-side counter, an invalid counter, or a missing ID produces `partial` and excludes the affected group from totals; a malformed line beside a valid envelope produces `partial`.
- A file with no supported envelope returns `unsupported` / `no-supported-envelope`; an unreadable path returns `unsupported` / `read-failed`.
- Running the helper on a real Claude parent transcript and on one of its `subagents/` transcripts returns `complete` or `partial` with nonzero totals for each (the child is not dropped as sidechain).
- An authorized `/next-plan-review` run with usage analysis requested reports per-transcript rows tied to proven session IDs, reads each inventoried Claude child from its derived `subagents/agent-<agentId>.jsonl` path, and writes Codex rows `unsupported` without invoking the helper; a run without the request does not invoke the helper.
- No output or documentation calls the counters billing, cost, context size, or proof of inefficiency.

## Coordination

No current executable Plan owns this measurement boundary.

## Notes

Re-derived from the draft added in 890cf103 and deleted without a stated reason in 90733255. Against that draft, this Plan changes the grouping rule (maximum `output_tokens` instead of requiring equal counters, per observed transcripts), states that sidechain records are counted, drops the `invalid-utf8` and signed 64-bit `counter-overflow` reasons as speculative hardening, and names the handoff field. The draft's note citing `.agents/scripts/Invoke-Jev.ps1` as the only local token counter no longer holds: that script is absent at baseline.
