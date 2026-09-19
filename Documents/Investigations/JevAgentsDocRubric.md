# Jev for the `/update-claude-docs` audit rubric

Open question: can Jev score the six-criterion rubric `/update-claude-docs`
uses to grade every `AGENTS.md`, so a whole-tree audit starts from a ranked
list and the full model reads the documents the rubric puts at grade C or
below first? Part of the series in `JevDecisionModelWorkflowUses.md`. Not
piloted.

## The decision today

`.agents/skills/update-claude-docs/references/audit-mode.md` `## Rubric`
scores each `AGENTS.md` on six weighted criteria — commands and workflows
(20), architecture clarity (20), non-obvious patterns (15), conciseness (15),
currency (15), actionability (15) — for a total of 100, with grades A to F
and "grade C or below as needing update". Each score is the auditor's
judgment of the document against its check question, and the audit walks every
`AGENTS.md` outside `ThirdParty/`, `Documents/Plans/`, and
`Documents/Features/`. Scoring dozens of documents is a large read for a full
model, and the score itself is rarely what matters: the ordered list of which
documents to fix is.

## The question to Jev

The composite-scoring shape from the TypeSafe docs, which is exactly what the
rubric already is: six `score` questions in one request per document, each
with 4 or 5 levels written as concrete situations ("every command is current
and runnable", "one command names a script that no longer exists", …), never
as numbers. Code multiplies each normalized answer by its weight and sums, so
the weights stay in `audit-mode.md` and changing one needs no new inference.
The document's own text is the state for four criteria; currency needs more —
whether cited paths resolve is a code check done first, with the unresolved
paths listed as a state field so the question is "given these broken paths,
how current is the prose".

Conciseness is partly measured already: `Get-AffectedAgentsDocs.ps1` reports
the advisory budget, so that criterion's code part is the number and Jev's part
is the "inventories and duplicated prose" half.

## What still needs a full model

The report's `Issues` and `Recommended improvements` lists, and every edit.
Jev supplies the ordering and the per-criterion score; the full model reads the
documents Jev ranks low and writes the improvements.

## The corpus

Past audit reports are the labels: every `## AGENTS.md Quality Report` a
session emitted carries a per-criterion score per file, and the file's content
at that commit is recoverable. Those reports live in session transcripts, not
in the tree, so the first step is to find how many exist. Failing that, one
full-model audit over the current tree produces the labels in a single run,
which is the same cost as one audit today.

## What would make this a Plan

Success: over the labelled set, Jev's weighted total lands in the same grade
band as the full model's for at least 85% of documents, and every document the
full model graded C or below is in Jev's bottom third. The wiring runs Jev
first and still hands the full model every document the audit covers, ordered
by Jev's ranking with Jev's per-criterion scores beside each, so the
lowest-graded ones are read first. Skipping the top-ranked documents waits for
a second measurement on a later audit, per shared decision 5 in
`JevDecisionModelWorkflowUses.md`.

## Decisions a Plan needs

1. The level texts for each criterion, kept beside the rubric in
   `audit-mode.md` so the question and the rule cannot drift.
2. Which checks move into code before the question: path resolution,
   budget, stub pairing, all-caps emphasis, and the other mechanical items in
   `## Assessment Examples`.
3. Whether the state is the document alone or the document plus its parent
   `AGENTS.md`, since duplicated parent rules are a listed defect that only
   the pair reveals.
4. The shared decisions in `JevDecisionModelWorkflowUses.md`.
