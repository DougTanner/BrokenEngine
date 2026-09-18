# Using the Jev decision model in this repository's workflow

Open question: where, if anywhere, would a cheap classify-only model earn its
place in this repository's skills and Change Workflow? This document lists the
places the workflow already makes a fixed-choice decision, says for each what
Jev would be asked and what would still need a full model, compares the
strongest candidates, recommends one first experiment, and lists the decisions a
Plan would have to make. Nothing here is implemented: no script calls Jev, no
skill mentions it, and no script under `.agents/scripts/` makes a network call
of any kind today.

## What Jev is

Jev, from TypeSafe AI, is a model that cannot write text. It takes a state (a
string, a JSON object, or an array of texts — no images) plus a set of named
questions and answers all of them in one pass. A question is a `choice` (one of
up to 255 named options, each with a description), a `score` (2 to 10 ordered
levels), or a boolean (a 0 to 1 probability); choice and score answers also carry
a confidence value. It runs in 70 to 500 ms, costs $0.042 per million input
tokens with output free, accepts 64k tokens per request and 32k for the state
plus the longest question, and allows 1,200 requests per minute. On the vendor's
own workflow evaluations it agreed with the reference answer about 68% of the
time — the same as Claude Sonnet 5, below Claude Opus 5 at 73% — for a few
hundredths of the cost, and the vendor's own advice is to keep any real decision
on a full model until Jev has been checked against your own labelled examples.

Two consequences shape everything below. Jev can only choose among options
someone else wrote down, so it is useless wherever the workflow needs prose: a
replacement comment, a fix, a finding's evidence sentence. And at roughly two
answers in three it cannot be the last word on anything that blocks a change;
the honest shape is "Jev shortens the list a full model then reads", never "Jev
decides".

## Where the workflow already makes fixed-choice decisions

Each candidate names the skill, quotes the text that fixes the choice, states
the question Jev would answer, names what enumerates the candidates and supplies
the file and line (always a script, never Jev), says what still needs a full
model, and says what happens when Jev is wrong.

### 1. Classifying comment blocks for `/comment-review`

- Where it is fixed: `.agents/skills/comment-review/references/comment-classes.md:9-14`
  is a six-row table — `boilerplate`, `history`, `speculative`, `navigation`,
  `false`, `dense` — each with the rule 64 clause it enforces and a fixed
  severity. Every finding row must carry exactly one of those six
  (`.agents/skills/comment-review/SKILL.md:59`). The worker's step 3 is
  literally "classify every scanned block by reading the surrounding code"
  (`.agents/skills/comment-review/references/worker.md:37-44`).
- Question: one `choice` per block over seven options — the six classes plus
  "ok, no finding". The enumeration already exists:
  `.agents/skills/comment-review/scripts/Find-CommentBlocks.ps1` walks the
  scoped files and emits one row per run of consecutive `//` lines with `path`,
  `startLine`, `lineCount`, `kinds`, and `firstLine`
  (`Find-CommentBlocks.ps1:105-111`), so locations never come from Jev. One gap:
  it emits only the block's first line, not the block text or the code beneath
  it, so whatever calls Jev must assemble the state from the file.
- Still needs a full model: the replacement text, "the shortest present-tense
  replacement keeping every preserved fact"
  (`.agents/skills/comment-review/SKILL.md:18-20`) — writing, not choosing. The
  gain is that the reviewer reads the flagged blocks rather than all of them.
- If Jev is wrong: a false positive costs the reviewer one block read and is
  caught, because the reviewer must still confirm the class against the code
  before writing a replacement. A false negative silently drops a finding — the
  real risk, and the reason a threshold has to be set to flag generously.
