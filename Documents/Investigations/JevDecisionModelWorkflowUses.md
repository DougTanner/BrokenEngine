# Using the Jev decision model in this repository's workflow

Open question: where, if anywhere, would a cheap classify-only model earn its
place in this repository's skills and Change Workflow? This is the overview of
a series: it owns what Jev is, the places it must never decide, the decisions
every candidate shares, and the table of candidates. Each candidate is its own
Investigation, written so it can be tested on its own and promoted to a Plan
when its test passes. Nothing in the series is implemented: no tracked script
calls Jev, no workflow skill uses it, and no script under `.agents/scripts/`
makes a network call of any kind today. The pilots below ran from untracked
scratch scripts against the live API; each candidate document states its method
precisely enough to rerun.

## What Jev is

Jev, from TypeSafe AI, is a model that cannot write text. It takes a state (a
string, a JSON object, or an array of texts — no images) plus a set of named
questions and answers all of them in one pass. A question is a `choice`, a
`score`, or a `noul`, defined in `/external-typesafe-ai`
(`.agents/skills/external-typesafe-ai/SKILL.md`), which every candidate below
names by those terms. The current model is `jev-1.13.0`, reached as
`jev-latest` at
`POST https://api.typesafe.ai/v1/systemone` with a bearer key. It accepts 64k
tokens per request and 32k for the state plus the longest question, costs
$0.042 per million input tokens with output free, and allows 1,200 requests per
minute. Two vendor warnings shape every candidate: accuracy falls as the state
grows with material unrelated to the decision, so code filters first and sends
only the fields a question needs; and calibrated single answers do not stay
calibrated once thresholds are applied and answers combined, so a threshold is
chosen from measurement on this repository's data, never in advance. English is
its primary language.

Two consequences follow. Jev can only choose among options someone else wrote
down, so it is useless wherever the workflow needs prose: a replacement
comment, a fix, a finding's evidence sentence. And it cannot be the last word
on anything that blocks a change; the honest shape is "Jev shortens or orders
the list a full model then reads", never "Jev decides".

## Not suitable

- Anything a script already decides: Plan selection is "the newest eligible
  executable plan by `(createdUtc descending, normalized path)`"
  (`Documents/Plans/AGENTS.md`), file classes come from the change inventory,
  and executable membership is deterministic validation
  (`.agents/skills/update-vcxproj/SKILL.md`). A model answer would be strictly
  worse than the answer the repository already has.
- Anything that must be the evidence of record. The root `AGENTS.md` Diagnosis
  Discipline directive requires a root cause confirmed from close code
  inspection or evidence, and every finding must carry its evidence
  (`.agents/references/subagent-handoff.md:15`). A probability is not evidence.
- Anything needing an image: every screenshot check in `/agent-harness` is out.
- Anything gating a landing. Exactly one user confirmation authorizes changing
  primary (`.agents/references/change-workflow.md`, Verify and land step);
  nothing on that path.
- Verdicts whose content is the reasoning, not the label: `/coherence-review`
  and `/verify-acceptance` produce those, so Jev can check their citations
  (`JevEvidenceCitationCheck.md`) but not replace them.

## Candidates

| document | where the choice is fixed today | primitive | pilot | next test |
|---|---|---|---|---|
| `JevCommentBlockTriage.md` | `/comment-review`'s six classes | `choice` + `ok` per block; `ok` probability orders reading | run: at `ok` < 0.5, 35/40 changed blocks flagged, 13/40 untouched flagged | hand-labelled 100 blocks from a second commit |
| `JevPlanAreaFiling.md` | `Documents/Plans/AGENTS.md` areas; the Plans/Features/Investigations test | `choice` over areas | run: 56/58 agree; both misses are the borderline cases and the lowest confidences | full tree plus Investigations and Features at a 0.6 gate |
| `JevDuplicatePlanDetection.md` | `/create-follow-up-plans` duplicate rule | `score` with levels = actions (distinct, related, same fix) | run: the three merged Plans are the top three of 50 pairs; no distinct pair reached 1.0 | a second historical merge; a code-side shortlist |
| `JevEvidenceCitationCheck.md` | finding evidence rows, acceptance rows, Plan citations | `choice` supports / contradicts / says_nothing | run: 27/30 real citations `supports`, 5/30 shifted regions | human read of the three flagged real citations; Plans predating a refactor |
| `JevStyleRuleJudgment.md` | `/code-style-review` hand-read rules 49, 56, 62 | one `noul` per rule per candidate | not run: scanner needed first | 60 hand-labelled candidates per rule |
| `JevSimplicityReviewTrigger.md` | `/plan-simplicity-review` dispatch trigger | two `noul`s, low threshold | not run | record beside main's decision for 30 plans |
| `JevRiskTierSurfaceFlags.md` | `risk-tiers.md` Tier-3 surfaces | one `noul` per surface per hunk, max-gated, escalate-only | not run: no tier corpus | record the tier per landed change, then 40 changes |
| `JevFindingTriage.md` | `/resolve-findings` intent and scope; the YAGNI warning; severity rule | four questions per finding in one request | not run: findings are not stored | record `(finding, labels, decision)` for 50 findings |
| `JevAffectedFileRanking.md` | `/update-affected-code` and `/prepare-change` search order | one `noul` per (change, hit) as sort key | not run | 20 Plan-completing commits: changed files versus search hits |
| `JevSkillRouting.md` | skill `description` matching | wide `choice` plus gating `noul`s, then a top-three re-read | not run: transcripts are local-only | 100 labelled past requests |
| `JevAgentsDocRubric.md` | `/update-claude-docs` six-criterion audit rubric | six `score`s per document, weighted in code | not run | past audit reports, or one full-model audit as labels |
| `JevTranscriptIntervalClassification.md` | `/next-plan-review` control-work classes; checkpoint review's result classes | three `choice`s per transcript interval | not run: needs an interval extractor | 300 intervals from three reviewed sessions |

