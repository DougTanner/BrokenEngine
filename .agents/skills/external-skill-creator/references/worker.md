# Skill Creator Worker

The authoring steps, the repository conventions, and the writing guidance. Triggers and the completion report live in [`../SKILL.md`](../SKILL.md).

## Steps

1. Extract what the conversation already establishes: capability, trigger contexts, inputs, outputs, success criteria, dependencies, and corrections. Done when each of those is recorded or listed as a gap.
2. Ask the user only about meaningful gaps. Done when no meaningful gap is left unanswered.
3. Inspect applicable repository instructions and nearby skills. Done when each has been read.
4. Read `../../validate-skill/references/frontmatter-schema.md` before writing frontmatter and [`client-compatibility.md`](client-compatibility.md) before adding client-specific behavior. Done when each is read before the work it gates.
5. Choose the smallest useful package: `SKILL.md` for the durable workflow; `references/` for details loaded on demand; `scripts/` for repeatable mechanics; `assets/` for output resources. Done when every chosen file has one of those roles.
6. Draft imperative, general instructions, explaining constraints where the reason helps judgment. Done when every workflow action is drafted that way.
7. Re-read the result with fresh eyes. Done when the whole package has been re-read.
8. Measure every changed Markdown file with the measurement command `/progressive-disclosure-review` owns, checking the result against the thresholds that skill states. Done when every changed file has a measurement compared against those thresholds.
9. Remove duplicated guidance, speculative options, and examples that do not clarify a non-trivial requirement. Done when none of those remain.
10. Restate each rule positively as the action to take, keeping a prohibition only where it cannot be stated positively and pairing it with what to do instead. Done when every rule is positive or paired that way.
11. End each workflow step on a checkable done-condition. Done when every step has one.
12. Run the repository `validate-skill` workflow on the finished skill. Fix every mechanical or Critical finding and rerun until it passes; treat `BLOCKED` as a stop condition. Done when the validator passes or a `BLOCKED` result stopped the run.

## Rules

- Research inline when the answer is local and small. Use available documentation or delegated research only when the skill depends on behavior that needs external or multi-source evidence.

### Repository Conventions

- Store each skill at `.agents/skills/<name>/SKILL.md`.
- Preserve an existing skill's directory and frontmatter name unless the user requests a migration.
- Treat `external-` as a naming convention, not an invocation policy. Configure implicit invocation and chaining per client from the actual workflow.
- Keep substantive trigger contexts in `description` or `when_to_use`; metadata is the text agents search when deciding whether to use the skill, and may be truncated.
- Use the shared frontmatter schema as the sole repository contract. Do not copy fields from a client installation into repository frontmatter without extending the schema and validator together.
- Keep the body lean because it remains in context after invocation; `/progressive-disclosure-review` owns the measurement command and the body and reference size thresholds.
- Write `scripts/` in PowerShell 7. Use Python only when a Python-only runtime or library forces it (RenderDoc, Gaea 2 terrain tooling, the code-quality-metrics analyzer). Host Python is located through `.agents/scripts/Detect-Python.ps1` (x64 CPython 3.12+, the repository-wide floor) or the skill's own pinned bootstrap; an external tool's embedded or version-matched interpreter (RenderDoc, per the agent-harness renderdoc reference) follows that tool's rules instead. A new Python script requires explicit justification.
- Follow `../../../references/skill-skeleton.md` for the body shape and public/private file split of every skill.

### Writing Guidance

- In `description`, lead with the capability, then name concrete user intents and specialized contexts. Do not encode invocation policy only in prose; apply the matching client controls.
- Apply the `Progressive disclosure` directive in root `AGENTS.md` `## Directives` while authoring: variant-specific tables, API details, long examples, and client syntax become focused references, and a bundled script replaces mechanics future invocations would otherwise recreate.
- Decide each remaining piece by how often it is needed: keep a lean template or reference inline when every invocation uses it, and move detail only some invocations need into a focused reference. A short output template that every invocation fills stays inline even when a long client-syntax table next to it moves out.
- Define an exact output template only when downstream work consumes it or consistent structure is part of success. Show one short input/output example for a non-trivial format.
