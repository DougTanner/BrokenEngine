# Sweep type: document

Audits and fixes `AGENTS.md` files or other documentation the sweep Plan
targets.

## Find rules

Hand-read every line of every unit file against the `AGENTS.md` audit rules
`/update-claude-docs` owns: `.agents/skills/update-claude-docs/references/audit-mode.md`
`## Rubric` and `.agents/skills/update-claude-docs/references/content-rules.md`.
The sweep Plan's `## Sweep rules` section adds to them. A finding's `Rule`
names the rubric item, content-rules section, or Plan rule it breaks.

## Fix bound

A fix may reword, move, merge, or delete text, provided every fact, command,
path, and rule still holds somewhere with the same meaning. A fix that would
change what an instruction requires is out of bound. A fix that moves a fact
out of the unit, or changes a reference outside the unit, is `cross-file`.

## Deferred fixes

Record: `Documents/Investigations/ChangeWorkflow/<Plan file stem>DeferredFixes.md`
