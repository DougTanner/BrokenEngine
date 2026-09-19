# Jev as a reading order for `/comment-review`

Open question: can Jev rank the comment blocks in a `/comment-review` scope so
the reviewer reads the likely findings first, without ever hiding a block? Part
of the series in `JevDecisionModelWorkflowUses.md`, which owns what Jev is, the
shared access-route decisions, and the list of places Jev must never decide.
This is the series' first experiment because the repository already holds a
labelled corpus for it, and a pilot has been run against that corpus.

## The decision today

`.agents/skills/comment-review/references/comment-classes.md` fixes six classes
— `boilerplate`, `history`, `speculative`, `navigation`, `false`, `dense` — each
tied to a rule 64 clause with a fixed severity, and every finding carries exactly
one of them. The worker classifies every scanned block by reading the
surrounding code (`references/worker.md` step 3). The block list comes from
`scripts/Find-CommentBlocks.ps1`, which emits `path`, `startLine`, `lineCount`,
`kinds`, and `firstLine` for up to 400 blocks and never decides anything.

## The question to Jev

One `choice` per block over seven options: the six classes plus `ok`. State is a
JSON object with four fields — `file`, `code_before` (3 lines), `comment_block`,
`code_after` (15 lines) — so the question is about the block in its context, not
the block alone. The criteria are the class rows of `comment-classes.md`
restated as one-sentence descriptions; `ok` is "a concise, true, present-tense
comment carrying rationale, a contract, or a constraint the code alone does not
show". The instruction states rule 64's test in one sentence and asks for the
best class or `ok`.

The signal the workflow consumes is not the chosen class but the probability of
`ok`: a low `ok` probability means "read this one first". The class is a hint
the reviewer confirms or discards against the code.

## What still needs a full model

Every class the review reports, and every replacement comment. The worker reads
each block against the surrounding code, confirms or discards the class Jev
suggested, and writes the replacement text the finding carries — none of which
Jev can do, since it cannot write text. Its answer only orders the reading.

## Pilot

Corpus: commit `65255669`, a comment cleanup across 123 files. In each changed
C++ file, every `//` block of the parent version whose lines the commit removed
or rewrote is a positive ("touched"); every block the commit left alone in
those files is a negative ("untouched"). That yields 223 positives and 2006
negatives. The pilot sampled 40 of each (fixed seed) and asked the question
above once per block, 80 calls, about 1,500 input tokens each.

| flag rule | touched flagged | untouched flagged |
|---|---|---|
| chosen class is not `ok` | 29 / 40 | 11 / 40 |
| `ok` probability below 0.5 | 35 / 40 | 13 / 40 |
| `ok` probability below 0.8 | 40 / 40 | 28 / 40 |

Classes Jev chose for touched blocks: dense 10, history 7, false 5, speculative
4, boilerplate 3, ok 11. For untouched blocks: ok 29, false 7, boilerplate 2,
dense 1, speculative 1.

What the numbers say. As a reading order the ranking is useful: at the 0.5
threshold the reviewer reads roughly a third of the blocks first and meets seven
of every eight blocks the sweep changed among them. As a filter it is not safe:
one touched block in eight sits above 0.5, and reaching every touched block
means reading two thirds of the untouched ones. The `false` class is
over-assigned on untouched blocks (7 of 11 untouched flags), which matches how
`comment-classes.md` defines that class: no fixed example applies, and `false`
is decided by reading the adjacent code and accepting the class only when that
code contradicts the stated fact. The class hint for `false` should be shown as
"check against code", never as a finding.

Caveats on the labels. "Touched" is a proxy: the sweep also reworded some
blocks that had no class violation, and it may have missed blocks that did, so
both rows carry label noise in the direction that makes Jev look worse than it
is. Fifteen lines of following code is a guess; `dense` and `false` may need
the whole function. The sample is 80 blocks from one commit.

## What would make this a Plan

Success is defined before the wiring exists: over a second, hand-labelled sample
of at least 100 blocks from a different commit, the `ok`-probability threshold
chosen from this pilot flags at least 85% of the hand-labelled findings while
flagging under 40% of the clean blocks. Report the two rates separately, since a
miss and a false flag cost different things. If that holds, the Plan wires Jev
as a reading order — flagged blocks first, every other block still read — and
never as a filter that hides blocks until a further measurement on a later
change confirms the first.

## Decisions a Plan needs

1. Whether the reading-order script is a new script beside
   `Find-CommentBlocks.ps1` or an extension of it. The scanner is documented
   as writing nothing and being safe under a read-only sandbox; calling
   `.agents/scripts/Invoke-Jev.ps1` (the shared caller
   `JevDecisionModelWorkflowUses.md` decided) breaks that property, so a
   separate script is the likely answer.
2. How much surrounding code goes into the state: the fixed 3-before/15-after
   window of the pilot, or the enclosing function found by brace matching.
3. The threshold, as a number written in the skill's references, and what the
   worker does when `Invoke-Jev.ps1` returns `blocked`: report the list
   unavailable, which is the existing scanner's precedent, or fall through and
   read every block in scanner order, which is the behaviour the workflow has
   today.
4. Whether the class hint is shown at all, and if so that `false` is shown as
   "check against code" rather than as a proposed class.
5. The shared decisions in `JevDecisionModelWorkflowUses.md`; the call itself
   is `.agents/scripts/Invoke-Jev.ps1` (that document's `## The caller`), so
   the Plan writes a request file and reads the result, never an HTTP call.
