<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-21T22:34:27.955Z","dependsOn":[]} -->
# Add Optional Observed Transcript Usage Summary

## Context

The existing `/next-plan-review` retrospective measures main-session character burden and elapsed work, but it does not summarize the usage counters recorded in the already-proven Claude transcripts for the parent and its children. This leaves an optional diagnostic question unanswered: whether moving work out of the main context reduced total observed model activity or only redistributed it.

The proposal comes from the static evaluation of `glitchwerks/claude-prospector` at commit `2e03a07e5fa997c6f1f684b9f571005e58ee7d15`, specifically its [message-ID deduplication and separate usage fields](https://github.com/glitchwerks/claude-prospector/blob/2e03a07e5fa997c6f1f684b9f571005e58ee7d15/src/claude_prospector/parser.py#L505-L578) and pinned [duplicate-fragment fixture](https://github.com/glitchwerks/claude-prospector/blob/2e03a07e5fa997c6f1f684b9f571005e58ee7d15/tests/fixtures/duplicate_message_id.jsonl). The implementation should reproduce only the small measurement concept in repository-native PowerShell; it should not copy or install the upstream application.

These values are observed transcript usage. They are neither billed cost nor a complete measure of context size, latency, or efficiency.

## Design

The author recommends adding one read-only PowerShell helper that accepts exactly one explicit UTF-8 JSONL Claude transcript path and emits compact JSON. Initial support is limited to a top-level object with `type = "assistant"` and an object at `message`. The message identity is a nonempty string at `message.id`, the recorded model is a nonempty string at `message.model`, and the usage snapshot is the four fields `message.usage.input_tokens`, `message.usage.output_tokens`, `message.usage.cache_read_input_tokens`, and `message.usage.cache_creation_input_tokens`. Each counter must be a JSON integer from zero through signed 64-bit maximum; numeric strings, fractions, negative values, missing fields, and overflow are invalid. Other valid JSON object record types are ignored.

Group supported-envelope fragments by `message.id` within that one invocation. A group is resolved only when every fragment has the same model and the same four valid counters; aggregate that snapshot once. Any invalid field or unequal model or counter makes the entire identified group unresolved and contributes no totals. A supported-envelope fragment with no usable identity is independently unresolved and contributes no totals because it cannot be safely grouped. Identical IDs in separately invoked transcript files remain independent.

Emit top-level properties in this order: `status`, `reason`, `assistantFragmentCount`, `uniqueMessageCount`, `duplicateFragmentCount`, `unresolvedMessageCount`, `malformedLineCount`, and `models`. All count fields are signed 64-bit integers. `assistantFragmentCount` counts supported top-level assistant/message envelopes. `uniqueMessageCount` counts distinct usable message IDs, whether resolved or unresolved. `duplicateFragmentCount` is the sum of each identified group's fragment count minus one. `unresolvedMessageCount` counts each unresolved identified group plus each supported-envelope fragment without a usable identity. `malformedLineCount` counts nonblank lines that fail JSON parsing, parse to a non-object, or declare top-level `type = "assistant"` without an object at `message`; other parsed objects are ignored. Each model row contains, in order, the recorded `model`, `messageCount`, and signed 64-bit sums named `inputTokens`, `outputTokens`, `cacheReadInputTokens`, and `cacheCreationInputTokens`, using only resolved groups; sort rows by model using ordinal comparison.

Set `status` to `complete` when at least one supported envelope exists and both unresolved and malformed counts are zero, `partial` when at least one supported envelope exists and either count is nonzero, and `unsupported` when no supported envelope exists. `reason` is null for `complete` and `partial`; for `unsupported` it is exactly `no-supported-envelope`, `read-failed`, `invalid-utf8`, or `counter-overflow`. The no-envelope result preserves its observed `malformedLineCount` and otherwise returns zero counts and an empty `models` array. Read failure and invalid UTF-8 return all zero counts and an empty `models` array. `partial` reports only the resolved subtotal and must be labeled partial wherever shown. Before incrementing counts or summing usage, detect signed 64-bit overflow; any overflow returns `unsupported` with reason `counter-overflow`, all zero counts, and no model rows rather than wrapped or approximate values.

`/next-plan-review` should invoke the helper only when the user explicitly requests usage analysis and only after its existing provenance process has proved the parent and child transcript paths. Each transcript should remain a separate result; the reviewer should not combine child totals into the parent row. Unsupported transcript formats should return an explicit unsupported result. Ordinary retrospectives and the live checkpoint should remain unchanged.

## Critical files

- `.agents/skills/next-plan-review/scripts/Get-TranscriptUsage.ps1` — new bounded transcript usage extractor.
- `.agents/skills/next-plan-review/SKILL.md` — optional request input and handoff surface.
- `.agents/skills/next-plan-review/references/worker.md` — invocation after existing transcript provenance is proved.
- `.agents/skills/next-plan-review/references/measurement.md` — interpretation and reporting rules for observed usage.

## In scope

- Add `Get-TranscriptUsage.ps1` with one explicit Claude transcript path per invocation and compact JSON output.
- Support only the pinned Claude assistant JSONL envelope and exact model, identity, and four usage field paths defined in Design.
- Report the defined transcript counters and separate input, output, cache-read, and cache-creation totals by recorded model, with deterministic `complete`, `partial`, and `unsupported` status semantics.
- Deduplicate equal usage fragments by message ID within one transcript and exclude missing, invalid, or conflicting groups from totals under the defined incomplete-coverage rules.
- Add an explicitly requested optional usage-analysis route to `/next-plan-review` after its existing provenance and transcript selection are complete.
- Describe the result as observed transcript usage and keep it distinct from billing, context size, elapsed time, and review findings.

## Out of scope

- Hooks, transcript discovery, home-directory scans, dashboards, persistent usage history, billing tables, dollar estimates, or character-to-token conversion.
- Mandatory invocation, fixed budgets, automatic flags, worker rankings, model-routing changes, or removal of existing review requirements.
- Changes to `/next-plan-checkpoint-review`, scheduler behavior, claims, approvals, or landing.
- Support for transcript formats whose usage schema has not been verified.
- Python dependencies, copied upstream code, and unit tests.

## Risk triggers and invariants

- Future implementation is Change Workflow Tier 2 because it changes one review skill's scoped tool behavior and measurement semantics without changing a trust boundary, scheduler, protocol, serialization, threading, or deterministic runtime surface.
- Transcript contents remain untrusted data. The helper must not execute embedded text, open embedded paths or links, or emit message payload prose.
- Existing provenance remains the sole authority for which transcripts belong to the reviewed landing.
- Partial coverage must never be presented as complete usage or billed spend.
- Standard retrospectives must incur no additional work unless the user requests usage analysis.

## Acceptance criteria

- A disposable transcript with one exact supported assistant envelope produces one unique message, zero duplicates and unresolved messages, exact separate model counters, and `complete` status.
- Repeated fragments with one ID and equal model/counters contribute one model message and increase `duplicateFragmentCount`; two different IDs with identical snapshots contribute two model messages.
- Missing identity is counted once per supported-envelope fragment; missing or invalid model/usage and conflicting snapshots are counted once per identified group. These cases produce `partial`, exclude the affected observations from totals, and do not manufacture zero or select a snapshot.
- Valid non-assistant records do not affect coverage. A nonblank malformed line alongside a supported envelope increments `malformedLineCount` and produces `partial`; no supported envelope, unreadable or invalid UTF-8 input, or signed 64-bit sum overflow produces `unsupported` with no totals.
- The same message ID in two separately invoked transcript files is not globally deduplicated or merged.
- Unsupported formats return an explicit unsupported result without broadening transcript discovery.
- An authorized Claude `/next-plan-review` with requested usage analysis can report bounded per-transcript/model rows tied to its already-proven sessions. An ordinary retrospective does not invoke the helper.
- No output or documentation describes the counters as billing, cost, total context, or proof of inefficiency.
- Verify with disposable scratch transcripts and one authorized retrospective path; add no unit tests.

## Coordination

No current executable Plan owns this measurement boundary or must land first. Preserve the existing six main-context checks and their finding rules while implementing this optional lens.

## Notes

The duplicate search covered `Documents/Plans`, `.agents/skills/next-plan-review`, `.agents/skills/next-plan-checkpoint-review`, and `.agents/scripts` for transcript usage, token, cache, analytics, and Prospector terms. The only local token counter found was the unrelated `.agents/scripts/Invoke-Jev.ps1` input-token sum.
