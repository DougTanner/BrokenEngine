# Authority order for plan decisions

The authority order itself, and the rule that a contradiction is surfaced as a
residual instead of silently resolved, are in root
[AGENTS.md](../../AGENTS.md) `### Diagnosis Discipline`.

Plans are agent-authored, so wherever a plan is consumed — preparation,
alternatives exploration, review, and plan authoring — a plan-internal decision
binds only when the user's own words choose it: the user's message itself, or
an execution card or dispatch brief quoting it. A reply that approves a
presentation, or picks one of its options, chooses every decision that
presentation showed, including each recommendation the approval accepted; a
presentation showed a decision only when its text stated it, and a card or
brief relays such a choice by quoting both the reply and the presented text
that stated the decision. A detail the presentation never stated, a card or
brief stating a decision without the user's words, and the plan's own text
claiming it bind nothing. A plan's `## In scope` and `## Out of scope`
boundaries are plan-internal decisions that bind on these same terms, except
as the ceiling a finished change is measured against, which
[`scope-authorization.md`](scope-authorization.md) governs. Hard binding
language such as "decided" or "not an option" is reserved for such user-chosen
decisions.
