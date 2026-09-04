# Main-Context, Control-Work, and Routing Measurement

Read this reference when assessing main-session token efficiency, control-work
share, and execution-model routing for [`/next-plan-review`](../SKILL.md).

## Contents

- [Measure main-session token efficiency](#measure-main-session-token-efficiency)
- [Measure control-work share](#measure-control-work-share)
- [Verify execution-model routing](#verify-execution-model-routing)

## Measure main-session token efficiency

The main session runs the most expensive model, so concern 4 asks what entered
its context and what each entry bought; a subagent's own context is out of
scope. Measure before judging. For a Claude parent transcript, run once
`pwsh -NoProfile -File .agents/skills/next-plan-checkpoint-review/scripts/Get-TranscriptProjection.ps1 -TranscriptPath <parent transcript path>`,
whose header comment states the row shapes: its rows select which records to
open, and the `len` column only orders that selection. Measure a finding's
chars from the opened record's own content, and a brief's from its dispatch
record. For any other transcript, measure from the record sizes it exposes, and
report `unverified` when it exposes none.

Then judge each check below. A finding names the emitting artifact — the
skill, reference, script, or role — the measured chars, and the concrete
replacement; without all three it is not a finding.

1. Script-able instruction: instruction prose main loaded that tells it to
   perform a deterministic procedure — a fixed sequence of lookups, checks,
   formatting, or file mechanics needing no judgment — that a bundled script
   could run and return as a bounded result. Name the instruction span and the
   script that would replace it, existing or new.
2. Public skill surface: a `SKILL.md` main loaded that carried more than a
   parent needs to decide on and dispatch the skill — steps, rules, or script
   mechanics that belong in `references/worker.md` — or that lacked a decision
   input main then read a reference to get. Name the lines to move or the
   missing input.
3. Brief assembly: for each delegation event, the reads, script runs, and turns
   main spent solely to fill the brief, and the brief's length against the
   skill's `## Inputs`. Flag an input the worker could derive from a path or
   script it is given, content pasted where a path plus selector would do, and
   an `## Inputs` list that makes main gather what the worker could gather.
4. Handoff volume: for each handoff main received, its chars against the shared
   limits in `../../../references/subagent-reporting.md`, and any material main
   did not act on — evidence pasted inline, a restated brief, narrated
   reasoning, or bulk data meant for a later worker. Name the path plus
   selector form, and for bulk data the file, that should have carried it.
5. Direct main work: a tool result main read itself that a worker's brief could
   have named instead, and any single result over the 20,000-character
   per-result budget — raw logs, images, screenshots, captures, and other
   binary or base64 payloads included. Name the role that should have consumed
   it, or the bounded form.
6. Repeats: the same instruction body, reference, or handoff entering main more
   than once, and a re-read of content main already held.

Not findings: content the user pasted or asked to display; a Plan body,
execution card, or user-facing text main itself must approve or present; a
handoff within the shared limits whose every row main acted on; and a narrow
change having no unnecessary subagents.

## Measure control-work share

Classify each transcript-observable active interval exactly once as
`control work`, `actual work`, or `unattributed`. Control work is work whose
immediate object is a workflow-control artifact: creating, reading, reconciling,
validating, explaining, or coordinating execution cards, claims, locks, the
landing summary/confirmation, or workflow routing — dispatch, claim, lock, and
landing routing only — including workflow-control artifact classes that existed
at the reviewed commit but have since been removed from the workflow. Actual
work is engineering or repository work directly delivering or validating the
governing objective: investigation, implementation, propagation, debugging,
build/harness setup or result analysis, and substantive review/testing.
Engineering planning or coordination whose immediate object is delivery or
validation of the governing objective is actual work, and routing counts as control
work only when its immediate object is workflow control rather than task
delivery. Split an evidenced mixed interval; otherwise classify it as
`unattributed`.

Measure non-overlapping active intervals within each agent and sum those
per-agent intervals as active agent-time; it is not wall-clock time. Exclude
explicit user/external pauses and passive waits from active agent-time, while
retaining the overall timing disclosure required by `## Reconstruct and assess`
in [`SKILL.md`](../SKILL.md). Use only cited timestamps or
event ranges, tool runtimes, and Git/tool evidence; never infer private
reasoning. For each category, report time, share, coverage, confidence, and a
range when evidence is sparse.

Let `T = control work + actual work + unattributed` observed active agent-time.
Category shares use `T`; coverage is `(control work + actual work) / T`. For a
category-ambiguous interval, the lower control-work bound assigns all of it away
from control work and the upper bound assigns all of it to control work. Show an
exact point control-work share only when the evidence supports it. When `T = 0`,
report the measure as `unverified`, not a division.

The control decision is independent of the time category: separately label each
control as `required safety/control`, `candidate removable`, or `unverified`.
Required control work remains control work, but is not automatically waste.
Preserve the unchanged-input/non-firing safeguards: a removal or consolidation
recommendation requires measured cost, frequency, unique signal, and its safety
tradeoff; one quiet run is not removal evidence.

Assess core-delegation compliance with concrete evidence: manager-only core
activity; one manager with a single level of workers below it; one scoped
worker per concern; prohibited duplicate search, restatement, or consensus
work; artifact-path-plus-selector evidence forwarding rather than raw
forwarding; and capsule/resume recovery rather than repetition of completed
work. Mandatory fresh review, independent verification, and required disjoint
fan-out are legitimate independent work,
not duplicate effort. A compliance finding cites the delegation record,
session ID and timestamp or event/line location, artifact selector, or concrete
repeated operation.

## Verify execution-model routing

For every routing-inventory row, classify the actual assigned task before
considering its role label: `planning/design`, `review/audit`,
`implementation/propagation/documentation`, `judgment-heavy research`,
`locate/build/mechanical`, or `unable to classify`. Map that concern to the
appropriate workflow role, then evaluate the requested, configured, and actual
model and effort against the commit-time root `AGENTS.md` mapping and fallback,
not the requested role alone. When the assigned task is unable to classify, or
the governing mapping cannot be established, do not infer compliance.

Use only these allowed evidence chains from claim to conclusion. An ordinary
Claude child is compliant only with its parent delegation event, recorded
returned child relationship, and child-session execution metadata naming the
actual executor/model and effort. A headless `/codex-review` is compliant only
with its parent wrapper
invocation/result, the commit-time `.codex/codex-review.ps1` explicit model and
effort pins, and fixed structured output. A requested role, explicit requested
model/effort, or configured mapping proves intent only; when required model or
effort evidence cannot be proved, the verdict is `unverified`. Record the parent
event and child or headless route; relationship evidence; actual concern;
requested role/type and explicit model/effort; commit-time configured
model/effort mapping; actual executor/model/effort proof; fallback evidence;
verdict; exposed tokens/active time; and citation. Aggregate affected-child
counts and token/active-time cost only
where exposed; otherwise report cost as unavailable.

When host-owned child-session execution metadata is unavailable, an ordinary
Claude child may instead be proved by its parent delegation event, recorded
returned child relationship, and the `Executor` line of its returned handoff
(`../../../references/subagent-reporting.md`), cited as self-reported. That line
carries a model and an effort, each independently proved by it or unproved, and
a self-reported value never substitutes for host metadata where host metadata is
present. The row is `compliant` only when both values are proved; a row whose
`Executor` line states a model but writes `unknown` for effort stays
`unverified`, with its citation recording the proved model half as
self-reported, and a missing or fully `unknown` `Executor` line leaves the row
plainly `unverified`.

Verdicts are `compliant`, `compliant fallback`, `violation`, `unverified`, or
`not-executed`. `not-executed` is a nonfinding only when the parent event/result
conclusively proves the dispatch failed before any executor started. A started
child later aborted or interrupted still needs normal actual-model proof and a
normal verdict. Every `violation` or `unverified` row is a cited finding. Prefer
a routing mechanism or evidence fix before reminder prose; no routing result is
automatically P0.
