# Jev as a pre-read on review findings

Open question: can one Jev request per review finding pre-fill the labels main
and `/resolve-findings` need — intent, scope, the YAGNI warning, and a
severity sanity check — so main reads each finding with those hints beside it?
Part of the series in `JevDecisionModelWorkflowUses.md`. Not piloted: findings
are consumed in session and not stored, so the corpus does not exist yet.

## The decision today

Main decides each finding once and is told to "be especially careful with
findings that add guards, options, or machinery for cases nobody has observed
(YAGNI and over-engineering)" (`.agents/references/change-workflow.md`,
Delegation roles). Every fix dispatch must carry intent `conformance` or
`plan_delta` and scope `non_structural` or `structural`
(`.agents/skills/resolve-findings/SKILL.md:32`), and the fixer accepts only
`conformance + non_structural`, returning `PLAN DELTA REQUIRED: yes` without
editing otherwise (`:41-47`). Reviewers assign severity themselves:
`Critical` for data loss, broken functionality, determinism failure, or an
equivalent contract breach, `Required` for everything else
(`.agents/skills/repo-code-review/SKILL.md`, `## Handoff`). Each finding is one
handoff row — ID, severity, `path:line`, claim, evidence
(`.agents/references/subagent-handoff.md:15`) — so no enumeration is needed.

## The question to Jev

One request per finding carrying four questions at once, the speculative
fan-out shape the TypeSafe docs recommend so no branch costs a second round
trip:

- `intent` — `choice` between `conformance` (the code fails to do what the
  approved plan says) and `plan_delta` (the finding asks for something the plan
  did not decide);
- `scope` — `choice` between `non_structural` and `structural`, with the
  `/resolve-findings` definitions as criteria;
- `speculative_guard` — `noul`: does the finding add a guard, option, or
  mechanism for a case no evidence in the finding shows occurring;
- `severity_matches` — `noul`: does the claimed severity match the severity
  rule quoted above.

State is a JSON object with the finding row, the plan's `## In scope` and
`## Out of scope` sections, and the cited code region. The `severity_matches`
question is a check on the reviewer, not a replacement: a `Critical` that Jev
reads as not matching is shown to main as "severity worth a second look".

## What still needs a full model

Accepting or rejecting the finding, which is where the judgment lives and stays
with main. The fixer's own refusal path already corrects a wrong intent or
scope label, which makes this the safest candidate in the series — and also the
lowest-volume one: a handful of findings per round.

## Why it is not first

Volume. At a few findings per review round the saving is a minute of main's
reading, and the corpus to validate against has to be created by recording
main's decision beside Jev's for many rounds. The value is not the saving but
the record: a log of `(finding, Jev labels, main's decision)` is the training
and evaluation set every other candidate in this series lacks, and the
`speculative_guard` answer is the one signal that could later be measured
against whether an accepted guard ever fired.

## What would make this a Plan

Success: over at least 50 recorded findings, the `intent` and `scope` answers
match the labels main put on the dispatch at least 90% of the time at
confidence 0.6 or above, and every `speculative_guard` probability over the
threshold marks a finding main also rejected or narrowed. The wiring shows the
four answers beside the finding in the review handoff main reads; it never
dispatches a fix on its own.

## Decisions a Plan needs

1. Where the record lives: a `Temp/` file per session is lost at cleanup, so a
   tracked log under `.agents/skills/resolve-findings/references/` or the
   landing commit body are the candidates.
2. Which side runs the call: the reviewer before returning its handoff (the
   finding and its cited region are already in that context) or main after
   receiving it.
3. Whether `severity_matches` is included, since it second-guesses a reviewer
   that already read the code.
4. The shared decisions in `JevDecisionModelWorkflowUses.md`; the call itself
   is `.agents/scripts/Invoke-Jev.ps1` (that document's `## The caller`), so
   the Plan writes a request file and reads the result, never an HTTP call.