Smaller fixed choices the sweep of the workflow found, not worth their own
document yet: whether a `/compile` change set may affect generated bytes and so
needs Local mode (`.agents/skills/compile/references/runtime-data-mode.md:18`),
a `noul` over the changed path list; whether a headless `/codex-review` result
is a review of its scope or drafting notes (`codex-review/SKILL.md:185-188`),
a `noul` over the result; and the `worth presenting` verdict on a
`/plan-alternatives` candidate, which is reasoning main should keep. The
handoff `Status`, per-criterion `PASS | FAIL | BLOCKED`, and every
acceptance-table cell are evidence of record and stay out.

Patterns from the TypeSafe docs that the series does not yet use, kept here so
a later candidate starts from them: feature discovery (Jev answers as columns
for a fitted predictor of, say, review escalation, which needs the corpora the
candidates above would create); hierarchical classification with beam search
(for a nested taxonomy larger than any this workflow has); structure recovery
without regeneration (classifying blocks of a plan rather than rewriting
them); and guardrail batteries over agent output, which `JevRiskTierSurfaceFlags.md`
already applies to diffs.

## Recommendation

Promote in the order the pilots justify, not the order of expected saving:

1. `JevEvidenceCitationCheck.md` and `JevDuplicatePlanDetection.md` first.
   Both run offline over the Plan tree, cost nothing on the critical path,
   report residuals rather than deciding anything, and their pilots separated
   the labelled cases cleanly. Each has one concrete next test that a human can
   finish in an hour.
2. `JevPlanAreaFiling.md` next, as a check inside the same tree sweep once
   the first two exist, since its wiring is nearly the same script.
3. `JevCommentBlockTriage.md` as the first in-round use, wired as a reading
   order only, after its second-commit measurement.
4. Every remaining candidate waits for a corpus that does not exist yet, and
   `JevSimplicityReviewTrigger.md` and `JevFindingTriage.md` are the cheapest
   ways to start collecting one, because they record Jev's answer beside a
   decision the workflow already makes.

## Decisions every Plan in the series shares

1. Access route: a PowerShell 7 script using `Invoke-RestMethod` against the
   HTTPS endpoint, or a vendor SDK. PowerShell 7 is the repository default
   (root `AGENTS.md` `## Environment`) and no SDK is vendored, so the raw call
   is the smaller change — but it would be the first tracked script to touch
   the network at all, which is itself a decision, and one shared caller script
   is the obvious place for it.
2. Where that caller lives: bundled under the first owning skill, or shared in
   `.agents/scripts/` because several skills would call it; either way it is
   invoked exactly as the bundled-scripts rule in root `AGENTS.md` states.
3. The key: the `TYPESAFE_API_KEY` user-level environment variable the vendor
   SDKs also read, never tracked and never printed. What a caller does when it
   is absent or a call fails: refuse and report the list unavailable, which is
   the precedent `Find-CommentBlocks.ps1` sets, or fall through to the
   behaviour the workflow has today. A reading-order use can fall through; a
   sweep that reports residuals should say it did not run.
4. Threshold policy: every threshold is a number written in the owning skill's
   references, chosen from the candidate's measurement, and every use errs
   toward an extra read rather than a miss.
5. No gate: no candidate hides an item from a reviewer, lowers a tier, skips a
   review, or dispatches a fix on its own until a second measurement on a
   later change confirms the first — and the vendor's warning about
   thresholds not composing applies the moment one does.
