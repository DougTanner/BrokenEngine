# Integrate `external-teach`

## Why this is worth integrating

A code walkthrough and a historical rationale answer solve different parts of understanding. People usually need both before they can make a safe change: what the system does, how data moves through it, and why the current shape may exist. The pstack `teach` package provides a focused wrapper that combines those two investigations into one plain explanation, choosing depth for the person and using diagrams only when they help.

Broken Engine can gain that teaching experience without adding a third investigator. `external-teach` will consume the already implemented `external-how` and `external-why` workflows, run each dependency once, and synthesize their evidence into a readable explanation. It will preserve `external-why`'s uncertainty language and gaps rather than smoothing them into a confident lesson. The source behavior is `pstack/skills/teach/SKILL.md` at commit `60c641e4fad674784b30abcf9f8915dea39df38d`; its declared dependencies are adapted to the two repository-native skills named above.

## Context

`external-how` explains current mechanics from repository evidence. `external-why` investigates motivation across the read-only sources available in the current session and reports confidence, coverage, contradictions, and gaps. Neither package should be duplicated inside a teaching skill. `external-teach` is the user-facing composition layer after both dependencies are implemented and validated.

The eventual Feature document will live under `Documents/Features/Skills/`, and the package will live under `.agents/skills/external-teach/`. The package is explicit/manual in both clients because teaching is a deliberate user request and can launch the two dependency workflows. Its ordinary use is still read-only and outside the Change Workflow.

## Design

### Package and dependencies

Create the smallest package:

- `.agents/skills/external-teach/SKILL.md` contains the teaching trigger, dependency contract, composition flow, plain-language rules, progressive visual rule, and output behavior. Its shared frontmatter uses the name `external-teach`, a description naming the explicit `/external-teach <change, subsystem, or focused question>` and `$external-teach <change, subsystem, or focused question>` teaching requests, `allowed-tools: [Read, Grep, Glob, Agent, PowerShell]`, `disable-model-invocation: true`, and `user-invocable: true`; `Agent` is present for the parent-controlled dependency passes.
- `.agents/skills/external-teach/agents/openai.yaml` declares the Codex `$external-teach <change, subsystem, or focused question>` interface and manual-only policy.
- `.agents/skills/external-teach/LICENSE` preserves the pstack MIT notice and identifies the adapted source package and source commit.

Do not add a second prompt-reference tree, a new investigator, a connector adapter, a critic mode, a script, or a teaching-specific research format. The package reads the established contracts from `.agents/skills/external-how/SKILL.md` and `.agents/skills/external-why/SKILL.md` and uses their returned outputs. Those two packages must already be present and validated before this package is implemented.

Because `external-why` is manual-only in both clients, `external-teach` is the parent workflow rather than a native nested skill invocation. When a user explicitly invokes `/external-teach <change, subsystem, or focused question>` or `$external-teach <change, subsystem, or focused question>`, the main context performs one mechanics dependency pass under the `external-how` contract and one rationale dependency pass under the `external-why` contract, then synthesizes them. It does not ask either dependency to become implicitly discoverable, and it does not run a second independent code or history investigation. The user's explicit teach request is the authorization for this parent composition.

The two dependency passes can run in parallel because neither changes state and neither consumes the other's result. The main context waits for both outputs, retains their evidence and gaps, and then writes the teaching explanation. If a dependency is missing or cannot complete, the skill reports that dependency gap and does not invent the missing half of the lesson.

### Invocation and teaching flow

The skill is explicit/manual in both clients:

- Exact human interfaces: `/external-teach <change, subsystem, or focused question>` in Claude Code and `$external-teach <change, subsystem, or focused question>` in Codex.
- Claude control: set `disable-model-invocation: true` and `user-invocable: true`.
- Codex control: set `policy.allow_implicit_invocation: false` and keep explicit invocation available.
- The frontmatter description names these explicit teaching requests.

The parent follows this fixed sequence:

