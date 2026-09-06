<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-06T14:30:53.938Z","dependsOn":[]} -->
# Audit: every skill's `## Handoff` against the minimum-return principle

## Context
The user set the governing principle for this repository's delegation this
session: a subagent returns to the main session only exactly what the main
session needs to decide on, at minimum, on success and on failure alike, and
everything else is written to a file that later subagents read, or that main
reads at a named selector when a decision needs it.

`.agents/references/subagent-reporting.md:98` already states "Return only
decision-relevant evidence", `.agents/references/subagent-reporting.md:116-119`
already routes any field over 10 rows or any handoff over 40 lines or 20,000
characters to an existing file or a `Temp/` file cited under `Evidence` as path
plus selector, and
`.agents/references/subagent-reporting.md:158-169` already tabulates what main
does with each field. What has never been checked is whether each skill's own
`## Handoff` section obeys that principle, because each skill extends the shared
form with its own fields and its own verbatim requirements.

One instance proves the gap is real rather than theoretical. `/compile`'s
`## Handoff` required the `builder` to include every build's captured
`broken-engine-build-result/v1` envelope verbatim, and
`.agents/references/subagent-reporting.md:120-123` exempts that envelope from
the size caps, so the requirement applied equally to a clean success. Across the
two `/compile` dispatches of one observed `/next-plan` run — every build
`status: success` with empty diagnostics — 10,478 and 9,175 characters, 19,653
in total, entered the main session as text main made no decision from. A
separate change addresses that one skill.

The only mechanism that catches such a case today is
`/next-plan-checkpoint-review`, which samples one run at a time
(`.agents/skills/next-plan-checkpoint-review/SKILL.md`). Nothing has ever swept
the tree, and there are 48 skill packages, 43 of whose `SKILL.md` files carry a
`## Handoff` section. A per-run sampler will surface the remaining violations
one accidental encounter at a time, at the cost of the wasted context each
encounter spends.

Session provenance for the observation recorded above (machine-local; not
reproducible after cleanup):
- Client: claude
- Conversation session ID: 6d55305a-7220-4373-bbaa-07b1001d0fbc
- Worktree/branch UUID: 4af84a34-1de8-40c7-ad6a-47354a6a589a
- Session branch: claude/4af84a34-1de8-40c7-ad6a-47354a6a589a
- Worktree: .claude\worktrees\BrokenEngine\4af84a34-1de8-40c7-ad6a-47354a6a589a
- Landing ref: claude/4af84a34-1de8-40c7-ad6a-47354a6a589a, this session's
  branch, whose landing commit will contain this Plan.

## Design
This is an audit-and-fix Plan. It runs in two phases inside one change, and the
audit result decides how much of the fix phase there is.

### Phase 1 — audit
Read every `## Handoff` section in `.agents/skills/*/SKILL.md`, plus any handoff
shape a package's `references/*.md` adds beyond it, plus every exemption from
the shared caps in `.agents/references/subagent-reporting.md` `## Handoffs`.
Judge each required return clause against one test, applied clause by clause:

> Does the main session make a decision from these bytes, using the routing in
> `.agents/references/subagent-reporting.md` `## Handoffs` "What main does with
> each field"? If the consumer is a later worker, a finalizer, or a reviewer
> rather than main itself, the bytes belong in a file cited as path plus
> selector, not in the handoff.

Record one row per audited skill in a `Temp/` findings file, each row naming the
skill, the clause, verdict `conforms` or `violates`, and for a violation the
consumer that actually reads the bytes. Cite that file under `Evidence` as path
plus one `##` selector per skill rather than pasting the table into the main
session — the audit's own return must obey the principle it audits.

A skill whose `SKILL.md` carries no `## Handoff` section at all is audited too:
either it correctly returns the shared form unextended, or its absence hides a
return shape stated loosely in prose, which is a violation.

### Phase 2 — minimal prose fixes
For each proven violation make the smallest prose change to that skill's own
`## Handoff` that leaves inline only what main decides on and moves the rest to
an existing file or a `Temp/` file cited under `Evidence` as path plus `##`
selector. Do not restructure a skill, do not add fields, do not change any
skill's purpose, triggers, or inputs, and do not touch a conforming skill.

Re-justify each typed-envelope and receipt exemption in
`.agents/references/subagent-reporting.md:120-123` by naming, in the audit
findings file, the consumer that requires the verbatim bytes and whether that
consumer is main. Narrow the exemption sentence only where the audit proves it
covers bytes main never decides on; leave it unchanged otherwise.

The author recommends against introducing any multi-file evidence convention.
The user floated splitting a worker's evidence across several files so later
readers can fetch one part; the existing rule already delivers that, because one
file with several `##` headings gives each part its own selector and each
selector its own `Evidence` row. Add a multi-file convention only if the audit
produces a concrete case the single-file-plus-selectors form cannot express, and
then present that case for a decision rather than deciding it inside this
change.

`/compile`'s `## Handoff` is audited under the same test as every other skill,
but the clean-success versus failure split in its envelope reporting is settled
by a separate change and is not re-litigated here; only a violation outside that
split is fixable in this Plan.

Two consequences to check before editing, because both are contracts other
sessions read:
- `.agents/skills/finalize-changes/references/landing-acceptance-table.md`
  fills rows from typed handoffs; a row must still resolve after a fix moves
  bytes to a file.
