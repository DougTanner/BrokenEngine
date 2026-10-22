<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-21T22:08:35.785Z","dependsOn":[]} -->
# Add evidence-based simplicity and contract checks to existing reviews

## Summary

Implement all the discussed ideas from Rich Hickey's *Simple Made Easy* and *Spec-ulation* in the planning and artifact-review instructions. The author's recommendation is to keep one shared definition of the new criteria and reuse existing review stages, finding formats, and repository conventions, so the criteria have one owner and fit the existing reviews.

Classify the implementation as Tier 2 because it changes review behavior. Scope is review rules only. Saving this Plan is a Tier-1 documentation change and does not implement those rules.

## In scope

The listed scope is both target and ceiling: make the smallest complete change in the named regions. Naming a file permits changes only to those regions and their mechanical necessities, such as links required by the named change.

Create `.agents/references/simplicity-contracts.md` with two independently addressable sections:

**Simplicity checks**

- Identify independent concerns that a change unnecessarily forces readers or callers to understand together.
- Judge helper extraction by whether its inputs, effects, and result can be understood locally. Moving the same dependencies behind another function is insufficient.
- Identify mandatory call ordering and whether the existing owner can enforce it once. Do not invent managers or state machines merely to centralize it.
- Reference the existing derived-member convention in `.agents/references/cpp-conventions.md`, the “Flags over booleans, derived members” bullet, for redundant state and justified caches; do not duplicate its rule.
- Assess resulting dependencies and maintenance obligations, rather than typing convenience, shorter functions, or fewer lines.
- Explain that size, nesting, and branch metrics are advisory signals, not measurements or proof of independence.

**Requires/provides checks**

- Examine changed caller requirements: inputs, initialization, ownership, thread, and frame phase.
- Examine changed guarantees: results, side effects, lifetime, and applicable determinism.
- Trace consumers when requirements increase, guarantees weaken, or existing names acquire different meanings—even when signatures remain unchanged.
- Preserve the repository's one-current-behavior policy: coordinate affected consumers and apply existing format-version rules instead of retaining obsolete APIs.
- Require findings to name concrete coupled concerns or an affected consumer, supporting evidence, and the smallest correction. Simplicity findings must identify the dependency removed; contract findings must identify the changed obligation or guarantee and its effect.

Connect those sections to existing reviewers in these exact owning regions:

| Existing owner and critical file | In-scope region and change |
|---|---|
| Plan simplicity review — `.agents/skills/plan-simplicity-review/references/worker.md` | Extend `## Review questions`, question 3 “Simpler mechanism” and question 7 “Actual consumer and observable benefit”, to apply both sections. Preserve question numbering and finding classes. |
| Shared scope authorization — `.agents/references/scope-authorization.md` | Link numbered pass 3, “KISS pass”, to the simplicity section, preserving its Tier-2+, changed-region, diff-observable limits. This reaches C++, shader, and non-C++ reviews. |
| C++ correctness review — `.agents/skills/repo-code-review/references/worker.md` | Link `## Steps`, the existing contract-reading and affected-caller tracing checks in steps 1–2, to the requires/provides section. |
| Coherence review — `.agents/skills/coherence-review/references/worker.md` | Extend `## Steps`, step 2's changed-region semantic review, to apply requires/provides analysis to changed workflow prerequisites, ordering, outputs, and consumers. |

The new shared reference's source table and attribution notes are also in scope, as specified in `## Source references` below.

## Out of scope

- No engine audit, runtime changes, new metrics, or backward-compatibility mechanisms.
- No new review stages or changes to review schedules or handoff schemas.
- No detailed criteria added to `AGENTS.md` or skill dispatcher contracts; keep those criteria in the shared reference and the named worker integration regions.
- No abstractions, configuration, refactors, or fixes to adjacent code encountered during implementation. Leave metrics tooling unchanged.

## Source references

Include a compact source table in the shared reference, distinguishing Hickey's arguments from Broken Engine applications:

- **Rich Hickey, [Simple Made Easy — Strange Loop, 2011](https://www.infoq.com/presentations/Simple-Made-Easy/):** simplicity versus counting at approximately 03:19; modularity versus independence at 33:35; state and time at 35:38; resulting artifacts versus authoring convenience at 59:53. Include the [searchable transcript](https://github.com/matthiasn/talk-transcripts/blob/master/Hickey_Rich/SimpleMadeEasy.md) as a navigation aid.
- **Rich Hickey, [Spec-ulation — Clojure/conj, 2016](https://www.youtube.com/watch?v=oyLBGkS5ICk):** requirements and guarantees; providing more/requiring less at 22:11; requiring more/providing less/changing meaning at 24:14. Include the [searchable transcript](https://github.com/matthiasn/talk-transcripts/blob/master/Hickey_Rich/Spec_ulation.md).
- Explicitly identify frame phases, determinism, cache policy, and coordinated format changes as repository-specific applications. Explain that the talk's preservation of old APIs is not adopted.

## Verification and delivery

Before editing the review rules, materialize the execution card and complete the required plan audit and simplicity review. The planning proposal records that the alternatives investigation already supports the shared-reference approach; this statement does not waive any preparation or review required by the current Change Workflow.

After implementation:

- Run the repository static checks, coherence review, skill validation, progressive-disclosure review, and acceptance verification.
- Verify every discussed idea has one owning rule and a reachable review entry point; existing rules are referenced rather than copied.
- Review illustrative cases: extracted helpers retaining shared-state dependencies; mandatory ordering scattered across callers; redundant state versus a justified cache; an unchanged signature gaining an initialization requirement; a workflow losing an output its consumer requires.
- Check negative cases: necessary frame ordering, deliberate mirrored code, and a coordinated contract change must not produce findings merely because dependencies exist.
- Validate local links, source attribution, and timestamps. Confirm existing finding formats can express the new findings without added fields.

No unit tests, builds, or game harness runs are needed for these instruction-only changes. Leave the verified implementation changes in the session worktree; landing remains a separate confirmation.
