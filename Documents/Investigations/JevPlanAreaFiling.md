# Jev as a second opinion on where a Plan is filed

Open question: can Jev check that a new Plan sits in the right
`Documents/Plans/<area>/` directory, and by extension whether a document belongs
in `Plans/`, `Features/`, or `Investigations/` at all? Part of the series in
`JevDecisionModelWorkflowUses.md`. Two pilots have run over every current Plan;
the second, which also ran the tree question and a code-only count beside Jev,
is the one to trust, and it does not promote the candidate.

## The decision today

`Documents/Plans/AGENTS.md` `## Plan files` defines four areas — `Engine/`,
`Game/`, `Tools/`, `ChangeWorkflow/` — each by the paths it owns, and fixes the
rule: the area is decided by the files the plan's critical-files and `## In
scope` sections name, not by what motivated it; a plan spanning areas goes to
the area owning most of its named files, and a tie stays in `Engine/`.
`Documents/AGENTS.md` `## Planning Trees` fixes the tree choice: not wanted
soon goes to `Features/`; wanted soon and decision-complete goes to `Plans/`;
otherwise `Investigations/` (when the pilot ran, the test was instead
decision-complete or not, then capability or not). Both decisions are
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

## Second pilot: the full tree, the tree question, and a code-only count

Method: a scratch script over the 58 current Plans, one `choice` per Plan over
the four areas, run twice; the script was not kept, because the pilot did not
promote the candidate, and this section is what a rerun starts from. The state
is one field, `document`, holding the Plan text with its marker line removed;
the path never enters the state, because the directory is the label under
test. The instruction is "The state is the full text of one Plan from a game
engine repository. Plans live in area subdirectories, and the area is decided
by the files the plan's critical-files and `## In scope` sections name, not by
what motivated it; a plan spanning areas goes to the area owning most of its
named files, and a tie stays in Engine. Which area directory does this Plan
belong in?", and each option's criterion is that area's definition from
`Plans/AGENTS.md` `## Plan files` quoted as written. Beside it, a regex
collected every backticked path in the same two sections and mapped it to an
area by prefix. One sweep is 58 calls, 115k input tokens, under a minute,
about half a cent.

| | run A | run B |
|---|---|---|
| agrees with the directory | 56 / 58 | 57 / 58 |
| agrees at confidence 0.6 or higher | 51 / 52 | 52 / 53 |
| flagged at a 0.6 gate (disagreement or agreement below it) | 7 | 6 |
| flagged Plans that are misfiled under the rule | 0 | 0 |
| code-only count: majority matches the directory | 56 / 58 | |
| code-only count: tie, filed under `Engine/` as the rule says | 2 / 58 | |
| code-only count: names no file | 0 / 58 | |

Confidence moved by 0.01 on average and 0.09 at most between the runs, and
one answer changed: `TweaksSettingsBoolRepresentation.md` went from `Game` at
0.37 to `Engine` at 0.42, though it names four `Engine/` files and one
`Projects/` file. `HudPendingControlReset.md` chose `Game` in both runs, at
0.61 and 0.70, above the gate; it is the three-to-three tie the rule files
under `Engine/`, so it is not a filing error. The other flagged rows agree at
0.40 to 0.59, most of them `Game/` Plans that name one or two `Engine/` files.

The tree question ran once over all 105 documents in `Plans/`, `Features/`,
and `Investigations/`, a `choice` over the three trees with the deciding test
`Documents/AGENTS.md` `## Planning Trees` carried at the time as the
instruction — decision-complete or not, then capability or not — and each
tree's row from that table as its criterion. 93 agreed. All 12 disagreements
are `Features/` documents Jev called `Investigations`, nine of them at
confidence 0.6 or higher and `IslandFlowMaskUsage.md` at 0.99. A different
nine of the twelve are the deferred designs `Features/AGENTS.md` keeps on
purpose, the `Revisit When` documents, which that test sent to
`Investigations/` because they are not decision-complete; the other three
(`DestructionBuffer.md`, `WaterFoam.md`,
`ReplaceDirectXTKAudioWithMiniaudio.md`) carry no such line.
The pilot exposed that the test and the Features rule disagreed, and the user
then fixed the trees' meaning: Features holds ideas saved for another day,
decided or not, and Investigations holds work wanted soon that is not yet
complete enough for a Plan. Under that rule all 12 are filed correctly, and
the tree choice turns on when the user wants the work, which the document's
text does not carry, so it is not a question Jev can answer from the document
at all. No `Plans/` or `Investigations/` document was called anything but its
own tree.

What the numbers say. The area answer clears the agreement bar but fails the
other half of the success test: the one disagreement above the gate is a tie
the rule already decides, not a filing error, and every one of the six or
seven flags per run is a false flag, because no current Plan is misfiled. The
code-only count, which is the rule applied literally, reproduces the directory
for all 58 Plans, ties included, so on this corpus Jev's answer is strictly
worse than the answer a script already has, which is the first case under
`## Not suitable` in the series overview. The reading Jev was to add, prose
where the sections are informal or name no file, has no instance in the tree.

## What would make this a Plan

Nothing today. The candidate is not promoted: a filing check, if one is ever
wanted, is the path count with the tie rule, needing no model, and it would
report nothing on the current tree. The Jev question earns another look only
if Plans appear whose scope sections name no file; the tree question is
closed, because the tree is chosen by when the work is wanted, which is the
user's intent and not in the document. The success bar stands as before: at
least 95% agreement at confidence 0.6 or higher, with every disagreement above
the gate a filing error a human confirms; the wiring would be a check, not a
decision, with the author still choosing.

## Decisions a Plan needs

1. Where the check runs: inside `/create-follow-up-plans` and `/save-plan` at
   authoring time, or as one pass in `Test-PlanSchedulerState.ps1`'s health
   check over the whole tree. The health check already walks every Plan and is
   run by `/next-plan`, so it is the smaller change, but it would make the
   scheduler health check depend on `.agents/scripts/Invoke-Jev.ps1` and so
   report the area pass as not run whenever that caller is `blocked`. Both
   authoring skills write the file through `.agents/scripts/New-PlanFile.ps1`,
   which already takes the area as `-Area` and validates the tree afterwards,
   so an authoring-time check has one place to sit rather than two.
2. Settled by the second pilot: the path majority is computed in code, with
   the tie rule, and is the whole check; Jev would be asked only for a Plan
   whose sections name no file, which the tree does not contain.
3. Settled by the second pilot: the tree question is out, because the tree
   is decided by when the user wants the work, which no document states.
4. The confidence gate as a number in the skill's references, and that a
   disagreement is reported, never applied.
5. The shared decisions in `JevDecisionModelWorkflowUses.md`; the call itself
   is `.agents/scripts/Invoke-Jev.ps1` (that document's `## The caller`), so
   the Plan writes a request file and reads the result, never an HTTP call.
