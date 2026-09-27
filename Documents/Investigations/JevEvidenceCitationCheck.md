# Jev for checking that a cited region supports its claim

Open question: can Jev check every `path:line` citation the workflow produces —
finding evidence, acceptance-table rows, Plan context sentences — against the
code it points at, and report the ones that no longer say what they are cited
for? Part of the series in `JevDecisionModelWorkflowUses.md`. Three pilots have
run over current Plan citations. The second, over every citation with a blind
read of a sample, is the one to trust for how often a pass or a flag is right.
The third compared the split question the script asks against the second's
wording, on the citations the two rank differently, and chose the wording.

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
string-matches — and only citations that pass get three yes-means-bad `noul`
questions about the pair: `absent` (something the claim attributes to the
region is not in the cited lines), `opposite` (the cited lines do the opposite
of what the claim says), and `elsewhere` (the claim describes a different
region). State is a JSON object with `claim` (the citing sentence),
`citation`, `file`, `lines`, `cited` (exactly the cited lines), and `context`
(the enclosing column-0 block when the range lies inside one of at most 120
lines, otherwise the cited lines plus two before). A citation's
`problemProbability` is the highest of the three answers and orders the human
read: flagged rows (at least 0.5) first, then passed rows, each from the
highest `problemProbability` down, and nothing is accepted on Jev's word alone.

The same claim-and-region pair serves `/verify-external-claims`, whose
`locator` returns a `VERIFIED | REFUTED | UNRESOLVED` verdict per proposition
from a fetched source: the source excerpt is the `cited` field and the
proposition the `claim`. That use is on the
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

## Third pilot: split question, enclosing context

Method: `Test-CitationSupport.ps1 -IncludeShiftedControls` over
`Documents/Plans`, run at one commit twice: once with the second pilot's
three-way `choice`, once with the three `noul` questions and the
`cited`/`context` state that `## The question to Jev` describes. Both runs
found 229 citations, skipped 23 at the pre-check (4 past the end of the file,
19 over 60 lines), and answered all 206 others; no row was left out for a
failed request. The rule below was fixed before either run.

The two wordings were compared at the same flag count. K = 49 is the three-way
run's flag count (30 `contradicts`, 19 `says_nothing`); the split run's flag
set is its 49 citations with the highest `problemProbability`. The two sets
differ in 38 rows: 19 flagged only by the three-way question and 19 only by
the split. All 38 were labelled (sampling fraction 1 in each direction, so each
weight is 1), shuffled so direction was not visible. Three `locator` agents
labelled them `holds` or `stale` from the claim, file, and lines alone, told
not to open either run's output; the label is the majority of the three. The
split question ships only if its split-only rows hold strictly more stale
citations than the three-way-only rows.

| flagged only by | rows | labelled stale | labelled holds | stale, unanimous / 2-1 |
|---|---|---|---|---|
| three-way question | 19 | 4 | 15 | 2 / 2 |
| split question | 19 | 5 | 14 | 3 / 2 |

Outcome: 5 against 4, so the split question ships. The labellers agreed on 30
of 38 rows and split 2-1 on 8 (4 of them labelled stale, 4 holds).

| | three-way | split |
|---|---|---|
| real citations flagged (the three-way by its choice, the split at `problemProbability` >= 0.5) | 49 / 206 | 148 / 206 |
| shifted controls flagged, same rule | 184 / 190 (97%) | 188 / 190 (99%) |

How firm the outcome is. The margin is one row, and five 2-1 votes can each
reverse it: one labeller changing either split-only stale row
(`AnimationData.cpp:65-72`, `ExplosionsSpawn.cpp:218-234`) would make it
4 against 4, and one changing any three-way-only row held by 2-1
(`Explosions.h:171-184`, `Spaceships.cpp:649-653`, `PlayerEvents.cpp:64`)
would make it 5 against 5; either keeps the three-way question. The split's 49th value,
`problemProbability` 0.79, is shared by 10 citations and only 2 are inside
the top 49, taken by listing order: `Collection.h:338-353`, flagged by both,
and `Buffer.cpp:77-84`, split-only and labelled `holds`. Any other listing
order that swaps only labelled rows keeps the split ahead, since no split-only
stale row sits at 0.79. An order that pushes `Collection.h:338-353` out makes
it three-way-only and pulls in one of six tied rows the three-way passed,
none labelled; if `Collection.h:338-353` were stale and the incoming row
held, the count would be 5 against 5 and the three-way question would stay.

What the stale rows are. Four of the five split-only stale citations point
next to the code the claim describes: `Graphics.cpp:692` names a comment at
688-691, `Graphics.cpp:662-711` stops before the sampler recreation at 712-717,
`ReconcileReplayTick.cpp:113-130` starts after the stamp at 112, and
`AnimationData.cpp:65-72` starts after the material-count check at 64. The
fifth, `ExplosionsSpawn.cpp:218-234`, is wrong in content: the region now has
the replay guard the claim says is missing. Of the four three-way-only stale
citations, one is one line off (`Localization.h:9-10`, a comment at 8-9) and
three cite a region that holds other code, with no nearby line the labellers
matched to the claim.

What the numbers say. The split question lifts off-by-one and adjacent-range
citations, the kind the second pilot's two missed passes were, above
citations that hold; the three-way question's extra flags hold 15 times in 19.
It does not make a flag more trustworthy: at 0.5 it flags 148 of 206 real
citations and 188 of 190 shifted controls, so its output is a reading order,
not a verdict. The 0.5 cutoff that splits `flagged` from `passed` is a fixed
number, not one chosen from this measurement, which departs from shared
decision 3 in `JevDecisionModelWorkflowUses.md`; the ship decision used no
cutoff. The labels are LLM labels, not the human blind read
`## What would make this a Plan` asks for, so that condition stays unmet.

## What would make this a Plan

The check runs as a reading order only: `Test-CitationSupport.ps1` over the
Plan tree, its flagged rows listed as residuals from the highest
`problemProbability` down,
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
3. Decided by the third pilot, which measured it together with the split
   question rather than on its own: each question sees the cited lines and
   their enclosing column-0 block of at most 120 lines, falling back to the
   cited lines plus two before; no wider fixed window.
4. The shared decisions in `JevDecisionModelWorkflowUses.md`.
