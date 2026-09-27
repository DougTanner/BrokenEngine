# Jev surface flags as a hint to the code reviewer

Open question: can Jev flag the Tier-3 surfaces a diff touches, as an advisory
hint in the reviewer brief at the Review and resolve correctness step? The
reviewer still does its full review and may escalate the tier; Jev never
changes, gates, or lowers it. Part of the series in
`JevDecisionModelWorkflowUses.md`. Not piloted: no landed commit records the
tier it was classified at, so the corpus must be built first.

## The decision today

`.agents/references/risk-tiers.md` fixes three tiers and the rule "classify the
whole change at the highest applicable tier before implementing"; main locks
the tier in at the Approve and classify step from `/prepare-change`'s evidence,
and a reviewer "may escalate the tier when the changed bytes expose a
higher-risk surface". Tier 3 is defined by a list of surfaces: determinism and
CRC, wire and protocol, serialization or data layout, save and replay
compatibility, threading, trust boundaries, build and bootstrap coordination
that can block other sessions, and a change spanning independently owned
subsystems. A downgrade skips required reviews, the worst failure in this
series; an upgrade costs extra review rounds.

## The question to Jev

Not a three-way tier `choice` and not a `score` over the tier ladder: Jev does
not classify, and a tier answer could argue for a lower one. Instead one
`noul` per Tier-3 surface per hunk, each with the surface's sentence from
`risk-tiers.md` as its `true` criterion — "does this hunk change what the
simulation computes into the CRC", "does it change what a message carries",
"does it change a persisted or replayed layout", "does it change which thread
runs or waits on what", "does it change what is trusted at a boundary" —
plus the two whole-change questions (build coordination, cross-subsystem
span) over the changed file list alone.
The combination is a max over hunks and surfaces, the shape the TypeSafe
extraction-cascade cookbook uses so one strong local signal cannot be averaged
away. Any surface probability over the cutoff becomes one line in the reviewer
brief: "Tier-3 surface: `<surface>` at `<path:hunk>`".

A deterministic path-to-surface list, with no Jev, runs first: a file under a
folder that owns a Tier-3 surface (network and wire, file, save and replay,
frame simulation — for example `Engine/Source/Network/`, `Engine/Source/File/`,
`Engine/Source/Frame/`) is flagged by path for free. Jev's per-hunk questions
target the subtle cases inside ordinary-looking files: a new network message
field, a new lock or thread, a removed check on client data. Blind spot,
shared by both: a change whose risk lives outside the hunk — a helper the
simulation calls that shifts the CRC, a struct field another file saves — is
visible to neither the hunk nor the path list.

State per hunk is the hunk, the file's path, and the enclosing function's
name, from `.agents/scripts/Get-SessionChangeInventory.ps1`'s change regions,
never the whole diff: the docs warn that unrelated context lowers accuracy.
Optionally it adds the enclosing column-0 block the style-rule judgment
(`.agents/scripts/Test-StyleRuleJudgment.ps1`) sends, capped at 120 lines as
the citation check settled (`JevEvidenceCitationCheck.md`). One request per
hunk — that state plus the five surface questions — stays at a few thousand
tokens, far under Jev's 32k limit for state plus question; an oversized hunk
is capped at a line limit.

## What still needs a full model

The review and the tier. Jev's output is a list of surfaces worth a second
look; the reviewer still does its full review and alone may escalate.

## Why advisory only

The calibration warning in `JevDecisionModelWorkflowUses.md` `## What Jev is`
applies here, where the combining step is a max over many hunks and surfaces.
That is acceptable only because a flag changes nothing but where the reviewer
looks. A miss costs nothing, since the reviewer does its normal pass anyway; a
false flag costs reviewer attention. So the cutoff is tuned so false flags
stay rare, starting reliability testing at 0.7, the block and name threshold
of the style-rule judgment. Classification is not wired: at the Approve and
classify step there is no diff to ask about.

## What would make this a Plan

The corpus first: for at least 40 landed changes, record the tier main locked
in and, where a reviewer escalated, which surface. Success is measured against
the false-flag rate first: Jev and the path list together flag few hunks in
recorded Tier-1 and Tier-2 changes, and then, as the secondary measure, catch
the trigger surfaces of recorded escalations and Tier-3 classifications that
lie inside a hunk. The wiring adds flags to the reviewer brief at the Review
and resolve correctness step only; it never changes the tier.

## Decisions a Plan needs

1. Where the tier and its trigger are recorded per landed change so the corpus
   accumulates: the landing commit message, the execution card, or a line in
   `/finalize-changes`'s acceptance table.
2. The exact surface list and its one-sentence criteria, kept in
   `risk-tiers.md` so the question and the rule cannot drift apart.
3. The path-to-surface list: which folders own which surface, and where the
   list lives.
4. Hunk context: the hunk alone, or with the capped enclosing block; and the
   oversized-hunk line limit.
5. The cutoff, tested from 0.7, and the false-flag rate it must hold.
6. The shared decisions in `JevDecisionModelWorkflowUses.md`; the call itself
   is `.agents/scripts/Invoke-Jev.ps1` (that document's `## The caller`), so
   the Plan writes a request file and reads the result, never an HTTP call.