1. Identify the few ideas the user needs from the request and known context. Do not quiz the user or ask them to restate an understandable target.
2. Establish the exact target boundary and pass the same target and question to `external-how` and `external-why` once each. The dependencies own repository exploration, source coverage, locator delegation, citations, and confidence classification.
3. Combine the dependency outputs without repeating their investigation. Use the mechanics output to explain what the system is and how it works. Use the rationale output to explain what evidence supports the design and what remains uncertain.
4. Start with a short plain definition tied to the target. Add the mechanics flow, rationale, edge cases, and only the context needed for the user's stated goal. Preserve every confidence hedge and explicit gap from `external-why`.
5. Add the smallest useful single visual only when it materially makes a relationship easier to understand. Use progressive stages only when they make the explanation materially clearer than that single visual; otherwise omit decorative or redundant diagrams. No image-generation or other external visual service is required.
6. End with the explanation itself and, when useful, one concise offer to go deeper. Do not report the orchestration as the product, dump raw investigator packets, or turn the lesson into an architecture critique.

The output is a plain spoken explanation rather than a fixed report. It must not erase `external-why`'s direct/supported/inferred/speculative distinctions, contradiction list, source coverage, or unknowns when those details affect the user's understanding. It may reword mechanics for clarity, but it may not upgrade a historical inference into a fact.

### Change Workflow placement

Implementing this plan is a Tier-3 implementation stage because it composes `external-how` and `external-why` and exposes `external-why`'s connector trust boundary. Step 1 records the execution card and dependency/output invariants; Step 2 runs `/plan-audit` and `/plan-simplicity-review`, then `/external-grill-plan` and user approval before implementation; Step 3 adds the package after the two dependencies are available; Step 4 runs targeted static checks; Step 5 runs `/adversarial-review` plus fresh non-C++ coherence and `/scope-review`; Step 6 runs `validate-skill`; Step 7 verifies every dependency-composition, manual-policy, trust-boundary, uncertainty, and visual invariant in realistic teaching scenarios; and Step 8 applies if the Feature is being landed. No build is required because no script or compiled code is planned.

An ordinary `/external-teach <change, subsystem, or focused question>` or `$external-teach <change, subsystem, or focused question>` invocation is outside the Change Workflow. It reads dependency outputs and presents a lesson without editing source, plans, skills, or external services. A request to change code after the lesson is a separate task and enters the normal workflow at its own tier.

## Critical files

Future implementation paths, all relative to the repository root:

- `.agents/skills/external-teach/SKILL.md` — manual trigger, dependency composition, no-repeat rule, teaching output, confidence preservation, and visual rule.
- `.agents/skills/external-teach/agents/openai.yaml` — Codex interface and `allow_implicit_invocation: false`.
- `.agents/skills/external-teach/LICENSE` — pstack MIT notice and adaptation attribution.
- `.agents/skills/external-how/SKILL.md` — required mechanics dependency and output contract; read-only prerequisite, not modified by this plan.
- `.agents/skills/external-why/SKILL.md` — required rationale dependency and confidence/coverage contract; read-only prerequisite, not modified by this plan.

Source references used for the adaptation are `pstack/skills/teach/SKILL.md` and `pstack/LICENSE`, both at commit `60c641e4fad674784b30abcf9f8915dea39df38d`. The dependency contracts are the future repository packages specified above.

## In scope

- Add the standalone manual skill package named `external-teach` under `.agents/skills/external-teach/` with the three package files listed above.
- Require implemented and validated `external-how` and `external-why` packages before this package is implemented.
- Make the parent composition explicit: one mechanics pass, one rationale pass, then one teaching synthesis; no repeated investigation and no duplicated source-specific logic.
- Set independent client controls for explicit/manual use: Claude `disable-model-invocation: true`; Codex `policy.allow_implicit_invocation: false`; both clients retain an explicit user entry point.
- Preserve `external-why` confidence language, contradictions, coverage, and gaps in the teaching result while making the prose plain and paced to the user's question.
- Use the smallest useful single visual by default when a visual is materially useful; use progressive stages only when they are materially clearer. Keep visual output optional and repository-local.
- Include the pstack MIT notice for the substantial adapted material in the package `LICENSE`.
- Run `validate-skill`, inbound-reference checks, and independent realistic forward scenarios for both clients, dependency composition, evidence preservation, and visual restraint without adding unit tests.

## Out of scope

