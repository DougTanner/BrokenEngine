# Workflow Incidents

One row per routed friction, context-efficiency, or isolation finding, and per
accepted finding of a full `/next-plan-review` audit other than governing-scope
conformance and solution minimality. Not recorded: an objectively wrong
statement or bug — instruction text that is untrue or contradicts its owner, or
a script or skill breaking its documented contract. Cost-only findings, even
unclear but true wording, are recorded.

Key: owner plus symptom. Owner is the path of the script, `AGENTS.md`, or
`.agents/references/` file at fault, or the skill directory
(`.agents/skills/<name>`) when a skill's text is. Rows share a symptom when one
fix removes both. Main, reading no code, adds its row first, reusing a matching
symptom verbatim. Evidence: selector, result, cost or bounding mechanism;
`|` as `;`; no transcript text or path, no absolute path; the row stays
within the static-checks `line-length` limit.

At a key's third row, never sooner, main has one `implementer` run
`/create-follow-up-plans` on its rows, which deletes them once the Plan is
created, mapped, or rejected.

## Entries

| Owner | Symptom | Date | Session branch | Evidence |
| --- | --- | --- | --- | --- |
