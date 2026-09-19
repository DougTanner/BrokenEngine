# Jev for checking that a cited region supports its claim

Open question: can Jev check every `path:line` citation the workflow produces —
finding evidence, acceptance-table rows, Plan context sentences — against the
code it points at, and report the ones that no longer say what they are cited
for? Part of the series in `JevDecisionModelWorkflowUses.md`. Two pilots have
run over current Plan citations; the second, over every citation with a blind
read of a sample, is the one to trust.

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
`supports` probability orders the human read: `says_nothing` and `contradicts`
answers are listed first, then `supports` answers in ascending probability,
and nothing is accepted on Jev's word alone.

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

## Second pilot: every Plan citation, blind-labelled

Method: `.agents/scripts/Test-CitationSupport.ps1` (tracked; it calls
`Invoke-Jev.ps1`) over every backticked `path:line` and `path:start-end`
citation of a `.cpp`/`.h`/`.inl` file in `Documents/Plans`, with
`-IncludeShiftedControls` adding one same-length window at least 40 lines away
per citation. The claim is the sentence holding the citation; the state also
names the citation under test, because a sentence often cites several regions.
Ranges over 60 lines, files that no longer exist, and ranges past the end of
the file are skipped before any call.

| | count |
|---|---|
| citations found | 700 |
| skipped by the pre-check (missing file 7, past end of file 16, over 60 lines 52) | 75 |
| asked | 625 |
| real citations Jev called `supports` | 440 (70%) |
| of those in files unchanged since the Plan was written | 168 / 192 (88%) |
| shifted controls Jev called `supports` | 36 / 585 (6%) |
| shifted controls with `supports` probability at least 0.8 | 0 / 585 |
| wall time, input tokens, cost for one sweep without controls | 25 s, 535k, about $0.02 |

Three other question wordings ran over the same corpus. The cookbook's
plainer criteria let 30% of controls through; naming the cited region in the
state brought that back to 9%; eight lines of leading context instead of two
let 34% through. The tracked wording is the best of the four. Two identical
sweeps agreed on 605 of 625 verdicts, every disagreement near the 0.5
probability boundary.

Blind read: 66 real citations — all 24 that Jev flagged in unchanged files, 20
random flagged in changed files, 22 random passed — were labelled by three
`locator` agents given only the claim, file, and lines, never Jev's answer.
That is an LLM read, not the human read the first pilot asked for, and it was
told to accept a terse Critical-files bullet when the region plainly is that
thing.

| Jev said | labeller agreed | labeller disagreed |
|---|---|---|
| `supports` (22) | 20 | 2 (both ranges one line off the declaration they name) |
| `contradicts` or `says_nothing` (44) | 15 | 29 |

By `supports` probability the labellers' 17 stale citations sit where the
ordering puts them: 12 below 0.2, 3 in 0.2–0.5, 1 in 0.5–0.8, 1 at or above
0.8; and 19 of the 22 citations in 0.2–0.5 hold.

What the numbers say. A pass is reliable: a citation Jev calls `supports`
holds about nine times in ten, and at probability 0.8 no shifted control gets
through. A flag is not: two of three flagged citations hold, most of them
`contradicts` answers at low confidence, where Jev reads a terse or partial
claim ("the fleet-read half", "sampled peaks, hash, and paint consumers") as
lacking something. The first pilot's 27/30 was a small sample of the
well-written end of the corpus; over all Plans the flag list is a reading
order, not a verdict. Ordering by probability concentrates stale citations at
the bottom: reading the 123 real citations below 0.2 reaches about seven in
ten of the stale ones at about one false flag in two, against 625 rows read
unordered.

The first pilot's success bar — under one false flag in ten — is not met and no
wording tried comes near it, so the check does not replace a read of the
citations a Plan makes. What it can do today is order that read and shorten
it: a Plan sweep that lists citations by ascending `supports` probability,
with the pre-check failures (23 of the 700 point at a missing file or past its
end, which needs no model) first.

## What would make this a Plan

The check runs as a reading order only: `Test-CitationSupport.ps1` over the
Plan tree, its flagged rows listed as residuals in ascending probability,
nothing hidden and nothing changed. The moved-citation follow-up and the
finding-row and `/verify-external-claims` uses wait until a wording or a
richer state (the enclosing function, or a quoted fragment) brings the flag
precision above one in two on a second blind read, this time by a human, and
the two one-line-off misses argue for that richer state.

## Decisions a Plan needs

1. Where the Plan sweep is invoked: from `/next-plan` at claim time for the
   claimed Plan only, or over the whole tree from `Test-PlanSchedulerState.ps1`.
2. Whether the pre-check failures (missing file, range past end of file) are
   reported on their own, since they need no model and are certain.
3. Context size around the cited lines, and whether the enclosing function is
   used instead of a fixed window — the change most likely to raise flag
   precision.
4. The shared decisions in `JevDecisionModelWorkflowUses.md`.
