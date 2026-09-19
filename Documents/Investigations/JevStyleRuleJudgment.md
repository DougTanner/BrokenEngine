# Jev for the hand-read style rules in `/code-style-review`

Open question: can Jev take over the hand-read pass for the three
`Documents/C++StyleGuide.txt` rules that are pure judgment on a short piece of
text, once a scanner exists to enumerate their candidates? Part of the series
in `JevDecisionModelWorkflowUses.md`. Not piloted: the enumeration it needs
does not exist yet.

## The decision today

`.agents/skills/code-style-review/references/worker.md:36-44` names the rules
the worker hand-reads because `.agents/scripts/Find-SessionCandidates.ps1` emits
no candidates for them. Three of those are judgment over a short span:

- rule 49 — no accessor or one-line pass-through functions
  (`Documents/C++StyleGuide.txt:201`);
- rule 56 — complete words in names, no abbreviations (`:264`);
- rule 62 — one `if` per guard condition instead of a packed `A || B || C`
  (`:299`).

A fourth judgment of the same shape lives outside the style guide: the log
level a new `LOG` call should carry, fixed as five options with one-line
definitions in `.agents/references/cpp-conventions.md:7`, decided from the
call site's frequency and purpose. A scanner for new `LOG` calls in changed
lines is trivial, so it can join the same Plan as a fourth rule.

Rule 57 (`Impl`/`Internal` suffixes, `:266`) is the counter-example: the
scanner already emits `style-rule-57` and the rule has no exceptions to judge,
so Jev adds nothing there. The worker may auto-fix only when the C++ meaning
is demonstrably unchanged (`references/worker.md:70-73`); the fix and the
meaning check stay with the worker whatever Jev says.

## The question to Jev

One `noul` per candidate per rule, with the rule text pasted verbatim as the
`true` criterion and the rule's explicit exceptions as the `false` criterion.
State is a JSON object with the candidate span and just enough context for the
rule: for rule 49 the function body plus its declaration; for rule 56 the
identifier plus the declaration it appears in, so the loop counters `i`/`j`/`k`
and iterators `it` the rule excepts can be recognized; for rule 62 the guard's
condition and its body. The TypeSafe docs' own warning applies: unrelated
context lowers accuracy, so the state is the span, not the file.

The enumeration is a scanner, never Jev: declarations with a one-statement
body for rule 49, identifiers in changed lines for rule 56, `if` conditions
containing `||` or `&&` for rule 62. Whether the scanner is a new mode of
`Find-SessionCandidates.ps1` or a separate script follows that script's
existing kind pattern.

## What still needs a full model

Every fix, and the meaning-preservation decision the auto-fix requires. Jev
only reorders or shortens the hand-read list. A whole-file "does this follow
the style guide" question is no substitute: the handoff row needs file, line,
rule number, and correction (`SKILL.md:41`), and only the scanner supplies the
first two.

## Why this is not first

The cost of a wrong answer has the same shape as the comment-block case in
`JevCommentBlockTriage.md` — a false flag costs one read, a miss lands a
violation in the code — but this candidate needs a scanner built before a
single question can be asked, and its answer key must be hand-made: no past
commit isolates rule 49, 56, or 62 fixes. The comment-block pilot's result on
the same "flag generously, never hide" shape transfers here, so the measurement
can wait until the scanner exists for its own sake.

## What would make this a Plan

Success: over a hand-labelled set of at least 60 candidates per rule from
recent session changes, a per-rule threshold flags at least 90% of the
violations while flagging under 30% of the compliant candidates. Rule 56 is
the one to watch, since its only exception is loop counters `i`/`j`/`k` and
iterators `it` (`Documents/C++StyleGuide.txt:264`), so every other shortened
name the question passes is a missed violation.

## Decisions a Plan needs

1. The three scanners: what a rule 49 candidate is (one-statement bodies only,
   or any body under N lines), which identifiers rule 56 scans (new names in
   changed lines only, or every identifier the change touches), and whether
   rule 62 also covers `&&`.
2. Whether the scanner output joins `Find-SessionCandidates.ps1`'s
   `style-rule-<n>` kinds so the worker's existing loop consumes it.
3. The per-rule threshold, written in the skill's references.
4. The shared decisions in `JevDecisionModelWorkflowUses.md`.
