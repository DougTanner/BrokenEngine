# Skill Creator and Validator Worker

Mode selection and executor boundaries. Triggers, inputs, and handoffs live in
[`../SKILL.md`](../SKILL.md).

## Steps

1. Read the supplied `mode` and reject any value other than `author` or
   `validate`. Done when exactly one procedure is selected.
2. In author mode, read [`authoring.md`](authoring.md) completely and perform
   its procedure. Done when its completion conditions are met.
3. In validate mode, read [`semantic-review.md`](semantic-review.md) and
   [`frontmatter-schema.md`](frontmatter-schema.md) completely, then perform the
   review procedure without editing the target. Done when its completion
   conditions are met.
4. Return the mode-specific extension fields from [`../SKILL.md`](../SKILL.md)
   with the shared handoff. Done when every declared field is filled.

## Rules

- Treat author and validate as separate executions. Independent validation uses
  a fresh reviewer that did not author the package.
- Stay within the supplied package scope and return separate-role work to the
  manager.