- Changing `external-how`, `external-why`, their references, their client policies, or their license files.
- Implementing another investigator, repeating code/history exploration, changing connector trust rules, installing/authenticating services, or mutating any local or external state.
- Adding an implicit trigger, native nested invocation that weakens dependency policy, critique/review behavior, quizzes, user modeling, or a separate teaching report schema.
- Image-generation integration, visual assets, scripts, tests, test fixtures, builds, runtime code, shaders, harness scenarios, or engine data changes.
- Changing root `AGENTS.md`, the Change Workflow, role definitions, validator rules, or unrelated skills.
- Changing the source pstack checkout or citing an absolute source-home path or conversation transcript.

## Risk tier and invariants

**Tier 3 — invariant/integration.** This package composes two existing explanation workflows and exposes `external-why`'s connector trust boundary. It adds no connector access or external mutation; the higher-risk historical-source boundary remains owned by `external-why`.

The implementation must preserve these invariants:

- Explicit/manual policy is enforced independently in Claude and Codex. A generic teaching request cannot implicitly start the package.
- Both dependencies run at most once for a teach request, and teach does not perform a second mechanics or history investigation.
- The target boundary and user question are consistent across both dependency passes. The final lesson does not combine evidence from unrelated scopes.
- Missing dependency output, historical uncertainty, contradictions, and unavailable sources remain visible. Teach never fills a gap with a new unsupported rationale.
- `external-why` confidence language is preserved. A supported or inferred statement cannot become a fact merely because it is easier to teach that way.
- The package is read-only and does not change repository, runtime, connector, or service state. It stays outside PostRender/CRC and all engine runtime invariants.
- Visuals are evidence-grounded and materially useful. A diagram is omitted when prose is clearer, a single visual is the default when one is useful, and progressive stages are added only when they materially improve clarity.
- Substantial adapted text keeps the pstack MIT notice and attribution in the package `LICENSE`.

## Acceptance criteria

- `validate-skill` self-validation and target validation both return `VALID` with exit `0` for `.agents/skills/external-teach/`; frontmatter, the manual Codex policy, and the package links pass the repository schema.
- `Find-SkillInboundReferences.ps1 -SkillName external-teach` finds the intended explicit entry points. The dependency references resolve to the implemented `external-how` and `external-why` packages.
- Independent Claude and Codex manual scenarios both invoke the exact `/external-teach <change, subsystem, or focused question>` or `$external-teach <change, subsystem, or focused question>` interface successfully, while no unprefixed generic teaching request implicitly invokes it. Each client proves its own manual policy.
- A realistic subsystem scenario shows exactly one mechanics dependency pass and one rationale dependency pass followed by one teaching synthesis. The final response starts with a plain definition, explains mechanics and rationale, and does not reproduce raw investigation packets or run a duplicate search.
- A scenario whose rationale output contains an inference, contradiction, unavailable source, or explicit gap shows that the teaching response preserves the confidence language and uncertainty rather than presenting an invented certainty.
- A target with no materially clearer visual omits a diagram. When a visual is useful, the scenario uses the smallest useful single Mermaid or ASCII visual by default; progressive stages appear only when they materially improve clarity and match the dependency evidence.
- Static inspection confirms that the package has no implicit policy, connector installation/authentication step, write-capable operation, critique branch, duplicated source-specific playbook, or second investigator.
- The package `LICENSE` contains the pstack MIT notice, copyright, permission, and disclaimer text and identifies source commit `60c641e4fad674784b30abcf9f8915dea39df38d`.
- No build or unit-test run is required. If future implementation adds scripts, visual assets, or executable code, the plan must be reclassified and the smallest relevant check added before implementation.

## Notes/Coordination

- Implement `external-how` and `external-why` first. Validate both packages and their client policies before implementing `external-teach`; this plan does not modify either dependency.
- When promoted, place this marker-less Feature body under `Documents/Features/Skills/`; do not add `broken-engine-plan/v1` metadata. This drafting stage creates no actual skill package files.
- Keep the dependency composition parent-controlled so `external-why` remains manual-only in both clients. The explicit `/external-teach <change, subsystem, or focused question>` or `$external-teach <change, subsystem, or focused question>` request authorizes the two dependency passes without changing their generic discovery policies.
- Use repository-relative source citations and the immutable source commit above in the eventual package and plan history. Do not record an absolute external checkout path or a transcript as provenance.
- The Feature is manually executed and never scheduler-claimed. Any later landing of the tracked package still follows the repository landing gate.
