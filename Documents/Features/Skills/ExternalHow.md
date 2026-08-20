# Integrate `external-how`

## Why this is worth integrating

Agents and engineers need a reliable read-only walkthrough before changing an unfamiliar subsystem. The existing pstack `how` package provides a useful shape for that job: it answers what a subsystem does, traces the flow, maps the important abstractions, and calls out gotchas. This integration makes that behavior fit Broken Engine's evidence-first repository workflow and supports both Claude Code and Codex.

The result is deliberately narrower than a general architecture review. It gives a reader a working mental model from the code that is present now, so the explanation can guide a later change without changing the repository or silently turning into a review. The source package behavior is from `pstack/skills/how` at commit `60c641e4fad674784b30abcf9f8915dea39df38d`.

## Context

Broken Engine has `what` for re-explaining an agent message and has review/diagnosis skills for architecture findings, bugs, and refactoring. It does not yet have a repository-native skill whose sole job is a codebase walkthrough. The pstack package is a good starting point, but its explorer and critic roles need to be adapted to the repository role table:

- Direct read-only inspection in the main context is the default for a walkthrough question.
- When independent breadth is materially useful, the main context may use disjoint `locator` slices for exact evidence, followed by one coherent explanation in the main context.
- The pstack critique branch is intentionally removed. Architecture concerns belong to `external-architecture-review` or `external-deep-analysis`, each with its own findings contract.

This is a manual Feature document. The eventual files will be under `Documents/Features/Skills/`; the executable skill package itself will be under `.agents/skills/external-how/`.

## Design

### Package and source adaptation

Create the smallest three-file package that supports the walkthrough workflow:

- `SKILL.md` contains the trigger description, the direct-inspection versus optional-locator decision, the read-only workflow, the concise locator packet, the synthesis rules, and the output contract.
- `agents/openai.yaml` explicitly enables Codex implicit discovery and explicit invocation.
- `LICENSE` preserves the pstack MIT notice and identifies the adapted source package and source commit.

Do not add separate locator or explainer prompt references, copy `critic-prompt.md` or `critique-rubric.md`, or add a critique mode, critic dispatch, architecture score, or architecture-finding output to this package. Do not add scripts, connector dependencies, or a second output format.

The shared frontmatter must keep the package portable and read-only. Use the name `external-how`, a description that names code walkthroughs, runtime-flow questions, placement/ownership/layering questions, and pre-change explanation, `allowed-tools: [Read, Grep, Glob, Agent]`, `disable-model-invocation: false`, and `user-invocable: true`. `Agent` is present because the main context dispatches repository `locator` roles; the body remains client-neutral and names the role contract rather than client syntax. The Codex companion must contain an interface plus `policy.allow_implicit_invocation: true`. The two policy settings are independent and must both be checked.

### Invocation and breadth

The skill supports automatic discovery and explicit invocation in both clients:

- Automatic trigger: a user asks how a Broken Engine subsystem, feature flow, function, ownership boundary, or placement/layering decision works, including a walkthrough before a change.
- Claude explicit interface: the user invokes `/external-how <question>`.
- Codex explicit interface: the user invokes `$external-how <question>`.
- The skill states its best interpretation when the question is ambiguous and proceeds without asking the user to restate it.

Direct read-only inspection in the main context is the default. The main context decides whether independent breadth is materially useful from the requested question, scope, and evidence gaps. When it is useful, the main context may dispatch one or more disjoint `locator` slices over the requested scope before synthesis. It imposes no numeric file threshold, required locator count, or fixed scheduling pattern; slices are chosen only when they improve independent coverage.

For each optional slice, the main context supplies the exact question, angle, target boundary, and applicable `AGENTS.md` paths. Each locator returns only a concise evidence packet with these fields:

- `Components`: symbols/types and their path and line evidence.
- `Flow`: caller/callee or data-flow edges, each anchored to path and line evidence.
- `Files read`: the complete list of inspected files.
- `Boundaries`: inputs, outputs, and cross-subsystem contracts with evidence.
- `Non-obvious`: surprising behavior or repository constraints with evidence.
- `Open questions`: gaps that the locator could not prove.

The main context reconciles overlapping packets, checks any contradiction against the code, and writes the final explanation. It does not present locator packets as the answer. The explanation uses `Overview`, `Key Concepts`, `How It Works`, `Where Things Live`, and `Gotchas` as applicable. It references the relevant repository paths and symbols. A diagram is included only when it makes a multi-part flow easier to understand; otherwise prose is sufficient. These packet and synthesis/output rules live once in `SKILL.md` rather than in separate prompt references.

The package is read-only. It never edits source, plans, or skill files, starts a build, changes runtime state, installs a plugin, authenticates a connector, or performs an external service mutation. If the user asks for a change after receiving the walkthrough, that is a separate request handled by the normal Change Workflow.

### Change Workflow placement

Implementing this plan will add tracked skill artifacts and is therefore a Tier-2 implementation stage. The implementation uses the normal Change Workflow: Step 1 records the execution card and Tier-2 triggers; Step 2 runs plan audit and simplicity review; Step 3 writes the package; Step 4 runs targeted static checks; Step 5 runs the fresh non-C++ coherence review and scope review; Step 6 runs `validate-skill` for the new package; Step 7 verifies the acceptance scenarios; and Step 8 applies only if this Feature is being landed. No build is required because no scripts or compiled code are planned.

An ordinary invocation of the installed skill is outside the Change Workflow. It only reads the repository and produces an explanation. It does not need a claim, implementation plan, review round, or landing gate. A later source change prompted by the explanation starts its own workflow at the tier appropriate to that change.

