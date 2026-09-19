# Jev for checking that a cited region supports its claim

Open question: can Jev check every `path:line` citation the workflow produces —
finding evidence, acceptance-table rows, Plan context sentences — against the
code it points at, and report the ones that no longer say what they are cited
for? Part of the series in `JevDecisionModelWorkflowUses.md`. A pilot has been
run over current Plan citations.

## The decision today

Citations are everywhere and nothing verifies them. Every finding row carries
`path:line — claim — evidence` (`.agents/references/subagent-handoff.md:15`);
`/verify-acceptance` maps each criterion "to evidence that settles it on its
own"; `/verify-external-claims` returns a rule that "settles the question on its
own"; every Plan's `## Context` and `## Critical files` cite regions by line.
Lines drift as files change, so a Plan written weeks ago can cite a region
that has moved or been rewritten. Whoever reads a citation decides whether
it still holds, by opening the file.

## The question to Jev

The citation-check shape from the TypeSafe docs: code does the deterministic
part — the file exists, the line range is inside it, a quoted fragment
string-matches — and only citations that pass get a `choice` over the pair:
`supports` (the code shown is what the claim describes), `contradicts` (it
covers the same behavior but does the opposite or lacks what is claimed),
`says_nothing` (unrelated). State is a JSON object with `claim` (the citing
sentence), `code` (the cited lines plus two before), and `file`. The
confidence orders the human read: `says_nothing` and `contradicts` answers are
listed first, then low-confidence `supports` answers, and nothing is accepted
on Jev's word alone.

The same pair question serves `/verify-external-claims`, whose `locator`
returns a `VERIFIED | REFUTED | UNRESOLVED` verdict per proposition from a
fetched source: the source excerpt is the `code` field and the proposition the
`claim`, and the verdict maps onto the three options. That use is on the
critical path of a review round, so it comes after the offline Plan sweep.

The pre-parsed selection shape covers the follow-up: when a citation says
nothing, code enumerates the file's functions and a second `choice` asks which
one the claim now describes, so a moved citation gets a proposed new line
range copied from real line numbers rather than invented.

## What still needs a full model

Every verdict. A flagged citation is a residual a human reads against the file
before anything is changed, and the `VERIFIED | REFUTED | UNRESOLVED` verdict
`/verify-external-claims` returns stays that skill's, as does an acceptance or
finding row's evidence sentence. Jev only shortens the list of citations
someone opens.

## Pilot

Corpus: every sentence in a current Plan that cites `path:start-end` in a C++
file with a range under 40 lines — 30 sampled as positives, each paired with
one negative made by taking a region of the same length from the same file at
least 40 lines away. 60 calls, a few hundred to about 1,500 input tokens each.

| pair | Jev chose `supports` | `supports` probability at least 0.8 |
|---|---|---|
| real citation | 27 / 30 | 14 / 30 |
| shifted region, same file | 5 / 30 | 1 / 30 |

For shifted regions Jev chose `says_nothing` 20 times and `contradicts` 5.

What the numbers say. The choice separates real from shifted regions well:
the 5 shifted "supports" answers came from files where the shifted window
still contained the same symbols, which a random shift in a small file
produces often. The three real citations Jev called `says_nothing` are the
interesting rows: they may be stale citations, which is what the check is for,
and a human read of those three is the next step this pilot leaves. The
probability threshold matters: at 0.8 the check passes fewer than half the
real citations, so the useful wiring is "flag `says_nothing` and
`contradicts` for a read", not "accept only above 0.8".

Caveats. Negatives are synthetic; a real stale citation usually points a few
lines off into related code, which is harder than a random shift. Two lines
of leading context may be too few for a claim about a function's caller.

## What would make this a Plan

Success: over the three flagged real citations plus 30 citations from Plans
that predate a large refactor of their files, a human confirms that every
citation Jev flags is stale or wrong, and that the ones it passes hold, with
under one false flag in ten. Then the check runs wherever citations are
produced or consumed: `Test-PlanSchedulerState.ps1` for the Plan tree, and
the review handoff for finding evidence, each reporting flags as residuals.

## Decisions a Plan needs

1. Which citations are in scope first: Plan files (a tree sweep, offline) or
   finding rows (in the review round, on the critical path).
2. The deterministic pre-check, and whether a quoted fragment is required for
   the string match or the line range alone is enough.
3. Context size around the cited lines, and whether the enclosing function is
   used instead of a fixed window.
4. Whether the moved-citation follow-up question is included.
5. The shared decisions in `JevDecisionModelWorkflowUses.md`.
