<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-06T16:37:53.888Z","dependsOn":[]} -->
# Fix: `/adversarial-review` and `/resolve-findings` — unbounded handoff prose returns more than main decides on

## Context

A `/next-plan` run's checkpoint review observed two handoffs returning
reasoning main had no action for.

`/adversarial-review`: the `Traced clean` field arrived as multi-clause
paragraph rows carrying the full refutation argument for each hypothesis that
produced no finding, in both the review handoff and its focused re-review
handoff. The emitter is
`.agents/skills/adversarial-review/SKILL.md` `## Handoff`, whose `Traced clean`
bullet states only "hypotheses traced and decisive refutation, or not
applicable" — no row form at all, unlike the one-line `Findings` fence declared
directly below it in the same section.

`/resolve-findings`: the declared compact item table arrived with a
multi-clause `Confirmed root cause and evidence` cell, and the handoff added
free prose beyond its declared fields — a worker-step commentary paragraph, an
out-of-scope-candidates line, and a multi-sentence self-audit paragraph. The
emitter is `.agents/skills/resolve-findings/SKILL.md` `## Handoff`, which
declares the item table's five columns but states no per-cell length bound and
no prohibition on prose outside the declared fields.

Both are the same root-cause class — a declared field with no bound, so the
worker fills it with argument main routes nothing from — and both take the same
fix shape, so they are recorded as one Plan.

The observation is out of scope of the change the observing session landed:
that change edited only the `## Handoff` sections its own audit proved to
violate its test, and its audit findings file recorded both of these skills as
`conforms` under the yardstick that existed before that session landed the
sharpened wording of `.agents/references/skill-skeleton.md` `## Section order`
item 5. Neither `.agents/skills/adversarial-review/SKILL.md` nor
`.agents/skills/resolve-findings/SKILL.md` appears in that session's changed
files.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: b15337ed-cbeb-4e28-9536-e4cfd385d660
- Worktree/branch UUID: 4f413250-4e4e-4c0f-9c0f-c49aa01d4ab6
- Session branch: claude/4f413250-4e4e-4c0f-9c0f-c49aa01d4ab6
- Worktree: .claude\worktrees\BrokenEngine\4f413250-4e4e-4c0f-9c0f-c49aa01d4ab6
- Landing ref: claude/4f413250-4e4e-4c0f-9c0f-c49aa01d4ab6, this session's
  branch, whose landing commit will contain this Plan.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design

First root-cause the friction from the current tree and this Plan's
`## Context`. Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <landing ref>` in bounded friction mode, supplying the
recorded client and conversation session ID. Then make the smallest fix inside
the `## In scope` boundary below. If root-causing shows the fix lies outside
that boundary, surface it for re-planning instead of expanding scope.

The author recommends bounding each field to one line per item on a declared
row form, with any longer trace or reasoning written to a `Temp/` file the
handoff cites under `Evidence` as path plus `##` selector. That is the shape
`.agents/references/skill-skeleton.md` `## Section order` item 5 already
prescribes — mandate inline only what the "What main does with each field"
table in `.agents/references/subagent-reporting.md` `## Handoffs` gives main an
action for, and no more text than that action needs — so the fix applies an
existing rule to two sections rather than inventing a convention. Re-read that
item as it then stands; the observing session was landing a change to it.

Recommended, subject to what root-causing finds:

1. `.agents/skills/adversarial-review/SKILL.md` `## Handoff`: give
   `Traced clean` a one-line-per-hypothesis row form, in the style of the
   `Findings` fence already in that section — for example
   `<hypothesis> — <decisive refutation> path:line` — and state that a longer
   trace goes to a `Temp/` file cited under `Evidence` as path plus `##`
   selector.
2. `.agents/skills/resolve-findings/SKILL.md` `## Handoff`: bound the item
   table's cells to `path:line` plus one clause, and state that the reasoning
   behind a cell goes to a `Temp/` file cited under `Evidence` as path plus
   `##` selector, and that no prose outside the declared fields is returned.

Change no field set, no status vocabulary, and no check either skill performs;
only what travels into main changes, and every byte a later reader needs stays
reachable at a cited path plus selector.

## Critical files

- `.agents/skills/adversarial-review/SKILL.md` — `## Handoff`, the `Traced
  clean` bullet and the `Findings` row-form fence beside it.
- `.agents/skills/resolve-findings/SKILL.md` — `## Handoff`, the item-table
  fence and the extension-field bullets below it.
- `.agents/references/skill-skeleton.md` — `## Section order` item 5 and the
  `## Reviewer checklist`; read-only yardstick for the fix.
- `.agents/references/subagent-reporting.md` — `## Handoffs`, the shared form,
  the over-cap file rule, and the "What main does with each field" table;
  read-only.

## In scope

- Root-cause investigation as `## Design` states.
- The smallest resulting prose fix, confined to the `## Handoff` section of
  `.agents/skills/adversarial-review/SKILL.md` and the `## Handoff` section of
  `.agents/skills/resolve-findings/SKILL.md`.
- The matching clause in either package's `references/worker.md` only where that
  file restates the same return shape and would otherwise contradict the fixed
  `## Handoff`.
- `/validate-skill` and `/progressive-disclosure-review` on both changed skill
  packages.

## Out of scope

- Every other skill package's `## Handoff`.
- `.agents/references/subagent-reporting.md`: its shared field set, its row
  forms, its size caps, its exemptions, and the "What main does with each
  field" table.
- `.agents/references/skill-skeleton.md` itself.
- `## Purpose`, `## When to use`, `## Inputs`, `## References`, frontmatter, and
  `agents/openai.yaml` in both packages, and every file under either package's
  `scripts/`.
- What either skill checks, its status vocabulary, and its field set; only the
  bound on what is returned inline changes.
- The landed change the observing session produced.
- Any transcript path or transcript text in the repository; any C++, GLSL, or
  project-membership change; adding any script.

## Risk tier and invariants

Expected Tier 2 under the root `AGENTS.md` risk tiers; this author's
classification, to be confirmed at the Approve and classify step. The trigger is
scoped tool behavior — the return contract of two individually owned skills,
with the field set, the status vocabulary, and the trust boundary unchanged.
Escalate to Tier 3 if the fix reaches `.agents/references/subagent-reporting.md`
or a third skill package, because the shared reference is one every session
reads.

Invariants to preserve:
- Every check either skill performs today still happens; no finding, refutation,
  or pass condition is dropped, only relocated.
- Every byte a later reader needs verbatim stays reachable at a cited path plus
  `##` selector.
- `Status`, `Build required`, and a last-position `Residuals` stay present in
  both declared forms; `Status` still carries exactly one token.
- Each `SKILL.md` remains the public file per the root `AGENTS.md`
  public/private skills directive.
- Changed files keep their existing encoding and line endings; no transcript
  path or home path enters the repository.

## Acceptance criteria

- `.agents/skills/adversarial-review/SKILL.md` `## Handoff` declares a
  one-line-per-hypothesis form for `Traced clean` and routes a longer trace to a
  cited `Temp/` file.
- `.agents/skills/resolve-findings/SKILL.md` `## Handoff` bounds each item-table
  cell to `path:line` plus one clause, routes the reasoning to a cited `Temp/`
  file, and states that no prose outside the declared fields is returned.
- No field is removed from either declared form, and no other skill package
  appears in the diff.
- `/validate-skill` reports both packages valid, and
  `/progressive-disclosure-review` raises no finding of this class against
  either.
- `pwsh -NoProfile -File .agents/scripts/Test-PlanSchedulerState.ps1` reports
  `status: valid` and `code: ok`.
