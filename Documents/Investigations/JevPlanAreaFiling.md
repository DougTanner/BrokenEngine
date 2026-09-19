# Jev as a second opinion on where a Plan is filed

Open question: can Jev check that a new Plan sits in the right
`Documents/Plans/<area>/` directory, and by extension whether a document belongs
in `Plans/`, `Features/`, or `Investigations/` at all? Part of the series in
`JevDecisionModelWorkflowUses.md`. A pilot has been run over every current Plan.

## The decision today

`Documents/Plans/AGENTS.md` `## Plan files` defines four areas — `Engine/`,
`Game/`, `Tools/`, `ChangeWorkflow/` — each by the paths it owns, and fixes the
rule: the area is decided by the files the plan's critical-files and `## In
scope` sections name, not by what motivated it; a plan spanning areas goes to
the area owning most of its named files, and a tie stays in `Engine/`.
`Documents/AGENTS.md` `## Planning Trees` fixes the tree choice with a
two-step test: not decision-complete goes to `Investigations/`; decision-complete
and a new capability goes to `Features/`; otherwise `Plans/`. Both decisions are
made by whoever authors the file — `/create-follow-up-plans`, `/save-plan`, or a
session writing a Plan by hand — and nothing checks them afterwards; a wrong area
does not break the scheduler, so it persists silently.

## The question to Jev

One `choice` per Plan over the four areas, with each area's definition from
`Plans/AGENTS.md` as its criterion and the filing rule quoted in the
instruction. State is the whole Plan file as one field. The same shape serves
the tree question: a `choice` over `Plans`, `Features`, `Investigations` with
the deciding test as criteria.

## What still needs a full model

The filing decision itself. The author of the document still chooses the area
and the tree, and a disagreement Jev reports is a residual a human reads
against the Plan's named files before anything moves. Jev only says which
filings are worth that read.

## Pilot

Corpus: all 58 current Plans; the label is the directory each sits in. Only
`Engine/` (45) and `Game/` (13) are populated today, so the pilot measures a
two-way split with two unused options present. 58 calls, about 1,700 to 4,000
input tokens each.

| result | count |
|---|---|
| agrees with the directory | 56 / 58 |
| disagrees | 2 / 58 |

Both disagreements chose `Game` for a Plan filed under `Engine/` and carried the
two lowest confidences in the run (0.52 and 0.35 on the second run; 0.56 and
0.44 on the first, so confidence moves a few hundredths between identical
runs). Only two agreeing answers sat below 0.6 (0.58 and 0.51); most were
above 0.8. Both misfiled Plans name files on both sides —
`HudPendingControlReset.md` lists three `Projects/` files and three `Engine/`
files, `ReplayResetCancelsPendingRecord.md` two `Projects/` and three
`Engine/` — so under the "most named files, tie to Engine" rule the second is
correctly `Engine/` and the first is a genuine tie. The two misses are the two
borderline cases, and Jev's confidence marked them as such.

What the numbers say. A confidence gate at 0.6 would have held back four Plans
for a human — both misses and two correct answers — and passed the other 54
with no error. The area rule is largely a count over paths, so a script could
compute the majority itself; Jev's contribution is reading the prose when the
named files are ambiguous or the sections are informal, which is exactly where
the two misses sat.

Caveats. Two of the four areas had no examples, so nothing is known about
`Tools/` or `ChangeWorkflow/` precision. The tree question (`Plans` versus
`Features` versus `Investigations`) was not piloted; the corpus for it is the
Investigations tree plus the Features tree, which is small.

## What would make this a Plan

Success: over the full Plan tree at the time of the Plan, plus every Investigation
and Feature document, the area answer agrees with the directory on at least 95%
of files at confidence 0.6 or higher, and every disagreement above that
confidence is a filing error a human confirms on reading. The wiring is a
check, not a decision: the author keeps choosing, and the check reports a
disagreement as a residual for the user.

## Decisions a Plan needs

1. Where the check runs: inside `/create-follow-up-plans` and `/save-plan` at
   authoring time, or as one pass in `Test-PlanSchedulerState.ps1`'s health
   check over the whole tree. The health check already walks every Plan and is
   run by `/next-plan`, so it is the smaller change, but it would make the
   scheduler health check depend on `.agents/scripts/Invoke-Jev.ps1` and so
   report the area pass as not run whenever that caller is `blocked`.
2. Whether to compute the path majority in code first and ask Jev only when the
   count is a tie or the sections name no files, which is the shape the
   TypeSafe docs recommend (filter in code, judge the remainder).
3. Whether the tree question is included at all, given its tiny corpus.
4. The confidence gate as a number in the skill's references, and that a
   disagreement is reported, never applied.
5. The shared decisions in `JevDecisionModelWorkflowUses.md`; the call itself
   is `.agents/scripts/Invoke-Jev.ps1` (that document's `## The caller`), so
   the Plan writes a request file and reads the result, never an HTTP call.