- `.agents/references/subagent-reporting.md` `## Task brief` requires callers to
  pass a worker's `Evidence` path plus selector to the next worker; a fix that
  moves bytes out of a handoff must leave the next worker a citable selector.

## Critical files
- `.agents/references/subagent-reporting.md` — `## Handoffs`: the shared form,
  the over-cap file rule (currently `:116-119`), the typed-envelope exemption
  (currently `:120-123`), and the "What main does with each field" table
  (currently `:158-169`); the audit's yardstick, edited only where the audit
  proves the exemption sentence false
- Every `.agents/skills/*/SKILL.md` `## Handoff` section — the audit subjects,
  edited only where a violation is proven
- Every `.agents/skills/*/references/*.md` clause that adds a handoff shape
  beyond its `SKILL.md` — audited, edited only on a proven violation
- `.agents/skills/finalize-changes/references/landing-acceptance-table.md` —
  read-only consumer whose rows must still resolve after any fix
- `.agents/references/skill-skeleton.md` — read-only: the shape a `## Handoff`
  section must keep

## In scope
- The audit described in `## Design` Phase 1, over every `## Handoff` section in
  `.agents/skills/*/SKILL.md`, every handoff shape added in a package's
  `references/*.md`, and every cap exemption in
  `.agents/references/subagent-reporting.md` `## Handoffs`
- The `Temp/` findings file holding one `##` section per audited skill
- Phase 2 prose edits confined to the `## Handoff` section of each skill proven
  to violate the test, and to the sections of that skill's `references/*.md`
  that state the same return shape
- A minimal wording change to the typed-envelope exemption sentence in
  `.agents/references/subagent-reporting.md`, only where the audit proves that
  sentence covers bytes main never decides on
- `/validate-skill` and `/progressive-disclosure-review` on every changed skill
  package

## Out of scope
- The shared handoff's field set, its row forms, its 40-line/20,000-character
  caps, and the "What main does with each field" table in
  `.agents/references/subagent-reporting.md` — the yardstick is not rewritten by
  the change it measures
- Any multi-file evidence convention, unless the audit proves a case the
  single-file-plus-`##`-selectors form cannot express; then it is presented for
  a decision, not implemented here
- The clean-success versus failure split in `/compile`'s envelope reporting,
  which a separate change settles
- `## Purpose`, `## When to use`, `## Inputs`, `## References`, frontmatter, and
  `agents/openai.yaml` in every skill package
- Every `.agents/skills/*/scripts/**` file, every typed schema those scripts
  emit, and every field such a schema carries
- The root `AGENTS.md` delegation role table, the Change Workflow steps, and
  `.claude/agents/*` and `.codex/agents/*` role definitions
- Any C++, GLSL, or project-membership change; adding any script; adding any
  test
- Rewriting a conforming skill's `## Handoff` for style, ordering, or wording

## Risk tier and invariants
Expected Tier 3 under the root `AGENTS.md` risk tiers; this author's
classification, to be confirmed at the Approve and classify step. The trigger is
that the change spans independently owned surfaces — the return contracts of
many separate skills plus the shared delegation reference every session reads —
and it can block other sessions, because a wrong edit to a return contract
changes what every future dispatch of that skill hands back. It touches no
determinism/CRC, wire/protocol, serialization, replay, or threading surface, and
no C++. Should the audit prove only a single skill violates the test and the
shared reference needs no change, the implementing session may reclassify to
Tier 2 and record that reclassification.

Invariants to preserve:
- Every check a skill performs today still happens; only what travels into main
  changes. No verification, finding, or pass condition is dropped
- Every byte a downstream consumer requires verbatim stays reachable verbatim,
  at a cited path plus selector when it leaves the handoff
- The landing acceptance table's rows still resolve from the handoffs and cited
  files the fixed skills document
- `Status`, `Build required`, and a last-position `Residuals` stay present in
  every skill's declared form; `Status` still carries exactly one token
- Each `SKILL.md` remains the public file, delegating mechanics to its
  references, per the root `AGENTS.md` progressive-disclosure directive
- Changed files keep their existing encoding and line endings; no transcript
  path or home path enters the repository

## Acceptance criteria
- The audit findings file names every `.agents/skills/*/SKILL.md` exactly once,
  each with a `conforms` or `violates` verdict, and for every violation the
  consumer that actually reads the bytes
- Every cap exemption in `.agents/references/subagent-reporting.md`
  `## Handoffs` has a findings row naming its consumer and stating whether that
  consumer is main
- Every skill the audit marks `violates` has a `## Handoff` that, after the fix,
  requires inline only material the "What main does with each field" table
  routes, with the remainder cited as path plus `##` selector
- No skill the audit marks `conforms` appears in the diff
- The diff touches only files named in `## In scope`
- `/validate-skill` and `/progressive-disclosure-review` pass on every changed
  skill package
- `pwsh -NoProfile -File .agents/scripts/Test-PlanSchedulerState.ps1` reports
  `status: valid` and `code: ok`

## Coordination
No mandatory constraint binds this Plan to another live Plan. The `/compile`
envelope work named in `## Design` is a directional overlap only: this Plan's
`## Out of scope` already excludes the split that work decides, so either order
of landing is safe.
