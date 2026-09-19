# Jev for the session-residue judgment in `/code-style-review`

Open question: can Jev decide, per scanner hit, whether a session-added `LOG`,
`printf`, `DEBUG_BREAK()`, `ASSERT(false)`, `// FIXME`, or `// HACK` line is
temporary instrumentation to remove or a permanent one to keep, so the
worker's residue step reads the likely-temporary hits first or only? Part of
the series in `JevDecisionModelWorkflowUses.md`. Not piloted: the landed
history holds almost no positives, because a removed line never reaches a
landing, so the corpus has to be built as `## Pilot` sketches.

## The decision today

`.agents/skills/code-style-review/references/worker.md:130-135` (step 17)
removes "confirmed temporary debug instrumentation added during the session",
naming the six kinds above and taking the added-versus-pre-existing
distinction from the scanner; the confirmation itself is the worker's reading
of each hit. `worker.md:153-155` keeps that judgment in the worker because the
scanner is candidates-only, and `worker.md:159-160` forbids adding a debug tag
to defer the cleanup and altering a pre-existing intentional log. The public
contract calls the outcome "the session's residue removed"
(`.agents/skills/code-style-review/SKILL.md:13`).

`.agents/scripts/Find-SessionCandidates.ps1:37-44` already does the
deterministic part: it scans only the head side of the session's added C++
lines and emits one hit per line matching the `log`, `printf`, `debug-break`,
`assert-false`, `fixme`, or `hack` regex, with `path`, `line`, `kind`, and
`text`, and a per-kind count. It "never decides whether a hit is temporary"
(`:1-6`). So every hit the worker reads is already a real `LOG`, `printf`,
`DEBUG_BREAK()`, `ASSERT(false)`, or FIXME/HACK comment on a session-added
line; what is left is exactly the yes/no this candidate asks.

The remainder is a judgment and not a rule, which is the test `## Not
suitable` in the overview sets. Every kind has permanent uses in tracked C++
outside `ThirdParty/`: 760 `LOG` calls, 68 `DEBUG_BREAK()` (the allocation
tracker and the assertion plumbing), 24 `ASSERT(false)` (unreachable
branches), and 4 `printf`. The one deterministic sub-case is the tagged log
`/external-diagnose-bug` requires: its worker tags every temporary log
`[DEBUG-<id>]` so "removal is one search"
(`.agents/skills/external-diagnose-bug/references/worker.md:79-84`); a
`[DEBUG-` hit is temporary by construction and needs no model. That search is
not run at landing, and it can miss: `Engine/Source/Frame/NavCellData.cpp:60`
carries a `[DEBUG-nav-crossing]` `kError` log landed in `e571f6f1` and still
present. It is the one positive the landed history offers, and evidence that
the hand read is worth ordering.

Volume is small: `Find-SessionCandidates.ps1` run per commit over the five
most recent commits touching C++ (`1f944e72`, `4a6868bf`, `2028ccd5`,
`7bf7aff0`, `78d7b763`, adding 128, 49, 54, 110, and 15 C++ lines) reports
`log` hits of 4, 1, 0, 0, and 0 and one `debug-break` hit,
`Engine/Source/Agent/Commands/ReplayFixtures.cpp:294` in `1f944e72`, a
`kError` log and `DEBUG_BREAK()` pair at `:293-294` that reads as permanent.
The cost of asking is negligible; the value is that the worker stops reading
every `LOG` a feature adds on purpose, and reads the one it should not have
kept.

## The question to Jev

