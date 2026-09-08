# Skill Creator Worker

The authoring steps, the repository conventions, and the writing guidance. Triggers and the completion report live in [`../SKILL.md`](../SKILL.md).

## Steps

1. Extract what the conversation already establishes: capability, trigger contexts, inputs, outputs, success criteria, dependencies, corrections, intended clients, and explicit, implicit, or chained invocation for each client. Done when each item is recorded or listed as a gap, and invocation intent exists before metadata is drafted.
2. Ask the user only about meaningful gaps. Done when no meaningful gap is left unanswered.
3. Inspect applicable repository instructions and nearby skills. Done when each has been read.
4. Read `../../validate-skill/references/frontmatter-schema.md` before writing frontmatter and [`client-compatibility.md`](client-compatibility.md) before adding client-specific behavior. Done when every required behavior is mapped to a verified control for each intended client or marked unverified.
5. Choose the smallest useful package around the two files every skill has, the public `SKILL.md` and the private `references/worker.md`, split as `../../../references/skill-skeleton.md` `## Section order` and `## Public and private files` state; then other `references/` for details loaded on demand, `scripts/` for repeatable mechanics, and `assets/` for output resources. Done when every section sits in the file the skeleton names, every other file has one of those roles, and the new skill's `## Handoff` meets that skeleton's `## Section order` item 5 rule on what may be mandated inline.
6. Link `references/worker.md` from `## References` with the private marker line `skill-skeleton.md` `## Public and private files` gives, and open `references/worker.md` with a short header naming what `../SKILL.md` owns. Done when both are present.
7. Draft imperative, general instructions, explaining constraints where the reason helps judgment. Done when every workflow action is drafted that way.
8. Re-read the result with fresh eyes. Done when the whole package has been re-read.
9. Measure every changed Markdown file with the measurement command `/progressive-disclosure-review` owns, checking the result against the thresholds that skill states. Done when every changed file has a measurement compared against those thresholds.
10. Remove duplicated guidance, speculative options, and examples that do not clarify a non-trivial requirement. Done when none of those remain.
11. Restate each rule positively as the action to take, keeping a prohibition only where it cannot be stated positively and pairing it with what to do instead. Done when every rule is positive or paired that way.
12. End each workflow step on a checkable done-condition. Done when every step has one.
13. Run the repository `/validate-skill` workflow on the finished skill. Fix every mechanical or Critical finding and rerun until it passes; treat `BLOCKED` as a stop condition. Done when the validator passes or a `BLOCKED` result stopped the run.
14. Apply [`validation.md`](validation.md): run a loader check when the authored change affects loading or claims loader compatibility, and run focused host checks only for changed client-dependent behavior or claimed compatibility. Done when the handoff separates structural, loader, observed runtime, and unverified evidence for every intended client without requiring model turns solely to remove an unverified result.

## Rules

- Research inline when the answer is local and small. Use available documentation or delegated research only when the skill depends on behavior that needs external or multi-source evidence.

### Repository Conventions

- Store each skill at `.agents/skills/<name>/SKILL.md`.
- Preserve an existing skill's directory and frontmatter name unless the user requests a migration.
- Treat `external-` as a naming convention, not an invocation policy. Configure implicit invocation and chaining per client from the actual workflow.
- Keep essential cross-client trigger contexts in `description`; use `when_to_use` only for supplemental Claude trigger text and do not rely on it for Codex discovery.
- Use the shared frontmatter schema as the sole repository contract. Do not copy fields from a client installation into repository frontmatter without extending the schema and validator together.
- Keep the body lean because it remains in context after invocation; `/progressive-disclosure-review` owns the measurement command and the body and reference size thresholds.
- Write `scripts/` in PowerShell 7. Use Python only when a Python-only runtime or library forces it (RenderDoc, Gaea 2 terrain tooling, the code-quality-metrics analyzer). Host Python is located through `.agents/scripts/Detect-Python.ps1` (x64 CPython 3.12+, the repository-wide floor) or the skill's own pinned bootstrap; an external tool's embedded or version-matched interpreter (RenderDoc, per the agent-harness renderdoc reference) follows that tool's rules instead. A new Python script requires explicit justification.
- `../../../references/skill-skeleton.md` owns the section order, the public/private file split, and the private marker line of every skill; steps 5 and 6 apply it, and its reviewer checklist is the shape check before validation.

### Writing Guidance

- In `description`, lead with the capability, then name concrete user intents and specialized contexts. Do not encode invocation policy only in prose; apply the matching client controls.
- Apply the `Progressive disclosure` directive in root `AGENTS.md` `## Directives` while authoring: variant-specific tables, API details, long examples, and client syntax become focused references, and a bundled script replaces mechanics future invocations would otherwise recreate.
- Decide each remaining piece by how often it is needed: keep a lean template or reference inline when every invocation uses it, and move detail only some invocations need into a focused reference. A short output template that every invocation fills stays inline even when a long client-syntax table next to it moves out.
- Define an exact output template only when downstream work consumes it or consistent structure is part of success. Show one short input/output example for a non-trivial format.
