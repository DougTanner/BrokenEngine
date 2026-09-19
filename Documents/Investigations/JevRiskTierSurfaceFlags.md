# Jev as an escalate-only second opinion on the risk tier

Open question: can Jev flag the Tier-3 surfaces a diff touches so the tier
main locked in is escalated when the changed bytes say so, without ever
lowering a tier? Part of the series in `JevDecisionModelWorkflowUses.md`. Not
piloted: no landed commit records the tier it was classified at, so the corpus
must be built first.

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

Not a three-way tier `choice` and not a `score` over the tier ladder: both
would let Jev's answer argue for a lower tier. Instead one `noul` per Tier-3
surface per hunk, each with the surface's sentence from `risk-tiers.md` as its
`true` criterion — "does this hunk change what the simulation computes into
the CRC", "does it change what a message carries", "does it change a persisted
or replayed layout", "does it change which thread runs or waits on what", "does
it change what is trusted at a boundary" — plus the two whole-change questions
(build coordination, cross-subsystem span) over the file list. The gate is a
max over hunks and surfaces, the shape the TypeSafe extraction-cascade cookbook
uses so one strong local signal cannot be averaged away. Any surface probability
over the threshold is reported as "Tier-3 surface: `<surface>` at `<path:hunk>`"
and can only raise the tier.

State per hunk is the hunk plus the enclosing function's name and the file's
path, from `.agents/scripts/Get-SessionChangeInventory.ps1`'s change regions,
never the whole diff: the docs warn that unrelated context lowers accuracy, and
a whole diff often exceeds one state anyway.

## What still needs a full model

The classification itself. Jev's output is a list of surfaces worth a second
look; main or the reviewer still decides the tier.

## Why the escalate-only shape

The calibration warning in `JevDecisionModelWorkflowUses.md` `## What Jev is`
applies here, where the combining step is a max over many hunks and surfaces.
That is acceptable only because the combined answer is allowed to do one thing:
add a review. Every false flag
costs a human read of one hunk against one sentence of `risk-tiers.md`.

## What would make this a Plan

The corpus first: for at least 40 landed changes, record the tier main locked
in and, where a reviewer escalated, which surface. Then success is that Jev's
surface flags include every recorded escalation and every Tier-3 classification's
trigger surface, while flagging under 20% of the hunks in recorded Tier-1 and
Tier-2 changes. The wiring reports flags at the Approve and classify step and
again at the Review and resolve correctness step's reviewer brief; it never
changes the tier itself.

## Decisions a Plan needs

1. Where the tier and its trigger are recorded per landed change so the corpus
   accumulates: the landing commit message, the execution card, or a line in
   `/finalize-changes`'s acceptance table.
2. The exact surface list and its one-sentence criteria, kept in
   `risk-tiers.md` so the question and the rule cannot drift apart.
3. Hunk context: the hunk alone, or the enclosing function.
4. The threshold, and that a flag is a residual for a human, never a tier.
5. The shared decisions in `JevDecisionModelWorkflowUses.md`; the call itself
   is `.agents/scripts/Invoke-Jev.ps1` (that document's `## The caller`), so
   the Plan writes a request file and reads the result, never an HTTP call.
