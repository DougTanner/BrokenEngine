# Jev for classifying transcript intervals in `/next-plan-review`

Open question: can Jev do the row-by-row classification of a session
transcript that `/next-plan-review` and `/next-plan-checkpoint-review` require
— control work against actual work, and the class of each oversized tool
result — so a summary table sits beside the transcript and orders what the
reviewer reads? Part of the series in `JevDecisionModelWorkflowUses.md`. Not
piloted.

## The decision today

`.agents/skills/next-plan-review/references/measurement.md` `## Classify
control work` tells the reviewer to "classify each transcript-observable
active interval exactly once as `control work`, `actual work`, or
`unattributed`", with definitions: control work has a workflow-control
artifact as its immediate object (execution cards, claims, locks, the landing
summary and confirmation, dispatch and landing routing); actual work delivers
or validates the governing objective (investigation, implementation,
propagation, debugging, build and harness work, substantive review). Each
control interval is then separately labelled `required safety/control`,
`candidate removable`, or `unverified`. `/next-plan-checkpoint-review` classes
every oversized tool result as `fixable-defect`, `necessary-evidence`, or
`active-change-blocker` (`references/worker.md:76-86`). A session transcript
has hundreds of intervals, each a small, self-contained judgment over a tool
call and its result — the map-reduce shape the TypeSafe docs name as a core
use.

## The question to Jev

One request per interval carrying the three questions at once: the
three-way interval `choice` with the two definitions above as criteria and
`unattributed` for a mixed or unclear interval; the three-way control label
`choice`, consumed only when the first answer is `control work`; and, for an
oversized result, the checkpoint review's three-way class. State is a JSON
object with the tool name, its arguments (truncated), the first lines of its
result, the preceding assistant text, and the running skill or role if the
transcript names it — never private reasoning, which the rule excludes anyway.
Code owns the intervals and timestamps, sums durations per class, and hands
the reviewer the totals plus the intervals Jev was least sure of.

## What still needs a full model

The findings: whether a removable control is really removable, and the
process improvements the review exists to produce. The rule already says to
"use only cited timestamps or event ranges, tool runtimes, and Git/tool
evidence", and Jev's answers are none of those; they order what the reviewer
cites.

## The corpus

Every past `/next-plan-review` handoff that cited intervals by class is a
label, and the transcripts are local. The first step is a script that extracts
intervals from a transcript file in the form above; that script is needed for
the wiring anyway.

## What would make this a Plan

Success: over at least 300 intervals from three reviewed sessions, the review's
own cited classification stays the evidence and Jev is measured against it —
Jev's interval class agrees with that cited classification on at least 90% of
the intervals the review cited, and Jev's summed control-work total per session
lands within 10% of the total those cited classifications give. The wiring
gives the reviewer the totals and the low-confidence intervals; the reviewer
still reads every interval it cites.

## Decisions a Plan needs

1. The interval extraction script: which transcript format each host writes,
   and what "active interval" means mechanically.
2. How much of a tool result goes into the state, given the context-rot
   warning and results that run to thousands of lines.
3. Whether the checkpoint review's oversized-result class is included, since
   it needs the bounding-mechanism fact the worker looks up.
4. The shared decisions in `JevDecisionModelWorkflowUses.md`; the call itself
   is `.agents/scripts/Invoke-Jev.ps1` (that document's `## The caller`), so
   the Plan writes a request file and reads the result, never an HTTP call.
