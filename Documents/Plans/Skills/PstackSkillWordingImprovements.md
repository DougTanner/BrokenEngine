<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-19T21:56:06.487Z","dependsOn":[]} -->
# Sharpen existing skill decision guidance from pstack

## Context

The clean `cursor/plugins` source snapshot at commit
`60c641e4fad674784b30abcf9f8915dea39df38d` was compared with the Broken Engine
skill packages. The resulting evidence is recorded in
`Temp/pstack-review-summary.md`; the current repository preparation is based on
primary `f48889a27ac7d278057a4b43fa015e90b1bac710`. That review rejected
standalone workflow imports where existing repository owners already cover the
capability, but it identified nine owner-local places where a more precise
decision rule would change how an existing review or authoring step judges a
case.

The user decision is to schedule a worthwhile semantic refinement even when its
owning skill already states the broad principle. This Plan therefore consolidates
those nine wording refinements into one implementation wave. It does not import
source-package mechanics, and it does not turn the rejected cross-session
preference-mining or restart-state-diagnosis ideas into repository behavior.

## Why this is worth integrating

Broad principles are not always enough at a decision boundary. A reviewer can
agree with “keep the design simple” yet miss a real consumer, a plan author can
describe a repeated failure without choosing the smallest durable enforcement,
and a code reviewer can notice a suspicious primitive or cast without knowing
which evidence makes it actionable. The nine refinements add a concrete test at
each existing owner while preserving that owner's current workflow.

Keeping the refinements in the existing skills gives future sessions one place
to find the decision, avoids competing standalone principles, and makes the
change statically reviewable. The value is the changed judgment at the existing
gates, not additional invocation machinery or a broader process.

## Design

Edit only the named local sections. Use independently phrased guidance; retain
the surrounding workflow, frontmatter, links, output templates, and client
controls. Each refinement is a bounded decision aid, not a new workflow.

### 1. Code-style cleanup and semantic comment candidates

In `.agents/skills/code-style-review/SKILL.md` under `## Session Cleanup`,
sharpen the changed-comment directive so that it preserves a comment's
technical constraint, removes only process or navigation wording made redundant,
and routes a comment that calls for semantic enforcement to the owning review or
implementation path. Keep the technical comment until the enforcement exists.

### 2. Durable, decision-changing next-plan improvements

In `.agents/skills/next-plan-review/SKILL.md`, refine the improvement filter in
the assessment and proposed-improvement guidance. Require a candidate to have
direct evidence, a durable signal rather than a one-off preference, a plausible
effect on a future decision, an identified existing owner when it is a
correction, and proof of a concrete missing or ambiguous decision rule in that
owner, even when its broad principle already exists. Route a
genuinely unowned capability through the existing capability path instead of
forcing it into a correction.

For a proven recurring failure, prefer the following compact enforcement ladder
in order: make the invalid state unrepresentable; use deterministic validation,
lint, or a banned-API rule; centralize the behavior in a canonical helper; add a
runtime check; or use prose when the judgment cannot be encoded. In the existing
single-reviewer brief under `## Fresh transcript analysis`, add this exact one
question: `Which reasonable alternative explanation for the recorded workflow outcome is not tested by the current evidence, and what existing artifact would confirm or refute it? Return none when every material explanation is already covered.` This remains part of the existing review and does not create a new
interview or retrospective workflow. Preserve the current routing for corrections
to existing owners and genuinely new capabilities.

### 3. Evidence-gated plan simplicity lenses

In `.agents/skills/plan-simplicity-review/SKILL.md` under `## Review`, add an
actual-consumer and observable-benefit lens for product, UI, tooling, and public
interface plans. Use it to judge the current plan only; it must not become a
request for extra features or expand the approved boundary.

Add an evidence-gated question about a requested signal or pass-through: identify
the real owner and consumer, establish whether the signal already exists at
either boundary, and require evidence before adding plumbing. Preserve required
layer, trust, client/server, and other repository boundaries while asking this
question.

Add a representation-fit check for coupled flags, repeated variants, and branch
families. Prefer a repository-native enum, `common::Flags`, SOA shape, or lookup
only when the plan's evidence shows that it deletes concrete branches or rules.
Reject a speculative registry or state machine that has no such demonstrated
deletion.

### 4. Progressive disclosure in skill authoring