One request per scanner hit, one `noul`: is the line temporary instrumentation
added while working on the change, rather than a permanent part of the code?
The state is a JSON object with `kind` (the scanner's), `hit` (the line), and
the enclosing function or a fixed window of code around it; `## Decisions a
Plan needs` leaves the window open. The `true` criteria name the temporary
forms: a log that prints raw variable values with no message an operator
acts on, or whose text says "debug", "here", "test", or "XXX", or that uses
`kError` for a trace; a `DEBUG_BREAK()` on a path that is not a contract
failure; an `ASSERT(false)` on a reachable path rather than an unreachable
branch; and any FIXME or HACK comment. The `false` criteria name the permanent
forms: a log carrying a state transition or failure at the level
`.agents/references/cpp-conventions.md:7` assigns it, `DEBUG_BREAK()` inside
the allocation tracker or an assertion path, `ASSERT(false)` in a `default:`
or other unreachable branch, and a `printf` in a tool that has no `LOG`. The
construct-shaped phrasing with the repository's own exceptions is the one
`JevStyleRuleJudgment.md` found works; the rule-text and "does it comply"
phrasings do not.

The signal consumed is the probability: hits ordered by it, the threshold
chosen from the pilot and written in the skill's references. Whether the
worker reads only hits above the threshold, as it does for the seven gated
style rules today, or reads every hit in that order, is `## Decisions a Plan
needs` item 4.

## What still needs a full model

The confirmation. A permanent-looking log that the session added only to watch
a value is temporary because of what the change is for, which Jev cannot know
unless the change objective joins the state (item 3 below), and a
`DEBUG_BREAK()` whose path is a contract failure needs the caller's
precondition read. The worker still reads a flagged hit against its function
before removing it, so the shape is "Jev shortens or orders the list the
worker reads", as the overview requires. The removal is a deletion and needs
no prose, which makes this the one candidate in the series whose fix is as
cheap as its judgment.

## Judgments of the same shape elsewhere

A sweep of every skill's `SKILL.md` and `references/worker.md` for steps that
decide by reading changed code whether something is temporary, leftover,
accidental, or out of place found no second hand-read of this exact shape.
The neighbours, and where each is owned:

- The `LOG` level a new call should carry: a `choice` over the five levels,
  `JevStyleRuleJudgment.md` open decision 7.
- Leftover narration in comments (`history`, `speculative`) and a `TODO`
  comment: `/comment-review` classes, `JevCommentBlockTriage.md`. No scanner
  emits a `TODO` kind; `/external-refactor-clean` routes future-work comments
  there (`references/worker.md:52-54`).
- Scope creep: the Tier-2+ authorization, minimality, and KISS passes of
  `.agents/references/scope-authorization.md`, run by `/repo-code-review` and
  `/coherence-review` over their changed regions, map each region to an
  `## In scope` entry and flag an unused option or speculative path. That is a
  `choice` per hunk over the plan's entries plus `none`, with the plan text as
  state; it is not covered by any row in the overview and would be its own
  candidate, not this one, because its state is a plan and not a line.
- A new standard-library or third-party `#include` in a PCH-backed file
  (`.agents/skills/repo-code-review/references/checks.md:180-182`): an added
  `#include <` line in a file that includes `Pch.h` is a regex, so it belongs
  to a scanner kind and not to Jev.
- Unused includes, unreachable statements, and unused locals
  (`/external-architecture-review` Lens A, `/external-refactor-clean` step 7):
  whole-area, explicit-request reviews over compiler-decidable facts, not a
  session read.
- `/verify-acceptance`'s "proven leftover that no approved criterion covers"
  (`references/worker.md:50`), `/implement-plan`'s "leaving unrelated cleanup
  alone" (`references/worker.md:26-27`), and `/update-affected-code`'s "edit
  only sites whose correctness clearly depends on the new contract"
  (`references/worker.md:48-53`): the first is evidence of record, the other
  two are implementer restraint over a search-hit list already covered by
  `JevAffectedFileRanking.md`.
- `/finalize-changes` has no hygiene step; the landing table is out by the
  overview's `## Not suitable`.

## Pilot

Not run. What it needs:

- Negatives: run `Find-SessionCandidates.ps1 -Baseline <parent> -Head
  <commit>` over the last ten landed C++ commits (the commits
  `JevStyleRuleJudgment.md`'s next test already names) and take every hit; a
  hit that survives at the current `main` is presumed permanent. The
  `NavCellData.cpp:60` log is the known label-noise case and is labelled
  temporary by hand.
- Positives: the landed history cannot supply them, so they come from two
  places. First, planted lines in the style of
  `.agents/skills/code-style-review/references/style-rule-judgment/cases.json`:
  a temporary `LOG`, `printf`, `DEBUG_BREAK()`, and `ASSERT(false)` each
  inserted into real functions taken from the negative set, at least 20 in
  all, including tagless logs that look permanent. Second, going forward,
  every step 17 removal recorded as one row `(path, line, kind, text,
  removed | kept)` in a tracked corpus file beside `cases.json`, the way
  `JevFindingTriage.md` records main's decision beside Jev's, so the second
  measurement uses real removals.
- Labels: at least 40 temporary and 100 permanent hits, each hand-labelled
  once; two sweeps, since the overview records that verdicts near the
  threshold move between runs.

## What would make this a Plan

Success, set before the run: at the chosen threshold at least 90% of the
temporary hits are flagged and under 30% of the permanent hits are, reported
as two rates because a miss lands residue and a false flag costs one read. A
`DEBUG_BREAK()` or `ASSERT(false)` result is reported per kind, since those
two carry the most permanent uses and a threshold that suits `log` may not
suit them. If that holds over both sweeps, the Plan wires the `noul` into
step 17 in whichever form items 2 and 4 below decide.

## Decisions a Plan needs

1. The state window: the enclosing function found by the Allman-shape
   enumerator `Test-StyleRuleJudgment.ps1` already has, or a fixed window
   around the hit. The rule 56 measurement in `JevStyleRuleJudgment.md` showed
   one short name diluted in a 39-line function, so a hit-centred window is
   the likely answer; the pilot measures both.
2. Same request or its own: since the style-rule Plan landed,
   `Test-StyleRuleJudgment.ps1` session mode already sends one request per
   changed block. The residue `noul` could ride that request, with the hit
   line named in the instruction and no second enumeration, or be its own
   request per scanner hit from a separate check script that reads the
   scanner's `hits` rows. Riding shares the block state and its dilution;
   a separate request sends only the hit and its window but adds a script
   and a second Jev call to step 17. Open; the pilot's window result decides
   which is even possible.
3. Whether the change objective (the execution card's one-line objective, or
   the Plan title) joins the state, so a permanent-looking log the change did
   not need scores as temporary; the vendor's dilution warning argues against
   any text the question does not need.
4. Gate or reading order: the series' shared decision 4 makes this a reading
   order until a second measurement confirms the first, and
   `JevStyleRuleJudgment.md` is the user-directed exception that gates the
   seven rules and halts on a `blocked` result. Whether step 17 follows that
   exception (read only flagged hits, halt when Jev is unavailable) or the
   series default (read every hit in probability order, fall through to the
   hand read when Jev is unavailable) is for the user.
5. The `[DEBUG-` tag: a scanner kind that is temporary by construction and
   skips the model, or left to the `noul` like every other hit. A kind is the
   simpler answer and would have caught `NavCellData.cpp:60` at landing.
6. The threshold, per kind or shared, from the pilot, written in the skill's
   references.
7. The shared decisions in `JevDecisionModelWorkflowUses.md`; the call itself
   is `.agents/scripts/Invoke-Jev.ps1` (that document's `## The caller`), so
   the Plan writes a request file and reads the result, never an HTTP call.
