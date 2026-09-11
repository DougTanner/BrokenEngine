# Main-Context, Control-Work, and Routing Checks

Read this reference when assessing main-session token efficiency, control-work
classification, and headless execution-model routing for
[`/next-plan-review`](../SKILL.md).

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
4. Handoff volume: for each handoff main received, its rows against the field
   rules in `../../../references/subagent-handoff.md`, and any material main
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
handoff within those field rules whose every row main acted on; and a narrow
change having no unnecessary subagents.

## Classify control work

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

Use only cited timestamps or event ranges, tool runtimes, and Git/tool evidence;
never infer private reasoning.

The control decision is independent of the interval classification above:
separately label each
control as `required safety/control`, `candidate removable`, or `unverified`.
Required control work remains control work, but is not automatically waste.

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

For every headless `/codex-review` attempt in the routing inventory, classify the
actual assigned task before considering its role label: `planning/design`,
`review/audit`, `implementation/propagation/documentation`,
`judgment-heavy research`, `locate/build/mechanical`, or `unable to classify`.
Map that concern to the appropriate workflow role, then evaluate the configured
and actual model and effort against the mapping in `AGENTS.md`,
`.agents/references/change-workflow.md`, and `.agents/references/risk-tiers.md`
as they existed at that commit, not the requested role alone. When the assigned
task is unable to classify, or the governing mapping cannot be established, do
not infer compliance.

Use only this allowed evidence chain from claim to conclusion: a headless
`/codex-review` is compliant only with its parent wrapper invocation/result, the
commit-time `.codex/agents/<agent>.toml` `model` and `model_reasoning_effort`
pins that `.codex/codex-review.ps1` applies, and fixed structured output.
A requested role, explicit requested model/effort, or configured mapping proves
intent only; when required model or effort evidence cannot be proved, the verdict
is `unverified`. Record the parent wrapper event, the actual concern, the
commit-time pinned model and effort with their proof, the verdict, and the
citation.

Verdicts are `compliant`, `violation`, `unverified`, or `not-executed`.
`not-executed` is a nonfinding only when the parent event/result conclusively
proves the dispatch failed before any executor started. A started attempt later
aborted or interrupted still needs that same pinned model and effort proof and a
normal verdict. Every `violation` or `unverified` row is a cited finding. Prefer
a routing mechanism or evidence fix before reminder prose; no routing result is
automatically `Critical`.