- Weak spot: `false` ("every statement is true of the adjacent code as it
  stands", `comment-classes.md:13`). Deciding it needs real understanding of the
  code, and `comment-classes.md:34-38` records that no repository instance was
  ever located. That class should stay with the reviewer whatever else happens.

### 2. Judgment-only style rules for `/code-style-review`

- Where it is fixed: the worker hand-reads a named rule list because the
  scanner does not emit candidates for it — "rules 3, 14, 16 …, 21, 49, 51, 56,
  61, 62" (`.agents/skills/code-style-review/references/worker.md:36-44`). Of
  those, the ones that are pure judgment on a short piece of text are rule 49,
  no accessor or one-line pass-through functions
  (`Documents/C++StyleGuide.txt:201`); rule 56, complete words in names, no
  abbreviations (`:264`); and rule 62, one `if` per guard condition instead of
  a packed `A || B || C` (`:299`).
- Question: one boolean per candidate ("does this violate rule N as quoted?").
- Enumeration: does not exist yet for these three. `Find-SessionCandidates.ps1`
  emits a `style-rule-<n>` kind per rule it covers
  (`.agents/scripts/Find-SessionCandidates.ps1:44-55`), and 49, 56, and 62 are
  not among them — that is exactly why the worker hand-reads them. So this
  candidate costs a new scanner (declarations, identifiers, guard conditions)
  before a Jev call has anything to answer about. Rule 57 (`Impl`/`Internal`
  suffixes, `Documents/C++StyleGuide.txt:266`) is the opposite case: the scanner
  already emits `style-rule-57` (`Find-SessionCandidates.ps1:54`) and the rule
  has no exceptions worth judging, so Jev would add nothing there.
- Still needs a full model: the fix, and whether it is meaning-preserving — the
  worker may only auto-fix when "the resulting C++ meaning is demonstrably
  unchanged" (`.agents/skills/code-style-review/references/worker.md:70-73`).
- If Jev is wrong: same shape as candidate 1, but a missed rule-49 or rule-56
  violation lands in the code, and the enumeration gap makes this the more
  expensive experiment for the same kind of payoff. A whole-file "does this file
  follow the style guide" boolean is no substitute: the handoff needs file,
  line, rule number, and correction
  (`.agents/skills/code-style-review/SKILL.md:41`).

### 3. The `/plan-simplicity-review` dispatch trigger

- Where it is fixed: the Change Workflow says to dispatch the review "when the
  plan adds new code or changes non-documentation behavior … when unsure
  whether a plan triggers it, dispatch it"
  (`.agents/references/change-workflow.md:93`). The skill defines both terms:
  new code is "a new tracked file, function, class, system, script, guard or
  recovery path, or configuration surface absent at the session baseline"
  (`.agents/skills/plan-simplicity-review/SKILL.md:33-36`), and a skill edit is
  behavior when it changes frontmatter, invocation, routing, a bundled script, a
  workflow step, a contract, a trigger, or a threshold — documentation when it
  only rewords (`:48-55`).
- Question: one boolean over the plan text, with a deliberately low threshold so
  the "when unsure, dispatch" default survives. No enumeration is needed; a plan
  file sits well inside the 32k-token state limit. The review itself is
  unchanged.
- Still needs a full model: the review itself. Jev would decide only whether to
  dispatch `/plan-simplicity-review`; the dispatch still runs the reviewer
  subagent, which reads the plan and writes the findings.
- If Jev is wrong: a false positive costs one extra reviewer dispatch, which is
  what the current rule already chooses on purpose; a false negative skips a
  review the workflow requires. So the saving is capped at the dispatches main
  would have made under uncertainty, while the error runs only in the expensive
  direction. Useful mainly as a low-stakes place to measure agreement.

### 4. Risk tier classification

- Where it is fixed: three tiers with written boundaries, "classify the whole
  change at the highest applicable tier before implementing"
  (`.agents/references/risk-tiers.md:5-9`), locked in by main at the Approve and
  classify step (`.agents/references/change-workflow.md:75-79`), and a reviewer
  "may escalate the tier when the changed bytes expose a higher-risk surface"
  (`risk-tiers.md:11`).
- Question: one three-way `choice` over the diff plus the tier text. The changed
  files and regions come from `.agents/scripts/Get-SessionChangeInventory.ps1`,
  which every review's scope already derives from
  (`.agents/skills/code-style-review/references/worker.md:14-27`).
- Still needs a full model: the classification itself, because the tier decides
  which reviews run.
- If Jev is wrong: downgrading a tier skips required reviews, the worst failure
  in this list. The only safe wiring is a second opinion allowed to agree or
  escalate and never to downgrade, matching the escalation sentence already in
  `risk-tiers.md:11`. Even then a whole diff is often larger than one state, so
  the question would be asked per file and combined — and the vendor's warning
  that calibrated single answers do not stay calibrated once thresholds are
  applied lands directly on that combining step.

### 5. Triage of accepted review findings

- Where it is fixed twice. Main decides each finding and is told to "be
  especially careful with findings that add guards, options, or machinery for
  cases nobody has observed (YAGNI and over-engineering)"
  (`.agents/references/change-workflow.md:30`). Then every fix dispatch must
  carry "intent `conformance` or `plan_delta`, and scope `non_structural` or
  `structural`" (`.agents/skills/resolve-findings/SKILL.md:32`), and the fixer
  accepts only `conformance + non_structural` (`:41-47`).
- Question: per finding, one boolean for "this adds a guard or machinery for a
  case nobody has observed", plus the two two-way choices the fix dispatch
  needs. A `score` for how reachable the described failure is would add a
  ranking, but nothing in the workflow consumes such a number today. No
  enumeration is needed: each finding is already one line with an ID, path,
  claim, and evidence (`.agents/references/subagent-handoff.md:15`), and its
  severity is already assigned by its reviewer
  (`.agents/skills/repo-code-review/SKILL.md:82-86`). Accepting or rejecting the
  finding stays with main, which is where the judgment lives.
- If Jev is wrong: a wrong intent or scope label sends a fix to a worker that
  must refuse it, which the fixer already handles by returning
  `PLAN DELTA REQUIRED: yes` without editing
  (`.agents/skills/resolve-findings/SKILL.md:41-43`). That self-correcting
  refusal makes this the safest of the five, but the volume is low — a handful
  of findings per round — so the saving is correspondingly small.

### Weaker possibilities, listed once

`/progressive-disclosure-review` findings carry a three-way class,
`duplication | misplacement | size`
(`.agents/skills/progressive-disclosure-review/SKILL.md:64`), but `size` is a
measurement and `duplication` needs the other location that already states the
fact — a search Jev cannot do; it could only confirm a pair a script proposed.
`/coherence-review` and `/verify-acceptance` produce verdicts whose content is
the reasoning, not the label.

## Not suitable

- Anything a script already decides: Plan selection is "the newest eligible
  executable plan by `(createdUtc descending, normalized path)`"
  (`Documents/Plans/AGENTS.md:13`), file classes come from the change inventory,
  and executable membership is deterministic validation
  (`.agents/skills/update-vcxproj/SKILL.md`). A model answer would be strictly
  worse than the answer the repository already has.
- Anything that must be the evidence of record. The root `AGENTS.md` Diagnosis
  Discipline directive requires a root cause confirmed from close code
  inspection or evidence, and every finding must carry its evidence
  (`.agents/references/subagent-handoff.md:15`). A probability is not evidence.
- Anything needing an image: Jev takes none, so every screenshot check in
  `/agent-harness` is out.
- Anything gating a landing. Exactly one user confirmation authorizes changing
  primary (`.agents/references/change-workflow.md:152-156`); nothing at 68%
  agreement belongs on that path.

## Comparing the top three

Judged on the same four criteria: whether labelled examples already exist to
validate against, the cost of a wrong answer, how much has to be built, and
what a correct answer actually saves.

| | 1 comment classes | 2 style rules 49/56/62 | 3 simplicity trigger |
|---|---|---|---|
| Labels available now | yes — commit `65255669` is a comment cleanup across 123 files, 615 insertions and 1275 deletions; every block it changed is a positive example with its class implied by the edit, and the blocks it left alone in those files are negatives | no — would have to be hand-labelled | thin — one boolean per past plan, and few plans record why the trigger fired |
| Cost of a wrong answer | false positive: one extra block read; false negative: a missed finding in a findings-only review that never gates a landing | same shape, but a missed rule-49 or rule-56 violation lands in the code | false negative skips a required review |
| What must be built | a caller for the existing scanner plus block-text extraction, since `Find-CommentBlocks.ps1:105-111` emits only `firstLine` | a whole new candidate scanner for three rules that have none, then the caller | a caller only |
| What it saves | the reviewer reads the flagged blocks instead of every block in scope; the scanner already emits up to 400 blocks per run (`Find-CommentBlocks.ps1:14`) | the hand-read pass over three rules across the changed ranges | occasional reviewer dispatches main would have made anyway |

Cost is not a differentiator: at $0.042 per million input tokens, a few hundred
comment blocks with their surrounding code is a fraction of a cent, and at 1,200
requests per minute a whole scope finishes in well under a minute.

## Recommendation

Run candidate 1 first, as a measurement and not as a gate.

The reason is the first row of the table: this repository already contains a
labelled corpus for it. Commit `65255669` ("Clarify codebase comments and record
pack reset race") is a comment cleanup across 123 files. The six worked examples
in `.agents/skills/comment-review/references/comment-classes.md:18-40` were
written against the state before it — `Common/WindowsUtils.h:35-43` no longer
holds the `Parameters:`/`Returns:`/`Thread-safety:` fields the example
describes, `Engine/Source/Frame/IslandTerrain.cpp:337` is now ordinary code,
`Common/Log/Log.h:214` is now the `LOG` macro definition, with an unrelated
comment above it at 212-213 — and `git log -S`
confirms `65255669` removed the first two. That makes the commit's diff a
ready-made answer key: the removed blocks with the class each one broke, and the
untouched blocks in the same files as the "ok" cases.

How to validate before it influences anything: take the blocks that commit
changed and the ones it left alone in the same files, hand-assign each a class
from the six in `comment-classes.md:9-14` or "ok", ask Jev the same question for
each, and record the chosen option, its probability, and its confidence. Report
two numbers separately, because they cost different amounts: how often Jev
flagged a block the sweep changed (the misses are the expensive error), and how
often it named the same class. Pick the flag threshold from that measurement
rather than in advance, erring toward extra reads. Only then wire it as a
reading order — flagged blocks first, every other block still read — and never
as a filter that hides blocks until a second measurement on a later change
confirms the first.

Do not start with candidate 2: it needs a scanner built before a single question
can be asked, and its answer key would have to be hand-made. Candidate 3 is
cheap to wire but can only err in the expensive direction. Candidates 4 and 5
should wait for a measured agreement number from candidate 1 — candidate 4
because a wrong tier skips required reviews, candidate 5 because the volume is
too low to pay for the integration on its own.

## Decisions a Plan needs

1. Access route: a PowerShell 7 script using `Invoke-RestMethod` against the
   HTTPS endpoint, or one of the vendor SDKs. PowerShell 7 is the repository
   default (root `AGENTS.md` `## Environment`) and no vendor SDK is vendored,
   so the raw call is the smaller change — but it would be the first repository
   script to touch the network at all, which is itself a decision.
2. Where the script lives: bundled under the owning skill like
   `.agents/skills/comment-review/scripts/Find-CommentBlocks.ps1`, or shared in
   `.agents/scripts/`; either way it is invoked exactly as the bundled-scripts
   rule in root `AGENTS.md` states. Separate from the existing scanner, most
   likely, since that one is documented as writing nothing and being safe under
   a read-only sandbox (`Find-CommentBlocks.ps1:1-5`) and a network call breaks
   that property.
3. How the block text and its surrounding code reach the state, given that the
   scanner emits only `startLine`, `lineCount`, and `firstLine` today, and how
   much surrounding code is enough to judge `dense` and `false`.
4. The key: a user-level environment variable, never tracked, and what the
   script does when it is absent — refuse, or fall through to the current
   all-blocks-read behavior. Same question for a failed call or a hit rate
   limit; the existing scanner sets the opposite precedent to a fall-through —
   a `blocked` or `error` status "means the block list is unavailable — report
   that rather than hunting comment blocks by hand"
   (`.agents/skills/comment-review/references/worker.md:27-30`).
5. Threshold policy and what a low confidence means. The candidate proposal is
   "flag on low probability, and treat low confidence as a flag", so uncertainty
   always costs a read and never a miss; a Plan must state the numbers and where
   they are written down.
6. Whether any measurement is ever allowed to become a gate, and what evidence
   would authorize that — the vendor's warning about calibrated single answers
   not composing into a calibrated workflow applies the moment a threshold hides
   anything from a reviewer.
7. Nothing: the stale examples in `comment-classes.md:18-40` are corrected
   separately, by
   `Documents/Plans/ChangeWorkflow/CommentClassStaleExamples.md`, which now owns
   that defect. It is independent of anything Jev-related, so no Jev Plan need
   wait on it or repeat it.
