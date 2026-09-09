# Skill Authoring Procedure

The authoring steps, repository conventions, and writing guidance selected by
[`worker.md`](worker.md). Triggers and the completion report live in
[`../SKILL.md`](../SKILL.md).

## Steps

1. Extract what the conversation already establishes: capability, trigger contexts, inputs, outputs, success criteria, dependencies, corrections, intended clients, and explicit, implicit, or chained invocation for each client. Done when each item is recorded or listed as a gap, and invocation intent exists before metadata is drafted.
2. Ask the user only about meaningful gaps. Done when no meaningful gap is left unanswered.
3. Inspect applicable repository instructions and nearby skills. Done when each has been read.
4. Read [`frontmatter-schema.md`](frontmatter-schema.md) before writing frontmatter and [`client-compatibility.md`](client-compatibility.md) before adding client-specific behavior. Done when every required behavior is mapped to a verified control for each intended client or marked unverified.
5. Choose the consumption shape by who reads the complete workflow, then apply `../../../references/skill-skeleton.md` and its linked type checklist. Add focused `references/` for details loaded on demand, `scripts/` for repeatable mechanics, and `assets/` for output resources. Done when worker-reference presence selects the intended shape, every ordered section has the required placement, every other file has one role, and `## Handoff` meets the skeleton's inline-content rule.
6. For a subagent package, link `references/worker.md` from `## References` with the private marker line `skill-skeleton.md` gives and open it with a short header naming what `../SKILL.md` owns. For a main-session package, keep the complete workflow in `SKILL.md` and omit the worker file. Done when the selected type checklist passes.
7. Draft imperative, general instructions, explaining constraints where the reason helps judgment. Done when every workflow action is drafted that way.
8. Re-read the result with fresh eyes. Done when the whole package has been re-read.
9. Measure every changed Markdown file with the measurement command `/progressive-disclosure-review` owns, checking the result against the thresholds that skill states. Done when every changed file has a measurement compared against those thresholds.
10. Remove duplicated guidance, speculative options, and examples that do not clarify a non-trivial requirement. Done when none of those remain.
11. Restate each rule positively as the action to take, keeping a prohibition only where it cannot be stated positively and pairing it with what to do instead. Done when every rule is positive or paired that way.
12. End each workflow step on a checkable done-condition. Done when every step has one.
13. Run the mechanical validator against the `external-skill-creator` package and then the finished skill. Fix target mechanical findings and rerun until both return `VALID` with exit `0`; treat a setup, invocation, read, or internal-validator failure as `BLOCKED`. Record both results in the author handoff, return `NEEDS_ACTION`, and name fresh `/external-skill-creator` validate-mode review as the manager's required next action under shared `Residuals`. The manager dispatches the independent reviewer and owns the finding-fix cycle; the author executor does not dispatch or wait for that review. Done when both author mechanical checks pass and the independent validation request is present in the handoff, or a `BLOCKED` result stopped the run.
14. Apply [`validation.md`](validation.md): run a loader check when the authored change affects loading or claims loader compatibility, and run focused host checks only for changed client-dependent behavior or claimed compatibility. Done when the handoff separates structural, loader, observed runtime, and unverified evidence for every intended client without requiring model turns solely to remove an unverified result.

## Rules

- Research inline when the answer is local and small. Return research requiring a separate role to the manager.

### Repository Conventions

- Store each skill at `.agents/skills/<name>/SKILL.md`.
- Preserve an existing skill's directory and frontmatter name unless the user requests a migration.
- Treat `external-` as a naming convention, not an invocation policy. Configure implicit invocation and chaining per client from the actual workflow.
- Keep essential cross-client trigger contexts in `description`; use `when_to_use` only for supplemental Claude trigger text and do not rely on it for Codex discovery.
- Use the shared frontmatter schema as the sole repository contract. Do not copy fields from a client installation into repository frontmatter without extending the schema and validator together.
- Keep the body lean because it remains in context after invocation; `/progressive-disclosure-review` owns the measurement command and the body and reference size thresholds.
- Write `scripts/` in PowerShell 7. Use Python only when a Python-only runtime or library forces it (RenderDoc, Gaea 2 terrain tooling, the code-quality-metrics analyzer). Host Python is located through `.agents/scripts/Detect-Python.ps1` (x64 CPython 3.12+, the repository-wide floor) or the skill's own pinned bootstrap; an external tool's embedded or version-matched interpreter (RenderDoc, per the agent-harness renderdoc reference) follows that tool's rules instead. A new Python script requires explicit justification.
- `../../../references/skill-skeleton.md` owns consumption shapes, section order and placement, and the subagent private marker line; steps 5 and 6 apply it, and its reviewer checklist is the shape check before validation.

### Writing Guidance

- In `description`, lead with the capability, then name concrete user intents and specialized contexts. Do not encode invocation policy only in prose; apply the matching client controls.
- Apply the `Progressive disclosure` directive in root `AGENTS.md` `## Directives` while authoring: variant-specific tables, API details, long examples, and client syntax become focused references, and a bundled script replaces mechanics future invocations would otherwise recreate.
- Decide each remaining piece by how often it is needed: keep a lean template or reference inline when every invocation uses it, and move detail only some invocations need into a focused reference. A short output template that every invocation fills stays inline even when a long client-syntax table next to it moves out.
- Define an exact output template only when downstream work consumes it or consistent structure is part of success. Show one short input/output example for a non-trivial format.