In `.agents/skills/external-skill-creator/SKILL.md` under `### Progressive
Disclosure`, distinguish universal from conditional material: keep a lean
template or reference inline when every invocation needs it, and move
conditional detail into a focused reference. Keep the selection logic and
end-to-end workflow inline as they are today.

### 5. Reader-load evidence in architecture review

In `.agents/skills/external-architecture-review/SKILL.md` under `### Lens D —
Cohesion and Generation Residue`, assess reader load on two axes: how many layers
a caller or verifier traverses to find a value's origin, and how many mutable
locations can change that value. Use explicit origin and change questions when
examining a suspected shallow seam.

Recommend collapsing a seam only when repository tracing proves a net reduction
in knowledge or change sites and the reduction preserves determinism,
serialization, trust boundaries, build affinity, CPU/GPU contracts,
client/server behavior, and deliberate mirrors. Do not use a timing, elapsed
work, or similar threshold as the architectural verdict.

### 6. Foundational constraints in interface design

In `.agents/skills/external-design-interface/SKILL.md`, make a newly stated
requirement a core design constraint before the three designs are generated.
Separate essential invariants from accidental seams inherited from a current
caller, and do not preserve an accidental seam as if it were a contract.
Continue to carry scope and compatibility constraints into every design and
retain the mandatory user choice when materially different public shapes remain;
the skill must not silently choose one for the user.

### 7. Canonical-target proof for concurrent writers

In `.agents/skills/plan-audit/SKILL.md` under `## Audit`, add a concurrent-writer
check. First prove whether the requirement truly needs one canonical target. If
it does not, partition ownership and publish the independent results. If it
does, require an existing structural coordination mechanism and trace ordering,
visibility, failure progress, serialization/CRC effects, and client/server
publication. An instruction that writers should “take turns” is not
synchronization evidence.

### 8. Focused checkpoints for approved repeated work

In `.agents/skills/implement-plan/SKILL.md` under `## Phase 1: Implement`, cover
approved repeated sweeps and migrations explicitly. When the Plan defines a
focused check for each unit, run it after that unit and before starting any
dependent unit. A failed checkpoint stops dependent work and returns the
evidence to the manager. Builder, runtime, and independent reviewer checks
remain manager-owned handoffs. The checkpoints do not create per-unit commits
or stages and do not replace the stage's final acceptance.

### 9. Type and domain modeling in C++ review

In `.agents/skills/repo-code-review/SKILL.md` under `## Correctness Checks`, add
a concise type/domain modeling subsection. Review only reachable primitive
mixups and contradictory independent discriminators. Where layout permits,
consider a focused domain type and prefer one discriminator over contradictory
independent discriminators. Treat casts and deliberate mirrors as investigation
leads rather than automatic findings. Rely on the existing target sections for
boundary validation, enum or variant consumer tracing, `common::Flags`, and
SoA, wire, CRC, layout, and existing mirror contracts.

## Critical files

- `.agents/skills/code-style-review/SKILL.md` — `## Session Cleanup` changed-comment directives.
- `.agents/skills/next-plan-review/SKILL.md` — assessment and `## Proposed improvements` guidance, plus the existing single-reviewer brief in `## Fresh transcript analysis`.
- `.agents/skills/plan-simplicity-review/SKILL.md` — `## Review` questions for plan value, plumbing, and representation fit.
- `.agents/skills/external-skill-creator/SKILL.md` — `### Progressive Disclosure`.
- `.agents/skills/external-architecture-review/SKILL.md` — `### Lens D — Cohesion and Generation Residue`.
- `.agents/skills/external-design-interface/SKILL.md` — `## Establish the Design Brief` through `## Compare and Decide`.
- `.agents/skills/plan-audit/SKILL.md` — `## Audit` concurrency and publication checks.
- `.agents/skills/implement-plan/SKILL.md` — `## Phase 1: Implement` repeated-unit checkpoint guidance.
- `.agents/skills/repo-code-review/SKILL.md` — `## Correctness Checks` type/domain modeling subsection.

No other skill file, companion policy, reference, license, script, or
configuration file is an implementation target.

## In scope

- Add the nine owner-local wording refinements described in Design items 1–9 to
  the nine exact `SKILL.md` files listed in `## Critical files`.
- In `next-plan-review`, add the exact blind-spot question specified in Design
  item 2 to the existing single-reviewer brief under `## Fresh transcript
  analysis`; this remains one question in that review and does not create a new
  interview or retrospective workflow.
- Keep every refinement in its owning section, with the existing terms,
  sequencing, routing, and output format intact except for the new checkable
  decision sentences.
