# Using the Jev decision model in this repository's workflow

Open question: where, if anywhere, would a cheap classify-only model earn its
place in this repository's skills and Change Workflow? This is the overview of
a series: it owns what Jev is, the places it must never decide, the decisions
every candidate shares, and the table of candidates. Each candidate is its own
Investigation, written so it can be tested on its own and promoted to a Plan
when its test passes. Three tracked scripts exist and no workflow skill uses
any yet: `.agents/scripts/Invoke-Jev.ps1` is the one caller every Jev use
goes through, `.agents/scripts/Test-CitationSupport.ps1` is the first
check built on it (`JevEvidenceCitationCheck.md`), and
`.agents/scripts/Test-StyleRuleJudgment.ps1` is the second, with its labelled
corpus (`JevStyleRuleJudgment.md`). The other pilots ran from
untracked scratch scripts against the live API; each candidate document states
its method precisely enough to rerun.

## The caller

`Invoke-Jev.ps1` takes `-RequestPath <json>` holding one `{state, questions}`
object or an array of them, posts each to the endpoint in parallel (eight at a
time, retrying only rate-limit and overload responses), and returns one result
document: `status`, `code`, `message`, `requestCount`, `failedCount`,
`inputTokens`, and `responses[]` in request order, each with the answers under
the question ids the request used. Without `-OutputPath` the document is the
whole of stdout, for an agent to read; with it the document is written to that
file and stdout carries one summary line, for a script to consume later. The
key is read from `TYPESAFE_API_KEY` and never printed. When the key is absent
the result is `blocked`/`jev.key-missing` with exit code 2; when every request
fails it is `blocked`/`jev.unavailable`; when some fail it is
`error`/`jev.partial` with the answered responses kept and each failed one
carrying its error. A consumer treats `blocked` as "the check did not run" and
does whatever it did before Jev existed, so a clone without a key loses only
the check. Answers are stable but not bit-identical between runs: over 625
identical citation questions two sweeps agreed on 605 verdicts, the
disagreements all near the 0.5 probability boundary, so a consumer orders by
probability rather than treating a verdict as exact.

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
  worse than the answer the repository already has. `JevPlanAreaFiling.md` is
  the measured case: its rule read like a judgment but is a count over named
  paths, and a script reproduced every filing, while Jev got 56 or 57 of 58. So
  before a pilot, write the rule's deterministic part as code and measure how
  much is left for the model; a candidate whose remainder is empty stops here.
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
| `JevPlanAreaFiling.md` | `Documents/Plans/AGENTS.md` areas; the Plans/Features/Investigations test | `choice` over areas | run twice: 56/58 and 57/58 agree, every flag a false flag; a path count reproduces all 58; the tree question turns on user intent the document does not carry | none: not promoted |
| `JevDuplicatePlanDetection.md` | `/create-follow-up-plans` duplicate rule | `score` with levels = actions (distinct, related, same fix) | run: the three merged Plans are the top three of 50 pairs; no distinct pair reached 1.0 | a second historical merge; a code-side shortlist |
| `JevEvidenceCitationCheck.md` | finding evidence rows, acceptance rows, Plan citations | `choice` supports / contradicts / says_nothing | run twice, tracked script: 440/625 real citations `supports`, 36/585 shifted regions; blind read of 66: passes right 20/22, flags right only 15/44 | reading order only; enclosing-function context, then a human blind read |
| `JevStyleRuleJudgment.md` | `/code-style-review` hand-read rules 3, 14, 16, 21, 41, 49, 51, 56, 61, 62 | one request per changed function with one `noul` per rule; rule 56 as one `noul` per name over the function's name list | run twice over 41 blocks: seven rules meet the success bar at 0.5 (no miss, at most one clean block flagged each), rule 56 needs the name-list form, rule 61 goes to a scanner, rule 3 needs 0.9 | rule 3 and rule 56 recorded beside the worker's verdict on real session changes |
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
   Both run offline over the Plan tree, cost nothing on the critical path, and
   report residuals rather than deciding anything. The citation check's full
   sweep showed its flags are a reading order and not a verdict, which is the
   shape every candidate is held to anyway; the duplicate check still has one
   concrete next test a human can finish in an hour.
2. `JevCommentBlockTriage.md` as the first in-round use, wired as a reading
   order only, after its second-commit measurement. `JevPlanAreaFiling.md` is
   not promoted: its second pilot showed the filing rule is a path count a
   script reproduces exactly, so it falls under `## Not suitable`.
3. `JevStyleRuleJudgment.md` next: its pilot met its own success bar for
   seven of ten hand-read rules over hand-labelled blocks; its Plan is decided
   apart from one style-guide question for the user, and rules 3 and 56 stay
   reading-order only until a second measurement on real session changes.
4. Every remaining candidate waits for a corpus that does not exist yet, and
   `JevSimplicityReviewTrigger.md` and `JevFindingTriage.md` are the cheapest
   ways to start collecting one, because they record Jev's answer beside a
   decision the workflow already makes.

## Decisions every Plan in the series shares

1. Decided — access route and location: `.agents/scripts/Invoke-Jev.ps1`
   (`## The caller` above), a PowerShell 7 `Invoke-RestMethod` call with no
   vendored SDK, shared because several skills would call it, invoked exactly
   as the bundled-scripts rule in root `AGENTS.md` states. A check script
   calls it in-process and never makes its own HTTP call.
2. Decided — the key and failure: `TYPESAFE_API_KEY`, never tracked and never
   printed; a missing key or unreachable service is `blocked`, and every
   consumer then behaves as the workflow does without Jev. A reading-order use
   falls through silently; a sweep that reports residuals says it did not run.
3. Threshold policy: every threshold is a number written in the owning skill's
   references, chosen from the candidate's measurement, and every use errs
   toward an extra read rather than a miss.
4. No gate: no candidate hides an item from a reviewer, lowers a tier, skips a
   review, or dispatches a fix on its own until a second measurement on a
   later change confirms the first — and the vendor's warning about
   thresholds not composing applies the moment one does.
