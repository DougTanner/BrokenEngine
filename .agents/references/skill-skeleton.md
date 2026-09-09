# Skill Skeleton

The consumption shapes and body order every `.agents/skills/` skill follows,
and the checklist a reviewer applies to it. Frontmatter and package mechanics
belong to `/external-skill-creator` validation mode; layering and size
thresholds belong to `/progressive-disclosure-review`.

## Consumption shapes

Choose a shape by who consumes the workflow. The presence of
`references/worker.md` is the mechanical discriminator; host `context` and
`agent` metadata do not select a shape. Their paired-field rule belongs to the
[`frontmatter-schema.md` Fields](../skills/external-skill-creator/references/frontmatter-schema.md#fields).

- A main-session skill has no `references/worker.md`. Its complete workflow is
  consumed by the invoking session, which may delegate work. Put every ordered
  section in `SKILL.md`; apply [`type-main-session.md`](type-main-session.md).
- A subagent skill has `references/worker.md`. Its `SKILL.md` is the public
  dispatcher contract, and its worker reads the public file plus the private
  executor procedure. Split the ordered sections as described below; apply
  [`type-subagent.md`](type-subagent.md).

## Section order

1. `Purpose` (`SKILL.md`) — what the skill produces, in at most 3 lines.
2. `When to use` (`SKILL.md`) — triggers only.
3. `Inputs` (`SKILL.md`) — which task-brief fields from
   [`subagent-reporting.md`](subagent-reporting.md) the skill consumes.
4. `Steps` — numbered, one imperative each, each ending on a checkable
   done-condition, no paragraph over 4 lines.
5. `Handoff` (`SKILL.md`) — the skill's extension fields, fixed shared values,
   and narrowed row forms as plain lines, or as a row-form fence holding no
   shared field name; it never re-renders the shared form from
   [`subagent-reporting.md`](subagent-reporting.md), `## Handoffs`. Applying
   that reference's 'What main does with each field' table, mandate inline only
   what the table gives main an action for, and no more text than that action
   needs, however short the excess is; that reference's `## Handoffs` size caps
   still govern the return. An extension field's own line counts when main acts
   on it as it acts on the field it extends, text main must present or ask
   verbatim stays inline per `## Section placement`, and everything else
   is cited as path plus selector. A field that judges each item of a supplied
   list carries a `<n>/<total>` count line, `<n>` named for what it counts,
   closed by the field's own passing word (`passed`, `traced`, `settled`),
   followed only by the items that did not pass, each citing where its settling
   evidence lives as a path plus selector or as the handoff row that holds it,
   never restating it.
6. `Rules` — judgment no step owns, as bullets.
7. `References` (`SKILL.md`) — each linked file owning one topic.

Omit a section the skill has no content for; never reorder.

## Section placement

Every section of a main-session skill belongs in `SKILL.md`.

For a subagent skill, `Purpose`, `When to use`, `Inputs`, `Handoff`, and
`References` belong in `SKILL.md`; `Steps` and `Rules` belong in
`references/worker.md`. `SKILL.md` is read by the dispatcher and executor;
`references/worker.md` is read only by the executor. Text the dispatcher must
present or ask verbatim is a subsection of `Handoff`.

`SKILL.md` links `references/worker.md` from `References` as its worker-entry
line, which opens with the private marker so a session that only dispatches
never loads the private steps into its context:

```
- [`references/worker.md`](references/worker.md) — private: read it only if you are the session executing this skill. <what it holds>.
```

`SKILL.md` on its own must suffice for main to dispatch the worker and to
present or ask anything verbatim; a `SKILL.md` missing text main must deliver
verbatim is the failure this rule prevents. Text main reads to decide whether or
how to dispatch — triggers, inputs, verbatim presentations and questions — is
public even when it would otherwise fall under `Steps` or `Rules`.

Each other reference is linked from `references/worker.md` when only the
executor reads it, or from `SKILL.md` when the dispatcher reads it. A
main-session skill links every needed reference from `SKILL.md`.

## Shared vocabulary

Status and severity words are the ones the shared handoff form in
`subagent-reporting.md` lists. `Critical` and `Required` are defined in
`.agents/skills/repo-code-review/SKILL.md`, and `Recommended` in
`.agents/skills/external-skill-creator/SKILL.md` validation mode. Introduce no
other status or severity term.

## Reviewer checklist

- Every action the previous body required is present exactly once across the
  files the selected consumption shape permits.
- Worker-reference presence, section placement, and host metadata follow
  `## Consumption shapes` and `## Section placement`.
- A subagent `SKILL.md` links `references/worker.md` with the private marker
  line from `## Section placement`; a main-session package has no worker file.
- `## Handoff` follows `## Section order` item 5: it renders no return fence
  holding a shared handoff field, and it mandates inline only what item 5
  allows.
- Size and layering pass `/progressive-disclosure-review`.