- Preserve the current manual/implicit invocation behavior and all existing
  role, delegation, client-compatibility, and validation contracts.
- Validate the final nine-package set and review the complete change as one
  Tier-3 wording wave.

## Out of scope

- Any new skill, package, script, reference, configuration surface, API,
  frontmatter field, Codex policy, or client invocation rule.
- Changes to root `AGENTS.md`, `CLAUDE.md`, repository-wide workflow authority,
  role routing, delegation policy, output schemas, or handoff templates.
- Cross-session preference mining, restart-state diagnosis, transcript stores,
  runtime mechanisms, structural validators, lint tooling, registries, state
  machines, or other behavior implied by the source ideas rather than by these
  wording refinements.
- Any source-package wording copied at substantial length or any source license
  artifact; the implementation must phrase the guidance independently.
- Changes outside the nine named `SKILL.md` sections, including unrelated skill
  improvements or documentation cleanup.
- Engine code, build/bootstrap coordination, runtime or harness scenarios,
  serialization, wire protocol, data layout, determinism/CRC behavior, or
  client/server simulation.
- Unit tests, builds, or runtime verification for the Plan implementation;
  existing output and validation formats remain the acceptance surface.

## Risk tier and invariants

The future implementation is Tier 3: it changes nine independently owned skill
workflows in one cross-owner integration wave. The change is prose-only, but
the skills govern repository review, planning, design, and implementation
decisions, so the Tier-3 plan audit, simplicity review, external grill, approval,
scope review, adversarial review, validation, and acceptance gates remain
required.

The implementation must preserve these invariants:

- Each refinement remains local to its owning skill; no duplicate global rule or
  contradictory authority is introduced.
- Existing invocation policy, client compatibility, role routing, delegation
  boundaries, output formats, and handoff contracts remain byte-for-byte
  equivalent in behavior.
- Wording does not create a new runtime, persistence, wire, serialization,
  determinism/CRC, build, or client/server contract; references to those
  contracts remain constraints that the skills already enforce.
- Structural or behavioral mechanisms are not added under the guise of prose;
  the enforcement ladder, checkpoints, and boundary checks describe existing
  choices for future plans and reviews only.
- No substantial source text or license artifact is copied, and all nine
  packages remain valid under the repository validator.

## Acceptance criteria

- The implementation diff contains only the nine exact `SKILL.md` files and the
  named regions in `## Critical files`; `git diff --check` is clean.
- A static scenario review covers Design items 1–9. For each item, one concrete
  judgment case shows that the added sentence selects, rejects, routes, or
  sequences a decision that the prior broad wording left ambiguous, without
  expanding the approved scope or adding a new mechanism.
- The `next-plan-review` refinement places exactly the specified blind-spot
  question in the existing `## Fresh transcript analysis` single-reviewer brief,
  permits `none` when every material explanation is covered, and adds no new
  reviewer, interview, or retrospective workflow.
- Each intended refinement appears once in its owner, with no duplicated global
  rule, contradictory owner, accidental invocation change, or changed output
  format.
- The repository `validate-skill` self-check and all nine target checks return
  `VALID` with exit `0`, including their existing bundled-link and client-policy
  checks.
- One fresh coherence reviewer, one whole-change scope review, and one
  adversarial review pass the wording wave. Before implementation, the future
  Tier-3 Plan also receives `/plan-audit`, `/plan-simplicity-review`,
  `/external-grill-plan`, and the required user approval.
- No unit tests, build, runtime, or harness check is introduced or required;
  the static scenario review and nine validator results are the decisive checks
  for this prose-only implementation.

## Coordination

The nine edits share one evidence source, one artifact class, one implementation
wave, one no-invocation-change invariant, and one validation strategy. They have
no directional prerequisite; this tracked executable Plan was created by
`New-PlanFile` with an empty `dependsOn` list. Keep the edits in one
decision-complete Plan; do not split out a standalone principle or a second Plan
for any one owner.

The implementation may use disjoint file slices, but all slices use this same
scope and acceptance table. The owning-skill boundary controls each edit; a
review finding that proposes a new mechanism, source import, or policy change
is outside this Plan and must return for re-planning.

## Notes

This file is a tracked executable Plan created by `New-PlanFile`; its byte-zero
`broken-engine-plan/v1` marker records an empty `dependsOn` list. The future
implementation is a separate change to the nine named skill files.