## Critical files

Future implementation paths, all relative to the repository root:

- `.agents/skills/external-how/SKILL.md` — trigger, read-only workflow, direct-inspection versus optional-locator rule, concise evidence packet, synthesis/output contract, and no-critique boundary.
- `.agents/skills/external-how/agents/openai.yaml` — Codex interface and implicit-invocation policy.
- `.agents/skills/external-how/LICENSE` — pstack MIT notice and adaptation attribution.

Source references used for the adaptation are `pstack/skills/how/SKILL.md` and `pstack/LICENSE`, both at commit `60c641e4fad674784b30abcf9f8915dea39df38d`.

## In scope

- Add the standalone skill package named `external-how` under `.agents/skills/external-how/` with the three files listed above.
- Preserve the source package's explain-mode concepts and output shape while replacing its generic explorer dispatch with Broken Engine `locator` evidence and main-context synthesis.
- Set Claude and Codex controls for automatic plus manual use: Claude `disable-model-invocation: false`; Codex `policy.allow_implicit_invocation: true`; both packages remain user-invocable.
- Keep direct inspection as the default, use optional locator slices only when independent breadth is materially useful, and keep the concise evidence packet plus final synthesis/output rules in `SKILL.md`.
- Remove critique mode and direct users with critique requests to the existing architecture-review workflows without adding a second critique implementation.
- Keep all reads local to the repository and preserve the source-path/line evidence rule so explanations do not invent behavior.
- Include the pstack MIT notice for the substantial adapted material in the package `LICENSE`.
- Run the repository skill validator and inbound-reference checks on the finished package, and exercise independent forward scenarios in both clients without adding unit tests.

## Out of scope

- Changing root `AGENTS.md`, the Change Workflow, the role table, repository skill-validation rules, or any existing skill.
- Implementing or modifying `external-why` or `external-teach`.
- Copying pstack critique references, adding architecture-review behavior, or dispatching critic agents.
- Any source, shader, build, runtime, harness, connector, plugin-installation, authentication, or external-service change.
- Adding scripts, tests, test fixtures, compatibility layers, or new configuration beyond the package frontmatter and Codex companion.
- Changing the source pstack checkout or citing an absolute source-home path or conversation transcript.

## Risk tier and invariants

**Tier 2 — scoped behavior.** The future package changes agent behavior for one read-only repository-explanation workflow. It does not touch simulation, CRC state, networking, serialization, client/server runtime code, or external trust boundaries.

The implementation must preserve these invariants:

- Read-only means no local or external writes, no tool-driven mutation, and no hidden build or runtime launch.
- Automatic and explicit invocation have matching, intentional controls in Claude and Codex. Neither client may silently become manual-only or automatic-only.
- When independent breadth is materially useful, optional locator slices receive evidence from the requested repository scope before synthesis. Locator packets remain evidence, and the main context owns the final narrative.
- Every concrete mechanics claim is traceable to inspected repository paths, symbols, and lines. Gaps remain gaps.
- The package has one explanation mode. Critique requests do not cause an undocumented critic branch.
- The output remains outside PostRender/CRC and all other engine runtime invariants because it is a read-only agent workflow.
- Substantial adapted text keeps the pstack MIT notice and attribution in the package `LICENSE`.

## Acceptance criteria

- `validate-skill` self-validation and target validation both return `VALID` with exit `0` for `.agents/skills/external-how/`; the three-file package (`SKILL.md`, `agents/openai.yaml`, and `LICENSE`) is accepted by the repository schema.
- `Find-SkillInboundReferences.ps1 -SkillName external-how` finds the intended repository entry points and no stale source-package path is required for invocation. Any new inbound documentation is limited to the package's approved scope.
- An independent Claude forward scenario using an implicit “how does this subsystem work?” request invokes the skill, reads only the requested repository scope, and emits the explanation sections without changing files; its explicit `/external-how <question>` interface also works.
- An independent Codex forward scenario using the same implicit request invokes the skill and leaves files unchanged; its explicit `$external-how <question>` interface also works, proving both behaviors from `agents/openai.yaml` without relying on Claude frontmatter to set Codex policy.
- A narrow function or one-file question uses direct inspection and produces a source-anchored explanation without locator fan-out. When independent breadth is materially useful, optional locator evidence packets produce one reconciled main-context explanation, with no critic dispatch.
- A placement or ownership question names the relevant boundary and evidence. A request for architectural critique is routed to the existing review skill rather than producing an `external-how` critique report.
- Static inspection confirms that the package contains only `SKILL.md`, `agents/openai.yaml`, and `LICENSE`, with no separate locator or explainer prompt, `critic-prompt.md`, `critique-rubric.md`, connector dependency, write/edit tool, write/edit instruction, write/edit operation, plugin-install/authentication step, or source edit instruction.
- The package `LICENSE` contains the pstack MIT notice, copyright, permission, and disclaimer text and identifies source commit `60c641e4fad674784b30abcf9f8915dea39df38d`.
- No build or unit-test run is required. If future implementation adds a script or executable artifact, the plan must be reclassified and the smallest relevant check added before implementation.

## Notes/Coordination

- Implement this package independently of `external-why` and `external-teach`; the latter may consume this package later but does not change this plan's scope.
- When promoted, place this marker-less Feature body under `Documents/Features/Skills/`; do not add `broken-engine-plan/v1` metadata. This drafting stage creates no actual skill package files.
- Use the repository-relative source citations above in the eventual package and plan history. Do not record the external checkout's absolute path or a transcript as provenance.
- The Feature remains manually executed and is never scheduler-claimed. Any later landing of the tracked package still follows the repository landing gate.
