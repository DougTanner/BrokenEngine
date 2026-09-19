# Jev for choosing the skill a request invokes

Open question: can Jev pick which of the repository's skills a user request
should run, including "none of them", more reliably than the host's own
description matching? Part of the series in `JevDecisionModelWorkflowUses.md`.
Not piloted; the corpus does not exist yet.

## The decision today

Every skill's `description` frontmatter carries its "use when" sentences, and
the host matches a request against those descriptions to decide which skill to
load. The repository has 48 skills, several of them lookalikes:
`/repo-code-review`, `/code-style-review`, `/comment-review`, and
`/coherence-review` all review changed files; `/prepare-change`,
`/plan-alternatives`, and `/external-design-interface` all precede a plan;
`/external-diagnose-bug` and `/adversarial-review` both try to disprove
something. A wrong load costs a skill's worth of context and a wrong workflow; a
needless load costs context alone. Nothing measures either today.

## The question to Jev

The two-request shape from the TypeSafe skill-suggestion cookbook, which is the
closest published analogue (182 skills, a chat assistant): first a wide
`choice` over every skill, keyed by name with its description as the
criterion, alongside gating `noul`s asking whether any skill is warranted at
all ("does the request act on the repository's files, build, or running
game", "would a careful engineer here follow a documented procedure",
"could a plain answer satisfy this with no tools"); then a second request
re-examining the top three with their full `## When to use` sections and
permission to reject all of them. The cookbook reports wrong loads falling
from 16.8% to 7.3% and needless loads from 9.8% to 4.0% against an oracle of
2.5% and 1.2%, with the remaining errors clustering among lookalikes — which
is why the second request reads full descriptions.

State is the user's message alone. Question IDs are the skill names, so the
answer needs no mapping.

## What still needs a full model

Everything after the load. And the routing the Change Workflow itself fixes —
which reviews a tier triggers, which role runs them — is a rule, not a
judgment, and stays out of this question.

## Why it is not first

The corpus must be built from past session transcripts: each user message
that invoked a skill, or should have, with the skill that ran and whether that
was right. Those transcripts exist locally but are not tracked, and labelling
"should have" needs a human. The host also owns the load decision, so the
wiring is a suggestion the session reads, not a replacement — the same
suggestion-only shape the cookbook uses.

## What would make this a Plan

Success: over at least 100 labelled past requests, the two-stage answer's
wrong-load and needless-load rates both fall below the host's on the same
requests. The measurement alone decides whether wiring is worth doing.

## Decisions a Plan needs

1. How the corpus is built and labelled, and where it lives, given that
   transcripts are local-only.
2. Whether `## When to use` sections are short enough to fit the second
   request for every skill, or need a summary field.
3. What "suggestion the session reads" means mechanically on each host.
4. The shared decisions in `JevDecisionModelWorkflowUses.md`; the call itself
   is `.agents/scripts/Invoke-Jev.ps1` (that document's `## The caller`), so
   the Plan writes a request file and reads the result, never an HTTP call.
